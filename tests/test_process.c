#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "process.h"

void setup_process(void) {
    init_process_system();
}

void teardown_process(void) {
    // cleanup - terminate all processes
    process_control_block_t *pcb = process_iterate(NULL);
    while (pcb != NULL) {
        process_terminate(pcb->pid);
        pcb = process_iterate(pcb);
    }
}

// test 1: process creation
void test_process_create(void) {
    setup_process();
    printf("  test_process_create... ");
    
    void test_task(void *args) {
        (void)args;
    }
    
    process_id_t pid = process_create("test_task", test_task, NULL, process_priority_normal);
    assert(pid != 0);
    assert(get_process_count() == 1);
    
    process_control_block_t *pcb = process_get_pcb(pid);
    assert(pcb != NULL);
    assert(pcb->state == process_state_ready);
    assert(strcmp(pcb->name, "test_task") == 0);
    assert(pcb->priority == process_priority_normal);
    
    printf("FUNCTIONAL\n");
    teardown_process();
}

// test 2: process termination
void test_process_terminate(void) {
    setup_process();
    printf("  test_process_terminate... ");
    
    void test_task(void *args) {
        (void)args;
    }
    
    process_id_t pid = process_create("test_task", test_task, NULL, process_priority_normal);
    assert(get_process_count() == 1);
    
    process_terminate(pid);
    assert(get_process_count() == 0);
    
    process_control_block_t *pcb = process_get_pcb(pid);
    assert(pcb == NULL);
    
    printf("FUNCTIONAL\n");
    teardown_process();
}

// test 3: process state management
void test_process_state(void) {
    setup_process();
    printf("  test_process_state... ");
    
    void test_task(void *args) {
        (void)args;
    }
    
    process_id_t pid = process_create("test_task", test_task, NULL, process_priority_normal);
    assert(process_get_state(pid) == process_state_ready);
    
    process_set_state(pid, process_state_running);
    assert(process_get_state(pid) == process_state_running);
    
    process_set_state(pid, process_state_waiting);
    assert(process_get_state(pid) == process_state_waiting);
    
    printf("FUNCTIONAL\n");
    teardown_process();
}

// test 4: multiple processes
void test_process_multiple(void) {
    setup_process();
    printf("  test_process_multiple... ");
    
    void test_task(void *args) {
        (void)args;
    }
    
    process_id_t pid1 = process_create("task1", test_task, NULL, process_priority_low);
    process_id_t pid2 = process_create("task2", test_task, NULL, process_priority_normal);
    process_id_t pid3 = process_create("task3", test_task, NULL, process_priority_high);
    
    assert(get_process_count() == 3);
    assert(pid1 != pid2 && pid2 != pid3 && pid1 != pid3);
    
    process_control_block_t *pcb1 = process_get_pcb(pid1);
    process_control_block_t *pcb2 = process_get_pcb(pid2);
    process_control_block_t *pcb3 = process_get_pcb(pid3);
    
    assert(pcb1 != NULL && pcb2 != NULL && pcb3 != NULL);
    assert(pcb1->priority == process_priority_low);
    assert(pcb2->priority == process_priority_normal);
    assert(pcb3->priority == process_priority_high);
    
    printf("FUNCTIONAL\n");
    teardown_process();
}

// test 5: process iteration
void test_process_iterate(void) {
    setup_process();
    printf("  test_process_iterate... ");
    
    void test_task(void *args) {
        (void)args;
    }
    
    process_create("task1", test_task, NULL, process_priority_normal);
    process_create("task2", test_task, NULL, process_priority_normal);
    process_create("task3", test_task, NULL, process_priority_normal);
    
    uint8_t count = 0;
    process_control_block_t *pcb = process_iterate(NULL);
    while (pcb != NULL) {
        count++;
        pcb = process_iterate(pcb);
    }
    
    assert(count == 3);
    
    printf("FUNCTIONAL\n");
    teardown_process();
}

// test 6: max processes limit
void test_process_max_limit(void) {
    setup_process();
    printf("  test_process_max_limit... ");
    
    void test_task(void *args) {
        (void)args;
    }
    
    // create max processes
    for (int i = 0; i < 16; i++) {
        char name[16];
        snprintf(name, sizeof(name), "task%d", i);
        process_id_t pid = process_create(name, test_task, NULL, process_priority_normal);
        assert(pid != 0);
    }
    
    assert(get_process_count() == 16);
    
    // try to create one more - should fail
    process_id_t pid = process_create("overflow", test_task, NULL, process_priority_normal);
    assert(pid == 0);
    assert(get_process_count() == 16);
    
    printf("FUNCTIONAL\n");
    teardown_process();
}

int main(void) {
    printf("[PROCESS TESTS]\n");
    test_process_create();
    test_process_terminate();
    test_process_state();
    test_process_multiple();
    test_process_iterate();
    test_process_max_limit();
    return 0;
}

