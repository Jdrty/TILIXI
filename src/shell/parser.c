#include "parser.h"
#include <string.h>
#include <stdlib.h>

#define max_tokens 32
#define max_token_len 256

static char *my_strdup(const char *s) {
    if (s == NULL) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, s, len);
    }
    return copy;
}

shell_tokens_t parser_tokenize(const char *input) {
    shell_tokens_t tokens = {0};
    
    if (input == NULL) {
        return tokens;
    }
    
    char *input_copy = my_strdup(input);
    if (input_copy == NULL) {
        return tokens;
    }
    
    char *token = strtok(input_copy, " \t\n\r");
    while (token != NULL && tokens.count < max_tokens) {
        if (strlen(token) < max_token_len) {
            tokens.tokens[tokens.count] = my_strdup(token);
            tokens.count++;
        }
        token = strtok(NULL, " \t\n\r");
    }
    
    free(input_copy);
    return tokens;
}

void parser_free_tokens(shell_tokens_t *tokens) {
    if (tokens == NULL) {
        return;
    }
    
    for (int i = 0; i < tokens->count; i++) {
        if (tokens->tokens[i] != NULL) {
            free(tokens->tokens[i]);
            tokens->tokens[i] = NULL;
        }
    }
    tokens->count = 0;
}

