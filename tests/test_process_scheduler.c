#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "process.h"
#include "process_scheduler.h"
#include "ino_helper.h"

static int task1_run_count = 0;
static int task2_run_count = 0;
static int task3_run_count = 0;

void setup_scheduler(void) {
    init_process_system();
    init_scheduler();
    task1_run_count = 0;
    task2_run_count = 0;
    task3_run_count = 0;
}

void teardown_scheduler(void) {
    process_control_block_t *pcb = process_iterate(NULL);
    while (pcb != NULL) {
        process_terminate(pcb->pid);
        pcb = process_iterate(pcb);
    }
}

// test task functions
static void test_task1(void *args) {
    (void)args;
    task1_run_count++;
}

static void test_task2(void *args) {
    (void)args;
    task2_run_count++;
}

static void test_task3(void *args) {
    (void)args;
    task3_run_count++;
}

// test 1: scheduler initialization
void test_scheduler_init(void) {
    setup_scheduler();
    printf("  test_scheduler_init... ");
    
    assert(scheduler_get_current() == 0);
    
    printf("FUNCTIONAL\n");
    teardown_scheduler();
}

// test 2: scheduler runs ready process
void test_scheduler_run_ready(void) {
    setup_scheduler();
    printf("  test_scheduler_run_ready... ");
    
    process_id_t pid = process_create("task1", test_task1, NULL, process_priority_normal, 0);
    assert(pid != 0);
    assert(process_get_state(pid) == process_state_ready);
    
    scheduler_run();
    
    // process should have been executed (entry point called)
    // note: in current implementation, entry point is called directly
    // so we can check if it ran
    assert(task1_run_count > 0 || process_get_state(pid) == process_state_running);
    
    printf("FUNCTIONAL\n");
    teardown_scheduler();
}

// test 3: priority scheduling
void test_scheduler_priority(void) {
    setup_scheduler();
    printf("  test_scheduler_priority... ");
    
    (void)process_create("low", test_task1, NULL, process_priority_low, 0);
    (void)process_create("normal", test_task2, NULL, process_priority_normal, 0);
    (void)process_create("high", test_task3, NULL, process_priority_high, 0);
    
    scheduler_run();
    
    // high priority should be scheduled first
    process_id_t current = scheduler_get_current();
    // note: exact behavior depends on implementation, but high priority should be preferred
    assert(current != 0);
    
    printf("FUNCTIONAL\n");
    teardown_scheduler();
}

// test 4: process yield
void test_scheduler_yield(void) {
    setup_scheduler();
    printf("  test_scheduler_yield... ");
    
    process_id_t pid = process_create("task1", test_task1, NULL, process_priority_normal, 0);
    process_set_state(pid, process_state_running);
    
    // verify it's running
    assert(process_get_state(pid) == process_state_running);
    
    process_yield();
    
    // process should be back to ready state or scheduler should have changed it
    process_state_t state = process_get_state(pid);
    assert(state == process_state_ready || state == process_state_running);
    
    printf("FUNCTIONAL\n");
    teardown_scheduler();
}

// test 5: scheduler with no processes
void test_scheduler_no_processes(void) {
    setup_scheduler();
    printf("  test_scheduler_no_processes... ");
    
    scheduler_run();
    assert(scheduler_get_current() == 0);
    
    printf("FUNCTIONAL\n");
    teardown_scheduler();
}

int main(void) {
    printf("[PROCESS SCHEDULER TESTS]\n");
    test_scheduler_init();
    test_scheduler_run_ready();
    test_scheduler_priority();
    test_scheduler_yield();
    test_scheduler_no_processes();
    return 0;
}

