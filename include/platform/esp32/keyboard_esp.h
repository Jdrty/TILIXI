#pragma once
#include "keyboard_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize ESP32 keyboard hardware
void keyboard_esp_init(void);

// Scan keyboard (call periodically from loop() or a task)
// Only available on ESP32 builds
#ifdef PLATFORM_ESP32
void keyboard_esp_scan(void);
#endif

#ifdef __cplusplus
}
#endif

