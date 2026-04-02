# Smart PixelBydgoszcz Flipdot Display (ESP32)

A high-fidelity weather and clock display system for ESP32-controlled flipdot boards. This project features real-time weather integration, professional pixel icons, and a sleek web-based dashboard for configuration.

## Features

- **Real-time Weather**: Synchronized with Open-Meteo API for accurate local conditions.
- **Professional Iconography**: Includes a custom library of 22 professional 16x16 weather icons.
- **Optimized UI**: Pixel-perfect typography and layout designed specifically for flipdot resolution.
- **Web Dashboard**: Integrated configuration portal for managing rotation speed and display modes.
- **Day/Night Modes**: Dynamic icon mapping based on sun/moon status and wind speed.

## Hardware Requirements

### Controller
- **ESP32 WROOM-32**: It is recommended to use the WROOM-32 variant. Standard ESP32 modules have multiple hardware UARTs (Serial/Serial2); some smaller or specialized variants may only have one, which is insufficient as the flipdot requires a dedicated Serial interface.

### Flipdot Connectivity
The display is connected via two separate connectors:

1. **Main Connector (6-pin connector)**:
   - **Red**: +24V DC Power
   - **Blue**: GND
   - **White**: RS485-A (Data)
   - **Yellow**: RS485-B (Data)

2. **Backlight Connector (2-pin T-style)**:
   - **Green**: +24V DC Power
   - **Black**: GND
   - *This connector is responsible for powering the internal backlight. (but not always, like in my case :))* 

### RS485 Converter
A TTL-to-RS485 converter module is required to interface the ESP32 (3.3V logic) with the display's RS485 bus.

**Wiring (ESP32 to Converter):**
| ESP32 Pin | Converter Pin | Description |
|-----------|---------------|-------------|
| 3.3V      | VCC           | Power (logic) |
| GND       | GND           | Common Ground |
| GPIO 18   | RXD           | ESP32 TX -> Converter RX |
| GPIO 19   | TXD           | ESP32 RX -> Converter TX |
| (Auto)    | EN            | Enable (if using auto-direction module) |

**Wiring (Converter to Display):**
| Converter Side | Display Side |
|----------------|--------------|
| A              | White (A)    |
| B              | Yellow (B)   |
| GND            | Blue (GND)   |

## Attributions

- **Core Flipdot Driver**: The low-level communication logic (ESP to Flipdot) was originally authored by **Dominik Szymański** ([@domints](https://github.com/domints/ESPPixelBydgoszcz)).
- **Weather Icons**: Professional monochrome icons sourced from **Dhole** ([weather-pixel-icons](https://github.com/Dhole/weather-pixel-icons)).
- **Project Expansion & UI**: All high-level module logic, weather integration, web interface, and UI refinement by **KM-Kinter**.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

Made with ❤️ by [Kinter](https://kinter.one)
