#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize keyboard pickup system
 * 
 * This function initializes:
 * - USB Host stack (configured for GPIO 19/20 via sdkconfig)
 * - HID Host driver
 * - Creates event queue and tasks
 * 
 * This function is non-blocking and should be called during boot sequence.
 * It assumes TFT display and terminal system are already initialized.
 * 
 * Note: USB pins D+ (GPIO 20) and D- (GPIO 19) should be configured
 * in sdkconfig. For ESP32-S3, these are the default USB OTG pins.
 * 
 * @return 0 on success, negative on error
 */
int keyboard_pickup_init(void);

/**
 * @brief Initialize keyboard pickup with terminal setup
 * 
 * This function sets up the terminal window for keyboard input display.
 * It should be called after terminal system is initialized.
 * 
 * @return 0 on success, negative on error
 */
int keyboard_pickup_setup_terminal(void);

#ifdef __cplusplus
}
#endif

