#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <Pixel.hpp>
#include <Adafruit_GFX_Pixel.hpp>
#include <U8g2_for_Adafruit_GFX.h>
#include "WeatherHelper.h"

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <vector>

PixelClass Pixel(Serial2, 22, 22);
Adafruit_Pixel Pixel_GFX(Pixel, 84, 1); // Address 1
U8G2_FOR_ADAFRUIT_GFX u8g2_gfx;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.nask.pl", 7200); // GMT+2
AsyncWebServer server(80);

// App Settings
std::vector<String> playlist = {"Hello"};
bool showClock = true;
bool showDate = true;
bool showCustom = true;
bool showWeather = true;
bool showAnalogClock = true;
bool showCombine = true;
bool showDrawing = true;
int rotationSpeed = 20; 
uint16_t drawBoard[84]; // Persistent 84x16 drawing
bool forceRefresh = false;

WeatherData currentWeather = {0, 0, false};
uint32_t lastWeatherUpdate = 0;

// Fonts - maximum size for Clock/Date and minimal for Text
#define FONT_TEXT u8g2_font_unifont_t_polish    // Supports Polish
#define FONT_CLOCK u8g2_font_logisoso16_tn      // Large, 16px high numbers
#define FONT_DATE u8g2_font_logisoso16_tf       // Large, 16px high proportional

// Helper to decode basic HTML entities like &#347; (ś)
String decodeEntities(String str) {
  String out = "";
  for (int i = 0; i < (int)str.length(); i++) {
    if (str[i] == '&' && i + 1 < (int)str.length() && str[i + 1] == '#') {
      int end = str.indexOf(';', i);
      if (end != -1) {
        int code = str.substring(i + 2, end).toInt();
        if (code > 0) {
          // UTF-8 encoding
          if (code < 0x80) {
            out += (char)code;
          } else if (code < 0x800) {
            out += (char)(0xC0 | (code >> 6));
            out += (char)(0x80 | (code & 0x3F));
          } else {
            out += (char)(0xE0 | (code >> 12));
            out += (char)(0x80 | ((code >> 6) & 0x3F));
            out += (char)(0x80 | (code & 0x3F));
          }
          i = end;
          continue;
        }
      }
    }
    out += str[i];
  }
  return out;
}

void saveConfig() {
  File f = LittleFS.open("/config.txt", "w");
  if (f) {
    f.println(showClock);
    f.println(showDate);
    f.println(showCustom);
    f.println(showWeather);
    f.println(showAnalogClock);
    f.println(showCombine);
    f.println(showDrawing);
    f.println(rotationSpeed);
    for (int i = 0; i < 84; i++) f.println(drawBoard[i]);
    for (const String& s : playlist) {
      if (s.length() > 0) f.println(s);
    }
    f.close();
    Serial.println("Config Saved");
  }
}

void loadConfig() {
  if (LittleFS.exists("/config.txt")) {
    File f = LittleFS.open("/config.txt", "r");
    if (f) {
      showClock = f.readStringUntil('\n').toInt();
      showDate = f.readStringUntil('\n').toInt();
      showCustom = f.readStringUntil('\n').toInt();
      showWeather = f.readStringUntil('\n').toInt();
      showAnalogClock = f.readStringUntil('\n').toInt();
      showCombine = f.readStringUntil('\n').toInt();
      showDrawing = f.readStringUntil('\n').toInt();
      String rotStr = f.readStringUntil('\n');
      rotStr.trim();
      if (rotStr.length() > 0) rotationSpeed = rotStr.toInt();
      
      for (int i = 0; i < 84; i++) {
        if (f.available()) drawBoard[i] = f.readStringUntil('\n').toInt();
        else drawBoard[i] = 0;
      }
      if (rotationSpeed < 2) rotationSpeed = 2;
      
      playlist.clear();
      while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) playlist.push_back(line);
      }
      f.close();
      if (playlist.empty()) playlist.push_back("HELLO");
      Serial.println("Config Loaded. Speed: " + String(rotationSpeed) + "s");
    }
  }
}

