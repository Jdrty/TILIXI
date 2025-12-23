#include "process.h"
#include "debug_helper.h"
#include <string.h>

// process registry - following the same pattern as actions/commands
#define max_processes 16
static process_control_block_t process_table[max_processes];
static process_id_t next_pid = 1;  // start at 1, 0 is invalid
static uint8_t process_count = 0;

#ifdef PLATFORM_ESP32
    #include <FreeRTOS.h>
    #include <task.h>
    #include <semphr.h>
    
    // mutex for thread-safe PCB access (ESP32 dual-core safe)
    static SemaphoreHandle_t process_table_mutex = NULL;
    
    // default stack size (configurable per process)
    #define process_task_stack_size_default 2048  // reduced from 4096 to save RAM
    #define process_task_priority_base 5  // base priority for normal priority processes
    
    // helper macros for critical sections
    #define ENTER_CRITICAL() if (process_table_mutex != NULL) xSemaphoreTake(process_table_mutex, portMAX_DELAY)
    #define EXIT_CRITICAL() if (process_table_mutex != NULL) xSemaphoreGive(process_table_mutex)
    
    // FreeRTOS task wrapper, this is the actual task entry point
    static void process_task_wrapper(void *pv_parameters) {
        process_control_block_t *pcb = (process_control_block_t *)pv_parameters;
        
        if (pcb == NULL || pcb->entry_point == NULL) {
            return;
        }
        
        // set state to running (protected access)
        ENTER_CRITICAL();
        pcb->state = process_state_running;
        EXIT_CRITICAL();
        
        DEBUG_PRINT("[PROCESS] Task started: PID=%d, name=%s\n", pcb->pid, pcb->name);
        
        // call the actual process entry point
        pcb->entry_point(pcb->args);
        
        // entry point returned, mark as terminated (protected access)
        ENTER_CRITICAL();
        pcb->state = process_state_terminated;
        pcb->active = 0;
        pcb->task_handle = NULL;  // clear handle before deletion
        process_count--;
        EXIT_CRITICAL();
        
        DEBUG_PRINT("[PROCESS] Task finished: PID=%d, name=%s\n", pcb->pid, pcb->name);
        
        // delete this task (self-terminate)
        vTaskDelete(NULL);
    }
#else
    // PC builds don't need mutex (single-threaded cooperative)
    #define ENTER_CRITICAL() ((void)0)
    #define EXIT_CRITICAL() ((void)0)
#endif

void init_process_system(void) {
#ifdef PLATFORM_ESP32
    // create mutex for thread-safe access
    if (process_table_mutex == NULL) {
        process_table_mutex = xSemaphoreCreateMutex();
        if (process_table_mutex == NULL) {
            DEBUG_PRINT("[PROCESS] ERROR: Failed to create mutex\n");
        }
    }
#endif
    
    // initialize all PCBs to inactive state
    for (uint8_t i = 0; i < max_processes; i++) {
        process_table[i].pid            = 0;
        process_table[i].state          = process_state_terminated;
        process_table[i].priority       = process_priority_normal;
        process_table[i].name           = NULL;
        process_table[i].entry_point    = NULL;
        process_table[i].args           = NULL;
        process_table[i].runtime        = 0;
        process_table[i].active         = 0;
#ifdef PLATFORM_ESP32
        process_table[i].task_handle    = NULL;
#endif
    }
    process_count = 0;
    next_pid = 1;
    DEBUG_PRINT("[PROCESS] Process system initialized\n");
}

