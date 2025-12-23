#include "process_scheduler.h"
#include "process.h"
#include "debug_helper.h"
#include "ino_helper.h"

#ifdef PLATFORM_ESP32
    #include <FreeRTOS.h>
    #include <task.h>
#endif

static process_id_t current_pid = 0;  // 0 means no process running
static uint32_t last_schedule_time = 0;
#define schedule_quantum_ms 10  // time slice per process

void init_scheduler(void) {
    current_pid = 0;
    last_schedule_time = get_time_ms();
    DEBUG_PRINT("[SCHEDULER] Scheduler initialized\n");
#ifdef PLATFORM_ESP32
    DEBUG_PRINT("[SCHEDULER] Using FreeRTOS preemptive scheduling\n");
#else
    DEBUG_PRINT("[SCHEDULER] Using cooperative multitasking\n");
#endif
}

// mostly just documentation here...
#ifdef PLATFORM_ESP32
// ESP32: FreeRTOS handles preemptive scheduling automatically
// the custom scheduler is redundant, FreeRTOS manages all scheduling via interrupts.
// these functions are thin wrappers for API compatibility.

void scheduler_run(void) {
    // no-op on ESP32 - FreeRTOS handles all scheduling automatically via tick interrupt
    // this function is kept for API compatibility with PC builds
    // FreeRTOS scheduler runs independently and preempts tasks based on priority
}

void scheduler_tick(void) {
    // no-op on ESP32 - FreeRTOS handles scheduling via tick interrupt
    // this is kept for API compatibility
    // note: FreeRTOS tick interrupt runs at configTICK_RATE_HZ (typically 1000Hz)
}

process_id_t scheduler_get_current(void) {
    // get PCB directly from task parameter (O(1) instead of O(n))
    // each FreeRTOS task has the PCB pointer as its parameter
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    if (current_task != NULL) {
        // get task parameter (which is the PCB pointer)
        process_control_block_t *pcb = (process_control_block_t *)pvTaskGetThreadLocalStoragePointer(
            current_task, 0);
        
        // fallback: if TLS not set, use parameter (pvParameters in xTaskCreate)
        if (pcb == NULL) {
            // mote: FreeRTOS doesn't provide direct access to pvParameters,
            // so we fall back to iteration (but this should rarely happen)
            pcb = process_iterate(NULL);
            while (pcb != NULL) {
                if (pcb->active && pcb->task_handle == current_task) {
                    return pcb->pid;
                }
                pcb = process_iterate(pcb);
            }
            return 0;
        }
        
        // verify PCB is still valid and matches current task
        if (pcb->active && pcb->task_handle == current_task) {
            return pcb->pid;
        }
    }
    return 0;
}

void process_yield(void) {
    // yield to FreeRTOS scheduler
    // NOTE: platform behavior difference:
    //   - PC: guarantees another task runs (cooperative)
    //   - ESP32: only yields if higher-priority task is ready (preemptive)
    //   on ESP32, if all other tasks are lower priority, this task continues immediately.
    //   this is correct preemptive behavior but differs from PC cooperative semantics.
    taskYIELD();
}

#else
// PC: Cooperative multitasking (original implementation)

// simple round-robin scheduler with priority
// higher priority processes get scheduled first
static process_id_t find_next_ready_process(void) {
    process_control_block_t *highest_priority = NULL;
    process_id_t highest_pid = 0;
    
    // iterate through all processes
    process_control_block_t *pcb = process_iterate(NULL);
    while (pcb != NULL) {
        if (pcb->state == process_state_ready) {
            if (highest_priority == NULL || 
                pcb->priority > highest_priority->priority) {
                highest_priority = pcb;
                highest_pid = pcb->pid;
            }
        }
        pcb = process_iterate(pcb);
    }
    
    return highest_pid;
}

void scheduler_run(void) {
    // if current process is still running and within quantum, continue
    if (current_pid != 0) {
        process_control_block_t *current_pcb = process_get_pcb(current_pid);
        if (current_pcb != NULL && current_pcb->state == process_state_running) {
            uint32_t elapsed = get_time_ms() - last_schedule_time;
            if (elapsed < schedule_quantum_ms) {
                return;  // still within time slice
            }
            // time slice expired, yield
            process_set_state(current_pid, process_state_ready);
        }
    }
    
    // find next ready process
    process_id_t next_pid = find_next_ready_process();
    
    if (next_pid == 0) {
        // no ready processes
        current_pid = 0;
        return;
    }
    
    // switch to next process
    if (current_pid != next_pid) {
        process_set_state(next_pid, process_state_running);
        current_pid = next_pid;
        last_schedule_time = get_time_ms();
        
        process_control_block_t *pcb = process_get_pcb(next_pid);
        if (pcb != NULL && pcb->entry_point != NULL) {
            DEBUG_PRINT("[SCHEDULER] Running: PID=%d, name=%s\n", 
                       next_pid, pcb->name);
            // execute process entry point
            pcb->entry_point(pcb->args);
        }
    }
}

void scheduler_tick(void) {
    scheduler_run();
}

process_id_t scheduler_get_current(void) {
    return current_pid;
}

void process_yield(void) {
    if (current_pid != 0) {
        process_set_state(current_pid, process_state_ready);
        current_pid = 0;
    }
    // don't immediately re-schedule, let the process remain ready
    // until the next scheduler cycle (called from main loop or timer)
}

#endif

