#pragma once

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

#ifdef __cplusplus
}
#endif


