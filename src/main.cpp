#include <Arduino.h>
#include <Pixel.hpp>
#include <Adafruit_GFX_Pixel.hpp>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>

PixelClass Pixel(Serial2, 22, 22);
Adafruit_Pixel Pixel_GFX(Pixel, 84, 1); // Address 1

void setup() {
  Serial.begin(115200);
  Serial2.begin(19200, SERIAL_8E1, 19, 18);
  Pixel.begin();
  Serial.println("Sleeping some...");
  delay(5000); 
  
  Serial.println("Initializing driver (Addr 1)...");
  Pixel_GFX.init();
  
  delay(500);
  Serial.println("Turning off backlight...");
  Pixel_GFX.setBacklight(0); 
  
  delay(500);
  Serial.println("Drawing with custom fonts...");
  Pixel_GFX.selectBuffer(0);
  Pixel_GFX.fillScreen(0);
  Pixel_GFX.setFont(&FreeSerif9pt7b);
  Pixel_GFX.setCursor(0, 14);
  Pixel_GFX.print("Siemano!");
  
  uint8_t errCode = Pixel_GFX.commitBufferToPage(0);
  Serial.print("Commit Page 0 response: ");
  Serial.println(errCode);
  
  delay(2000);
  
  Pixel_GFX.selectBuffer(1);
  Pixel_GFX.fillScreen(0);
  Pixel_GFX.setFont(&FreeSerif9pt7b);
  Pixel_GFX.setCursor(0, 14);
  Pixel_GFX.print("01.04.2026");
  
  errCode = Pixel_GFX.commitBufferToPage(1);
  Serial.print("Commit Page 1 response: ");
  Serial.println(errCode);
  
  Serial.println("Done! Check the display.");
}

void loop() {
  // put your main code here, to run repeatedly:
}