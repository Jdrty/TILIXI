#pragma once
#include "terminal.h"

#ifdef __cplusplus
extern "C" {
#endif

void firstboot_begin_if_needed(terminal_state *term);
int firstboot_is_active(void);
void firstboot_handle_key_event(key_event evt);

#ifdef __cplusplus
}
#endif


