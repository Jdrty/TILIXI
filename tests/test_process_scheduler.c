/**
 * NOTE: these tests rely on the cooperative scheduler semantics used in
 * the PC build (where scheduler_run() synchronously invokes process
 * entry points). on ESP32, FreeRTOS provides a fully preemptive scheduler
 * and scheduler_run() is a no-op, so these tests would be invalid. ¯\_(ツ)_/¯
 */

#ifdef PLATFORM_ESP32

#include <stdio.h>

int main(void) {
    printf("[PROCESS SCHEDULER TESTS] Skipped on ESP32 (preemptive FreeRTOS scheduler)\n");
    return 0;
}

#else

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
    
    // process should have been executed exactly once and be marked running
    assert(task1_run_count == 1);
    assert(process_get_state(pid) == process_state_running);
    assert(scheduler_get_current() == pid);
    
    printf("FUNCTIONAL\n");
    teardown_scheduler();
}

// test 3: priority scheduling
void test_scheduler_priority(void) {
    setup_scheduler();
    printf("  test_scheduler_priority... ");
    
    process_id_t pid_low    = process_create("low",    test_task1, NULL, process_priority_low,    0);
    process_id_t pid_normal = process_create("normal", test_task2, NULL, process_priority_normal, 0);
    process_id_t pid_high   = process_create("high",   test_task3, NULL, process_priority_high,   0);
    
    assert(pid_low != 0 && pid_normal != 0 && pid_high != 0);
    
    scheduler_run();
    
    // high priority should be scheduled first and its entry point should run
    process_id_t current = scheduler_get_current();
    assert(current == pid_high);
    assert(task3_run_count == 1);
    assert(task1_run_count == 0);
    assert(task2_run_count == 0);
    
    printf("FUNCTIONAL\n");
    teardown_scheduler();
}

// test 4: process yield
void test_scheduler_yield(void) {
    setup_scheduler();
    printf("  test_scheduler_yield... ");
    
    process_id_t pid = process_create("task1", test_task1, NULL, process_priority_normal, 0);
    assert(pid != 0);
    
    // start running the process
    scheduler_run();
    assert(scheduler_get_current() == pid);
    assert(process_get_state(pid) == process_state_running);
    
    // yield should move it back to ready and allow another process to run
    process_yield();
    assert(process_get_state(pid) == process_state_ready);
    
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

#endif  // PLATFORM_ESP32

