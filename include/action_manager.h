#pragma once
#include <stdint.h>
#include <string.h>
#include "terminal.h"
#include "debug_helper.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*action_handler)(void);    // function pointer type

typedef struct {
    const char *name;
    action_handler handler;
} action;

void register_action(const char *name, action_handler handler);
void execute_action(const char *name);
void init_actions(void);

#ifdef __cplusplus
}
#endif
