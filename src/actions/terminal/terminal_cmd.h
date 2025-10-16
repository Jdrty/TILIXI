#pragma once
#include <string.h>
#include <stdio.h>

typedef int (*cmd_handler) (int argc, char **argv); // tells us the number of arguments and what each argument is (array of pointers to strings)

typedef struct {
    cmd_handler handler;
    const char *name;
    const char *help;
} term_cmd;

// functions
void register_cmd(cmd_handler handler, const char *name, const char *help);
