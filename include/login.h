#pragma once
#include "terminal.h"

#ifdef __cplusplus
extern "C" {
#endif

void login_begin_if_needed(terminal_state *term);
int login_is_active(void);
void login_handle_key_event(key_event evt);

#ifdef __cplusplus
}
#endif

