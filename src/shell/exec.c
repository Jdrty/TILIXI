#include "exec.h"
#include "builtins.h"
#include "shell.h"
#include "shell_error.h"
#include "shell_codes.h"
#include "terminal.h"
#include <string.h>

int exec_dispatch(shell_tokens_t *tokens) {
    if (tokens == NULL || tokens->count == 0) {
        return SHELL_ERR;
    }
    
    const char *cmd_name = tokens->tokens[0];
    if (cmd_name == NULL) {
        return SHELL_ERR;
    }
    
    terminal_state *term = shell_get_terminal();
    if (!term) term = get_active_terminal();
    if (!term) return SHELL_ERR;
    
    builtin_cmd *cmd = builtins_find(cmd_name);
    if (cmd != NULL) {
        return cmd->handler(term, tokens->count, tokens->tokens);
    }
    
    shell_error(term, "command not found: %s", cmd_name);
    return SHELL_ERR;
}

