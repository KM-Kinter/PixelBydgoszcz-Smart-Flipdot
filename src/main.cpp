#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <Pixel.hpp>
#include <Adafruit_GFX_Pixel.hpp>
#include <U8g2_for_Adafruit_GFX.h>
#include <ElegantOTA.h>

// --- DEBUG SYSTEM ---
#ifdef DEBUG_LOGS
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <vector>
#include "WeatherHelper.h"

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
    DEBUG_PRINTLN("Config Saved");
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
      DEBUG_PRINTLN("Config Loaded. Speed: " + String(rotationSpeed) + "s");
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
    DEBUG_PRINTLN("LittleFS Mount OK");
    loadConfig();
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
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Setup MDNS
  if (MDNS.begin("flipdot")) {
    DEBUG_PRINTLN("MDNS responder started: flipdot.local");
    MDNS.addService("http", "tcp", 80);
  }

  timeClient.begin();
  DEBUG_PRINT("Sync Time");
  for(int i=0; i<10 && !timeClient.update(); i++){ delay(500); DEBUG_PRINT("."); }
  DEBUG_PRINTLN(" OK");
  forceRefresh = true;

  server.on("/timer", HTTP_GET, [](AsyncWebServerRequest *request){
    uint32_t ms = 0;
    if(request->hasParam("h")) ms += request->getParam("h")->value().toInt() * 3600000;
    if(request->hasParam("m")) ms += request->getParam("m")->value().toInt() * 60000;
    if(request->hasParam("s")) ms += request->getParam("s")->value().toInt() * 1000;
    timerTarget = millis() + ms;
    if(request->hasParam("msg")) timerMsg = request->getParam("msg")->value();
    timerRunning = true; timerPhase = 0; request->send(200, "text/plain", "OK");
  });  server.on("/timerStop", HTTP_GET, [](AsyncWebServerRequest *request){
    timerRunning = false; request->send(200, "text/plain", "OK");
  });
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    File f = LittleFS.open("/index.html", "r");
    if(!f) { request->send(404, "text/plain", "File not found"); return; }
    String html = f.readString();
    f.close();

    html.replace("{{VERSION}}", "6.0");
    html.replace("{{PWR_CHK}}", systemOn ? "checked" : "");
    html.replace("{{PWR_STATE}}", systemOn ? "false" : "true");
    html.replace("{{C1_ACT}}", showClock ? "active" : "");
    html.replace("{{C1_CHK}}", showClock ? "checked" : "");
    html.replace("{{C2_ACT}}", showDate ? "active" : "");
    html.replace("{{C2_CHK}}", showDate ? "checked" : "");
    html.replace("{{C3_ACT}}", showCustom ? "active" : "");
    html.replace("{{C3_CHK}}", showCustom ? "checked" : "");
    html.replace("{{C4_ACT}}", showWeather ? "active" : "");
    html.replace("{{C4_CHK}}", showWeather ? "checked" : "");
    html.replace("{{C5_ACT}}", showAnalogClock ? "active" : "");
    html.replace("{{C5_CHK}}", showAnalogClock ? "checked" : "");
    html.replace("{{C6_ACT}}", showCombine ? "active" : "");
    html.replace("{{C6_CHK}}", showCombine ? "checked" : "");
    html.replace("{{C7_ACT}}", showDrawing ? "active" : "");
    html.replace("{{C7_CHK}}", showDrawing ? "checked" : "");
    html.replace("{{C8_ACT}}", showNightMode ? "active" : "");
    html.replace("{{C8_CHK}}", showNightMode ? "checked" : "");
    
    // Board Data
    String bd = "[";
    for(int i=0; i<84; i++) bd += String(drawBoard[i]) + (i==83?"":",");
    bd += "]";
    html.replace("{{BOARD_DATA}}", bd);

    // Playlist Data
    String pl = "[";
    for (size_t i=0; i<playlist.size(); i++) {
        String s = playlist[i];
        s.replace("\"", "\\\"");
        pl += "\"" + s + "\"" + (i == playlist.size()-1 ? "" : ",");
    }
    pl += "]";
    html.replace("{{PLAYLIST_DATA}}", pl);
    html.replace("{{ROTATION_SPEED}}", String(rotationSpeed));

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
    if (request->hasParam("speed", true)) {
        String val = request->getParam("speed", true)->value();
        if (val.length() > 0) {
            rotationSpeed = val.toInt();
            if (rotationSpeed < 2) rotationSpeed = 2;
        }
    }

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

  ElegantOTA.begin(&server);
  server.begin();
  DEBUG_PRINTLN("Web Server running.");
}

void loop() {
  ElegantOTA.loop();
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