#include "builtins.h"
#include "shell.h"
#include <string.h>

extern void builtins_register_all(void);

#define max_builtins 32

static builtin_cmd builtins[max_builtins];
static int builtin_count = 0;

void builtins_register(const char *name, builtin_handler handler, const char *help) {
    if (builtin_count >= max_builtins || name == NULL || handler == NULL) {
        return;
    }
    
    builtins[builtin_count].name = name;
    builtins[builtin_count].handler = handler;
    builtins[builtin_count].help = help;
    builtin_count++;
}

void builtins_register_descriptor(const builtin_cmd *cmd) {
    if (builtin_count >= max_builtins || cmd == NULL || cmd->name == NULL || cmd->handler == NULL) {
        return;
    }
    
    builtins[builtin_count] = *cmd;
    builtin_count++;
}

builtin_cmd *builtins_find(const char *name) {
    if (name == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < builtin_count; i++) {
        if (strcmp(builtins[i].name, name) == 0) {
            return &builtins[i];
        }
    }
    
    return NULL;
}

void builtins_init(void) {
    builtin_count = 0;
    memset(builtins, 0, sizeof(builtins));
    builtins_register_all();
}

