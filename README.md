# ESP32 Water Tracker LCD

ESP32-S3 based water tracker UI project using a 0.85" SPI LCD display.

## Features
- Custom framebuffer rendering
- SPI LCD communication
- Water bottle visualization
- Progress tracking UI
- ESP-IDF based project

## Hardware
- XIAO ESP32-S3
- 0.85" SPI LCD

## Pin Connections
- MOSI -> GPIO9
- SCLK -> GPIO7
- CS -> GPIO4
- DC -> GPIO5
- RST -> GPIO6
- BL -> GPIO3

## Build
```bash
idf.py build
idf.py -p COM6 flash monitor