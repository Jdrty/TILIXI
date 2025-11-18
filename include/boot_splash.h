#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// initialize boot display (TFT) and show TILIXI text
// this should be called early in setup() before other initialization
void boot_init(void);

// refresh/redraw the boot screen (call periodically if needed)
void boot_refresh(void);

#ifdef __cplusplus
}
#endif


