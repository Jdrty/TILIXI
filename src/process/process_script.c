#include "process_script.h"
#include "process.h"
#include "process_scheduler.h"
#include "debug_helper.h"
#include <string.h>
#include <stdlib.h>

// script handler registry (following the same pattern as actions/commands)
#define max_script_handlers 16
typedef struct {
    const char *name;
    script_handler_t handler;
} script_handler_entry_t;

static script_handler_entry_t script_handlers[max_script_handlers];
static uint8_t script_handler_count = 0;

void init_script_system(void) {
    script_handler_count = 0;
    for (uint8_t i = 0; i < max_script_handlers; i++) {
        script_handlers[i].name = NULL;
        script_handlers[i].handler = NULL;
    }
    DEBUG_PRINT("[SCRIPT] Script system initialized\n");
}

void register_script_handler(const char *name, script_handler_t handler) {
    if (script_handler_count >= max_script_handlers) {
        DEBUG_PRINT("[SCRIPT] Max script handler count reached\n");
        return;
    }
    
    script_handlers[script_handler_count].name = name;
    script_handlers[script_handler_count].handler = handler;
    script_handler_count++;
    DEBUG_PRINT("[SCRIPT] Registered handler: %s\n", name);
}

script_handler_t find_script_handler(const char *name) {
    for (uint8_t i = 0; i < script_handler_count; i++) {
        if (script_handlers[i].name != NULL && 
            strcmp(script_handlers[i].name, name) == 0) {
            return script_handlers[i].handler;
        }
    }
    return NULL;
}

// simple script entry point wrapper
static void script_entry_wrapper(void *args) {
    script_context_t *ctx = (script_context_t *)args;
    if (ctx == NULL || ctx->commands == NULL) {
        return;
    }
    
    // execute first command (for now, simple execution)
    // in a full implementation, this would handle pipelining
    if (ctx->command_count > 0) {
        script_command_t *cmd = &ctx->commands[0];
        (void)cmd;  // suppress unused warning for now
        DEBUG_PRINT("[SCRIPT] Executing command: %s\n", cmd->command);
        // command execution would be handled by terminal_cmd system
        // or by registered script handlers
    }
}

process_id_t execute_script(const char *script_name, char **args, uint8_t arg_count) {
    script_handler_t handler = find_script_handler(script_name);
    
    if (handler == NULL) {
        DEBUG_PRINT("[SCRIPT] Unknown script: %s\n", script_name);
        return 0;
    }
    
    // create script context
    script_context_t *ctx = (script_context_t *)malloc(sizeof(script_context_t));
    if (ctx == NULL) {
        DEBUG_PRINT("[SCRIPT] Failed to allocate script context\n");
        return 0;
    }
    
    // allocate command array
    ctx->commands = (script_command_t *)malloc(sizeof(script_command_t));
    if (ctx->commands == NULL) {
        free(ctx);
        return 0;
    }
    
    ctx->command_count = 1;
    ctx->commands[0].command = script_name;
    ctx->commands[0].args = args;
    ctx->commands[0].arg_count = arg_count;
    ctx->commands[0].input_fd = -1;   // stdin
    ctx->commands[0].output_fd = -1;  // stdout
    ctx->process_ids = NULL;
    
    // create process for script execution
    process_id_t pid = process_create(script_name, script_entry_wrapper, 
                                      ctx, process_priority_normal);
    
    if (pid == 0) {
        free(ctx->commands);
        free(ctx);
    }
    
    return pid;
}

process_id_t execute_pipeline(script_command_t *commands, uint8_t command_count) {
    if (commands == NULL || command_count == 0) {
        return 0;
    }
    
    // create script context for pipeline
    script_context_t *ctx = (script_context_t *)malloc(sizeof(script_context_t));
    if (ctx == NULL) {
        return 0;
    }
    
    ctx->commands = commands;
    ctx->command_count = command_count;
    ctx->process_ids = (process_id_t *)malloc(sizeof(process_id_t) * command_count);
    
    if (ctx->process_ids == NULL) {
        free(ctx);
        return 0;
    }
    
    // create processes for each command in pipeline
    // for now, simple sequential execution
    // full pipelining would require pipe setup between processes
    for (uint8_t i = 0; i < command_count; i++) {
        ctx->process_ids[i] = process_create(
            commands[i].command,
            script_entry_wrapper,
            ctx,
            process_priority_normal
        );
    }
    
    DEBUG_PRINT("[SCRIPT] Pipeline created with %d commands\n", command_count);
    return ctx->process_ids[0];  // return first PID
}

