#include "terminal_cmd.h"
#include "terminal.h"
#include "debug_helper.h"
#include <string.h>
#include <stdio.h>

// cmd registry
#define max_cmd 24
static term_cmd cmds[max_cmd];
static uint8_t cmd_count = 0;

void register_cmd(cmd_handler handler, const char *name, const char *help) {
    if (cmd_count < max_cmd) {
        cmds[cmd_count++] = (term_cmd) { handler, name, help };
        DEBUG_PRINT("[CMD] Registered: %s\n", name);
    }
}

term_cmd *find_cmd(const char *name) {
    if (name == NULL) return NULL;
    
    for (uint8_t i = 0; i < cmd_count; i++) {
        if (strcmp(cmds[i].name, name) == 0) {
            return &cmds[i];
        }
    }
    return NULL;
}

// cmd implementations
static int cmd_echo(int argc, char **argv) {
    terminal_state *term = get_active_terminal();
    if (term == NULL) return -1;
    
    for (int i = 1; i < argc; i++) {
        terminal_write_string(term, argv[i]);
        if (i < argc - 1) {
            terminal_write_char(term, ' ');
        }
    }
    terminal_newline(term);
    return 0;
}

static int cmd_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    terminal_state *term = get_active_terminal();
    if (term == NULL) return -1;
    
    terminal_clear(term);
    return 0;
}

static int cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;
    terminal_state *term = get_active_terminal();
    if (term == NULL) return -1;
    
    terminal_write_line(term, "Available commands:");
    for (uint8_t i = 0; i < cmd_count; i++) {
        char line[128];
        snprintf(line, sizeof(line), "  %s - %s\n", cmds[i].name, cmds[i].help);
        terminal_write_string(term, line);
    }
    return 0;
}

static int cmd_ls(int argc, char **argv) {
    (void)argc;
    (void)argv;
    terminal_state *term = get_active_terminal();
    if (term == NULL) return -1;
    
    // placeholder - this will list files when filesystem is implemented
    terminal_write_line(term, "Filesystem not yet implemented");
    return 0;
}

static int cmd_cd(int argc, char **argv) {
    (void)argv;  // suppress unused parameter warning
    terminal_state *term = get_active_terminal();
    if (term == NULL) return -1;
    
    if (argc == 1) {
        // no args - would go to home directory
        terminal_write_line(term, "cd: home directory not implemented");
        return 0;
    } else if (argc == 2) {
        // change to specified directory
        // once filesystem is implemented, check if directory exists
        terminal_write_line(term, "cd: filesystem not yet implemented");
        return 0;
    } else {
        terminal_write_line(term, "cd: too many arguments");
        return -1;
    }
    // i really dont want to impliment all of this....
}

static int cmd_pwd(int argc, char **argv) {
    (void)argc;
    (void)argv;
    terminal_state *term = get_active_terminal();
    if (term == NULL) return -1;
    
    // placeholder - would show current directory
    terminal_write_line(term, "/");
    return 0;
}

static int cmd_exit(int argc, char **argv) {
    (void)argc;
    (void)argv;
    terminal_state *term = get_active_terminal();
    if (term == NULL) return -1;
    
    close_terminal();
    return 0;
}

void init_terminal_commands(void) {
    register_cmd(cmd_echo, "echo", "Echo arguments to terminal");
    register_cmd(cmd_clear, "clear", "Clear terminal screen");
    register_cmd(cmd_help, "help", "Show available commands");
    register_cmd(cmd_ls, "ls", "List directory contents");
    register_cmd(cmd_cd, "cd", "Change directory");
    register_cmd(cmd_pwd, "pwd", "Print working directory");
    register_cmd(cmd_exit, "exit", "Close terminal");
    
    DEBUG_PRINT("[CMD] Initialized %d commands\n", cmd_count);
}
