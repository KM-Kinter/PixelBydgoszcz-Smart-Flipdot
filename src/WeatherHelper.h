#ifndef WEATHER_HELPER_H
#define WEATHER_HELPER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Weather Icons (16x16)
static const unsigned char PROGMEM icon_sun[] = {
  0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x09, 0x90, 0x07, 0xe0, 0x0f, 0xf0, 0x3f, 0xfc, 0x3f, 0xfc, 
  0x3f, 0xfc, 0x3f, 0xfc, 0x0f, 0xf0, 0x07, 0xe0, 0x09, 0x90, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00
};

static const unsigned char PROGMEM icon_cloud[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xc0, 0x0f, 0xe0, 0x1f, 0xf0, 0x1f, 0xf8, 0x3f, 0xfc, 
  0x7f, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char PROGMEM icon_rain[] = {
  0x00, 0x00, 0x07, 0xc0, 0x0f, 0xe0, 0x1f, 0xf0, 0x1f, 0xf8, 0x3f, 0xfc, 0x7f, 0xfe, 0xff, 0xff, 
  0xff, 0xff, 0x7f, 0xfe, 0x00, 0x00, 0x24, 0x48, 0x12, 0x24, 0x09, 0x12, 0x04, 0x08, 0x00, 0x00
};

static const unsigned char PROGMEM icon_snow[] = {
  0x00, 0x00, 0x01, 0x80, 0x13, 0xc8, 0x05, 0xa0, 0x3d, 0xbc, 0x11, 0x88, 0x01, 0x80, 0x7f, 0xfe, 
  0x7f, 0xfe, 0x01, 0x80, 0x11, 0x88, 0x3d, 0xbc, 0x05, 0xa0, 0x13, 0xc8, 0x01, 0x80, 0x00, 0x00
};

struct WeatherData {
    float temp;
    int code;
    bool valid;
};

class WeatherHelper {
public:
    static WeatherData getWSWWeather() {
        WeatherData data = {0, 0, false};
        HTTPClient http;
        // Warszawa coordinates: 52.2297, 21.0122
        String url = "http://api.open-meteo.com/v1/forecast?latitude=52.2297&longitude=21.0122&current=temperature_2m,weather_code&timezone=Europe%2FWarsaw";
        
        http.begin(url);
        int httpCode = http.GET();
        
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            
            if (!error) {
                data.temp = doc["current"]["temperature_2m"];
                data.code = doc["current"]["weather_code"];
                data.valid = true;
                Serial.printf("Weather OK: %.1f C, Code: %d\n", data.temp, data.code);
            } else {
                Serial.println("Weather JSON fail");
            }
        } else {
            Serial.printf("Weather HTTP fail: %d\n", httpCode);
        }
        http.end();
        return data;
    }

    static const unsigned char* getIconForCode(int code) {
        if (code == 0) return icon_sun;
        if (code >= 1 && code <= 3) return icon_cloud;
        if (code >= 45 && code <= 48) return icon_cloud;
        if (code >= 51 && code <= 67) return icon_rain;
        if (code >= 71 && code <= 77) return icon_snow;
        if (code >= 80 && code <= 82) return icon_rain;
        if (code >= 95) return icon_rain; // Thunderstorm -> rain icon for now
        return icon_sun;
    }
};

#endif
