#include "process.h"
#include "debug_helper.h"
#include <string.h>

// process registry - following the same pattern as actions/commands
#define max_processes 16
static process_control_block_t process_table[max_processes];
static process_id_t next_pid = 1;  // start at 1, 0 is invalid
static uint8_t process_count = 0;

void init_process_system(void) {
    // initialize all PCBs to inactive state
    for (uint8_t i = 0; i < max_processes; i++) {
        process_table[i].pid = 0;
        process_table[i].state = process_state_terminated;
        process_table[i].priority = process_priority_normal;
        process_table[i].name = NULL;
        process_table[i].entry_point = NULL;
        process_table[i].args = NULL;
        process_table[i].runtime = 0;
        process_table[i].active = 0;
    }
    process_count = 0;
    next_pid = 1;
    DEBUG_PRINT("[PROCESS] Process system initialized\n");
}

process_id_t process_create(const char *name, void (*entry_point)(void *args), 
                            void *args, process_priority_t priority) {
    if (process_count >= max_processes) {
        DEBUG_PRINT("[PROCESS] Max process count reached\n");
        return 0;  // invalid PID
    }

    // find free slot
    for (uint8_t i = 0; i < max_processes; i++) {
        if (!process_table[i].active) {
            process_table[i].pid = next_pid++;
            process_table[i].state = process_state_ready;
            process_table[i].priority = priority;
            process_table[i].name = name;
            process_table[i].entry_point = entry_point;
            process_table[i].args = args;
            process_table[i].runtime = 0;
            process_table[i].active = 1;
            process_count++;
            
            DEBUG_PRINT("[PROCESS] Created: PID=%d, name=%s, priority=%d\n", 
                       process_table[i].pid, name, priority);
            return process_table[i].pid;
        }
    }
    
    return 0;  // no free slot found
}

void process_terminate(process_id_t pid) {
    for (uint8_t i = 0; i < max_processes; i++) {
        if (process_table[i].active && process_table[i].pid == pid) {
            process_table[i].state = process_state_terminated;
            process_table[i].active = 0;
            process_table[i].entry_point = NULL;
            process_table[i].args = NULL;
            process_count--;
            DEBUG_PRINT("[PROCESS] Terminated: PID=%d, name=%s\n", 
                       pid, process_table[i].name);
            return;
        }
    }
    DEBUG_PRINT("[PROCESS] Terminate failed: PID=%d not found\n", pid);
}

void process_set_state(process_id_t pid, process_state_t state) {
    for (uint8_t i = 0; i < max_processes; i++) {
        if (process_table[i].active && process_table[i].pid == pid) {
            process_table[i].state = state;
            DEBUG_PRINT("[PROCESS] State change: PID=%d -> state=%d\n", pid, state);
            return;
        }
    }
}

process_state_t process_get_state(process_id_t pid) {
    for (uint8_t i = 0; i < max_processes; i++) {
        if (process_table[i].active && process_table[i].pid == pid) {
            return process_table[i].state;
        }
    }
    return process_state_terminated;
}

process_control_block_t *process_get_pcb(process_id_t pid) {
    for (uint8_t i = 0; i < max_processes; i++) {
        if (process_table[i].active && process_table[i].pid == pid) {
            return &process_table[i];
        }
    }
    return NULL;
}

uint8_t get_process_count(void) {
    return process_count;
}

process_control_block_t *process_iterate(process_control_block_t *prev) {
    uint8_t start_idx = 0;
    
    if (prev != NULL) {
        // find index of previous PCB
        for (uint8_t i = 0; i < max_processes; i++) {
            if (&process_table[i] == prev) {
                start_idx = i + 1;
                break;
            }
        }
    }
    
    // find next active PCB
    for (uint8_t i = start_idx; i < max_processes; i++) {
        if (process_table[i].active) {
            return &process_table[i];
        }
    }
    
    return NULL;  // no more processes
}

