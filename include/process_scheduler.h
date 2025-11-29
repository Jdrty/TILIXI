#pragma once
#include "process.h"

#ifdef __cplusplus
extern "C" {
#endif

// scheduler initialization
void init_scheduler(void);

// scheduler operations
void scheduler_tick(void);  // called periodically to run scheduler
void scheduler_run(void);    // run the scheduler once
process_id_t scheduler_get_current(void);  // get currently running process

// yield control to scheduler (for cooperative multitasking)
void process_yield(void);

#ifdef __cplusplus
}
#endif

