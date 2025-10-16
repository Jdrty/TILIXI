#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "terminal/terminal.h"

typedef void (*action_handler)(void);    // function pointer type

typedef struct {
    const char *name;
    action_handler handler;
} action;

void register_action(const char *name, action_handler handler);
void execute_action(const char *name);
void init_actions(void);
