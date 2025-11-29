#include "process.h"
#include "process_scheduler.h"
#include "action_manager.h"
#include "debug_helper.h"

// example process entry point
static void example_process_task(void *args) {
    const char *task_name = (const char *)args;
    (void)task_name;  // suppress unused warning for now
    DEBUG_PRINT("[PROCESS_TASK] Running task: %s\n", task_name);
    // process would do work here
    // for now, just show it works, holy shit this is so much to keep track of, i should add to docs, or even, just flesh out one thing... hmm
    process_yield();  // yield control back to scheduler
}

// action handler: create a new process
static void action_spawn_process(void) {
    process_id_t pid = process_create("example_task", example_process_task, 
                                      (void *)"example_task", 
                                      process_priority_normal);
    if (pid != 0) {
        DEBUG_PRINT("[ACTION] Spawned process with PID=%d\n", pid);
    } else {
        DEBUG_PRINT("[ACTION] Failed to spawn process\n");
    }
}

// action handler: show process list
static void action_list_processes(void) {
    uint8_t count = get_process_count();
    DEBUG_PRINT("[ACTION] Active processes: %d\n", count);
    (void)count;  // used in DEBUG_PRINT, but compiler doesn't see it
    
    process_control_block_t *pcb = process_iterate(NULL);
    while (pcb != NULL) {
        DEBUG_PRINT("[PROCESS] PID=%d, name=%s, state=%d, priority=%d\n",
                   pcb->pid, pcb->name, pcb->state, pcb->priority);
        pcb = process_iterate(pcb);
    }
}

// register process-related actions
void register_process_actions(void) {
    register_action("spawn_process", action_spawn_process);
    register_action("list_processes", action_list_processes);
}

