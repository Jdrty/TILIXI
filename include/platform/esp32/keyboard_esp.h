#pragma once
#include "keyboard_core.h"

#ifdef __cplusplus
extern "C" {
#endif

void keyboard_esp_init(void);

#ifdef PLATFORM_ESP32
void keyboard_esp_scan(void);
#endif

#ifdef __cplusplus
}
#endif

