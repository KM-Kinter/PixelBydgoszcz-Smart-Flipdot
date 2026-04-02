# Smart PixelBydgoszcz Flipdot Display (ESP32)

A high-fidelity weather and clock display system for ESP32-controlled flipdot boards. This project features real-time weather integration, professional pixel icons, and a sleek web-based dashboard for configuration.

## Features

- **Multi-Mode Clock**:
    - **Digital**: Precise NTP-synced time.
    - **Analog**: Classic clock face rendering.
    - **Combine**: Hybrid mode (Analog left, Digital right).
- **Interactive Drawing Board**: Real-time 84x16 `<canvas>` painting interface with Eraser tool and Flash persistence.
- **Conway's Game of Life**: Cellular automation simulation with stagnation detection and automatic re-seeding. It's on a separate branch because it doesn't work well yet.
- **Dynamic Calendar**: Automated date tracking.
- **Real-time Weather**: Open-Meteo API integration with a custom library of 22 professional 16x16 weather icons.
- **Message Editor**: Dynamic playlist for custom scrolling or fixed text messages.
- **Web Dashboard**: Sleek, grid-based UI with SVG iconography and auto-submit controls.
- **Hardware Optimization**: ADC-safe random seed generation to prevent WiFi/ADC2 conflicts on ESP32.
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

### Hardware Configuration (DIP Switches)
> [!TIP]
> **Note to self and others:** Inside the flipdot panel, there is an 8-position DIP switch block. **Switch #5** is responsible for for enabling/disabling the internal LED backlight hardware-side. If your backlight is permanently on or off regardless of software commands, check this switch!
> 
> *Pro-tip: Verify this physical bypass first to avoid spending several days attempting to troubleshoot the backlight state via software integration like I did :)*

### RS485 Interface

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
