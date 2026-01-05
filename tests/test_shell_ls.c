#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "terminal.h"
#include "vfs.h"
#include "builtins.h"
#include "shell.h"
#include "shell_codes.h"

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

void test_ls_no_args(void) {
    printf("test_ls_no_args:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    TEST_ASSERT(term != NULL, "terminal exists");
    
    builtin_cmd *cmd = builtins_find("ls");
    TEST_ASSERT(cmd != NULL, "ls command registered");
    
    char *argv[] = {"ls", NULL};
    int result = cmd->handler(term, 1, argv);
    
    TEST_ASSERT(result == SHELL_OK, "ls with no args returns success");
    
    teardown_test();
    printf("\n");
}

void test_ls_with_cwd_set(void) {
    printf("test_ls_with_cwd_set:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cd_cmd = builtins_find("cd");
    builtin_cmd *ls_cmd = builtins_find("ls");
    
    char *cd_argv[] = {"cd", "/", NULL};
    cd_cmd->handler(term, 2, cd_argv);
    
    TEST_ASSERT(term->cwd != NULL, "cwd is set after cd");
    
    char *ls_argv[] = {"ls", NULL};
    int result = ls_cmd->handler(term, 1, ls_argv);
    
    TEST_ASSERT(result == SHELL_OK, "ls with cwd set returns success");
    
    teardown_test();
    printf("\n");
}

void test_ls_with_path(void) {
    printf("test_ls_with_path:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cmd = builtins_find("ls");
    
    char *argv[] = {"ls", "/", NULL};
    int result = cmd->handler(term, 2, argv);
    
    TEST_ASSERT(result == SHELL_OK, "ls / returns success");
    
    teardown_test();
    printf("\n");
}

void test_ls_with_invalid_path(void) {
    printf("test_ls_with_invalid_path:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cmd = builtins_find("ls");
    
    char *argv[] = {"ls", "/nonexistent", NULL};
    int result = cmd->handler(term, 2, argv);
    
    TEST_ASSERT(result == SHELL_ENOENT, "ls with invalid path returns ENOENT");
    
    teardown_test();
    printf("\n");
}

void test_ls_null_terminal(void) {
    printf("test_ls_null_terminal:\n");
    setup_test();
    
    builtin_cmd *cmd = builtins_find("ls");
    
    char *argv[] = {"ls", NULL};
    int result = cmd->handler(NULL, 1, argv);
    
    TEST_ASSERT(result != SHELL_OK, "ls with null terminal returns error");
    
    teardown_test();
    printf("\n");
}

void test_ls_with_relative_path(void) {
    printf("test_ls_with_relative_path:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cd_cmd = builtins_find("cd");
    builtin_cmd *ls_cmd = builtins_find("ls");
    
    char *cd_argv[] = {"cd", "/", NULL};
    cd_cmd->handler(term, 2, cd_argv);
    
    char *ls_argv[] = {"ls", ".", NULL};
    int result = ls_cmd->handler(term, 2, ls_argv);
    
    TEST_ASSERT(result == SHELL_OK, "ls . returns success");
    
    teardown_test();
    printf("\n");
}

void test_ls_with_parent_directory(void) {
    printf("test_ls_with_parent_directory:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cd_cmd = builtins_find("cd");
    builtin_cmd *ls_cmd = builtins_find("ls");
    
    char *cd_argv[] = {"cd", "/", NULL};
    cd_cmd->handler(term, 2, cd_argv);
    
    char *ls_argv[] = {"ls", "..", NULL};
    int result = ls_cmd->handler(term, 2, ls_argv);
    
    TEST_ASSERT(result == SHELL_OK || result == SHELL_ENOENT, "ls .. returns valid result");
    
    teardown_test();
    printf("\n");
}

void test_ls_empty_directory(void) {
    printf("test_ls_empty_directory:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cmd = builtins_find("ls");
    
    char *argv[] = {"ls", NULL};
    int result = cmd->handler(term, 1, argv);
    
    TEST_ASSERT(result == SHELL_OK, "ls on empty/null cwd returns success");
    
    teardown_test();
    printf("\n");
}

void test_ls_multiple_args(void) {
    printf("test_ls_multiple_args:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cmd = builtins_find("ls");
    
    char *argv[] = {"ls", "/", "/", NULL};
    int result = cmd->handler(term, 3, argv);
    
    TEST_ASSERT(result == SHELL_EINVAL, "ls with multiple args returns EINVAL");
    
    teardown_test();
    printf("\n");
}

void test_ls_directory_iteration(void) {
    printf("test_ls_directory_iteration:\n");
    setup_test();
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cd_cmd = builtins_find("cd");
    builtin_cmd *ls_cmd = builtins_find("ls");
    
    char *cd_argv[] = {"cd", "/", NULL};
    cd_cmd->handler(term, 2, cd_argv);
    
    char *ls_argv[] = {"ls", NULL};
    int result1 = ls_cmd->handler(term, 1, ls_argv);
    TEST_ASSERT(result1 == SHELL_OK, "first ls call succeeds");
    
    int result2 = ls_cmd->handler(term, 1, ls_argv);
    TEST_ASSERT(result2 == SHELL_OK, "second ls call succeeds");
    
    teardown_test();
    printf("\n");
}

int main(void) {
    printf("[SHELL LS TESTS]\n\n");
    
    test_ls_no_args();
    test_ls_with_cwd_set();
    test_ls_with_path();
    test_ls_with_invalid_path();
    test_ls_null_terminal();
    test_ls_with_relative_path();
    test_ls_with_parent_directory();
    test_ls_empty_directory();
    test_ls_multiple_args();
    test_ls_directory_iteration();
    
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

