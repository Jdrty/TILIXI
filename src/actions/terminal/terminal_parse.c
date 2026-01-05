#include "terminal.h"
#include <string.h>
#include <stdlib.h>

// portable tokenizer helper, finds next token in string
// returns pointer to start of token, or NULL if no more tokens
// modifies the string by null-terminating tokens
static char *next_token(char *str, const char *delimiters, char **saveptr) {
    if (str == NULL) {
        str = *saveptr;
    }
    
    if (str == NULL) {
        return NULL;
    }
    
    // skip leading delimiters
    while (*str != '\0' && strchr(delimiters, *str) != NULL) {
        str++;
    }
    
    if (*str == '\0') {
        *saveptr = NULL;
        return NULL;
    }
    
    char *token_start = str;
    
    // find end of token
    while (*str != '\0' && strchr(delimiters, *str) == NULL) {
        str++;
    }
    
    if (*str != '\0') {
        *str = '\0';
        str++;
    }
    
    *saveptr = str;
    return token_start;
}

// command parsing
command_tokens_t terminal_parse_command(const char *input) {
    command_tokens_t result = {0};
    uint8_t storage_idx = 0;
    
    if (input == NULL || strlen(input) == 0) {
        return result;
    }
    
    // use stack-allocated buffer instead of strdup() to avoid heap allocation
    // input is already limited to terminal_cols, so this is safe
    char input_copy[terminal_cols];
    size_t input_len = strlen(input);
    if (input_len >= terminal_cols) {
        input_len = terminal_cols - 1;
    }
    memcpy(input_copy, input, input_len);
    input_copy[input_len] = '\0';
    
    // portable tokenizer (replaces strtok_r)
    char *saveptr = input_copy;
    char *token = next_token(input_copy, " \t\n", &saveptr);
    
    while (token != NULL && result.token_count < terminal_cols && storage_idx < terminal_cols) {
        // check for pipe
        if (strcmp(token, "|") == 0) {
            result.has_pipe = 1;
            result.pipe_pos = result.token_count;
        } else {
            // copy token to per-instance storage (not static)
            size_t token_len = strlen(token);
            if (token_len >= terminal_cols) {
                token_len = terminal_cols - 1;
            }
            memcpy(result.token_storage[storage_idx], token, token_len);
            result.token_storage[storage_idx][token_len] = '\0';
            result.tokens[result.token_count] = result.token_storage[storage_idx];
            result.token_count++;
            storage_idx++;
        }
        token = next_token(NULL, " \t\n", &saveptr);
    }
    
    return result;
}

