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
bool systemOn = true;
bool showNightMode = true;
int rotationSpeed = 20; 
uint16_t drawBoard[84]; 
bool forceRefresh = false;
uint32_t timerTarget = 0;
String timerMsg = "";
bool timerRunning = false;
int timerPhase = 0;

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
    f.println(systemOn);
    f.println(showNightMode);
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
      String s8 = f.readStringUntil('\n'); s8.trim(); if(s8.length()>0) systemOn = s8.toInt(); else systemOn = true;
      String s9 = f.readStringUntil('\n'); s9.trim(); if(s9.length()>0) showNightMode = s9.toInt(); else showNightMode = true;
      String rotStr = f.readStringUntil('\n'); rotStr.trim();
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
  Serial.println("\n\n=== BOOTING PIXEL FLIPDOT ===\n");
  
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
  Serial.print("Sync Time");
  for(int i=0; i<10 && !timeClient.update(); i++){ delay(500); Serial.print("."); }
  Serial.println(" OK");
  forceRefresh = true;

  server.on("/timer", HTTP_GET, [](AsyncWebServerRequest *request){
    uint32_t ms = 0;
    if(request->hasParam("h")) ms += request->getParam("h")->value().toInt() * 3600000;
    if(request->hasParam("m")) ms += request->getParam("m")->value().toInt() * 60000;
    if(request->hasParam("s")) ms += request->getParam("s")->value().toInt() * 1000;
    timerTarget = millis() + ms;
    if(request->hasParam("msg")) timerMsg = request->getParam("msg")->value();
    timerRunning = true; timerPhase = 0; request->send(200, "text/plain", "OK");
  });
  server.on("/timerStop", HTTP_GET, [](AsyncWebServerRequest *request){
    timerRunning = false; request->send(200, "text/plain", "OK");
  });

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
                  ":root{--bg:#0d1117;--card:#161b22;--border:#30363d;--text:#c9d1d9;--sub:#8b949e;--blue:#58a6ff;--green:#238636;--red:#f85149;}"
                  "body{background:var(--bg);color:var(--text);font-family:'Segoe UI',system-ui,sans-serif;margin:0;padding:20px;display:flex;flex-direction:column;align-items:center;}"
                  "h1{color:var(--blue);margin:0 0 5px;font-size:1.8em;text-align:center;width:100%;}"
                  ".subtitle{color:var(--sub);font-size:0.9em;margin-bottom:30px;text-align:center;width:100%;}"
                  ".subtitle a{color:var(--sub);text-decoration:underline;}"
                  ".container{width:100%;max-width:1000px;}"
                  ".section-title{font-size:0.8em;color:var(--sub);text-transform:uppercase;margin:40px 0 15px;font-weight:bold;letter-spacing:1.5px;text-align:left;width:100%;}"
                  
                  ".tile-grid{display:grid;grid-template-columns:repeat(auto-fit, minmax(105px, 1fr));gap:8px;width:100%;}"
                  ".tile{background:var(--card);border:1px solid var(--border);border-radius:14px;padding:15px 5px;cursor:pointer;text-align:center;transition:0.2s cubic-bezier(0.4,0,0.2,1);position:relative;display:flex;flex-direction:column;gap:8px;user-select:none;}"
                  ".tile:hover{border-color:var(--sub);background:#1c2128;}"
                  ".tile.active{background:rgba(35, 134, 54, 0.15);border-color:#2ea043;box-shadow:0 0 15px rgba(35,134,54,0.1);}"
                  ".tile.active .icon svg{stroke:#3fb950;}"
                  ".tile.active .label{color:#fff;}"
                  ".tile .icon svg{width:28px;height:28px;stroke:var(--sub);transition:0.2s;}"
                  ".tile .label{font-size:0.85em;font-weight:600;color:var(--sub);}"
                  ".tile input{display:none;}"
                  
                  ".board-card{background:var(--card);border:1px solid var(--border);border-radius:16px;padding:20px;width:100%;box-sizing:border-box;margin-bottom:20px;}"
                  ".canvas-wrapper{width:100%;overflow-x:auto;background:#000;border-radius:8px;border:2px solid var(--border);margin-bottom:15px;touch-action:none;}"
                  "canvas{display:block;width:100%;height:auto;image-rendering:pixelated;cursor:crosshair;}"
                  
                  ".msg-card{background:var(--card);border:1px solid var(--border);border-radius:16px;padding:20px;width:100%;box-sizing:border-box;}"
                  ".list-item{display:flex;justify-content:space-between;align-items:center;background:var(--bg);margin:8px 0;padding:12px 16px;border-radius:10px;border:1px solid var(--border);}"
                  ".del-btn{background:transparent;color:var(--red);border:1px solid #444;border-radius:6px;padding:6px 12px;cursor:pointer;}"
                  "input[type=text], input[type=number]{background:var(--bg);border:1px solid var(--border);color:white;padding:14px;border-radius:10px;font-size:1em;width:100%;box-sizing:border-box;margin-bottom:10px;}"
                  ".btn{border:none;padding:14px;border-radius:10px;font-size:1em;cursor:pointer;font-weight:bold;transition:0.2s;}"
                  ".btn-blue{background:#1f6feb;color:white;width:100%;}"
                  ".btn-green{background:var(--green);color:white;width:100%;margin-top:20px;font-size:1.1em;box-shadow:0 4px 15px rgba(35,134,54,0.3);}"
                  ".btn-red{background:rgba(248,81,73,0.1);color:var(--red);border:1px solid var(--red);}"
                  ".btn-gray{background:#30363d;color:#fff;border:1px solid var(--border);}"
                  ".btn:hover{filter:brightness(1.2);}"
                  
                  ".speed-card{background:var(--card);border:1px solid var(--border);border-radius:16px;padding:20px;width:100%;box-sizing:border-box;display:flex;justify-content:space-between;align-items:center;margin-top:20px;}"
                  ".switch{position:relative;display:inline-block;width:50px;height:28px;}.switch input{opacity:0;width:0;height:0;}"
                  ".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:rgba(248,81,73,0.1);transition:.3s;border-radius:28px;border:1px solid var(--red);}"
                  ".slider:before{position:absolute;content:\"\";height:20px;width:20px;left:3px;bottom:3px;background:var(--red);transition:.3s;border-radius:50%;}"
                  "input:checked+.slider{background:rgba(63,185,80,0.2);border-color:#3fb950;}input:checked+.slider:before{transform:translateX(22px);background:#3fb950;}"
                  ".btn-trash{background:var(--red);color:white;border:none;border-radius:6px;padding:6px;cursor:pointer;display:flex;align-items:center;justify-content:center;transition:0.2s;}"
                  ".btn-trash:hover{background:#ff615a;transform:scale(1.05);}.btn-trash svg{width:16px;height:16px;stroke:white;}"
                  ".footer{margin-top:50px;padding:20px;color:var(--sub);font-size:0.95em;border-top:1px solid var(--border);width:100%;max-width:1000px;text-align:center;}"
                  ".footer a{color:#79c0ff;text-decoration:none;font-weight:600;}"
                  "</style></head><body>"
                  " <div class='container'>"
                  
                  " <div style='display:flex;justify-content:space-between;align-items:center;width:100%;margin-bottom:5px;'>"
                  "  <h1 style='text-align:left;margin:0;'>Smart Flipdot</h1>"
                  "  <div style='display:flex;align-items:center;gap:6px;'>"
                  "    <span style='font-size:0.8em;color:var(--sub);font-weight:bold;'>OFF</span>"
                  "    <label class='switch'><input type='checkbox' id='pwr' " + String(systemOn?"checked":"") + " onclick='togglePower()'><span class='slider'></span></label>"
                  "    <span style='font-size:0.8em;color:var(--sub);font-weight:bold;'>ON</span>"
                  "  </div>"
                  " </div>"
                  " <div class='subtitle' style='text-align:left;'>Pixel v5.4 | <a href='http://flipdot.local'>flipdot.local</a></div>"
                  " <form action='/save' method='POST' id='mainForm'>"
                  " <div class='section-title'>Control Panel</div>"
                  " <div class='tile-grid'>"
                  "  <div class='tile " + String(showClock?"active":"") + "' onclick='toggleTile(this)'>"
                  "    <div class='icon'><svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5' stroke-linecap='round' stroke-linejoin='round'><circle cx='12' cy='12' r='10' stroke-width='2.5'/><path d='M12 7v5l4 2' stroke-width='2.5'/><path d='M12 2v2M12 20v2M20 12h2M2 12h2' stroke-width='1.5'/></svg></div><span class='label'>Digital</span><input type='checkbox' name='c1' " + String(showClock?"checked":"") + "></div>"
                  "  <div class='tile " + String(showAnalogClock?"active":"") + "' onclick='toggleTile(this)'>"
                  "    <div class='icon'><svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5' stroke-linecap='round' stroke-linejoin='round'><circle cx='12' cy='12' r='10' stroke-width='2.5'/><path d='M12 7v5l4 2' stroke-width='2.5'/><path d='M12 2v2M12 20v2M20 12h2M2 12h2' stroke-width='1.5'/></svg></div><span class='label'>Analog</span><input type='checkbox' name='c5' " + String(showAnalogClock?"checked":"") + "></div>"
                  "  <div class='tile " + String(showCombine?"active":"") + "' onclick='toggleTile(this)'>"
                  "    <div class='icon'><svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5' stroke-linecap='round' stroke-linejoin='round'><circle cx='12' cy='12' r='10' stroke-width='2.5'/><path d='M12 7v5l4 2' stroke-width='2.5'/><path d='M12 2v2M12 20v2M20 12h2M2 12h2' stroke-width='1.5'/></svg></div><span class='label'>Combine</span><input type='checkbox' name='c6' " + String(showCombine?"checked":"") + "></div>"
                  "  <div class='tile " + String(showDrawing?"active":"") + "' onclick='toggleTile(this)'>"
                  "    <div class='icon'><svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M12 19l7-7 3 3-7 7-3-3z'/><path d='M18 13l-1.5-7.5L2 2l3.5 14.5L13 18l5-5z'/><path d='M2 2l5 5'/><circle cx='12' cy='12' r='1'/></svg></div><span class='label'>Drawing</span><input type='checkbox' name='c7' " + String(showDrawing?"checked":"") + "></div>"
                  "  <div class='tile " + String(showDate?"active":"") + "' onclick='toggleTile(this)'>"
                  "    <div class='icon'><svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><rect x='3' y='4' width='18' height='18' rx='2' ry='2'/><line x1='16' y1='2' x2='16' y2='6'/><line x1='8' y1='2' x2='8' y2='6'/><line x1='3' y1='10' x2='21' y2='10'/></svg></div><span class='label'>Date</span><input type='checkbox' name='c2' " + String(showDate?"checked":"") + "></div>"
                  "  <div class='tile " + String(showWeather?"active":"") + "' onclick='toggleTile(this)'>"
                  "    <div class='icon'><svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><circle cx='12' cy='12' r='5'/><path d='M12 1v2'/><path d='M12 21v2'/><path d='M4.22 4.22l1.42 1.42'/><path d='M18.36 18.36l1.42 1.42'/><path d='M1 12h2'/><path d='M21 12h2'/><path d='M4.22 19.78l1.42-1.42'/><path d='M18.36 5.64l1.42-1.42'/></svg></div><span class='label'>Weather</span><input type='checkbox' name='c4' " + String(showWeather?"checked":"") + "></div>"
                  "  <div class='tile " + String(showCustom?"active":"") + "' onclick='toggleTile(this)'>"
                  "    <div class='icon'><svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z'/></svg></div><span class='label'>Messages</span><input type='checkbox' name='c3' " + String(showCustom?"checked":"") + "></div>"
                  "  <div class='tile " + String(showNightMode?"active":"") + "' onclick='toggleTile(this)'>"
                  "    <div class='icon'><svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z'/></svg></div><span class='label'>Night Mode</span><input type='checkbox' name='c8' " + String(showNightMode?"checked":"") + "></div>"
                  " </div>"

                  " <div class='section-title'>Drawing Board</div>"
                  " <div class='board-card'>"
                  "  <div class='canvas-wrapper'><canvas id='paintCanvas' width='840' height='160'></canvas></div>"
                  "  <div style='display:grid; grid-template-columns:1fr 70px 70px; gap:12px; height:50px;'>"
                  "    <button type='button' class='btn btn-gray' style='height:100%; display:flex; align-items:center; justify-content:center;' onclick='clearCanvas()'>CLEAR BOARD</button>"
                  "    <button type='button' id='pencilBtn' class='btn btn-blue' style='height:100%; display:flex; justify-content:center; align-items:center;' onclick='setTool(1)'><svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5' stroke-linecap='round' stroke-linejoin='round' style='width:24px;height:24px;'><path d='M17 3a2.85 2.83 0 1 1 4 4L7.5 20.5 2 22l1.5-5.5Z'/><path d='m15 5 4 4'/></svg></button>"
                  "    <button type='button' id='eraserBtn' class='btn btn-blue' style='height:100%; opacity:0.5; display:flex; justify-content:center; align-items:center;' onclick='setTool(0)'><svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5' stroke-linecap='round' stroke-linejoin='round' style='width:24px;height:24px;'><path d='m7 21-4.3-4.3c-1-1-1-2.5 0-3.4l9.9-9.9c1-1 2.5-1 3.4 0l4.4 4.4c1 1 1 2.5 0 3.4l-8.4 8.4c-1 1-2.5 1-3.4 0Z'/><path d='m22 21-2.1-2.1'/><path d='m11 5 9 9'/></svg></button>"
                  "  </div>"
                  "  <div style='margin-top:12px; text-align:center;'> "
                  "    <div style='display:flex; gap:10px;'>"
                  "      <input type='text' id='boardName' placeholder='Name your board...' style='margin:0; flex:1'>"
                  "      <button type='button' class='btn btn-blue' style='width:120px; height:50px; display:flex; align-items:center; justify-content:center;' onclick='saveAsNew()'>SAVE NEW</button>"
                  "    </div>"
                  "    <div id='gallery' style='display:grid; grid-template-columns: repeat(auto-fill, minmax(140px, 1fr)); gap:10px; margin-top:15px;'></div>"
                  "  </div>"
                  " </div>"
                  
                  " <div style='display:flex; flex-wrap:wrap; gap:20px; width:100%;'>"
                  "  <div class='msg-card' style='flex:1.2; min-width:300px; margin:0;'>"
                  "   <div class='section-title' style='margin-top:0'>Message Editor</div>"
                  "   <div id='msgList'></div>"
                  "   <div style='margin-top:20px; display:flex; gap:10px;'>"
                  "    <input type='text' id='newMsg' placeholder='Write something...' style='margin:0'>"
                  "    <button type='button' class='btn btn-blue' onclick='addMsg()' style='width:100px'>ADD</button>"
                  "   </div>"
                  "  </div>"

                  "  <div class='speed-card' style='flex:1; min-width:300px; margin:0; flex-direction:column; gap:12px; align-items:stretch;'>"
                  "   <div class='section-title' style='margin-top:0'>Countdown Timer</div>"
                  "   <div style='display:flex; gap:8px;'>"
                  "    <input type='number' id='th' placeholder='H' style='flex:1; margin:0; text-align:center;'>"
                  "    <input type='number' id='tm' placeholder='M' style='flex:1; margin:0; text-align:center;'>"
                  "    <input type='number' id='ts' placeholder='S' style='flex:1; margin:0; text-align:center;'>"
                  "   </div>"
                  "   <input type='text' id='tmsg' placeholder='Message...' style='width:100%; margin:0;'>"
                  "   <div style='display:grid; grid-template-columns: 1fr 100px; gap:10px;'>"
                  "    <button type='button' class='btn btn-blue' onclick='stT()'>START</button>"
                  "    <button type='button' class='btn btn-red' style='background:var(--red); color:white; border:none;' onclick='spT()'>STOP</button>"
                  "   </div>"
                  "  </div>"
                  " </div>"
                  
                  " <div class='section-title'>General Settings</div>"
                  " <div class='speed-card'>"
                  "  <span style='font-weight:600'>Rotation speed:</span>"
                  "  <div style='display:flex; gap:8px;'>"
                  "   <input type='number' id='sh' placeholder='H' style='width:60px; margin:0; text-align:center;'>"
                  "   <input type='number' id='sm' placeholder='M' style='width:60px; margin:0; text-align:center;'>"
                  "   <input type='number' id='ss' placeholder='S' style='width:60px; margin:0; text-align:center;'>"
                  "  </div>"
                  "  <input type='hidden' name='speed' id='totalSpeed'>"
                  " </div>"

                  " <input type='hidden' name='msgs' id='msgsInput'>"
                  " <button type='submit' class='btn btn-green'>APPLY</button>"
                  " </form>"
                  
                  " <div class='footer'>"
                  " Made with ❤️ by <a href='https://kinter.one' target='_blank'>Kinter</a> © 2026"
                  " </div>"
                  "</div>"

                  "<script>"
                  "function updateSpeed(){const h=document.getElementById('sh').value||0,m=document.getElementById('sm').value||0,s=document.getElementById('ss').value||0; document.getElementById('totalSpeed').value = parseInt(h)*3600 + parseInt(m)*60 + parseInt(s);}"
                  "function toggleTile(el) { el.classList.toggle('active'); const cb = el.querySelector('input'); cb.checked = !cb.checked; updateSpeed(); document.getElementById('mainForm').submit(); }"
                  "const TRASH_SVG = `<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M3 6h18'/><path d='M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6'/><path d='M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2'/></svg>`;"
                  "const canvas = document.getElementById('paintCanvas');"
                  "const ctx = canvas.getContext('2d');"
                  "const W = 84, H = 16, SCALE = 10;"
                  "let drawing = false; let lastX = -1, lastY = -1; let tool = 1;"
                  "let boardRaw = " + boardData + ";"
                  "let board = Array(W).fill(0).map((_, x) => Array(H).fill(0).map((_, y) => (boardRaw[x] & (1 << y)) ? 1 : 0));"
                  
                  "function initBoard() {"
                  "  ctx.fillStyle = '#000'; ctx.fillRect(0,0,840,160); ctx.strokeStyle = '#111'; ctx.lineWidth=0.5;"
                  "  for(let i=0; i<=W; i++){ctx.beginPath();ctx.moveTo(i*SCALE,0);ctx.lineTo(i*SCALE,160);ctx.stroke();}"
                  "  for(let i=0; i<=H; i++){ctx.beginPath();ctx.moveTo(0,i*SCALE);ctx.lineTo(840,i*SCALE);ctx.stroke();}"
                  "  for(let x=0; x<W; x++)for(let y=0; y<H; y++)if(board[x][y]){ctx.fillStyle='#ffdd00';ctx.fillRect(x*SCALE+1,y*SCALE+1,SCALE-2,SCALE-2);}"
                  "}"
                  
                  "function setPixel(x, y, on) {"
                  "  if (x<0||x>=W||y<0||y>=H||board[x][y]===on) return;"
                  "  board[x][y]=on; ctx.fillStyle=on?'#ffdd00':'#000'; ctx.fillRect(x*SCALE+1,y*SCALE+1,SCALE-2,SCALE-2);"
                  "  fetch(`/api/draw?x=${x}&y=${y}&on=${on}`);"
                  "}"

                  "function handleMove(e) {"
                  "  if (!drawing) return;"
                  "  const rect = canvas.getBoundingClientRect(); const ev = (e.touches && e.touches[0]) || e;"
                  "  const x = Math.floor((ev.clientX-rect.left)/(rect.width/W));"
                  "  const y = Math.floor((ev.clientY-rect.top)/(rect.height/H));"
                  "  if (x!==lastX||y!==lastY){setPixel(x,y,tool);lastX=x;lastY=y;}"
                  "}"

                  "function setTool(t){tool=t;document.getElementById('pencilBtn').style.opacity=t===1?'1':'0.5';document.getElementById('eraserBtn').style.opacity=t===0?'1':'0.5';}"
                  "function saveAsNew(){const n=document.getElementById('boardName').value; if(!n)alert('Name req'); else fetch('/api/boards/save?name='+encodeURIComponent(n)).then(()=>loadGallery());}"
                  "function loadGallery(){fetch('/api/boards/list').then(r=>r.json()).then(data=>{const g=document.getElementById('gallery');g.innerHTML='';data.forEach(n=>{"
                  "  const d=document.createElement('div');d.className='list-item';d.style.padding='8px 12px';d.style.margin='0';"
                  "  d.innerHTML='<span style=\"font-size:0.95em;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;margin-right:8px\">'+n+'</span>'+"
                  "    `<div style=\"display:flex;gap:8px\"><button type=\"button\" onclick=\"loadBoard('${n}')\" style=\"background:var(--blue);border:none;border-radius:6px;padding:6px;cursor:pointer;display:flex;align-items:center\"><svg style=\"width:16px;height:16px;stroke:#fff\" viewBox=\"0 0 24 24\" fill=\"none\" stroke-width=\"2.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4\"/><polyline points=\"7 10 12 15 17 10\"/><line x1=\"12\" y1=\"15\" x2=\"12\" y2=\"3\"/></svg></button>`+"
                  "    `<button type=\"button\" onclick=\"deleteBoard('${n}')\" class=\"btn-trash\">${TRASH_SVG}</button></div>`;"
                  "  g.appendChild(d);});});}"
                  "function loadBoard(n){fetch('/api/boards/load?name='+encodeURIComponent(n)).then(()=>location.reload());}"
                  "function deleteBoard(n){if(confirm('Delete '+n+'?'))fetch('/api/boards/delete?name='+encodeURIComponent(n)).then(()=>loadGallery());}"
                  "canvas.onmousedown=(e)=>{drawing=true;handleMove(e);}; window.onmouseup=()=>{drawing=false;lastX=-1;lastY=-1;}; canvas.onmousemove=handleMove;"
                  "canvas.addEventListener('touchstart',(e)=>{drawing=true;handleMove(e);e.preventDefault();},{passive:false});"
                  "canvas.addEventListener('touchmove',(e)=>{handleMove(e);e.preventDefault();},{passive:false});"
                  "canvas.addEventListener('touchend',()=>{drawing=false;});"
                  "function clearCanvas(){fetch('/api/draw?clear=1').then(()=>{board.forEach(c=>c.fill(0));initBoard();});}"
                  "function saveCanvas(){fetch('/api/draw?save=1').then(()=>alert('Saved to Flash! 🏮'));}"
                  "let playlist = " + playlistJson + ";"
                  "function render(){"
                  "  const l=document.getElementById('msgList');l.innerHTML='';playlist.forEach((m,i)=>{"
                  "    l.innerHTML+=`<div class='list-item'><span>${m}</span><button type='button' class='btn-trash' onclick='delMsg(${i})' style='width:36px;height:36px;'>${TRASH_SVG}</button></div>`;"
                  "  });"
                  "  document.getElementById('msgsInput').value=playlist.join('|');"
                  "}"
                  "function addMsg(){const i=document.getElementById('newMsg');if(i.value){playlist.push(i.value);i.value='';render();}}"
                  "function delMsg(i){playlist.splice(i,1);render();}"
                  "document.getElementById('newMsg').addEventListener('keypress',(e)=>{if(e.key==='Enter'){e.preventDefault();addMsg();}});"
                  "function togglePower(){const n=! " + String(systemOn?"true":"false") + "; fetch('/api/power?on='+(n?1:0)).then(()=>location.reload());}"
