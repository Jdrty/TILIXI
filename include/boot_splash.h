#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// initialize boot display (TFT) and show TILIXI text
// this should be called early in setup() before other initialization
void boot_init(void);

// refresh/redraw the boot screen (call periodically if needed)
void boot_refresh(void);

// draw a raw RGB565 logo from SD card
// returns 1 if shown, 0 otherwise
int boot_show_logo_from_sd(const char *path);
int boot_draw_rgb565_scaled(const char *path, int16_t x, int16_t y,
                            int16_t width, int16_t height);
void boot_tft_draw_rgb565(int16_t x, int16_t y, const uint16_t *data,
                          int16_t width, int16_t height);

#ifdef __cplusplus
}
#endif


