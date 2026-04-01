#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Pixel.hpp>
#include <Adafruit_GFX_Pixel.hpp>
#include <Fonts/FreeSerif9pt7b.h>

PixelClass Pixel(Serial2, 22, 22);
Adafruit_Pixel Pixel_GFX(Pixel, 84, 1); // Address 1

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.nask.pl", 7200); // 7200 = GMT+2 (Summer Poland)

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
  Serial2.begin(19200, SERIAL_8E1, 19, 18);
  Pixel.begin();
  
  Serial.println("\n--- Phase 1: WiFi & NTP (NASK) ---");
  Pixel_GFX.init();
  
  Pixel_GFX.selectBuffer(0);
  Pixel_GFX.fillScreen(0);
  Pixel_GFX.setFont(&FreeSerif9pt7b);
  printCentered("WIFI...");
  Pixel_GFX.commitBufferToPage(0);

  WiFiManager wm;
  if (!wm.autoConnect("Pixel_Flipdot_AP")) {
    Serial.println("Failed to connect");
    ESP.restart();
  }

  Serial.println("WiFi Connected!");
  timeClient.begin();
  timeClient.forceUpdate();
  
  delay(1000);
}

void loop() {
  static uint32_t lastNTP = 0;
  if (millis() - lastNTP > 300000) { // 5 min
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
  static bool showTime = true;
  if (millis() - lastToggle > 5000) {
    lastToggle = millis();
    showTime = !showTime;
    
    String toShow = showTime ? String(timeStr) : String(dateStr);
    
    Pixel_GFX.selectBuffer(0);
    Pixel_GFX.fillScreen(0);
    Pixel_GFX.setFont(&FreeSerif9pt7b);
    printCentered(toShow);
    Pixel_GFX.commitBufferToPage(0);
  }
}