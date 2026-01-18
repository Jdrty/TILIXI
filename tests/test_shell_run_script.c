#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "terminal.h"
#include "vfs.h"
#include "builtins.h"
#include "shell_codes.h"

extern int vfs_stub_register_file(const char *path, const char *content);

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

static int count_substr(const char *haystack, const char *needle) {
    int count = 0;
    size_t len = strlen(needle);
    if (len == 0) {
        return 0;
    }
    const char *p = haystack;
    while ((p = strstr(p, needle)) != NULL) {
        count++;
        p += len;
    }
    return count;
}

void setup_test(void) {
    vfs_init();
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

void test_run_script_loops_conditionals(void) {
    printf("test_run_script_loops_conditionals:\n");
    setup_test();
    
    const char *script =
        "echo start\n"
        "if echo yes; then\n"
        "  echo in_if\n"
        "else\n"
        "  echo in_else\n"
        "fi\n"
        "while echo loop\n"
        "do\n"
        "  echo body\n"
        "  break\n"
        "  echo after\n"
        "done;\n"
        "echo end\n";
    
    int reg_ok = vfs_stub_register_file("/script.sh", script);
    TEST_ASSERT(reg_ok == 1, "registered script file in stub vfs");
    
    terminal_state *term = get_active_terminal();
    builtin_cmd *cmd = builtins_find("run");
    TEST_ASSERT(cmd != NULL, "run command registered");
    
    terminal_capture_start();
    char *argv[] = {"run", "/script.sh", NULL};
    int result = cmd->handler(term, 2, argv);
    size_t out_len = 0;
    char *out = terminal_capture_stop(&out_len);
    
    TEST_ASSERT(result == SHELL_OK, "run returns success");
    TEST_ASSERT(out != NULL, "captured output");
    if (out != NULL) {
        TEST_ASSERT(strstr(out, "start") != NULL, "script prints start");
        TEST_ASSERT(strstr(out, "yes") != NULL, "if condition ran");
        TEST_ASSERT(strstr(out, "in_if") != NULL, "if branch executed");
        TEST_ASSERT(strstr(out, "in_else") == NULL, "else branch skipped");
        TEST_ASSERT(strstr(out, "body") != NULL, "loop body executed");
        TEST_ASSERT(strstr(out, "after") == NULL, "break stops loop body");
        TEST_ASSERT(strstr(out, "end") != NULL, "script prints end");
        TEST_ASSERT(count_substr(out, "loop") == 1, "loop condition ran once");
        free(out);
    }
    
    teardown_test();
    printf("\n");
}

int main(void) {
    printf("[SHELL RUN SCRIPT TESTS]\n\n");
    test_run_script_loops_conditionals();
    
    printf("Tests run: %d, Passed: %d, Failed: %d\n",
           test_count, test_passed, test_failed);
    return test_failed == 0 ? 0 : 1;
}

