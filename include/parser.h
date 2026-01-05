#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define max_tokens 32

typedef struct {
    char *tokens[max_tokens];
    int count;
} shell_tokens_t;

shell_tokens_t parser_tokenize(const char *input);
void parser_free_tokens(shell_tokens_t *tokens);

#ifdef __cplusplus
}
#endif

