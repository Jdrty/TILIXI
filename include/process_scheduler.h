#pragma once
#include "process.h"

#ifdef __cplusplus
extern "C" {
#endif

// scheduler initialization
void init_scheduler(void);

// scheduler operations
// NOTE: Platform behavior differences:
//   - PC: cooperative multitasking, scheduler_run() executes processes synchronously
//   - ESP32: preemptive multitasking, FreeRTOS handles scheduling automatically
//            scheduler_run() and scheduler_tick() are no-ops on ESP32
void scheduler_tick(void);  // called periodically to run scheduler (no-op on ESP32)
void scheduler_run(void);    // run the scheduler once (no-op on ESP32)
process_id_t scheduler_get_current(void);  // get currently running process

// yield control to scheduler
// NOTE: Platform behavior differences:
//   - PC: guarantees another task runs (cooperative yield)
//   - ESP32: only yields if higher-priority task is ready (preemptive yield)
void process_yield(void);

#ifdef __cplusplus
}
#endif

