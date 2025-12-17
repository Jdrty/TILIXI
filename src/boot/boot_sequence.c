#define _POSIX_C_SOURCE 200809L

#ifndef ARDUINO
#include <time.h>

static inline void delay_ms(unsigned int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
#endif

#include "boot_sequence.h"
#include "boot_splash.h"
#include "debug_helper.h"
#include "process.h"
#include "process_scheduler.h"
#include "process_script.h"
#include "terminal.h"
#include "terminal_cmd.h"
#include <string.h>
#include <stdio.h>

#ifdef ARDUINO
    #include <Arduino.h>
    #include "ino_helper.h"
    
    void boot_tft_set_cursor(int16_t x, int16_t y);
    void boot_tft_set_text_size(uint8_t size);
    void boot_tft_set_text_color(uint16_t color, uint16_t bg);
    void boot_tft_print(const char *str);
    void boot_tft_fill_screen(uint16_t color);
    void boot_tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    
    #define COLOR_BLACK   0x0000
    #define COLOR_WHITE  0xFFFF
    #define COLOR_GREEN  0x07E0
    #define COLOR_RED    0xF800
#else
    #include <stdlib.h>
    #define COLOR_BLACK   0
    #define COLOR_WHITE   1
    #define COLOR_GREEN   2
    #define COLOR_RED     3
#endif

// boot sequence state
#ifdef ARDUINO
static uint8_t boot_line_y = 20;  // current Y position for boot messages
#else
static uint8_t boot_line_y = 0;  // unused on PC
#endif
static uint8_t boot_initialized = 0;

void boot_sequence_init(void) {
    if (boot_initialized) return;
    
#ifdef ARDUINO
    // TFT is already initialized by boot_init() in main_esp32.cpp
    // start displaying boot messages (TILIXI title is handled by boot_splash)
    boot_tft_set_text_size(2);
    boot_tft_set_text_color(COLOR_BLACK, COLOR_WHITE);
    boot_line_y = 20;  // start at top for boot messages
#endif
    
    boot_initialized = 1;
    DEBUG_PRINT("[BOOT] Boot sequence initialized\n");
}

void boot_display_message(const char *step_name, boot_status_t status) {
    if (!boot_initialized) {
        boot_sequence_init();
    }
    
#ifdef ARDUINO
    // set cursor position for step name (left side)
    boot_tft_set_cursor(10, boot_line_y);
    boot_tft_set_text_size(2);
    boot_tft_set_text_color(COLOR_BLACK, COLOR_WHITE);
    
    // print step name (truncate if too long to fit on screen, um ill do a better method later but its not really needed right now)
    char line[32];
    int len = strlen(step_name);
    if (len > 28) {
        strncpy(line, step_name, 28);
        line[28] = '\0';
    } else {
        strncpy(line, step_name, sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';
    }
    boot_tft_print(line);
    
    // calculate position for status (something at column ~350)
    // status is 6 chars "[ OK ]" or "[FAIL]"
    int status_x = 350;  // right-aligned position for status
    boot_tft_set_cursor(status_x, boot_line_y);
    
    // print status with appropriate color
    if (status == BOOT_STATUS_OK) {
        boot_tft_set_text_color(COLOR_GREEN, COLOR_WHITE);
        boot_tft_print("[ OK ]");
    } else {
        boot_tft_set_text_color(COLOR_RED, COLOR_WHITE);
        boot_tft_print("[FAIL]");
    }
    
    // move to next line for next message
    boot_line_y += 20;
    
    // scroll if we run out of space
    // this isn't really a scroll, ill figure out how to make real scrolling later
    // i dont like working with this library......
    if (boot_line_y > 300) {
        // clear the boot message area
        boot_tft_fill_rect(0, 20, 480, 280, COLOR_WHITE);
        boot_line_y = 20;  // reset to start
    }
#endif
    
    // also print to debug
    if (status == BOOT_STATUS_OK) {
        DEBUG_PRINT("[BOOT] %-30s [ OK ]\n", step_name);
    } else {
        DEBUG_PRINT("[BOOT] %-30s [FAIL]\n", step_name);
    }
}

boot_step_result_t boot_step(const char *step_name, int (*init_func)(void)) {
    boot_step_result_t result = {BOOT_STATUS_FAIL, step_name};
    (void)step_name;  // used in result.message
    
    if (init_func == NULL) {
        boot_display_message(result.message, BOOT_STATUS_FAIL);
        return result;
    }
    
    int ret = init_func();
    
    if (ret == 0) {
        result.status = BOOT_STATUS_OK;
        boot_display_message(result.message, BOOT_STATUS_OK);
    } else {
        result.status = BOOT_STATUS_FAIL;
        boot_display_message(result.message, BOOT_STATUS_FAIL);
    }
    
    return result;
}

void boot_halt(const char *message) {
    DEBUG_PRINT("[BOOT] HALT: %s\n", message);
    
#ifdef ARDUINO
    // display halt message
    boot_tft_set_cursor(10, boot_line_y + 10);
    boot_tft_set_text_size(2);
    boot_tft_set_text_color(COLOR_RED, COLOR_WHITE);
    boot_tft_print("BOOT FAILED - Entering Emergency TTY");
    
    delay_ms(1000);
    
    // enter emergency TTY
    // this doesn't actually exist yet
    // and i dont know if it ever will, why do you need emergency tty on something like this?
    // eh ill add it anyway
    // this reminds me I need to add root
    boot_emergency_tty();
#else
    // PC: exit
    fprintf(stderr, "SYSTEM HALTED: %s\n", message ? message : "Unknown error");
    exit(1);
#endif
}

void boot_emergency_tty(void) {
    DEBUG_PRINT("[BOOT] Entering emergency TTY\n");
    
#ifdef ARDUINO
    // clear screen and show emergency prompt
    boot_tft_fill_screen(COLOR_WHITE);
    boot_tft_set_cursor(10, 10);
    boot_tft_set_text_size(2);
    boot_tft_set_text_color(COLOR_RED, COLOR_WHITE);
    boot_tft_print("EMERGENCY TTY");
    boot_tft_set_cursor(10, 40);
    boot_tft_set_text_color(COLOR_BLACK, COLOR_WHITE);
    boot_tft_print("System in emergency mode");
    boot_tft_set_cursor(10, 70);
    boot_tft_print("PLACEHOLDER - Not implemented");
    
    // this would provide a minimal terminal for recovery
    while (1) {
        delay_ms(1000);
        // emergency TTY loop would go here
    }
#else
    fprintf(stderr, "EMERGENCY TTY (placeholder)\n");
    while (1) {
        delay_ms(1000);
    }
#endif
}

void boot_start_desktop(void) {
    DEBUG_PRINT("[BOOT] Starting desktop\n");
    
#ifdef ARDUINO
    // PLACEHOLDER: Desktop not implemented yet
    // this would start the main desktop environment
    boot_tft_set_cursor(10, boot_line_y + 10);
    boot_tft_set_text_size(2);
    boot_tft_set_text_color(COLOR_BLACK, COLOR_WHITE);
    boot_tft_print("Starting desktop...");
    delay_ms(500);
    
    // desktop would be initialized here
    // for now, just continue to main loop
#else
    fprintf(stderr, "Desktop (placeholder)\n");
#endif
}

int boot_verify_systems_ready(void) {
    // verify all critical systems are ready
    // return 0 if all ready, non-zero if any system failed
    
    // check process system
    // check scheduler
    // check terminal system
    // check event system
    
    // for now, all checks pass
    return 0;
}

// boot step implementations
int boot_low_level_bringup(void) {
    // Low-level hardware initialization
    // - GPIO setup
    // - Clock configuration
    // - Basic peripherals
    
#ifdef ARDUINO
    // ESP32 low-level init is handled by Arduino framework
    // additional low-level setup can go here
    delay_ms(100);  // allow hardware to stabilize
    return 0;
#else
    // PC: no low-level hardware
    return 0;
#endif
}

int boot_init_core_services(void) {
    // Initialize core system services
    // - event queue
    // - debug system
    // - basic I/O
    
    // these are already initialized by framework, but we verify
    return 0;
}

int boot_mount_sd(void) {
    // mount micro SD card
    // PLACEHOLDER: SD card not connected yet
    
#ifdef ARDUINO
    // SD card initialization would go here
    // for now, just return success (placeholder)
    // so many placeholders..
    delay_ms(50);
    return 0;
#else
    // PC: no SD card
    return 0;
#endif
}

int boot_init_os_subsystems(void) {
    // Initialize all OS subsystems
    // - Process system
    // - Scheduler
    // - Script system
    // - Terminal system
    
    #include "process.h"
    #include "process_scheduler.h"
    #include "process_script.h"
    #include "terminal.h"
    
    init_process_system();
    init_scheduler();
    init_script_system();
    init_terminal_system();
    
    return 0;
    // need checks later TODO
}

int boot_register_commands(void) {
    // register built-in terminal commands
    #include "terminal_cmd.h"
    
    init_terminal_commands();
    
    return 0;
}

int boot_init_processes(void) {
    // start initial system processes
    // PLACEHOLDER: No initial processes yet
    
    // could start background processes here
    return 0;
}

int boot_start_event_loop(void) {
    // start main event loop/sequencer
    // PLACEHOLDER: Event loop is in main loop()
    
    // this step just verifies everything is ready
    return 0;
}

void boot_sequence_run(void) {
    boot_sequence_init();
    
    DEBUG_PRINT("[BOOT] Starting boot sequence...\n");
    
    // step 1: Low-level bring-up
    boot_step_result_t result = boot_step("Low-level bring-up", boot_low_level_bringup);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("Low-level bring-up failed");
    }
    
    // step 2: Init core services
    result = boot_step("Init core services", boot_init_core_services);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("Core services initialization failed");
    }
    
    // step 3: Mount SD card
    result = boot_step("Mount micro SD", boot_mount_sd);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("SD card mount failed");
    }
    
    // step 4: Init OS subsystems
    result = boot_step("Init OS subsystems", boot_init_os_subsystems);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("OS subsystems initialization failed");
    }
    
    // step 5: Register commands
    result = boot_step("Register commands", boot_register_commands);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("Command registration failed");
    }
    
    // step 6: Init processes
    result = boot_step("Init processes", boot_init_processes);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("Process initialization failed");
    }
    
    // step 7: Start event loop
    result = boot_step("Start event loop", boot_start_event_loop);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("Event loop start failed");
    }
    
    // step numero 8: Verify all systems ready
    result = boot_step("Verify systems ready", boot_verify_systems_ready);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("System verification failed");
    }
    
    // boot complete! {:
    boot_display_message("Boot complete", BOOT_STATUS_OK);
    DEBUG_PRINT("[BOOT] Boot sequence complete!\n");
    
#ifdef ARDUINO
    delay_ms(500);  // a short respite before continuing.. {:
    
    // start desktop!
    boot_start_desktop();
#endif
}

