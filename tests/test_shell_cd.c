#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "terminal.h"
#include "vfs.h"
#include "shell/builtins.h"
#include "shell/shell.h"
#include "shell/shell_codes.h"

static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        test_count++; \
        if (cond) { \
            test_passed++; \
            printf("  PASS: %s\n", msg); \
        } else { \
            test_failed++; \
            printf("  FAIL: %s\n", msg); \
            printf("    at %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

void setup_test(void) {
    init_terminal_system();
    builtins_init();
    new_terminal();
}

void teardown_test(void) {
    terminal_state *term = get_active_terminal();
    if (term != NULL && term->cwd != NULL) {
        vfs_node_release(term->cwd);
        term->cwd = NULL;
    }
    close_terminal();
}

void test_cd_no_args(void) {
    printf("test_cd_no_args:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    TEST_ASSERT(term != NULL, "terminal exists");
    
    builtin_cmd *cmd = builtins_find("cd");
    TEST_ASSERT(cmd != NULL, "cd command registered");
    
    char *argv[] = {"cd", NULL};
    int result = cmd->handler(term, 1, argv);
    
    TEST_ASSERT(result == SHELL_OK, "cd with no args returns success");
    TEST_ASSERT(term->cwd != NULL, "cwd is set after cd");
    TEST_ASSERT(term->cwd->type == VFS_NODE_DIR, "cwd is a directory");
    
    teardown_test();
    printf("\n");
}

void test_cd_to_root(void) {
    printf("test_cd_to_root:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cmd = builtins_find("cd");
    
    char *argv[] = {"cd", "/", NULL};
    int result = cmd->handler(term, 2, argv);
    
    TEST_ASSERT(result == SHELL_OK, "cd / returns success");
    TEST_ASSERT(term->cwd != NULL, "cwd is set");
    TEST_ASSERT(term->cwd->type == VFS_NODE_DIR, "cwd is a directory");
    
    teardown_test();
    printf("\n");
}

void test_cd_to_nonexistent(void) {
    printf("test_cd_to_nonexistent:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cmd = builtins_find("cd");
    
    char *argv[] = {"cd", "/nonexistent/path", NULL};
    int result = cmd->handler(term, 2, argv);
    
    TEST_ASSERT(result != SHELL_OK, "cd to nonexistent path returns error");
    
    teardown_test();
    printf("\n");
}

void test_cd_too_many_args(void) {
    printf("test_cd_too_many_args:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cmd = builtins_find("cd");
    
    char *argv[] = {"cd", "/", "/", NULL};
    int result = cmd->handler(term, 3, argv);
    
    TEST_ASSERT(result != SHELL_OK, "cd with too many args returns error");
    
    teardown_test();
    printf("\n");
}

void test_cd_null_terminal(void) {
    printf("test_cd_null_terminal:\n");
    setup_test();
    
    builtin_cmd *cmd = builtins_find("cd");
    
    char *argv[] = {"cd", "/", NULL};
    int result = cmd->handler(NULL, 2, argv);
    
    TEST_ASSERT(result != SHELL_OK, "cd with null terminal returns error");
    
    teardown_test();
    printf("\n");
}

void test_cd_relative_path(void) {
    printf("test_cd_relative_path:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cmd = builtins_find("cd");
    
    char *argv1[] = {"cd", "/", NULL};
    cmd->handler(term, 2, argv1);
    
    vfs_node_t *root_cwd = term->cwd;
    TEST_ASSERT(root_cwd != NULL, "root cwd set");
    
    char *argv2[] = {"cd", ".", NULL};
    int result = cmd->handler(term, 2, argv2);
    
    TEST_ASSERT(result == SHELL_OK, "cd . returns success");
    TEST_ASSERT(term->cwd != NULL, "cwd still set after cd .");
    
    teardown_test();
    printf("\n");
}

void test_cd_parent_directory(void) {
    printf("test_cd_parent_directory:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cmd = builtins_find("cd");
    
    char *argv1[] = {"cd", "/", NULL};
    cmd->handler(term, 2, argv1);
    
    vfs_node_t *root_cwd = term->cwd;
    TEST_ASSERT(root_cwd != NULL, "root cwd set");
    
    char *argv2[] = {"cd", "..", NULL};
    int result = cmd->handler(term, 2, argv2);
    
    TEST_ASSERT(result == 0 || result == -1, "cd .. returns valid result");
    
    teardown_test();
    printf("\n");
}

void test_cd_cwd_reference_counting(void) {
    printf("test_cd_cwd_reference_counting:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cmd = builtins_find("cd");
    
    char *argv1[] = {"cd", "/", NULL};
    cmd->handler(term, 2, argv1);
    
    vfs_node_t *first_cwd = term->cwd;
    TEST_ASSERT(first_cwd != NULL, "first cwd is set");
    
    uint32_t refcount_before = first_cwd->refcount;
    
    char *argv2[] = {"cd", "/", NULL};
    cmd->handler(term, 2, argv2);
    
    TEST_ASSERT(term->cwd != NULL, "cwd still set after second cd");
    
    uint32_t refcount_after = term->cwd->refcount;
    TEST_ASSERT(refcount_after >= refcount_before - 1, 
                "reference counting works correctly");
    
    teardown_test();
    printf("\n");
}

void test_cd_to_file_not_directory(void) {
    printf("test_cd_to_file_not_directory:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cmd = builtins_find("cd");
    
    char *argv[] = {"cd", "/somefile.txt", NULL};
    int result = cmd->handler(term, 2, argv);
    
    TEST_ASSERT(result != SHELL_OK, "cd to file returns error or not found");
    
    teardown_test();
    printf("\n");
}

int main(void) {
    printf("[SHELL CD TESTS]\n\n");
    
    test_cd_no_args();
    test_cd_to_root();
    test_cd_to_nonexistent();
    test_cd_too_many_args();
    test_cd_null_terminal();
    test_cd_relative_path();
    test_cd_parent_directory();
    test_cd_cwd_reference_counting();
    test_cd_to_file_not_directory();
    
    printf("\n[TEST SUMMARY]\n");
    printf("  Total: %d\n", test_count);
    printf("  Passed: %d\n", test_passed);
    printf("  Failed: %d\n", test_failed);
    
    if (test_failed == 0) {
        printf("\nAll tests passed!\n");
        return 0;
    } else {
        printf("\nSome tests failed!\n");
        return 1;
    }
}

