#include "shell.h"
#include "parser.h"
#include "exec.h"
#include "builtins.h"
#include "terminal.h"
#include "vfs.h"
#include <string.h>

static terminal_state *current_terminal = NULL;

void shell_set_terminal(terminal_state *term) {
    current_terminal = term;
}

terminal_state *shell_get_terminal(void) {
    return current_terminal;
}

void shell_repl(void) {
    if (current_terminal == NULL) {
        current_terminal = get_active_terminal();
    }
    
    if (current_terminal == NULL || !current_terminal->active) {
        return;
    }
    
    char *input = current_terminal->input_line;
    if (input == NULL || strlen(input) == 0) {
        return;
    }
    
    shell_tokens_t tokens = parser_tokenize(input);
    if (tokens.count > 0) {
        exec_dispatch(&tokens);
    }
    
    parser_free_tokens(&tokens);
}