void drawUTF8Centered(const String& text, int y = 14) {
  int w = u8g2_gfx.getUTF8Width(text.c_str());
  int x = (84 - w) / 2;
  if (x < 0) x = 0;
  u8g2_gfx.setCursor(x, y);
  u8g2_gfx.print(text);
}

void drawAnalogClock(int h, int m, int cx = 42, int cy = 8) {
  int r = 7;   

  Pixel_GFX.drawCircle(cx, cy, r, 1);
  
  float mAngle = (m * 6 - 90) * 0.0174533f;
  Pixel_GFX.drawLine(cx, cy, cx + (int)(cos(mAngle) * 6), cy + (int)(sin(mAngle) * 6), 1);
  
  float hAngle = ((h % 12) * 30 + m * 0.5f - 90) * 0.0174533f;
  Pixel_GFX.drawLine(cx, cy, cx + (int)(cos(hAngle) * 4), cy + (int)(sin(hAngle) * 4), 1);

  // 12 hour markers (dots inside the circle)
  for (int i = 0; i < 12; i++) {
    float angle = (i * 30 - 90) * 0.0174533f;
    int px = cx + (int)(cos(angle) * 6); // Radius 6 to be "glued" to the circle (7)
    int py = cy + (int)(sin(angle) * 6);
    Pixel_GFX.drawPixel(px, py, 1);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== BOOTING PIXEL FLIPDOT V2.8 ===\n");
  
  Serial2.begin(19200, SERIAL_8E1, 19, 18);
  Pixel.begin();
  
  if(!LittleFS.begin(true)){
    Serial.println("LittleFS FAILED");
  } else {
    loadConfig();
    Serial.println("LittleFS OK");
  }

  Pixel_GFX.init();
  u8g2_gfx.begin(Pixel_GFX);
  u8g2_gfx.setFontMode(1); 
  u8g2_gfx.setFontDirection(0);
  u8g2_gfx.setForegroundColor(1); 

  Pixel_GFX.selectBuffer(0);
  Pixel_GFX.fillScreen(0);
  u8g2_gfx.setFont(FONT_TEXT); 
  drawUTF8Centered("WiFi...");
  Pixel_GFX.commitBufferToPage(0);

  WiFiManager wm;
  wm.setConnectTimeout(30);
  if (!wm.autoConnect("Pixel_Flipdot_AP")) {
    delay(3000);
    ESP.restart();
  }

  Serial.println("WiFi Connected!");
  
  // Setup MDNS
  if (MDNS.begin("flipdot")) {
    Serial.println("MDNS responder started: flipdot.local");
  }

  timeClient.begin();
  timeClient.forceUpdate();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String playlistJson = "[";
    for (size_t i=0; i<playlist.size(); i++) {
        String s = playlist[i];
        s.replace("\"", "\\\"");
        playlistJson += "\"" + s + "\"" + (i == playlist.size()-1 ? "" : ",");
    }
    playlistJson += "]";

    String boardData = "[";
    for(int i=0; i<84; i++) boardData += String(drawBoard[i]) + (i==83?"":",");
    boardData += "]";

    String html = "<!DOCTYPE html><html lang='pl'><head>"
                  "<meta charset='UTF-8'>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, user-scalable=0'>"
                  "<title>Flipdot Dashboard</title>"
                  "<style>"
                  "body{background:#0d1117;color:#c9d1d9;font-family:'Segoe UI',system-ui,sans-serif;margin:0;padding:20px;display:flex;flex-direction:column;align-items:center;}"
                  "h1{color:#58a6ff;margin:0 0 5px;font-size:1.8em;}"
                  ".subtitle{color:#8b949e;font-size:0.9em;margin-bottom:30px;}"
                  ".container{width:100%;max-width:1000px;}"
                  ".section-title{font-size:0.8em;color:#8b949e;text-transform:uppercase;margin:30px 0 15px;font-weight:bold;letter-spacing:1.5px;text-align:left;width:100%;}"
                  
                  ".tile-grid{display:grid;grid-template-columns:repeat(auto-fit, minmax(130px, 1fr));gap:12px;width:100%;}"
                  ".tile{background:#161b22;border:1px solid #30363d;border-radius:14px;padding:20px 10px;cursor:pointer;text-align:center;transition:0.2s cubic-bezier(0.4,0,0.2,1);position:relative;display:flex;flex-direction:column;gap:10px;user-select:none;}"
                  ".tile:hover{border-color:#8b949e;background:#1c2128;}"
                  ".tile.active{background:rgba(35, 134, 54, 0.15);border-color:#2ea043;box-shadow:0 0 15px rgba(35,134,54,0.1);}"
                  ".tile.active .icon{color:#3fb950;}"
                  ".tile.active .label{color:#fff;}"
                  ".tile .icon{font-size:1.8em;color:#8b949e;transition:0.2s;}"
                  ".tile .label{font-size:0.85em;font-weight:600;color:#8b949e;}"
                  ".tile input{display:none;}"
                  
                  ".board-card{background:#161b22;border:1px solid #30363d;border-radius:16px;padding:20px;width:100%;box-sizing:border-box;margin-bottom:20px;}"
                  ".canvas-wrapper{width:100%;overflow-x:auto;background:#000;border-radius:8px;border:2px solid #30363d;margin-bottom:15px;touch-action:none;}"
                  "canvas{display:block;width:100%;height:auto;image-rendering:pixelated;cursor:crosshair;}"
                  
                  ".msg-card{background:#161b22;border:1px solid #30363d;border-radius:16px;padding:20px;width:100%;box-sizing:border-box;}"
                  ".list-item{display:flex;justify-content:space-between;align-items:center;background:#0d1117;margin:8px 0;padding:12px 16px;border-radius:10px;border:1px solid #30363d;}"
                  ".del-btn{background:transparent;color:#f85149;border:1px solid #444;border-radius:6px;padding:6px 12px;cursor:pointer;}"
                  "input[type=text], input[type=number]{background:#0d1117;border:1px solid #30363d;color:white;padding:14px;border-radius:10px;font-size:1em;width:100%;box-sizing:border-box;margin-bottom:10px;}"
                  ".btn{border:none;padding:14px;border-radius:10px;font-size:1em;cursor:pointer;font-weight:bold;transition:0.2s;}"
                  ".btn-blue{background:#1f6feb;color:white;width:100%;}"
                  ".btn-green{background:#238636;color:white;width:100%;margin-top:20px;font-size:1.1em;}"
                  ".btn-red{background:rgba(248,81,73,0.1);color:#f85149;border:1px solid #f85149;}"
                  ".btn:hover{filter:brightness(1.2);}"
                  
                  ".speed-row{display:flex;align-items:center;gap:15px;margin-top:20px;}"
                  "</style></head><body>"
                  
                  "<div class='container'>"
                  "<h1>Smart Flipdot</h1>"
                  "<div class='subtitle'>Pixel v3.9 | <a href='http://flipdot.local'>flipdot.local</a></div>"
                  
                  "<form action='/save' method='POST' id='mainForm'>"
                  "<div class='section-title'>Control Panel</div>"
                  "<div class='tile-grid'>"
                  "  <div class='tile " + String(showClock?"active":"") + "' onclick='toggleTile(this)'>"
                  "    <i class='icon'>🕒</i><span class='label'>Digital</span><input type='checkbox' name='c1' " + String(showClock?"checked":"") + "></div>"
                  "  <div class='tile " + String(showAnalogClock?"active":"") + "' onclick='toggleTile(this)'>"
                  "    <i class='icon'>🕰️</i><span class='label'>Analog</span><input type='checkbox' name='c5' " + String(showAnalogClock?"checked":"") + "></div>"
                  "  <div class='tile " + String(showCombine?"active":"") + "' onclick='toggleTile(this)'>"
                  "    <i class='icon'>🌓</i><span class='label'>Combine</span><input type='checkbox' name='c6' " + String(showCombine?"checked":"") + "></div>"
                  "  <div class='tile " + String(showDrawing?"active":"") + "' onclick='toggleTile(this)'>"
                  "    <i class='icon'>🎨</i><span class='label'>Drawing</span><input type='checkbox' name='c7' " + String(showDrawing?"checked":"") + "></div>"
                  "  <div class='tile " + String(showDate?"active":"") + "' onclick='toggleTile(this)'>"
                  "    <i class='icon'>📅</i><span class='label'>Date</span><input type='checkbox' name='c2' " + String(showDate?"checked":"") + "></div>"
                  "  <div class='tile " + String(showWeather?"active":"") + "' onclick='toggleTile(this)'>"
                  "    <i class='icon'>☀️</i><span class='label'>Weather</span><input type='checkbox' name='c4' " + String(showWeather?"checked":"") + "></div>"
                  "  <div class='tile " + String(showCustom?"active":"") + "' onclick='toggleTile(this)'>"
                  "    <i class='icon'>💬</i><span class='label'>Messages</span><input type='checkbox' name='c3' " + String(showCustom?"checked":"") + "></div>"
                  "</div>"

                  "<div class='section-title'>Interactive Tablica (84x16)</div>"
                  "<div class='board-card'>"
                  "  <div class='canvas-wrapper'><canvas id='paintCanvas' width='840' height='160'></canvas></div>"
                  "  <div style='display:flex; gap:10px;'>"
                  "    <button type='button' class='btn btn-red' style='flex:1' onclick='clearCanvas()'>CLEAR</button>"
                  "    <button type='button' class='btn btn-blue' style='flex:2' onclick='saveCanvas()'>SAVE PERMANENTLY</button>"
                  "  </div>"
                  "</div>"

                  "<div class='section-title'>Message Playlist</div>"
                  "<div class='msg-card'>"
                  "  <div id='msgList'></div>"
                  "  <div style='margin-top:20px;'>"
                  "    <input type='text' id='newMsg' placeholder='Write something...'>"
                  "    <button type='button' class='btn btn-blue' onclick='addMsg()'>ADD TO LIST</button>"
                  "  </div>"
                  "  <div class='speed-row'>"
                  "    <span style='font-size:0.9em;color:#8b949e'>Rotation:</span>"
                  "    <input type='number' name='speed' value='" + String(rotationSpeed) + "' style='width:100px;margin:0'> "
                  "    <span style='font-size:0.9em;color:#8b949e'>sec</span>"
                  "  </div>"
                  "</div>"

                  "<input type='hidden' name='msgs' id='msgsInput'>"
                  "<button type='submit' class='btn btn-green'>APPLY ALL CHANGES</button>"
                  "</form>"
                  "</div>"

                  "<script>"
                  "function toggleTile(el) { el.classList.toggle('active'); const cb = el.querySelector('input'); cb.checked = !cb.checked; }"
                  "const canvas = document.getElementById('paintCanvas');"
                  "const ctx = canvas.getContext('2d');"
                  "const W = 84, H = 16, SCALE = 10;"
                  "let drawing = false; let lastX = -1, lastY = -1;"
                  "let boardRaw = " + boardData + ";"
                  "let board = Array(W).fill(0).map((_, x) => Array(H).fill(0).map((_, y) => (boardRaw[x] & (1 << y)) ? 1 : 0));"
                  
                  "function initBoard() {"
                  "  ctx.fillStyle = '#000'; ctx.fillRect(0,0,840,160);"
                  "  ctx.strokeStyle = '#111'; ctx.lineWidth=0.5;"
                  "  for(let i=0; i<=W; i++) { ctx.beginPath(); ctx.moveTo(i*SCALE,0); ctx.lineTo(i*SCALE,160); ctx.stroke(); }"
                  "  for(let i=0; i<=H; i++) { ctx.beginPath(); ctx.moveTo(0,i*SCALE); ctx.lineTo(840,i*SCALE); ctx.stroke(); }"
                  "  for(let x=0; x<W; x++) for(let y=0; y<H; y++) if(board[x][y]) { ctx.fillStyle='#ffdd00'; ctx.fillRect(x*SCALE+1, y*SCALE+1, SCALE-2, SCALE-2); }"
                  "}"
                  
                  "function setPixel(x, y, on) {"
                  "  if (x < 0 || x >= W || y < 0 || y >= H) return;"
                  "  if (board[x][y] === on) return;"
                  "  board[x][y] = on; ctx.fillStyle = on ? '#ffdd00' : '#000';"
                  "  ctx.fillRect(x*SCALE+1, y*SCALE+1, SCALE-2, SCALE-2);"
                  "  fetch(`/api/draw?x=${x}&y=${y}&on=${on}`);"
                  "}"

                  "function handleMove(e) {"
                  "  if (!drawing) return;"
                  "  const rect = canvas.getBoundingClientRect();"
                  "  const ev = (e.touches && e.touches[0]) || e;"
                  "  const x = Math.floor((ev.clientX - rect.left) / (rect.width / W));"
                  "  const y = Math.floor((ev.clientY - rect.top) / (rect.height / H));"
                  "  if (x !== lastX || y !== lastY) { setPixel(x, y, 1); lastX = x; lastY = y; }"
                  "}"

                  "canvas.onmousedown = (e) => { drawing = true; handleMove(e); };"
                  "window.onmouseup = () => { drawing = false; lastX = -1; lastY = -1; };"
                  "canvas.onmousemove = handleMove;"
                  "canvas.addEventListener('touchstart', (e) => { drawing = true; handleMove(e); e.preventDefault(); }, {passive:false});"
                  "canvas.addEventListener('touchmove', (e) => { handleMove(e); e.preventDefault(); }, {passive:false});"
                  "canvas.addEventListener('touchend', () => { drawing = false; });"

                  "function clearCanvas() { if(confirm('Clear all?')) { fetch('/api/draw?clear=1').then(() => { board.forEach(c => c.fill(0)); initBoard(); }); } }"
                  "function saveCanvas() { fetch('/api/draw?save=1').then(() => alert('Successfully saved to Flash! 🏮')); }"

                  "let playlist = " + playlistJson + ";"
                  "function render() {"
                  "  const list = document.getElementById('msgList'); list.innerHTML = '';"
                  "  playlist.forEach((m, i) => {"
                  "    list.innerHTML += `<div class='list-item'><span>${m}</span><button type='button' class='del-btn' onclick='delMsg(${i})'>&times;</button></div>`;"
                  "  });"
                  "  document.getElementById('msgsInput').value = playlist.join('|');"
                  "}"
                  "function addMsg() {"
                  "  const input = document.getElementById('newMsg'); if(input.value) { playlist.push(input.value); input.value = ''; render(); }"
                  "}"
                  "function delMsg(i) { playlist.splice(i, 1); render(); }"
                  "document.getElementById('newMsg').addEventListener('keypress', (e) => { if(e.key === 'Enter') { e.preventDefault(); addMsg(); } });"
                  "initBoard(); render();"
                  "</script></body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    showClock = request->hasParam("c1", true);
    showDate = request->hasParam("c2", true);
    showCustom = request->hasParam("c3", true);
    showWeather = request->hasParam("c4", true);
    showAnalogClock = request->hasParam("c5", true);
    showCombine = request->hasParam("c6", true);
    showDrawing = request->hasParam("c7", true);
    if (request->hasParam("speed", true)) rotationSpeed = request->getParam("speed", true)->value().toInt();
    if (rotationSpeed < 2) rotationSpeed = 2;

    if (request->hasParam("msgs", true)) {
        String msgs = request->getParam("msgs", true)->value();
        playlist.clear();
        int lastPos = 0;
        int nextPos;
        while ((nextPos = msgs.indexOf('|', lastPos)) != -1) {
            String m = msgs.substring(lastPos, nextPos); m.trim();
            if (m.length() > 0) playlist.push_back(m);
            lastPos = nextPos + 1;
        }
        String last = msgs.substring(lastPos); last.trim();
        if (last.length() > 0) playlist.push_back(last);
    }
    saveConfig();
    forceRefresh = true;
    request->redirect("/");
  });

  server.on("/api/draw", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("clear")) {
        memset(drawBoard, 0, sizeof(drawBoard));
        saveConfig();
        forceRefresh = true;
        request->send(200, "text/plain", "Cleared");
        return;
    }
    if (request->hasParam("x") && request->hasParam("y") && request->hasParam("on")) {
        int x = request->getParam("x")->value().toInt();
        int y = request->getParam("y")->value().toInt();
        int on = request->getParam("on")->value().toInt();
        if (x >= 0 && x < 84 && y >= 0 && y < 16) {
            if (on) drawBoard[x] |= (1 << y);
            else drawBoard[x] &= ~(1 << y);
            // Instant feedback: jump to drawing screen (masterIdx 5)
            static uint32_t lastApiUpdate = 0;
            if (millis() - lastApiUpdate > 500) { // Throttle re-renders and saves
                lastApiUpdate = millis();
                forceRefresh = true;
                // We'll let loop() pick it up
            }
        }
        request->send(200, "text/plain", "OK");
    } else if (request->hasParam("save")) {
        saveConfig();
        request->send(200, "text/plain", "Saved");
    } else {
        request->send(400, "text/plain", "Bad Request");
    }
  });

  server.begin();
  Serial.println("Web Server running.");
}

