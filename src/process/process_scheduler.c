#include "process_scheduler.h"
#include "process.h"
#include "debug_helper.h"
#include "ino_helper.h"

static process_id_t current_pid = 0;  // 0 means no process running
static uint32_t last_schedule_time = 0;
#define schedule_quantum_ms 10  // time slice per process

void init_scheduler(void) {
    current_pid = 0;
    last_schedule_time = get_time_ms();
    DEBUG_PRINT("[SCHEDULER] Scheduler initialized\n");
}

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
    scheduler_run();
}

