#pragma once
#include "terminal.h"

#ifdef __cplusplus
extern "C" {
#endif

void shell_error(terminal_state *term, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

