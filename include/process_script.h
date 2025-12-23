#pragma once
#include "process.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// script command structure (for pipelining)
typedef struct {
    const char *command;      // command name
    char **args;              // command arguments
    uint8_t arg_count;        // number of arguments
    int input_fd;             // input file descriptor (for pipes)
    int output_fd;            // output file descriptor (for pipes)
} script_command_t;

// script execution context
typedef struct {
    script_command_t *commands;  // array of commands in pipeline
    uint8_t command_count;       // number of commands
    process_id_t *process_ids;    // PIDs of spawned processes
    uint8_t ref_count;            // reference count for shared contexts (for execute_pipeline)
} script_context_t;

// script handler function type
typedef void (*script_handler_t)(script_context_t *ctx);

// script registry (following action/command pattern)
void register_script_handler(const char *name, script_handler_t handler);
script_handler_t find_script_handler(const char *name);

// script execution
process_id_t execute_script(const char *script_name, char **args, uint8_t arg_count);
process_id_t execute_pipeline(script_command_t *commands, uint8_t command_count);

// initialize script system
void init_script_system(void);

// cleanup script context (frees allocated memory)
// should be called when a script process terminates
void cleanup_script_context(void *args);

// script entry point wrapper (used to identify script processes)
void script_entry_wrapper(void *args);

#ifdef __cplusplus
}
#endif

