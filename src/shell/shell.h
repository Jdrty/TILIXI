#pragma once
#include "terminal.h"

#ifdef __cplusplus
extern "C" {
#endif

void shell_set_terminal(terminal_state *term);
terminal_state *shell_get_terminal(void);
void shell_repl(void);

#ifdef __cplusplus
}
#endif

