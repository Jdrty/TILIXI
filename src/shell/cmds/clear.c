#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"
#include "shell_error.h"
#include <string.h>

int cmd_clear(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_clear_def = {
    .name = "clear",
    .handler = cmd_clear,
    .help = "Clear terminal screen and history"
};

int cmd_clear(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    (void)argv;
    
    if (argc > 1) {
        shell_error(term, "clear: too many arguments");
        return SHELL_EINVAL;
    }
    
    // clear terminal buffer
    terminal_clear(term);

    // clear fastfetch overlay state
    term->fastfetch_image_active = 0;
    term->fastfetch_image_path[0] = '\0';
    term->fastfetch_start_row = 0;
    term->fastfetch_line_count = 0;
#ifdef ARDUINO
    if (term->fastfetch_image_pixels != NULL) {
        free(term->fastfetch_image_pixels);
        term->fastfetch_image_pixels = NULL;
    }
    term->fastfetch_image_w = 0;
    term->fastfetch_image_h = 0;
#endif
    
    // clear terminal history
    term->history_count = 0;
    term->history_pos = 0;
    for (uint8_t i = 0; i < max_input_history; i++) {
        memset(term->history[i], 0, terminal_cols);
    }
    
    #ifdef ARDUINO
    extern void terminal_render_all(void);
    terminal_render_all();
    #endif
    return SHELL_OK;
}




