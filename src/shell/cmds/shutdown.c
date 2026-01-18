#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"
#include "shell_error.h"
#include "boot_sequence.h"
#include "boot_splash.h"
#ifdef ARDUINO
#include <Arduino.h>
#include <esp_sleep.h>
#endif

int cmd_shutdown(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_shutdown_def = {
    .name = "shutdown",
    .handler = cmd_shutdown,
    .help = "Shutdown system"
};

int cmd_shutdown(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    (void)argv;
    
    if (argc > 1) {
        shell_error(term, "shutdown: too many arguments");
        return SHELL_EINVAL;
    }
    
    terminal_write_line(term, "Shutting down...");
#ifdef ARDUINO
    boot_sd_unmount();
    boot_tft_shutdown();
    delay(100);
    esp_deep_sleep_start();
    while (1) {
        delay(1000);
    }
#else
    return SHELL_OK;
#endif
}