void loop() {
  static uint32_t lastNTP = 0;
  if (millis() - lastNTP > 3600000) { lastNTP = millis(); timeClient.forceUpdate(); }
  
  if (WiFi.status() == WL_CONNECTED && (millis() - lastWeatherUpdate > 300000 || lastWeatherUpdate == 0)) {
    lastWeatherUpdate = millis();
    currentWeather = WeatherHelper::getWSWWeather();
  }
  
  timeClient.update();
  time_t now = timeClient.getEpochTime();
  struct tm * timeinfo = localtime(&now);

  char timeStr[6]; sprintf(timeStr, "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
  char dateStr[9]; sprintf(dateStr, "%02d.%02d.%02d", timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year % 100);

  static uint32_t lastToggle = 0;
  static int masterIdx = 0; 
  
  if (millis() - lastToggle > (rotationSpeed * 1000) || forceRefresh) {
    lastToggle = millis();
    forceRefresh = false;

    String toShow = "";
    int attempts = 0;
    while (attempts < 20) {
        masterIdx = (masterIdx + 1) % (6 + playlist.size());
        
        if (masterIdx == 0 && showClock) { 
            u8g2_gfx.setFont(FONT_CLOCK);
            toShow = timeStr; break; 
        }
        if (masterIdx == 1 && showDate) { 
            u8g2_gfx.setFont(FONT_DATE);
            toShow = dateStr; break; 
        }
        if (masterIdx == 2 && showWeather && currentWeather.valid) {
            // Weather screen handled specially
            break;
        }
        if (masterIdx == 3 && showAnalogClock) {
            // Analog clock handled specially
            break;
        }
        if (masterIdx == 4 && showCombine) {
            // Combine mode handled specially
            break;
        }
        if (masterIdx == 5 && showDrawing) {
            // Drawing screen
            break;
        }
        if (masterIdx >= 6 && showCustom) {
            int pIdx = masterIdx - 6;
            if (pIdx >= 0 && pIdx < (int)playlist.size()) { 
                u8g2_gfx.setFont(FONT_TEXT);
                toShow = playlist[pIdx]; break; 
            }
        }
        attempts++;
    }

    if (masterIdx == 2 && showWeather && currentWeather.valid) {
        Serial.println("Updating display: Weather");
        Pixel_GFX.selectBuffer(0);
        Pixel_GFX.fillScreen(0);
        
        const uint8_t* icon = WeatherHelper::getIconForCode(currentWeather.code, currentWeather.is_day, currentWeather.wind_speed);
        Pixel_GFX.drawXBitmap(0, 0, icon, 16, 16, 1);
        
        u8g2_gfx.setFont(u8g2_font_unifont_t_polish);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", currentWeather.temp);
        String tNum = String(buf);
        int wNum = u8g2_gfx.getUTF8Width(tNum.c_str());
        int wC = u8g2_gfx.getUTF8Width("C");
        int totalW = wNum + 5 + wC; // 5 = gap(1) + circle(3) + gap(1)
        
        int startX = 83 - totalW; // Leaves 1px empty on the right (pixel 83)
        
        u8g2_gfx.setCursor(startX, 13); 
        u8g2_gfx.print(tNum);
        
        int degX = startX + wNum + 1;
        Pixel_GFX.drawCircle(degX + 1, 3, 1, 1);
        u8g2_gfx.setCursor(degX + 4, 13);
        u8g2_gfx.print("C");
        
        Pixel_GFX.commitBufferToPage(0);
        delay(200);
    } else if (masterIdx == 3 && showAnalogClock) {
        Serial.println("Updating display: Analog Clock");
        Pixel_GFX.selectBuffer(0);
        Pixel_GFX.fillScreen(0);
        drawAnalogClock(timeinfo->tm_hour, timeinfo->tm_min);
        Pixel_GFX.commitBufferToPage(0);
        delay(200);
    } else if (masterIdx == 4 && showCombine) {
        Serial.println("Updating display: Combine");
        Pixel_GFX.selectBuffer(0);
        Pixel_GFX.fillScreen(0);
        
        u8g2_gfx.setFont(FONT_CLOCK);
        int wDig = u8g2_gfx.getUTF8Width(timeStr);
        int totalW = 14 + 4 + wDig; // Analog(14) + Gap(4) + Digital
        int startX = (84 - totalW) / 2;
        
        // Draw Analog on Left
        drawAnalogClock(timeinfo->tm_hour, timeinfo->tm_min, startX + 7, 8);
        
        // Draw Digital on Right
        u8g2_gfx.setCursor(startX + 14 + 4, 16);
        u8g2_gfx.print(timeStr);
        
        Pixel_GFX.commitBufferToPage(0);
        delay(200);
    } else if (masterIdx == 5 && showDrawing) {
        // Just draw the board
        Pixel_GFX.selectBuffer(0);
        Pixel_GFX.fillScreen(0);
        for(int x=0; x<84; x++) {
            for(int y=0; y<16; y++) {
                if (drawBoard[x] & (1 << y)) Pixel_GFX.drawPixel(x, y, 1);
            }
        }
        Pixel_GFX.commitBufferToPage(0);
        delay(500); 
    } else if (toShow != "") {
      Serial.println("Updating display: " + toShow);
      Pixel_GFX.selectBuffer(0);
      Pixel_GFX.fillScreen(0);
      // yPos: 16 for large fonts (fills rows 0-15), 14 for narrow text (baseline)
      int yPos = (u8g2_gfx.getFontAscent() > 13 ? 16 : 14);
      drawUTF8Centered(toShow, yPos);
      Pixel_GFX.commitBufferToPage(0);
      delay(200); 
    }
  }
}