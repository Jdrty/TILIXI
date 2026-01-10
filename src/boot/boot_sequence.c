// THIS FILE NEEEEEDS TO BE REFACTORED

// NOTE TO SELF SINCE I REALIZED SOMETHING WHILE ADDING SD:
// sd card contents are gonna have to be put onto the esp itself while in use since turns out
// even if you have two sets of spi pins, you can't use both at the same time.
// therefore, to make any of this work, we need to find a way to determine whether a file is needed,
// if we know it will be needed no matter what when entering a dir, load it as well as the dir we enter
// there will obviously be some common essentials that will always be loaded onto the esp.
// then, some actions may need specific dependancies, so while doing that action, do a quick swap from the tft to spi and load it.
// anyway, you get the point, create the illusion of persistent files while in reality we just load whatevers needed.
// while writing this, i realize this is what i would've done even without this hurdle just without turning off the tft.......
// because obviously you cant interact with sd card contents as if they're active on the microcontroller. whatever im not deleting this

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
    void boot_tft_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    int16_t boot_tft_get_width(void);
    int16_t boot_tft_get_height(void);
    
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
static uint8_t boot_complete = 0;  // track if boot sequence has finished

void boot_sequence_init(void) {
    if (boot_initialized) return;
    
#ifdef ARDUINO
    // TFT is already initialized by boot_init() in main_esp32.cpp
    // start displaying boot messages 
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
    boot_emergency_tty(message);
#else
    // PC: exit
    fprintf(stderr, "SYSTEM HALTED: %s\n", message ? message : "Unknown error");
    exit(1);
#endif
}

void boot_emergency_tty(const char *failure_reason) {
    DEBUG_PRINT("[BOOT] Entering emergency TTY\n");
    if (failure_reason) {
        DEBUG_PRINT("[BOOT] Boot failure reason: %s\n", failure_reason);
    }
    
#ifdef ARDUINO
    // clear screen and show emergency prompt
    boot_tft_fill_screen(COLOR_WHITE);
    boot_tft_set_cursor(10, 10);
    boot_tft_set_text_size(2);
    boot_tft_set_text_color(COLOR_RED, COLOR_WHITE);
    boot_tft_print("EMERGENCY TTY");
    
    // display boot failure reason
    boot_tft_set_cursor(10, 40);
    boot_tft_set_text_color(COLOR_BLACK, COLOR_WHITE);
    boot_tft_print("Boot failed:");
    
    boot_tft_set_cursor(10, 70);
    boot_tft_set_text_color(COLOR_RED, COLOR_WHITE);
    if (failure_reason && strlen(failure_reason) > 0) {
        // truncate long messages to fit on screen (approximately 40 chars at size 2)
        char display_msg[45];
        int len = strlen(failure_reason);
        if (len > 40) {
            strncpy(display_msg, failure_reason, 37);
            display_msg[37] = '.';
            display_msg[38] = '.';
            display_msg[39] = '.';
            display_msg[40] = '\0';
        } else {
            strncpy(display_msg, failure_reason, sizeof(display_msg) - 1);
            display_msg[sizeof(display_msg) - 1] = '\0';
        }
        boot_tft_print(display_msg);
    } else {
        boot_tft_print("Unknown error");
    }
    
    boot_tft_set_cursor(10, 100);
    boot_tft_set_text_color(COLOR_BLACK, COLOR_WHITE);
    boot_tft_print("System in emergency mode");
    boot_tft_set_cursor(10, 130);
    boot_tft_print("PLACEHOLDER - Not implemented");
    
    // this would provide a minimal terminal for recovery
    while (1) {
        delay_ms(1000);
        // emergency TTY loop would go here
    }
#else
    fprintf(stderr, "EMERGENCY TTY (placeholder)\n");
    if (failure_reason) {
        fprintf(stderr, "Boot failure reason: %s\n", failure_reason);
    }
    while (1) {
        delay_ms(1000);
    }
#endif
}