process_id_t process_create(const char *name, void (*entry_point)(void *args), 
                            void *args, process_priority_t priority,
                            uint16_t stack_size_words) {
    (void)stack_size_words;  // suppress unused warning on PC builds
    ENTER_CRITICAL();
    
    if (process_count >= max_processes) {
        EXIT_CRITICAL();
        DEBUG_PRINT("[PROCESS] Max process count reached\n");
        return 0;  // invalid PID
    }

    // find free slot
    uint8_t slot_idx = max_processes;
    for (uint8_t i = 0; i < max_processes; i++) {
        if (!process_table[i].active) {
            slot_idx = i;
            break;
        }
    }
    
    if (slot_idx >= max_processes) {
        EXIT_CRITICAL();
        return 0;  // no free slot found
    }
    
    // initialize PCB
    process_table[slot_idx].pid            = next_pid++;
    process_table[slot_idx].state          = process_state_ready;
    process_table[slot_idx].priority       = priority;
    process_table[slot_idx].name           = name;
    process_table[slot_idx].entry_point    = entry_point;
    process_table[slot_idx].args           = args;
    process_table[slot_idx].runtime        = 0;
    process_table[slot_idx].active         = 1;
    
#ifdef PLATFORM_ESP32
    process_table[slot_idx].task_handle    = NULL;
    
    // determine stack size (use default if invalid)
    uint16_t stack_size = stack_size_words;
    if (stack_size < 512) {
        stack_size = process_task_stack_size_default;
    }
    
    // map process priority to FreeRTOS priority
    UBaseType_t freertos_priority = process_task_priority_base + (UBaseType_t)priority;
    
            // create FreeRTOS task for this process
            // Note: xTaskCreate is called outside critical section to avoid blocking
            EXIT_CRITICAL();
            
            BaseType_t result = xTaskCreate(
                process_task_wrapper,              // task function
                name,                              // task name
                stack_size,                        // stack size in words
                &process_table[slot_idx],          // parameters (PCB pointer)
                freertos_priority,                 // priority
                &process_table[slot_idx].task_handle  // task handle
            );
            
            // Store PCB pointer in task's thread-local storage for O(1) lookup
            if (result == pdPASS && process_table[slot_idx].task_handle != NULL) {
                vTaskSetThreadLocalStoragePointer(process_table[slot_idx].task_handle, 
                                                   0, (void *)&process_table[slot_idx]);
            }
    
    ENTER_CRITICAL();
    
    if (result != pdPASS) {
        DEBUG_PRINT("[PROCESS] Failed to create FreeRTOS task for PID=%d\n", 
                   process_table[slot_idx].pid);
        process_table[slot_idx].active = 0;
        process_table[slot_idx].pid = 0;
        EXIT_CRITICAL();
        return 0;
    }
    
    process_count++;
    process_id_t new_pid = process_table[slot_idx].pid;
    EXIT_CRITICAL();
    
    DEBUG_PRINT("[PROCESS] Created with FreeRTOS task: PID=%d, name=%s, priority=%d, stack=%d\n", 
               new_pid, name, priority, stack_size);
    return new_pid;
#else
    // PC build: cooperative multitasking (no FreeRTOS)
    process_count++;
    process_id_t new_pid = process_table[slot_idx].pid;
    EXIT_CRITICAL();
    
    DEBUG_PRINT("[PROCESS] Created: PID=%d, name=%s, priority=%d\n", 
               new_pid, name, priority);
    return new_pid;
#endif
}

void process_terminate(process_id_t pid) {
    ENTER_CRITICAL();
    
    process_control_block_t *pcb = NULL;
#ifdef PLATFORM_ESP32
    TaskHandle_t task_to_delete = NULL;
#endif
    
    for (uint8_t i = 0; i < max_processes; i++) {
        if (process_table[i].active && process_table[i].pid == pid) {
            pcb = &process_table[i];
#ifdef PLATFORM_ESP32
            task_to_delete = process_table[i].task_handle;
            process_table[i].task_handle = NULL;  // clear before deletion
#endif
            process_table[i].state = process_state_terminated;
            process_table[i].active = 0;
            process_table[i].entry_point = NULL;
            process_table[i].args = NULL;
            process_count--;
            break;
        }
    }
    
    EXIT_CRITICAL();
    
    if (pcb != NULL) {
#ifdef PLATFORM_ESP32
        // delete FreeRTOS task outside critical section (may block)
        if (task_to_delete != NULL) {
            vTaskDelete(task_to_delete);
        }
#endif
        DEBUG_PRINT("[PROCESS] Terminated: PID=%d, name=%s\n", pid, pcb->name);
    } else {
        DEBUG_PRINT("[PROCESS] Terminate failed: PID=%d not found\n", pid);
    }
}

void process_set_state(process_id_t pid, process_state_t state) {
    ENTER_CRITICAL();
    for (uint8_t i = 0; i < max_processes; i++) {
        if (process_table[i].active && process_table[i].pid == pid) {
            process_table[i].state = state;
            EXIT_CRITICAL();
            DEBUG_PRINT("[PROCESS] State change: PID=%d -> state=%d\n", pid, state);
            return;
        }
    }
    EXIT_CRITICAL();
}

process_state_t process_get_state(process_id_t pid) {
    ENTER_CRITICAL();
    for (uint8_t i = 0; i < max_processes; i++) {
        if (process_table[i].active && process_table[i].pid == pid) {
            process_state_t state = process_table[i].state;
            EXIT_CRITICAL();
            return state;
        }
    }
    EXIT_CRITICAL();
    return process_state_terminated;
}

process_control_block_t *process_get_pcb(process_id_t pid) {
    ENTER_CRITICAL();
    for (uint8_t i = 0; i < max_processes; i++) {
        if (process_table[i].active && process_table[i].pid == pid) {
            process_control_block_t *pcb = &process_table[i];
            EXIT_CRITICAL();
            return pcb;
        }
    }
    EXIT_CRITICAL();
    return NULL;
}

uint8_t get_process_count(void) {
    ENTER_CRITICAL();
    uint8_t count = process_count;
    EXIT_CRITICAL();
    return count;
}

process_control_block_t *process_iterate(process_control_block_t *prev) {
    ENTER_CRITICAL();
    
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
            process_control_block_t *pcb = &process_table[i];
            EXIT_CRITICAL();
            return pcb;
        }
    }
    
    EXIT_CRITICAL();
    return NULL;  // no more processes
}


