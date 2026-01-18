#pragma once
#include "terminal.h"

#ifdef __cplusplus
extern "C" {
#endif

int passwd_is_active(void);
void passwd_handle_key_event(key_event evt);

#ifdef __cplusplus
}
#endif


