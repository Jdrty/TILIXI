#pragma once
#include "terminal.h"
#include "parser.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*builtin_handler)(terminal_state *term, int argc, char **argv);

typedef struct {
    const char *name;
    builtin_handler handler;
    const char *help;
} builtin_cmd;

void builtins_register(const char *name, builtin_handler handler, const char *help);
void builtins_register_descriptor(const builtin_cmd *cmd);
builtin_cmd *builtins_find(const char *name);
void builtins_init(void);

#ifdef __cplusplus
}
#endif