void boot_start_desktop(void) {
    DEBUG_PRINT("[BOOT] Starting desktop\n");
    
#ifdef ARDUINO
    // wait 2 seconds after boot completes
    delay_ms(2000);
    
    // create first terminal (fullscreen)
    #include "terminal.h"
    #include "action_manager.h"
    new_terminal();
    
    DEBUG_PRINT("[BOOT] Desktop started - Terminal ready\n");
#else
    fprintf(stderr, "Desktop started - Terminal ready\n");
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
    // low-level hardware initialization
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

int boot_init_serial(void) {
    // initialize serial communication for keyboard input
    // this ensures serial is ready for post-boot use
    // note: Serial.begin() is already called in setup() before boot_sequence_run()
    // this step just marks serial as ready in the boot sequence
    // actual serial initialization happens in main_esp32.cpp setup()
    
#ifdef ARDUINO
    // serial is already initialized in setup(), just wait a bit for it to stabilize
    delay_ms(100);  // give serial time to stabilize
    DEBUG_PRINT("[BOOT] Serial verified and ready for keyboard input\n");
    return 0;
#else
    // PC: serial is stdout/stderr, always available
    return 0;
#endif
}

int boot_init_core_services(void) {
    // initialize core system services
    // - event queue
    // - debug system
    // - basic I/O
    
    // these are already initialized by framework, but we verify
    return 0;
}

int boot_mount_sd(void) {
    // mount micro SD card
    
#ifdef ARDUINO
    int ret = boot_sd_mount();
    if (ret != 0) {
        DEBUG_PRINT("[BOOT] SD card initialization failed\n");
        return -1;
    }
    
    DEBUG_PRINT("[BOOT] SD card mounted successfully\n");
    return 0;
#else
    // PC: no SD card
    return 0;
#endif
}

int boot_init_sd_filesystem(void) {
    // check SD card filesystem and initialize required structure
    //
    // NOTE: This function currently uses direct SD library calls for initial
    // filesystem structure setup. This is a temporary exception during boot.
    // All other filesystem operations MUST go through VFS (see include/vfs.h).
    // This function will be migrated to use VFS in a future update.
    
#ifdef ARDUINO
    // check if SD card is mounted
    if (boot_sd_available() != 0) {
        DEBUG_PRINT("[BOOT] SD card not available for filesystem init\n");
        return -1;
    }
    
    // check if root directory is empty
    int root_empty = boot_sd_is_directory_empty("/");
    
    if (root_empty) {
        DEBUG_PRINT("[BOOT] SD card is empty, creating full filesystem structure\n");
        
        // create all required directories
        boot_sd_ensure_directory("/bin");
        boot_sd_ensure_directory("/dev");
        boot_sd_ensure_directory("/dev/input");
        boot_sd_ensure_directory("/dev/pipe");
        boot_sd_ensure_directory("/etc");
        boot_sd_ensure_directory("/home");
        boot_sd_ensure_directory("/home/user");
        boot_sd_ensure_directory("/home/user/documents");
        boot_sd_ensure_directory("/proc");
        boot_sd_ensure_directory("/proc/tasks");
        boot_sd_ensure_directory("/run");
        boot_sd_ensure_directory("/run/pipes");
        boot_sd_ensure_directory("/run/tasks");
        boot_sd_ensure_directory("/run/events");
        boot_sd_ensure_directory("/tmp");
        boot_sd_ensure_directory("/usr");
        boot_sd_ensure_directory("/usr/bin");
        boot_sd_ensure_directory("/usr/bin/games");
        boot_sd_ensure_directory("/usr/bin/demos");
        boot_sd_ensure_directory("/usr/share");
        boot_sd_ensure_directory("/usr/share/help");
        boot_sd_ensure_directory("/usr/share/fonts");
        boot_sd_ensure_directory("/usr/share/banners");
        boot_sd_ensure_directory("/var");
        boot_sd_ensure_directory("/var/log");
        
        // create all required files in /bin
        boot_sd_ensure_file("/bin/sh", NULL);
        boot_sd_ensure_file("/bin/ls", NULL);
        boot_sd_ensure_file("/bin/cat", NULL);
        boot_sd_ensure_file("/bin/echo", NULL);
        boot_sd_ensure_file("/bin/ps", NULL);
        boot_sd_ensure_file("/bin/kill", NULL);
        boot_sd_ensure_file("/bin/clear", NULL);
        boot_sd_ensure_file("/bin/help", NULL);
        boot_sd_ensure_file("/bin/reboot", NULL);
        boot_sd_ensure_file("/bin/nano", NULL);
        boot_sd_ensure_file("/bin/top", NULL);
        boot_sd_ensure_file("/bin/uptime", NULL);
        boot_sd_ensure_file("/bin/meminfo", NULL);
        boot_sd_ensure_file("/bin/logread", NULL);
        
        // create all required files in /dev
        boot_sd_ensure_file("/dev/tty", NULL);
        boot_sd_ensure_file("/dev/tty0", NULL);
        boot_sd_ensure_file("/dev/null", NULL);
        boot_sd_ensure_file("/dev/input/keyboard", NULL);
        
        // create all required files in /etc
        boot_sd_ensure_file("/etc/passwd", NULL);
        boot_sd_ensure_file("/etc/shells", NULL);
        boot_sd_ensure_file("/etc/system.conf", NULL);
        boot_sd_ensure_file("/etc/tty.conf", NULL);
        boot_sd_ensure_file("/etc/keymap.conf", NULL);
        boot_sd_ensure_file("/etc/motd", NULL);
        
        // create all required files in /home/user
        boot_sd_ensure_file("/home/user/.profile", NULL);
        boot_sd_ensure_file("/home/user/.history", NULL);
        boot_sd_ensure_file("/home/user/.editorrc", NULL);
        
        // create all required files in /proc
        boot_sd_ensure_file("/proc/uptime", NULL);
        boot_sd_ensure_file("/proc/meminfo", NULL);
        boot_sd_ensure_file("/proc/version", NULL);
        boot_sd_ensure_file("/proc/sched", NULL);
        
        // create all required files in /run
        boot_sd_ensure_file("/run/tty.lock", NULL);
        boot_sd_ensure_file("/run/scheduler.lock", NULL);
        boot_sd_ensure_file("/run/pipes/3", NULL);
        boot_sd_ensure_file("/run/pipes/4", NULL);
        boot_sd_ensure_file("/run/tasks/1", NULL);
        boot_sd_ensure_file("/run/tasks/2", NULL);
        boot_sd_ensure_file("/run/events/queue", NULL);
        
        // create all required files in /tmp
        boot_sd_ensure_file("/tmp/.keep", NULL);
        
        // create all required files in /var/log
        boot_sd_ensure_file("/var/log/kernel.log", NULL);
        boot_sd_ensure_file("/var/log/scheduler.log", NULL);
        boot_sd_ensure_file("/var/log/terminal.log", NULL);
        boot_sd_ensure_file("/var/log/input.log", NULL);
        boot_sd_ensure_file("/var/log/boot.log", NULL);
        
        DEBUG_PRINT("[BOOT] Filesystem structure created successfully\n");
    } else {
        DEBUG_PRINT("[BOOT] SD card is not empty, verifying required files exist\n");
        
        // verify and create missing directories
        boot_sd_ensure_directory("/bin");
        boot_sd_ensure_directory("/dev");
        boot_sd_ensure_directory("/dev/input");
        boot_sd_ensure_directory("/dev/pipe");
        boot_sd_ensure_directory("/etc");
        boot_sd_ensure_directory("/home");
        boot_sd_ensure_directory("/home/user");
        boot_sd_ensure_directory("/home/user/documents");
        boot_sd_ensure_directory("/proc");
        boot_sd_ensure_directory("/proc/tasks");
        boot_sd_ensure_directory("/run");
        boot_sd_ensure_directory("/run/pipes");
        boot_sd_ensure_directory("/run/tasks");
        boot_sd_ensure_directory("/run/events");
        boot_sd_ensure_directory("/tmp");
        boot_sd_ensure_directory("/usr");
        boot_sd_ensure_directory("/usr/bin");
        boot_sd_ensure_directory("/usr/bin/games");
        boot_sd_ensure_directory("/usr/bin/demos");
        boot_sd_ensure_directory("/usr/share");
        boot_sd_ensure_directory("/usr/share/help");
        boot_sd_ensure_directory("/usr/share/fonts");
        boot_sd_ensure_directory("/usr/share/banners");
        boot_sd_ensure_directory("/var");
        boot_sd_ensure_directory("/var/log");
        
        // verify and create missing files in /bin
        boot_sd_ensure_file("/bin/sh", NULL);
        boot_sd_ensure_file("/bin/ls", NULL);
        boot_sd_ensure_file("/bin/cat", NULL);
        boot_sd_ensure_file("/bin/echo", NULL);
        boot_sd_ensure_file("/bin/ps", NULL);
        boot_sd_ensure_file("/bin/kill", NULL);
        boot_sd_ensure_file("/bin/clear", NULL);
        boot_sd_ensure_file("/bin/help", NULL);
        boot_sd_ensure_file("/bin/reboot", NULL);
        boot_sd_ensure_file("/bin/nano", NULL);
        boot_sd_ensure_file("/bin/top", NULL);
        boot_sd_ensure_file("/bin/uptime", NULL);
        boot_sd_ensure_file("/bin/meminfo", NULL);
        boot_sd_ensure_file("/bin/logread", NULL);
        
        // verify and create missing files in /dev
        boot_sd_ensure_file("/dev/tty", NULL);
        boot_sd_ensure_file("/dev/tty0", NULL);
        boot_sd_ensure_file("/dev/null", NULL);
        boot_sd_ensure_file("/dev/input/keyboard", NULL);
        
        // verify and create missing files in /etc
        boot_sd_ensure_file("/etc/passwd", NULL);
        boot_sd_ensure_file("/etc/shells", NULL);
        boot_sd_ensure_file("/etc/system.conf", NULL);
        boot_sd_ensure_file("/etc/tty.conf", NULL);
        boot_sd_ensure_file("/etc/keymap.conf", NULL);
        boot_sd_ensure_file("/etc/motd", NULL);
        
        // verify and create missing files in /home/user
        boot_sd_ensure_file("/home/user/.profile", NULL);
        boot_sd_ensure_file("/home/user/.history", NULL);
        boot_sd_ensure_file("/home/user/.editorrc", NULL);
        
        // verify and create missing files in /proc
        boot_sd_ensure_file("/proc/uptime", NULL);
        boot_sd_ensure_file("/proc/meminfo", NULL);
        boot_sd_ensure_file("/proc/version", NULL);
        boot_sd_ensure_file("/proc/sched", NULL);
        
        // verify and create missing files in /run
        boot_sd_ensure_file("/run/tty.lock", NULL);
        boot_sd_ensure_file("/run/scheduler.lock", NULL);
        boot_sd_ensure_file("/run/pipes/3", NULL);
        boot_sd_ensure_file("/run/pipes/4", NULL);
        boot_sd_ensure_file("/run/tasks/1", NULL);
        boot_sd_ensure_file("/run/tasks/2", NULL);
        boot_sd_ensure_file("/run/events/queue", NULL);
        
        // verify and create missing files in /tmp
        boot_sd_ensure_file("/tmp/.keep", NULL);
        
        // verify and create missing files in /var/log
        boot_sd_ensure_file("/var/log/kernel.log", NULL);
        boot_sd_ensure_file("/var/log/scheduler.log", NULL);
        boot_sd_ensure_file("/var/log/terminal.log", NULL);
        boot_sd_ensure_file("/var/log/input.log", NULL);
        boot_sd_ensure_file("/var/log/boot.log", NULL);
        
        DEBUG_PRINT("[BOOT] Filesystem structure verified and missing files added\n");
    }
    
    // restore SPI for TFT display now that filesystem initialization is complete
    boot_sd_restore_tft_spi();
    
    return 0;
#else
    // PC: no SD card filesystem
    return 0;
#endif
}

int boot_init_os_subsystems(void) {
    // initialize all OS subsystems
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

// process dependency definition
typedef struct {
    const char *name;                      // process name
    void (*entry_point)(void *args);       // entry point function
    void *args;                            // arguments for entry point
    process_priority_t priority;            // process priority
    uint16_t stack_size_words;             // stack size (0 for default)
    const char **dependencies;             // array of dependency names (NULL-terminated)
    uint8_t dependency_count;              // number of dependencies
    process_id_t pid;                      // assigned PID after creation
    uint8_t started;                       // whether process has been started
} boot_process_def_t;

// maximum number of initial processes
#define max_boot_processes 8

// process health check result
typedef struct {
    process_id_t pid;
    const char *name;
    uint8_t is_healthy;                    // 1 if healthy, 0 if not
    process_state_t state;
} process_health_t;

// initial system processes (add more as needed)
static boot_process_def_t boot_processes[max_boot_processes];
static uint8_t boot_process_count = 0;

// example system process entry points (can be expanded)
static void system_idle_task(void *args) {
    (void)args;
    DEBUG_PRINT("[BOOT_PROCESS] System idle task started\n");
    // idle task runs continuously
    while (1) {
        process_yield();
        delay_ms(100);
    }
}

// register a boot process definition
static void boot_register_process(const char *name, 
                                  void (*entry_point)(void *args),
                                  void *args,
                                  process_priority_t priority,
                                  uint16_t stack_size_words,
                                  const char **dependencies) {
    if (boot_process_count >= max_boot_processes) {
        DEBUG_PRINT("[BOOT] Warning: Max boot processes reached, cannot register: %s\n", name);
        return;
    }
    
    boot_process_def_t *def = &boot_processes[boot_process_count];
    def->name = name;
    def->entry_point = entry_point;
    def->args = args;
    def->priority = priority;
    def->stack_size_words = stack_size_words;
    def->dependencies = dependencies;
    def->pid = 0;
    def->started = 0;
    
    // count dependencies
    def->dependency_count = 0;
    if (dependencies != NULL) {
        while (dependencies[def->dependency_count] != NULL) {
            def->dependency_count++;
        }
    }
    
    boot_process_count++;
    DEBUG_PRINT("[BOOT] Registered boot process: %s (dependencies: %d)\n", 
               name, def->dependency_count);
}

// check if a process dependency is satisfied (dependency process is running)
static uint8_t boot_check_dependency_satisfied(const char *dep_name) {
    // find the dependency process
    for (uint8_t i = 0; i < boot_process_count; i++) {
        if (boot_processes[i].name != NULL && 
            strcmp(boot_processes[i].name, dep_name) == 0) {
            // check if dependency process is started and running
            if (boot_processes[i].started && boot_processes[i].pid != 0) {
                process_control_block_t *pcb = process_get_pcb(boot_processes[i].pid);
                if (pcb != NULL && pcb->active) {
                    process_state_t state = process_get_state(boot_processes[i].pid);
                    // dependency is satisfied if process is running or ready
                    return (state == process_state_running || 
                            state == process_state_ready);
                }
            }
            return 0;  // dependency not started or not running
        }
    }
    // dependency not found in boot processes (might be external)
    // for now, assume satisfied if not found
    return 1;
}

// check if all dependencies for a process are satisfied
static uint8_t boot_check_all_dependencies_satisfied(boot_process_def_t *def) {
    if (def->dependencies == NULL || def->dependency_count == 0) {
        return 1;  // no dependencies
    }
    
    for (uint8_t i = 0; i < def->dependency_count; i++) {
        if (!boot_check_dependency_satisfied(def->dependencies[i])) {
            return 0;  // at least one dependency not satisfied
        }
    }
    return 1;  // all dependencies satisfied
}

// start a single boot process
static int boot_start_process(boot_process_def_t *def) {
    if (def->started) {
        DEBUG_PRINT("[BOOT] Process already started: %s\n", def->name);
        return 0;  // already started
    }
    
    // check dependencies
    if (!boot_check_all_dependencies_satisfied(def)) {
        DEBUG_PRINT("[BOOT] Dependencies not satisfied for: %s\n", def->name);
        return -1;  // dependencies not ready
    }
    
    // create the process
    process_id_t pid = process_create(def->name, def->entry_point, def->args,
                                     def->priority, def->stack_size_words);
    
    if (pid == 0) {
        DEBUG_PRINT("[BOOT] Failed to create process: %s\n", def->name);
        return -1;  // creation failed
    }
    
    def->pid = pid;
    def->started = 1;
    
    DEBUG_PRINT("[BOOT] Started process: %s (PID=%d)\n", def->name, pid);
    
    // give process time to initialize (especially on ESP32)
    delay_ms(50);
    
    return 0;
}

// start all boot processes in dependency order
static int boot_start_all_processes(void) {
    uint8_t started_this_round = 1;
    uint8_t max_iterations = boot_process_count * 2;  // prevent infinite loops
    uint8_t iteration = 0;
    
    while (started_this_round > 0 && iteration < max_iterations) {
        started_this_round = 0;
        iteration++;
        
        // try to start each process that hasn't been started yet
        for (uint8_t i = 0; i < boot_process_count; i++) {
            if (!boot_processes[i].started) {
                if (boot_start_process(&boot_processes[i]) == 0) {
                    started_this_round++;
                }
            }
        }
    }
    
    // check if all processes were started
    uint8_t all_started = 1;
    for (uint8_t i = 0; i < boot_process_count; i++) {
        if (!boot_processes[i].started) {
            DEBUG_PRINT("[BOOT] Failed to start process: %s\n", boot_processes[i].name);
            all_started = 0;
        }
    }
    
    if (!all_started) {
        DEBUG_PRINT("[BOOT] Warning: Some processes failed to start\n");
    }
    
    return (all_started ? 0 : -1);
}

// check health of a single process
static process_health_t boot_check_process_health(boot_process_def_t *def) {
    process_health_t health = {0};
    health.pid = def->pid;
    health.name = def->name;
    health.is_healthy = 0;
    health.state = process_state_terminated;
    
    if (!def->started || def->pid == 0) {
        return health;  // process not started
    }
    
    process_control_block_t *pcb = process_get_pcb(def->pid);
    if (pcb == NULL || !pcb->active) {
        return health;  // process not found or inactive
    }
    
    health.state = process_get_state(def->pid);
    
    // process is healthy if it's running or ready
    // terminated or blocked might indicate issues
    health.is_healthy = (health.state == process_state_running || 
                        health.state == process_state_ready);
    
    return health;
}

// check health of all boot processes
static int boot_check_all_processes_health(void) {
    uint8_t all_healthy = 1;
    
    for (uint8_t i = 0; i < boot_process_count; i++) {
        process_health_t health = boot_check_process_health(&boot_processes[i]);
        
        if (boot_processes[i].started) {
            if (!health.is_healthy) {
                DEBUG_PRINT("[BOOT] Process health check failed: %s (PID=%d, state=%d)\n",
                           health.name, health.pid, health.state);
                all_healthy = 0;
            } else {
                DEBUG_PRINT("[BOOT] Process health check OK: %s (PID=%d, state=%d)\n",
                           health.name, health.pid, health.state);
            }
        }
    }
    
    return (all_healthy ? 0 : -1);
}

int boot_init_processes(void) {
    // initialize boot process registry
    boot_process_count = 0;
    for (uint8_t i = 0; i < max_boot_processes; i++) {
        boot_processes[i].name = NULL;
        boot_processes[i].entry_point = NULL;
        boot_processes[i].args = NULL;
        boot_processes[i].priority = process_priority_normal;
        boot_processes[i].stack_size_words = 0;
        boot_processes[i].dependencies = NULL;
        boot_processes[i].dependency_count = 0;
        boot_processes[i].pid = 0;
        boot_processes[i].started = 0;
    }
    
    // register initial system processes
    // example: system idle task (no dependencies)
    boot_register_process("system_idle", system_idle_task, NULL,
                         process_priority_low, 0, NULL);
    
    // add more initial processes here as needed
    // example with dependencies:
    // const char *service_deps[] = {"system_idle", NULL};
    // boot_register_process("service_name", service_entry, NULL,
    //                      process_priority_normal, 0, service_deps);
    
    // start all processes in dependency order
    int ret = boot_start_all_processes();
    if (ret != 0) {
        DEBUG_PRINT("[BOOT] Warning: Some processes failed to start\n");
        // continue anyway, but log the issue
    }
    
    // perform initial health check
    delay_ms(100);  // give processes time to initialize
    ret = boot_check_all_processes_health();
    if (ret != 0) {
        DEBUG_PRINT("[BOOT] Warning: Some processes failed health check\n");
        // continue anyway, but log the issue
    }
    
    DEBUG_PRINT("[BOOT] Initialized %d boot processes\n", boot_process_count);
    return 0;  // return success even if some processes have issues
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
    
    // step 2: Init serial communication
    result = boot_step("Init serial", boot_init_serial);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("Serial initialization failed");
    }
    
    // step 3: Init core services
    result = boot_step("Init core services", boot_init_core_services);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("Core services initialization failed");
    }
    
    // step 4: Mount SD card
    result = boot_step("Mount micro SD", boot_mount_sd);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("SD card mount failed");
    }
    
    // step 5: Init SD filesystem
    result = boot_step("Init SD filesystem", boot_init_sd_filesystem);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("SD filesystem initialization failed");
    }
    
    // step 6: Init OS subsystems
    result = boot_step("Init OS subsystems", boot_init_os_subsystems);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("OS subsystems initialization failed");
    }
    
    // step 7: Register commands
    result = boot_step("Register commands", boot_register_commands);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("Command registration failed");
    }
    
    // step 8: Init processes
    result = boot_step("Init processes", boot_init_processes);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("Process initialization failed");
    }
    
    // step 9: Start event loop
    result = boot_step("Start event loop", boot_start_event_loop);
    if (result.status != BOOT_STATUS_OK) {
        boot_halt("Event loop start failed");
    }
    
    // step 10: Verify all systems ready
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
    
    // mark boot as complete - keyboard input can now be active
    boot_complete = 1;
    DEBUG_PRINT("[BOOT] Keyboard input now active\n");
#endif
}

int boot_is_complete(void) {
    // return 1 if boot sequence has finished, 0 otherwise
    return boot_complete;
}

