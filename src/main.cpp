#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Pixel.hpp>
#include <Adafruit_GFX_Pixel.hpp>
#include <U8g2_for_Adafruit_GFX.h>

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
int rotationSpeed = 20; 
bool forceRefresh = false;

// Fonts - maximum size for Clock/Date and minimal for Text
#define FONT_TEXT u8g2_font_6x13_tf         // Narrow, supports Polish
#define FONT_CLOCK u8g2_font_logisoso16_tn  // Large, 16px high numbers
#define FONT_DATE u8g2_font_logisoso16_tf   // Large, 16px high proportional

void saveConfig() {
  File f = LittleFS.open("/config.txt", "w");
  if (f) {
    f.println(showClock);
    f.println(showDate);
    f.println(showCustom);
    f.println(rotationSpeed);
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
      String rotStr = f.readStringUntil('\n');
      if (rotStr.length() > 0) rotationSpeed = rotStr.toInt();
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

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== BOOTING PIXEL FLIPDOT V2.7 ===\n");
  
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
  timeClient.begin();
  timeClient.forceUpdate();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String playlistJson = "[";
    for (size_t i=0; i<playlist.size(); i++) {
        playlistJson += "\"" + playlist[i] + "\"" + (i == playlist.size()-1 ? "" : ",");
    }
    playlistJson += "]";

    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<style>"
                  "body{background:#0d1117;color:#c9d1d9;font-family:'Segoe UI',Roboto,Helvetica,Arial,sans-serif;text-align:center;padding:15px;line-height:1.5;}"
                  "h1{color:#58a6ff;margin-bottom:20px;font-weight:600;font-size:1.8em;letter-spacing:-0.5px;}"
                  ".card{background:#161b22;padding:24px;border-radius:16px;margin:10px auto;max-width:450px;text-align:left;border:1px solid #30363d;box-shadow:0 8px 24px rgba(0,0,0,0.3);}"
                  ".section-title{font-size:0.9em;color:#8b949e;text-transform:uppercase;margin:20px 0 10px;font-weight:bold;letter-spacing:1px;}"
                  ".row{display:flex;justify-content:space-between;align-items:center;padding:14px 0;border-bottom:1px solid #30363d;}"
                  ".switch{position:relative;display:inline-block;width:48px;height:24px;}"
                  ".switch input{opacity:0;width:0;height:0;}"
                  ".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#30363d;transition:.3s;border-radius:24px;}"
                  ".slider:before{position:absolute;content:'';height:18px;width:18px;left:3px;bottom:3px;background-color:white;transition:.3s;border-radius:50%;}"
                  "input:checked + .slider{background-color:#238636;}"
                  "input:checked + .slider:before{transform:translateX(24px);}"
                  ".list-item{display:flex;justify-content:space-between;align-items:center;background:#0d1117;margin:8px 0;padding:12px;border-radius:8px;border:1px solid #30363d;transition:0.2s;}"
                  ".list-item:hover{border-color:#58a6ff;}"
                  ".del-btn{background:transparent;color:#f85149;border:1px solid #30363d;border-radius:6px;padding:6px 12px;cursor:pointer;font-weight:bold;transition:0.2s;}"
                  ".del-btn:hover{background:#da3633;color:white;border-color:#da3633;}"
                  ".add-row{display:flex;flex-direction:column;margin-top:20px;gap:12px;}"
                  "input[type=text], input[type=number]{background:#0d1117;border:1px solid #30363d;color:white;padding:12px;border-radius:8px;outline:none;font-size:1em;transition:0.2s;width:100%;box-sizing:border-box;}"
                  "input[type=text]:focus{border-color:#58a6ff;box-shadow:0 0 0 3px rgba(88,166,255,0.1);}"
                  ".add-btn{background:#1f6feb;color:white;border:none;padding:12px;border-radius:8px;cursor:pointer;font-weight:bold;font-size:1em;width:100%;transition:0.2s;}"
                  ".add-btn:hover{background:#388bfd;}"
                  ".save-btn{background:#238636;color:white;border:none;padding:16px;width:100%;border-radius:10px;font-size:1.1em;cursor:pointer;margin-top:24px;font-weight:bold;box-shadow:0 4px 12px rgba(35,134,54,0.2);transition:0.2s;}"
                  ".save-btn:hover{background:#2ea043;transform:translateY(-1px);}"
                  "</style></head><body>"
                  "<h1>Smart Flipdot</h1>"
                  "<form id='configForm' action='/save' method='POST'>"
                  "<div class='card'>"
                  "<div class='section-title'>Control</div>"
                  "<div class='row'><span>Clock</span><label class='switch'><input type='checkbox' name='c1' " + String(showClock?"checked":"") + "><span class='slider'></span></label></div>"
                  "<div class='row'><span>Date</span><label class='switch'><input type='checkbox' name='c2' " + String(showDate?"checked":"") + "><span class='slider'></span></label></div>"
                  "<div class='row'><span>Playlist</span><label class='switch'><input type='checkbox' name='c3' " + String(showCustom?"checked":"") + "><span class='slider'></span></label></div>"
                  "<div class='row'><span>Rotation (sec)</span><input type='number' name='speed' value='" + String(rotationSpeed) + "' style='width:80px;'></div>"
                  "<div class='section-title'>Messages</div>"
                  "<div id='msgList'></div>"
                  "<div class='add-row'><input type='text' id='newMsg' placeholder='Write...'><button type='button' class='add-btn' onclick='addMsg()'>ADD TO LIST</button></div>"
                  "<input type='hidden' name='msgs' id='msgsInput'>"
                  "<button type='submit' class='save-btn'>SAVE & UPDATE DISPLAY</button>"
                  "</div></form>"
                  "<script>"
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
                  "render();"
                  "</script></body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    showClock = request->hasParam("c1", true);
    showDate = request->hasParam("c2", true);
    showCustom = request->hasParam("c3", true);
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

  server.begin();
  Serial.println("Web Server running.");
}

void loop() {
  static uint32_t lastNTP = 0;
  if (millis() - lastNTP > 3600000) { lastNTP = millis(); timeClient.forceUpdate(); }
  
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
        masterIdx = (masterIdx + 1) % (2 + (playlist.size() > 0 ? playlist.size() : 1));
        
        if (masterIdx == 0 && showClock) { 
            u8g2_gfx.setFont(FONT_CLOCK);
            toShow = timeStr; break; 
        }
        if (masterIdx == 1 && showDate) { 
            u8g2_gfx.setFont(FONT_DATE);
            toShow = dateStr; break; 
        }
        if (masterIdx >= 2 && showCustom) {
            int pIdx = masterIdx - 2;
            if (pIdx >= 0 && pIdx < playlist.size()) { 
                u8g2_gfx.setFont(FONT_TEXT);
                toShow = playlist[pIdx]; break; 
            }
        }
        attempts++;
    }

    if (toShow != "") {
      Serial.println("Updating display: " + toShow);
      Pixel_GFX.selectBuffer(0);
      Pixel_GFX.fillScreen(0);
      // yPos: 15 for Clock/Date (max height), 13 for narrow text (baseline)
      int yPos = (u8g2_gfx.getFontAscent() > 13 ? 15 : 13);
      drawUTF8Centered(toShow, yPos);
      Pixel_GFX.commitBufferToPage(0);
      delay(200); 
    }
  }
}