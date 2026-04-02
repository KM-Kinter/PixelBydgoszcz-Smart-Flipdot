#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Pixel.hpp>
#include <Adafruit_GFX_Pixel.hpp>
#include <Fonts/FreeSerif9pt7b.h>

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

PixelClass Pixel(Serial2, 22, 22);
Adafruit_Pixel Pixel_GFX(Pixel, 84, 1); // Address 1

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.nask.pl", 7200); // 7200 = GMT+2 (Summer Poland)
AsyncWebServer server(80);

// App Settings
String webText = "HELLO!";
bool showClock = true;
bool showDate = true;
bool showCustom = true;
bool forceRefresh = false;

void saveConfig() {
  File f = LittleFS.open("/config.txt", "w");
  if (f) {
    f.println(webText);
    f.println(showClock);
    f.println(showDate);
    f.println(showCustom);
    f.close();
    Serial.println("Config Saved");
  }
}

void loadConfig() {
  if (LittleFS.exists("/config.txt")) {
    File f = LittleFS.open("/config.txt", "r");
    if (f) {
      webText = f.readStringUntil('\n'); webText.trim();
      showClock = f.readStringUntil('\n').toInt();
      showDate = f.readStringUntil('\n').toInt();
      showCustom = f.readStringUntil('\n').toInt();
      f.close();
      Serial.println("Config Loaded: " + webText);
    }
  }
}

void printCentered(const String& text, int y = 14) {
  int16_t x1, y1;
  uint16_t w, h;
  Pixel_GFX.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int x = (84 - w) / 2;
  Pixel_GFX.setCursor(x, y);
  Pixel_GFX.print(text);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== BOOTING PIXEL FLIPDOT V2 ===");
  
  // RS485 Init
  Serial2.begin(19200, SERIAL_8E1, 19, 18);
  Pixel.begin();
  
  // FS Init
  Serial.print("Mounting LittleFS... ");
  if(!LittleFS.begin(true)){
    Serial.println("FAILED");
  } else {
    Serial.println("OK");
    loadConfig();
  }

  // Display Init
  Serial.println("Initializing GFX...");
  Pixel_GFX.init();
  Pixel_GFX.selectBuffer(0);
  Pixel_GFX.fillScreen(0);
  Pixel_GFX.setFont(&FreeSerif9pt7b);
  printCentered("WiFi...");
  Pixel_GFX.commitBufferToPage(0);

  // WiFi Init
  Serial.println("Starting WiFiManager...");
  WiFiManager wm;
  wm.setConnectTimeout(30);
  if (!wm.autoConnect("Pixel_Flipdot_AP")) {
    Serial.println("WiFi Config Timeout - Restarting...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("WiFi Connected!");
  Serial.print("NTP Syncing... ");
  timeClient.begin();
  if (timeClient.forceUpdate()) {
    Serial.println("DONE");
  } else {
    Serial.println("TIMEOUT (Using default time)");
  }

  // Web Server Routes
  Serial.print("Configuring Web Server... ");
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<style>body{background:#121212;color:#e0e0e0;font-family:sans-serif;text-align:center;padding:20px;}"
                  ".btn{background:#00bcd4;color:white;border:none;padding:15px;width:100%;border-radius:8px;font-size:1.2em;cursor:pointer;}"
                  ".card{background:#1e1e1e;padding:20px;border-radius:12px;margin:10px auto;max-width:400px;text-align:left;}"
                  "input[type=text]{padding:12px;width:100%;background:#333;border:1px solid #444;color:white;border-radius:8px;margin:10px 0;box-sizing:border-box;}"
                  "h1{color:#00bcd4; margin-bottom:5px;}"
                  ".row{display:flex;justify-content:space-between;align-items:center;padding:10px 0;border-bottom:1px solid #333;}"
                  "input[type=checkbox]{transform:scale(1.5);}</style></head><body>"
                  "<h1>Smart Flipdot</h1>"
                  "<div class='card'>"
                  "<form action='/save' method='GET'>"
                  "<div class='row'><span>Show Clock</span><input type='checkbox' name='c1' " + String(showClock?"checked":"") + "></div>"
                  "<div class='row'><span>Show Date</span><input type='checkbox' name='c2' " + String(showDate?"checked":"") + "></div>"
                  "<div class='row'><span>Show Custom Text</span><input type='checkbox' name='c3' " + String(showCustom?"checked":"") + "></div>"
                  "<input type='text' name='txt' placeholder='Message...' value='" + webText + "'>"
                  "<input type='submit' value='SAVE & UPDATE' class='btn'>"
                  "</form></div>"
                  "<p style='color:#555;font-size:0.8em;'>IP: " + WiFi.localIP().toString() + "</p>"
                  "</body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/save", HTTP_GET, [](AsyncWebServerRequest *request){
    showClock = request->hasParam("c1");
    showDate = request->hasParam("c2");
    showCustom = request->hasParam("c3");
    if (request->hasParam("txt")) {
        webText = request->getParam("txt")->value();
        webText.toUpperCase(); // Flipdots like uppercase
    }
    saveConfig();
    forceRefresh = true;
    request->redirect("/");
  });

  server.begin();
  Serial.println("OK (Port 80)");
  delay(1000);
  Serial.println("=== SYSTEM READY ===");
}

void loop() {
  static uint32_t lastNTP = 0;
  if (millis() - lastNTP > 3600000) { // Every hour
    lastNTP = millis();
    timeClient.forceUpdate();
  }
  
  timeClient.update();
  time_t now = timeClient.getEpochTime();
  struct tm * timeinfo = localtime(&now);

  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
  char dateStr[10];
  sprintf(dateStr, "%02d.%02d.%02d", timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year % 100);

  static uint32_t lastToggle = 0;
  static int displayMode = 0; // 0=Clock, 1=Date, 2=Custom
  
  bool shouldUpdate = (millis() - lastToggle > 5000);
  
  if (shouldUpdate || forceRefresh) {
    lastToggle = millis();
    
    if (!forceRefresh) {
        int attempts = 0;
        do {
          displayMode = (displayMode + 1) % 3;
          attempts++;
        } while (attempts < 4 && (
          (displayMode == 0 && !showClock) ||
          (displayMode == 1 && !showDate) ||
          (displayMode == 2 && (!showCustom || webText == ""))
        ));
    }
    
    forceRefresh = false;

    String toShow = "";
    if (displayMode == 0 && showClock) toShow = timeStr;
    else if (displayMode == 1 && showDate) toShow = dateStr;
    else if (displayMode == 2 && showCustom) toShow = webText;

    if (toShow != "") {
      Serial.println("Updating display: " + toShow);
      Pixel_GFX.selectBuffer(0);
      Pixel_GFX.fillScreen(0);
      Pixel_GFX.setFont(&FreeSerif9pt7b);
      printCentered(toShow);
      Pixel_GFX.commitBufferToPage(0);
    }
  }
}