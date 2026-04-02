#ifndef WEATHER_HELPER_H
#define WEATHER_HELPER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "WeatherIcons.h"

struct WeatherData {
    float temp;
    int code;
    bool is_day;
    float wind_speed;
    bool valid;
};

class WeatherHelper {
public:
    static WeatherData getWSWWeather() {
        WeatherData data = {0, 0, true, 0, false};
        HTTPClient http;
        // API Query: temperature, weather_code, is_day, wind_speed_10m
        String url = "http://api.open-meteo.com/v1/forecast?latitude=52.2297&longitude=21.0122&current=temperature_2m,weather_code,is_day,wind_speed_10m&timezone=Europe%2FWarsaw";
        
        http.begin(url);
        int httpCode = http.GET();
        
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            
            if (!error) {
                data.temp = doc["current"]["temperature_2m"];
                data.code = doc["current"]["weather_code"];
                data.is_day = doc["current"]["is_day"] == 1;
                data.wind_speed = doc["current"]["wind_speed_10m"];
                data.valid = true;
                Serial.printf("Weather OK: %.1f C, Code: %d, Day: %d, Wind: %.1f\n", 
                             data.temp, data.code, data.is_day, data.wind_speed);
            } else {
                Serial.println("Weather JSON fail");
            }
        } else {
            Serial.printf("Weather HTTP fail: %d\n", httpCode);
        }
        http.end();
        return data;
    }

    static const uint8_t* getIconForCode(int code, bool is_day, float wind_speed) {
        // High Wind priority
        if (wind_speed > 30.0) { // arbitrary "strong wind" threshold in km/h
            if (code >= 1 && code <= 3) return is_day ? cloud_wind_sun_bits : cloud_wind_moon_bits;
            return wind_bits;
        }

        switch (code) {
            case 0: // Clear sky
                return is_day ? sun_bits : moon_bits;
            
            case 1: 
            case 2: // Mainly clear, partly cloudy
                return is_day ? cloud_sun_bits : cloud_moon_bits;
            
            case 3: // Overcast
                return clouds_bits;
            
            case 45:
            case 48: // Fog
                return clouds_bits;
            
            case 51:
            case 53:
            case 55: // Drizzle
                if (is_day) return rain0_sun_bits;
                return rain0_bits;
            
            case 61: // Slight rain
                if (is_day) return rain1_sun_bits;
                return rain1_bits;
            
            case 63:
            case 65: // Moderate/Heavy rain
                return rain2_bits;
            
            case 71:
            case 73:
            case 75: // Snow fall
                if (is_day) return snow_sun_bits;
                if (!is_day) return snow_moon_bits;
                return snou_bits;
            
            case 77: // Snow grains
                return snou_bits;
            
            case 80:
            case 81:
            case 82: // Rain showers
                if (is_day) return rain1_sun_bits;
                return rain1_moon_bits;
            
            case 85:
            case 86: // Snow showers
                return is_day ? snow_sun_bits : snow_moon_bits;
            
            case 95: // Thunderstorm
                return lightning_bits;
            
            case 96:
            case 99: // Thunderstorm with hail
                return rain_lightning_bits;
            
            default:
                return is_day ? sun_bits : moon_bits;
        }
    }
};

#endif
