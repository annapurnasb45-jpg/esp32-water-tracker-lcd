# 💧 ESP32-S3 Water Tracker with SPI LCD

A real-time hydration tracking system built on **ESP32-S3** using a **0.85" SPI TFT LCD (GC9107 / ST77xx compatible)**.  
This project demonstrates **low-level LCD driver development, framebuffer-based rendering, and real-time UI visualization** on embedded hardware.

---

## 🚀 Features

- 📊 Real-time hydration tracking (Goal / Drank / Left)
- 💧 Dynamic water bottle visualization
- 🌊 Wave animation to simulate fluid surface
- 📈 Progress bar with percentage indicator
- 🎨 Custom framebuffer graphics engine (no external libraries)
- ⚡ High-speed SPI communication (~20–27 MHz)

---

## 🧰 Hardware Used

- XIAO ESP32-S3  
- 0.85" TFT LCD (128×128, SPI, RGB565)

---

##  Pin Configuration

| LCD Pin | Function            | XIAO ESP32-S3 Pin | GPIO |
|--------|--------------------|------------------|------|
| VCC    | Power Supply       | VCC              | —    |
| GND    | Ground             | GND              | —    |
| DIN    | SPI MOSI           | D10              | GPIO9 |
| CLK    | SPI Clock          | D8               | GPIO7 |
| CS     | Chip Select        | D3               | GPIO4 |
| DC     | Command/Data       | D4               | GPIO5 |
| RST    | Hardware Reset     | D5               | GPIO6 |
| BL     | Backlight Control  | D2               | GPIO3 |

---

## 🧠 System Architecture

```mermaid
flowchart TD
    A[Water State Input] --> B[Render Screen]
    B --> C[Framebuffer RAM]
    C --> D[Flush to LCD]
    D --> E[SPI Driver]
    E --> F[LCD Controller]
    F --> G[Display Output]

flowchart TD
    A[Start] --> B[GPIO Init DC RST BL]
    B --> C[SPI Bus Init]
    C --> D[Hardware Reset]
    D --> E[Send Init Commands]
    E --> F[Display ON]
    F --> G[Backlight ON]

flowchart TD
    A[Set Column Address] --> B[Set Row Address]
    B --> C[Start RAM Write]
    C --> D[Send Pixel Data RGB565]
    D --> E[Repeat for Rows]

flowchart LR
    ESP32[ESP32 S3 SPI Master] --> LCD[LCD Controller]
    LCD --> Display[128x128 TFT Display]

flowchart TD
    A[DC 0] --> B[Send Command]
    C[DC 1] --> D[Send Data]
    B --> E[LCD Registers]
    D --> F[Pixel Data GRAM]
