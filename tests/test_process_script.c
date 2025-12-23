#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "process.h"
#include "process_script.h"

void setup_process_script(void) {
    init_process_system();
    init_script_system();
}

void teardown_process_script(void) {
    process_control_block_t *pcb = process_iterate(NULL);
    while (pcb != NULL) {
        process_terminate(pcb->pid);
        pcb = process_iterate(pcb);
    }
}

// test 1: script system initialization
void test_script_init(void) {
    setup_process_script();
    printf("  test_script_init... ");
    
    // system should be initialized
    script_handler_t handler = find_script_handler("test");
    assert(handler == NULL);  // no handlers registered yet
    
    printf("FUNCTIONAL\n");
    teardown_process_script();
}

// test 2: script handler registration
void test_script_register(void) {
    setup_process_script();
    printf("  test_script_register... ");
    
    void test_handler(script_context_t *ctx) {
        (void)ctx;
    }
    
    register_script_handler("test_script", test_handler);
    
    script_handler_t handler = find_script_handler("test_script");
    assert(handler != NULL);
    assert(handler == test_handler);
    
    printf("FUNCTIONAL\n");
    teardown_process_script();
}

// test 3: script execution (with registered handler)
void test_script_execute(void) {
    setup_process_script();
    printf("  test_script_execute... ");
    
    void test_handler(script_context_t *ctx) {
        (void)ctx;
    }
    
    register_script_handler("test_script", test_handler);
    
    char *args[] = {"arg1", "arg2", NULL};
    process_id_t pid = execute_script("test_script", args, 2);
    
    // script should create a process with the script name
    assert(pid != 0);
    process_control_block_t *pcb = process_get_pcb(pid);
    assert(pcb != NULL);
    assert(strcmp(pcb->name, "test_script") == 0);
    assert(pcb->state == process_state_ready || pcb->state == process_state_running);
    
    printf("FUNCTIONAL\n");
    teardown_process_script();
}

// test 4: script execution (unknown script)
void test_script_execute_unknown(void) {
    setup_process_script();
    printf("  test_script_execute_unknown... ");
    
    char *args[] = {NULL};
    process_id_t pid = execute_script("unknown_script", args, 0);
    
    assert(pid == 0);  // should fail
    
    printf("FUNCTIONAL\n");
    teardown_process_script();
}

// test 5: pipeline execution
void test_script_pipeline(void) {
    setup_process_script();
    printf("  test_script_pipeline... ");
    
    script_command_t commands[2];
    
    commands[0].command = "cmd1";
    commands[0].args = NULL;
    commands[0].arg_count = 0;
    commands[0].input_fd = -1;
    commands[0].output_fd = -1;
    
    commands[1].command = "cmd2";
    commands[1].args = NULL;
    commands[1].arg_count = 0;
    commands[1].input_fd = -1;
    commands[1].output_fd = -1;
    
    uint8_t initial_count = get_process_count();
    process_id_t pid = execute_pipeline(commands, 2);
    
    // pipeline should create one process per command (at least)
    assert(pid != 0);
    assert(get_process_count() >= initial_count + 2);
    
    printf("FUNCTIONAL\n");
    teardown_process_script();
}

int main(void) {
    printf("[PROCESS SCRIPT TESTS]\n");
    test_script_init();
    test_script_register();
    test_script_execute();
    test_script_execute_unknown();
    test_script_pipeline();
    return 0;
}

