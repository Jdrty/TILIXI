# ESP32-S3 Wiring Reference

ESP32-S3 WROOM (Freenove) on breadboard.

## Power
- VCC → VCC
- GND → GND

## TFT Display
- VCC → 3.3V
- GND → GND
- CS → GPIO 5
- DC/RS → GPIO 21
- Reset → GPIO 4
- LED → 3.3V
- MOSI → GPIO 11
- SCK → GPIO 12
- MISO → GPIO 13
- IMPORTANT NOTE, INIT IS DONE WITH tft.init(320, 480), NOT 480, 320

## MicroSD
- CS → GPIO 18
- SCK → GPIO 17
- MOSI → GPIO 15
- MISO → GPIO 16

## Micro USB Adapter (Keyboard)
- D- → GPIO 20
- D+ → GPIO 19
- DI → GND