"function stT(){const h=document.getElementById('th').value||0,m=document.getElementById('tm').value||0,s=document.getElementById('ts').value||0,msg=document.getElementById('tmsg').value;"
"fetch(`/timer?h=${h}&m=${m}&s=${s}&msg=${encodeURIComponent(msg)}`).then(()=>location.reload());}"
"function spT(){fetch('/timerStop').then(()=>location.reload());}"
"window.addEventListener('load',()=>{loadGallery();initBoard();render();"
"const rs="+String(rotationSpeed)+"; document.getElementById('sh').value=Math.floor(rs/3600)||''; document.getElementById('sm').value=Math.floor((rs%3600)/60)||''; document.getElementById('ss').value=(rs%60)||'';});"
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
    showNightMode = request->hasParam("c8", true);
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
            static uint32_t lastApiUpdate = 0;
            if (millis() - lastApiUpdate > 500) { 
                lastApiUpdate = millis();
                forceRefresh = true;
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

  server.on("/api/boards/list", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "[";
    File root = LittleFS.open("/boards");
    if (!root || !root.isDirectory()) {
        LittleFS.mkdir("/boards");
        root = LittleFS.open("/boards");
    }
    File file = root.openNextFile();
    bool first = true;
    while(file) {
        String name = String(file.name());
        if (name.endsWith(".bin")) {
            if (!first) json += ",";
            json += "\"" + name.substring(0, name.length() - 4) + "\"";
            first = false;
        }
        file = root.openNextFile();
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  server.on("/api/boards/save", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("name")) {
        String name = request->getParam("name")->value();
        name.replace(" ", "_");
        File f = LittleFS.open("/boards/" + name + ".bin", "w");
        if (f) {
            f.write((uint8_t*)drawBoard, sizeof(drawBoard));
            f.close();
            request->send(200, "text/plain", "Saved " + name);
        } else {
            request->send(500, "text/plain", "Error opening file");
        }
    } else {
        request->send(400, "text/plain", "Missing name");
    }
  });

  server.on("/api/boards/load", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("name")) {
        String name = request->getParam("name")->value();
        File f = LittleFS.open("/boards/" + name + ".bin", "r");
        if (f) {
            f.read((uint8_t*)drawBoard, sizeof(drawBoard));
            f.close();
            saveConfig();
            forceRefresh = true;
            request->send(200, "text/plain", "Loaded " + name);
        } else {
            request->send(404, "text/plain", "Not found");
        }
    } else {
        request->send(400, "text/plain", "Missing name");
    }
  });

  server.on("/api/boards/delete", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("name")) {
        String name = request->getParam("name")->value();
        if (LittleFS.remove("/boards/" + name + ".bin")) {
            request->send(200, "text/plain", "Deleted");
        } else {
            request->send(500, "text/plain", "Error deleting");
        }
    } else {
        request->send(400, "text/plain", "Missing name");
    }
  });

  server.on("/api/power", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("on")) {
        systemOn = request->getParam("on")->value().toInt();
        saveConfig();
        forceRefresh = true;
        request->send(200, "text/plain", "OK");
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
  
  int h = timeinfo->tm_hour;
  int m = timeinfo->tm_min;

  if (!systemOn) { forceRefresh = false; return; }

  if (timerRunning) {
    uint32_t now = millis();
    Pixel_GFX.selectBuffer(0); Pixel_GFX.fillScreen(0);
    if (timerPhase == 0) { // Countdown
      if (now < timerTarget) {
        uint32_t rem = (timerTarget - now) / 1000;
        int h = rem / 3600; int m = (rem % 3600) / 60; int s = rem % 60;
        char buf[12];
        if (h > 0) snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
        else snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
        u8g2_gfx.setFont(FONT_CLOCK); drawUTF8Centered(buf, 16);
      } else {
        timerPhase = 1; timerTarget = now + 180000; // 3 minutes
        forceRefresh = true;
      }
    } else { // Message phase
      if (now < timerTarget) {
        u8g2_gfx.setFont(FONT_TEXT); drawUTF8Centered(timerMsg != "" ? timerMsg : "TIME'S UP!", 14); // Standard Y=14
      } else {
        timerRunning = false; timerPhase = 0; forceRefresh = true;
      }
    }
    Pixel_GFX.commitBufferToPage(0); delay(500); return;
  }

  static int lastAutoNight = -1;
  if (h == 22 && lastAutoNight != timeinfo->tm_mday) {
    showNightMode = true; lastAutoNight = timeinfo->tm_mday;
    forceRefresh = true;
  }

  // Schedules
  int currentInterval = rotationSpeed * 1000;
  bool nightActive = (h >= 22 || h < 5) && showNightMode;
  bool morningStatic = (h >= 5 && h < 7);
  bool morningRotate = (h == 7 && m < 15);

  if (nightActive || morningStatic) {
    if (millis() - lastToggle > 60000 || forceRefresh) { 
      lastToggle = millis(); forceRefresh = false;
      Pixel_GFX.selectBuffer(0); Pixel_GFX.fillScreen(0);
      u8g2_gfx.setFont(u8g2_font_unifont_t_polish);
      drawUTF8Centered(nightActive ? "Goodnight!" : "Morning!", 13);
      Pixel_GFX.commitBufferToPage(0);
    }
    return;
  }

  if (morningRotate) currentInterval = 30000;

  if (millis() - lastToggle > currentInterval || forceRefresh) {
    lastToggle = millis();
    forceRefresh = false;

    String toShow = "";
    int attempts = 0;
    while (attempts < 20) {
        masterIdx = (masterIdx + 1) % (6 + playlist.size());
        if (morningRotate) { 
           static int rotatePart = 0;
           rotatePart = (rotatePart + 1) % 4;
           if (rotatePart == 0) { u8g2_gfx.setFont(FONT_TEXT); toShow = "Morning!"; break; }
           if (rotatePart == 1) { u8g2_gfx.setFont(FONT_CLOCK); toShow = timeStr; break; }
           if (rotatePart == 2 && currentWeather.valid) { masterIdx = 2; break; } // Show Weather
           if (rotatePart == 3) { u8g2_gfx.setFont(FONT_DATE); toShow = dateStr; break; }
        }
        
        if (masterIdx == 0 && showClock) { u8g2_gfx.setFont(FONT_CLOCK); toShow = timeStr; break; }
        if (masterIdx == 1 && showDate) { u8g2_gfx.setFont(FONT_DATE); toShow = dateStr; break; }
        if (masterIdx == 2 && showWeather && currentWeather.valid) break;
        if (masterIdx == 3 && showAnalogClock) break;
        if (masterIdx == 4 && showCombine) break;
        if (masterIdx == 5 && showDrawing) break;
        if (masterIdx >= 6 && showCustom) {
            int pIdx = masterIdx - 6;
            if (pIdx >= 0 && pIdx < (int)playlist.size()) { 
                u8g2_gfx.setFont(FONT_TEXT); toShow = playlist[pIdx]; break; 
            }
        }
        attempts++;
    }

    if (masterIdx == 2 && showWeather && currentWeather.valid) {
        Pixel_GFX.selectBuffer(0); Pixel_GFX.fillScreen(0);
        const uint8_t* icon = WeatherHelper::getIconForCode(currentWeather.code, currentWeather.is_day, currentWeather.wind_speed);
        Pixel_GFX.drawXBitmap(0, 0, icon, 16, 16, 1);
        u8g2_gfx.setFont(u8g2_font_unifont_t_polish);
        char buf[16]; snprintf(buf, sizeof(buf), "%.1f", currentWeather.temp);
        String tNum = String(buf);
        int wNum = u8g2_gfx.getUTF8Width(tNum.c_str());
        int wC = u8g2_gfx.getUTF8Width("C");
        int startX = 83 - (wNum + 5 + wC);
        u8g2_gfx.setCursor(startX, 13); u8g2_gfx.print(tNum);
        Pixel_GFX.drawCircle(startX + wNum + 2, 3, 1, 1);
        u8g2_gfx.setCursor(startX + wNum + 5, 13); u8g2_gfx.print("C");
        Pixel_GFX.commitBufferToPage(0);
    } else if (masterIdx == 3 && showAnalogClock) {
        Pixel_GFX.selectBuffer(0); Pixel_GFX.fillScreen(0);
        drawAnalogClock(timeinfo->tm_hour, timeinfo->tm_min);
        Pixel_GFX.commitBufferToPage(0);
    } else if (masterIdx == 4 && showCombine) {
        Pixel_GFX.selectBuffer(0); Pixel_GFX.fillScreen(0);
        u8g2_gfx.setFont(FONT_CLOCK);
        int wDig = u8g2_gfx.getUTF8Width(timeStr);
        int startX = (84 - (14 + 4 + wDig)) / 2;
        drawAnalogClock(timeinfo->tm_hour, timeinfo->tm_min, startX + 7, 8);
        u8g2_gfx.setCursor(startX + 18, 16); u8g2_gfx.print(timeStr);
        Pixel_GFX.commitBufferToPage(0);
    } else if (masterIdx == 5 && showDrawing) {
        Pixel_GFX.selectBuffer(0); Pixel_GFX.fillScreen(0);
        for(int x=0; x<84; x++) for(int y=0; y<16; y++) if (drawBoard[x] & (1 << y)) Pixel_GFX.drawPixel(x, y, 1);
        Pixel_GFX.commitBufferToPage(0);
    } else if (toShow != "") {
      Pixel_GFX.selectBuffer(0); Pixel_GFX.fillScreen(0);
      int yPos = (u8g2_gfx.getFontAscent() > 13 ? 16 : 14);
      drawUTF8Centered(toShow, yPos);
      Pixel_GFX.commitBufferToPage(0);
    }
  }
}