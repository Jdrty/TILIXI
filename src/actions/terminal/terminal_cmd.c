#include "../../../include/terminal_cmd.h"

// cmds implimentations
int change_directory(int argc, char **argv) {
    if (argc == 2) {    // once filesystem is implimented, check if the target directory exists
        // again, once implimented change current dir
        return 0;
    }
    return -1;
};

// cmd registry
#define max_cmd 24
static term_cmd cmds[max_cmd];
static uint8_t cmd_count = 0;

void register_cmd(cmd_handler handler, const char *name, const char *help) {
    if (cmd_count < max_cmd) {
        cmds[cmd_count++] = (term_cmd) { handler, name, help };
    }
};

void essential_cmds() {
}
