#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// process states
typedef enum __attribute__((packed)) {
    process_state_ready,
    process_state_running,
    process_state_waiting,
    process_state_blocked,
    process_state_terminated
} process_state_t;

// process priority levels
typedef enum __attribute__((packed)) {
    process_priority_low = 0,
    process_priority_normal = 1,
    process_priority_high = 2
} process_priority_t;

// process ID type
typedef uint8_t process_id_t;

// process control block (PCB)
typedef struct __attribute__((packed)) {
    process_id_t pid;                    // process identifier
    process_state_t state;               // current state
    process_priority_t priority;         // priority level
    const char *name;                    // process name
    void (*entry_point)(void *args);     // entry point function
    void *args;                          // arguments for entry point
    uint32_t runtime;                    // runtime in ticks/ms
    uint8_t active;                      // is this PCB slot in use
} process_control_block_t;

// process creation and management
process_id_t process_create(const char *name, void (*entry_point)(void *args), 
                            void *args, process_priority_t priority);
void process_terminate(process_id_t pid);
void process_set_state(process_id_t pid, process_state_t state);
process_state_t process_get_state(process_id_t pid);
process_control_block_t *process_get_pcb(process_id_t pid);

// process registry functions
void init_process_system(void);
uint8_t get_process_count(void);

// iterator for scheduler (returns next active PCB, NULL when done)
// pass NULL to start iteration
process_control_block_t *process_iterate(process_control_block_t *prev);

// register process-related actions with action manager
void register_process_actions(void);

#ifdef __cplusplus
}
#endif

