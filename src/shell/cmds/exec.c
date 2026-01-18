#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"
#include "shell_error.h"
#include "vfs.h"
#include "compat.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

int cmd_run(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_run_def = {
    .name = "run",
    .handler = cmd_run,
    .help = "Execute shell script"
};

#define MAX_SCRIPT_VARS 32

typedef struct {
    char name[32];
    char *value;
} script_var_t;

typedef struct {
    script_var_t vars[MAX_SCRIPT_VARS];
    int count;
    int loop_depth;
    int break_requested;
    int continue_requested;
} script_ctx_t;

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} line_accum_t;

typedef struct {
    char **lines;
    size_t count;
    size_t cap;
} script_lines_t;

typedef enum {
    END_NONE = 0,
    END_ELSE = 1,
    END_ELIF = 2,
    END_FI = 3,
    END_DONE = 4
} block_end_type_t;

typedef struct {
    block_end_type_t type;
    char *line;
} block_end_t;

static int line_accum_append(line_accum_t *acc, char c) {
    if (acc->len + 1 >= acc->cap) {
        size_t new_cap = acc->cap == 0 ? 128 : acc->cap * 2;
        char *new_buf = (char*)realloc(acc->buf, new_cap);
        if (new_buf == NULL) {
            return 0;
        }
        acc->buf = new_buf;
        acc->cap = new_cap;
    }
    acc->buf[acc->len++] = c;
    return 1;
}

static int script_lines_append(script_lines_t *lines, char *line) {
    if (lines->count + 1 >= lines->cap) {
        size_t new_cap = lines->cap == 0 ? 32 : lines->cap * 2;
        char **new_lines = (char**)realloc(lines->lines, new_cap * sizeof(char*));
        if (new_lines == NULL) {
            return 0;
        }
        lines->lines = new_lines;
        lines->cap = new_cap;
    }
    lines->lines[lines->count++] = line;
    return 1;
}

static char *trim_whitespace(char *s) {
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
    return s;
}

static int line_is_keyword(const char *line, const char *kw) {
    size_t len = strlen(kw);
    if (strncmp(line, kw, len) != 0) {
        return 0;
    }
    const char *p = line + len;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (*p == '\0') {
        return 1;
    }
    if (*p == ';') {
        p++;
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        return *p == '\0';
    }
    return 0;
}

static const char *ctx_get_var(script_ctx_t *ctx, const char *name) {
    if (ctx == NULL || name == NULL) {
        return NULL;
    }
    for (int i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0) {
            return ctx->vars[i].value;
        }
    }
    return NULL;
}

static int ctx_set_var(script_ctx_t *ctx, const char *name, const char *value) {
    if (ctx == NULL || name == NULL || name[0] == '\0') {
        return 0;
    }
    for (int i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0) {
            free(ctx->vars[i].value);
            ctx->vars[i].value = value ? strdup(value) : strdup("");
            return ctx->vars[i].value != NULL;
        }
    }
    if (ctx->count >= MAX_SCRIPT_VARS) {
        return 0;
    }
    strncpy(ctx->vars[ctx->count].name, name, sizeof(ctx->vars[ctx->count].name) - 1);
    ctx->vars[ctx->count].name[sizeof(ctx->vars[ctx->count].name) - 1] = '\0';
    ctx->vars[ctx->count].value = value ? strdup(value) : strdup("");
    if (ctx->vars[ctx->count].value == NULL) {
        return 0;
    }
    ctx->count++;
    return 1;
}

static void ctx_free(script_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    for (int i = 0; i < ctx->count; i++) {
        free(ctx->vars[i].value);
        ctx->vars[i].value = NULL;
    }
    ctx->count = 0;
}

static char *strip_comments_and_trim(const char *line) {
    if (line == NULL) {
        return NULL;
    }
    size_t len = strlen(line);
    char *out = (char*)malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    size_t o = 0;
    int in_single = 0;
    int in_double = 0;
    int escaped = 0;
    for (size_t i = 0; i < len; i++) {
        char c = line[i];
        if (escaped) {
            out[o++] = c;
            escaped = 0;
            continue;
        }
        if (!in_single && c == '\\') {
            escaped = 1;
            out[o++] = c;
            continue;
        }
        if (!in_double && c == '\'') {
            in_single = !in_single;
            out[o++] = c;
            continue;
        }
        if (!in_single && c == '"') {
            in_double = !in_double;
            out[o++] = c;
            continue;
        }
        if (!in_single && !in_double && c == '#') {
            break;
        }
        out[o++] = c;
    }
    out[o] = '\0';
    char *trimmed = trim_whitespace(out);
    if (trimmed != out) {
        memmove(out, trimmed, strlen(trimmed) + 1);
    }
    return out;
}

static char *expand_vars(script_ctx_t *ctx, const char *line) {
    if (line == NULL) {
        return NULL;
    }
    size_t cap = strlen(line) + 32;
    char *out = (char*)malloc(cap);
    if (out == NULL) {
        return NULL;
    }
    size_t o = 0;
    int in_single = 0;
    int in_double = 0;
    int escaped = 0;
    for (size_t i = 0; line[i] != '\0'; i++) {
        char c = line[i];
        if (escaped) {
            if (o + 1 >= cap) {
                cap *= 2;
                char *new_out = (char*)realloc(out, cap);
                if (new_out == NULL) {
                    free(out);
                    return NULL;
                }
                out = new_out;
            }
            out[o++] = c;
            escaped = 0;
            continue;
        }
        if (!in_single && c == '\\') {
            escaped = 1;
            if (o + 1 >= cap) {
                cap *= 2;
                char *new_out = (char*)realloc(out, cap);
                if (new_out == NULL) {
                    free(out);
                    return NULL;
                }
                out = new_out;
            }
            out[o++] = c;
            continue;
        }
        if (!in_double && c == '\'') {
            in_single = !in_single;
            out[o++] = c;
            continue;
        }
        if (!in_single && c == '"') {
            in_double = !in_double;
            out[o++] = c;
            continue;
        }
        if (!in_single && c == '$') {
            if (line[i + 1] == '$') {
                out[o++] = '$';
                i++;
                continue;
            }
            if (line[i + 1] == '{') {
                size_t j = i + 2;
                char name[32];
                size_t n = 0;
                while (line[j] != '\0' && line[j] != '}' && n + 1 < sizeof(name)) {
                    name[n++] = line[j++];
                }
                if (line[j] == '}') {
                    name[n] = '\0';
                    const char *val = ctx_get_var(ctx, name);
                    if (val != NULL) {
                        size_t vlen = strlen(val);
                        if (o + vlen + 1 >= cap) {
                            cap = (o + vlen + 1) * 2;
                            char *new_out = (char*)realloc(out, cap);
                            if (new_out == NULL) {
                                free(out);
                                return NULL;
                            }
                            out = new_out;
                        }
                        memcpy(out + o, val, vlen);
                        o += vlen;
                    }
                    i = j;
                    continue;
                }
            }
            if (isalpha((unsigned char)line[i + 1]) || line[i + 1] == '_') {
                size_t j = i + 1;
                char name[32];
                size_t n = 0;
                while ((isalnum((unsigned char)line[j]) || line[j] == '_') &&
                       n + 1 < sizeof(name)) {
                    name[n++] = line[j++];
                }
                name[n] = '\0';
                const char *val = ctx_get_var(ctx, name);
                if (val != NULL) {
                    size_t vlen = strlen(val);
                    if (o + vlen + 1 >= cap) {
                        cap = (o + vlen + 1) * 2;
                        char *new_out = (char*)realloc(out, cap);
                        if (new_out == NULL) {
                            free(out);
                            return NULL;
                        }
                        out = new_out;
                    }
                    memcpy(out + o, val, vlen);
                    o += vlen;
                }
                i = j - 1;
                continue;
            }
        }
        if (o + 1 >= cap) {
            cap *= 2;
            char *new_out = (char*)realloc(out, cap);
            if (new_out == NULL) {
                free(out);
                return NULL;
            }
            out = new_out;
        }
        out[o++] = c;
    }
    out[o] = '\0';
    return out;
}

static int is_valid_name(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    if (!(isalpha((unsigned char)name[0]) || name[0] == '_')) {
        return 0;
    }
    for (const char *p = name + 1; *p != '\0'; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_')) {
            return 0;
        }
    }
    return 1;
}

static int is_assignment(const char *line, char *out_name, size_t name_len,
                         const char **out_value) {
    int in_single = 0;
    int in_double = 0;
    int escaped = 0;
    const char *eq = NULL;
    for (const char *p = line; *p != '\0'; p++) {
        if (escaped) {
            escaped = 0;
            continue;
        }
        if (!in_single && *p == '\\') {
            escaped = 1;
            continue;
        }
        if (!in_double && *p == '\'') {
            in_single = !in_single;
            continue;
        }
        if (!in_single && *p == '"') {
            in_double = !in_double;
            continue;
        }
        if (!in_single && !in_double && isspace((unsigned char)*p)) {
            return 0;
        }
        if (!in_single && !in_double && *p == '=' && eq == NULL) {
            eq = p;
        }
    }
    if (eq == NULL) {
        return 0;
    }
    size_t nlen = (size_t)(eq - line);
    if (nlen == 0 || nlen >= name_len) {
        return 0;
    }
    memcpy(out_name, line, nlen);
    out_name[nlen] = '\0';
    if (!is_valid_name(out_name)) {
        return 0;
    }
    *out_value = eq + 1;
    return 1;
}

static char *extract_value(script_ctx_t *ctx, const char *value) {
    size_t len = strlen(value);
    if (len >= 2 && value[0] == '\'' && value[len - 1] == '\'') {
        char *out = (char*)malloc(len - 1);
        if (out == NULL) {
            return NULL;
        }
        memcpy(out, value + 1, len - 2);
        out[len - 2] = '\0';
        return out;
    }
    if (len >= 2 && value[0] == '"' && value[len - 1] == '"') {
        char *inner = (char*)malloc(len - 1);
        if (inner == NULL) {
            return NULL;
        }
        memcpy(inner, value + 1, len - 2);
        inner[len - 2] = '\0';
        char *expanded = expand_vars(ctx, inner);
        free(inner);
        return expanded;
    }
    return expand_vars(ctx, value);
}

static void execute_command_string(terminal_state *term, char *cmd) {
    command_tokens_t *tokens = (command_tokens_t*)malloc(sizeof(command_tokens_t));
    if (tokens == NULL) {
        return;
    }
    terminal_parse_command(cmd, tokens);
    if (tokens->token_count == 0) {
        free(tokens);
        return;
    }
    if (tokens->has_pipe) {
        terminal_execute_pipeline(term, tokens);
    } else {
        terminal_execute_command(term, tokens);
    }
    free(tokens);
}

static int execute_condition(terminal_state *term, script_ctx_t *ctx,
                             const char *cmd) {
    char *expanded = expand_vars(ctx, cmd);
    if (expanded == NULL) {
        return SHELL_ERR;
    }
    command_tokens_t *tokens = (command_tokens_t*)malloc(sizeof(command_tokens_t));
    if (tokens == NULL) {
        free(expanded);
        return SHELL_ERR;
    }
    terminal_parse_command(expanded, tokens);
    if (tokens->token_count == 0) {
        free(tokens);
        free(expanded);
        return SHELL_ERR;
    }
    if (tokens->has_pipe) {
        terminal_execute_pipeline(term, tokens);
        free(tokens);
        free(expanded);
        return SHELL_OK;
    }
    builtin_cmd *builtin = builtins_find(tokens->tokens[0]);
    if (builtin == NULL) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "damocles: unknown command: %s\n",
                 tokens->tokens[0]);
        terminal_write_string(term, error_msg);
        free(tokens);
        free(expanded);
        return SHELL_ERR;
    }
    int result = builtin->handler(term, tokens->token_count, (char**)tokens->tokens);
    free(tokens);
    free(expanded);
    return result;
}

static void execute_segment(terminal_state *term, script_ctx_t *ctx, char *segment) {
    char *trimmed = trim_whitespace(segment);
    if (trimmed[0] == '\0' || trimmed[0] == '#') {
        return;
    }
    if (line_is_keyword(trimmed, "break")) {
        if (ctx->loop_depth == 0) {
            shell_error(term, "run: break outside loop");
            return;
        }
        ctx->break_requested = 1;
        return;
    }
    if (line_is_keyword(trimmed, "continue")) {
        if (ctx->loop_depth == 0) {
            shell_error(term, "run: continue outside loop");
            return;
        }
        ctx->continue_requested = 1;
        return;
    }
    char name[32];
    const char *value = NULL;
    if (is_assignment(trimmed, name, sizeof(name), &value)) {
        char *resolved = extract_value(ctx, value);
        if (resolved != NULL) {
            ctx_set_var(ctx, name, resolved);
            free(resolved);
        }
        return;
    }
    char *expanded = expand_vars(ctx, trimmed);
    if (expanded == NULL) {
        return;
    }
    execute_command_string(term, expanded);
    free(expanded);
}

static void execute_simple_line(terminal_state *term, script_ctx_t *ctx,
                                char *line) {
    int in_single = 0;
    int in_double = 0;
    int escaped = 0;
    char *segment = line;
    for (char *p = line; *p != '\0'; p++) {
        if (escaped) {
            escaped = 0;
            continue;
        }
        if (!in_single && *p == '\\') {
            escaped = 1;
            continue;
        }
        if (!in_double && *p == '\'') {
            in_single = !in_single;
            continue;
        }
        if (!in_single && *p == '"') {
            in_double = !in_double;
            continue;
        }
        if (!in_single && !in_double && *p == ';') {
            *p = '\0';
            execute_segment(term, ctx, segment);
            segment = p + 1;
        }
    }
    execute_segment(term, ctx, segment);
}

static int line_starts_with(const char *line, const char *kw, const char **out_rest) {
    size_t len = strlen(kw);
    if (strncmp(line, kw, len) != 0) {
        return 0;
    }
    if (line[len] != '\0' && !isspace((unsigned char)line[len])) {
        return 0;
    }
    if (out_rest != NULL) {
        const char *rest = line + len;
        while (*rest && isspace((unsigned char)*rest)) {
            rest++;
        }
        *out_rest = rest;
    }
    return 1;
}


static int parse_inline_token(const char *rest, const char *token,
                              char **out_cmd, int *has_inline) {
    int in_single = 0;
    int in_double = 0;
    int escaped = 0;
    const char *split = NULL;
    for (const char *p = rest; *p != '\0'; p++) {
        if (escaped) {
            escaped = 0;
            continue;
        }
        if (!in_single && *p == '\\') {
            escaped = 1;
            continue;
        }
        if (!in_double && *p == '\'') {
            in_single = !in_single;
            continue;
        }
        if (!in_single && *p == '"') {
            in_double = !in_double;
            continue;
        }
        if (!in_single && !in_double && *p == ';') {
            split = p;
            break;
        }
    }
    if (split != NULL) {
        const char *after = split + 1;
        while (*after && isspace((unsigned char)*after)) {
            after++;
        }
        size_t tok_len = strlen(token);
        if (strncmp(after, token, tok_len) == 0) {
            const char *tail = after + tok_len;
            while (*tail && isspace((unsigned char)*tail)) {
                tail++;
            }
            if (*tail == '\0') {
                size_t cmd_len = (size_t)(split - rest);
                char *cmd = (char*)malloc(cmd_len + 1);
                if (cmd == NULL) {
                    return 0;
                }
                memcpy(cmd, rest, cmd_len);
                cmd[cmd_len] = '\0';
                char *trimmed_cmd = trim_whitespace(cmd);
                *out_cmd = strdup(trimmed_cmd);
                free(cmd);
                *has_inline = 1;
                return *out_cmd != NULL;
            }
        }
    }
    *out_cmd = strdup(rest);
    *has_inline = 0;
    return *out_cmd != NULL;
}

static int consume_expected(char **lines, size_t count, size_t *idx,
                            const char *expected) {
    while (*idx < count) {
        char *line = strip_comments_and_trim(lines[*idx]);
        (*idx)++;
        if (line == NULL) {
            return 0;
        }
        if (line[0] == '\0') {
            free(line);
            continue;
        }
        int ok = line_is_keyword(line, expected);
        free(line);
        return ok;
    }
    return 0;
}

static int find_matching_done(char **lines, size_t count, size_t start) {
    int loop_depth = 0;
    int if_depth = 0;
    for (size_t i = start; i < count; i++) {
        char *line = strip_comments_and_trim(lines[i]);
        if (line == NULL) {
            return -1;
        }
        if (line[0] == '\0') {
            free(line);
            continue;
        }
        const char *rest = NULL;
        if (line_starts_with(line, "if", &rest)) {
            if_depth++;
        } else if (line_is_keyword(line, "fi")) {
            if (if_depth > 0) {
                if_depth--;
            }
        } else if (line_starts_with(line, "while", &rest) ||
                   line_starts_with(line, "for", &rest)) {
            loop_depth++;
        } else if (line_is_keyword(line, "done")) {
            if (loop_depth == 0 && if_depth == 0) {
                free(line);
                return (int)i;
            }
            if (loop_depth > 0) {
                loop_depth--;
            }
        }
        free(line);
    }
    return -1;
}

static block_end_t execute_block_until(terminal_state *term, script_ctx_t *ctx,
                                       char **lines, size_t count, size_t *idx,
                                       int execute, int stop_mask);

static int parse_if_line(const char *line, const char *keyword,
                         char **out_cmd, int *inline_then) {
    const char *rest = NULL;
    if (!line_starts_with(line, keyword, &rest)) {
        return 0;
    }
    if (rest == NULL || rest[0] == '\0') {
        return 0;
    }
    return parse_inline_token(rest, "then", out_cmd, inline_then);
}

static int parse_while_line(const char *line, char **out_cmd, int *inline_do) {
    const char *rest = NULL;
    if (!line_starts_with(line, "while", &rest)) {
        return 0;
    }
    if (rest == NULL || rest[0] == '\0') {
        return 0;
    }
    return parse_inline_token(rest, "do", out_cmd, inline_do);
}

static char **split_words(const char *line, int *out_count) {
    size_t cap = 8;
    size_t count = 0;
    char **out = (char**)malloc(cap * sizeof(char*));
    if (out == NULL) {
        return NULL;
    }
    line_accum_t acc = {0};
    int in_single = 0;
    int in_double = 0;
    int escaped = 0;
    for (size_t i = 0; line[i] != '\0'; i++) {
        char c = line[i];
        if (escaped) {
            if (!line_accum_append(&acc, c)) {
                break;
            }
            escaped = 0;
            continue;
        }
        if (!in_single && c == '\\') {
            escaped = 1;
            continue;
        }
        if (!in_double && c == '\'') {
            in_single = !in_single;
            continue;
        }
        if (!in_single && c == '"') {
            in_double = !in_double;
            continue;
        }
        if (!in_single && !in_double && isspace((unsigned char)c)) {
            if (acc.len > 0) {
                if (!line_accum_append(&acc, '\0')) {
                    break;
                }
                if (count + 1 >= cap) {
                    cap *= 2;
                    char **new_out = (char**)realloc(out, cap * sizeof(char*));
                    if (new_out == NULL) {
                        break;
                    }
                    out = new_out;
                }
                out[count++] = strdup(acc.buf);
                acc.len = 0;
            }
            continue;
        }
        if (!line_accum_append(&acc, c)) {
            break;
        }
    }
    if (acc.len > 0) {
        if (line_accum_append(&acc, '\0')) {
            if (count + 1 >= cap) {
                cap *= 2;
                char **new_out = (char**)realloc(out, cap * sizeof(char*));
                if (new_out != NULL) {
                    out = new_out;
                }
            }
            out[count++] = strdup(acc.buf);
        }
    }
    free(acc.buf);
    if (out_count != NULL) {
        *out_count = (int)count;
    }
    return out;
}

static void free_words(char **words, int count) {
    if (words == NULL) {
        return;
    }
    for (int i = 0; i < count; i++) {
        free(words[i]);
    }
    free(words);
}

static int parse_for_line(const char *line, char **out_var,
                          char ***out_items, int *out_item_count,
                          int *inline_do) {
    const char *rest = NULL;
    if (!line_starts_with(line, "for", &rest)) {
        return 0;
    }
    if (rest == NULL || rest[0] == '\0') {
        return 0;
    }
    char *list_part = NULL;
    int ok = parse_inline_token(rest, "do", &list_part, inline_do);
    if (!ok || list_part == NULL) {
        free(list_part);
        return 0;
    }
    int count = 0;
    char **words = split_words(list_part, &count);
    free(list_part);
    if (words == NULL || count < 3) {
        free_words(words, count);
        return 0;
    }
    if (strcmp(words[1], "in") != 0) {
        free_words(words, count);
        return 0;
    }
    *out_var = words[0];
    int item_count = count - 2;
    char **items = (char**)malloc(item_count * sizeof(char*));
    if (items == NULL) {
        free_words(words, count);
        return 0;
    }
    for (int i = 0; i < item_count; i++) {
        items[i] = words[i + 2];
    }
    free(words[1]);
    free(words);
    *out_items = items;
    *out_item_count = item_count;
    return 1;
}

static block_end_t execute_if(terminal_state *term, script_ctx_t *ctx,
                              char **lines, size_t count, size_t *idx,
                              const char *line) {
    block_end_t end = {END_NONE, NULL};
    char *cond = NULL;
    int inline_then = 0;
    if (!parse_if_line(line, "if", &cond, &inline_then)) {
        shell_error(term, "run: malformed if");
        free(cond);
        return end;
    }
    if (!inline_then) {
        if (!consume_expected(lines, count, idx, "then")) {
            shell_error(term, "run: missing then");
            free(cond);
            return end;
        }
    }
    int result = execute_condition(term, ctx, cond);
    free(cond);
    int cond_true = (result == SHELL_OK);
    if (cond_true) {
        end = execute_block_until(term, ctx, lines, count, idx, 1,
                                  (1 << END_ELSE) | (1 << END_ELIF) | (1 << END_FI));
        if (end.type == END_ELIF || end.type == END_ELSE) {
            while (end.type != END_FI && *idx < count) {
                if (end.type == END_ELIF && end.line != NULL) {
                    char *elif_line = end.line;
                    end.line = NULL;
                    char *elif_cond = NULL;
                    int elif_inline = 0;
                    if (!parse_if_line(elif_line, "elif", &elif_cond, &elif_inline)) {
                        free(elif_line);
                        break;
                    }
                    free(elif_cond);
                    free(elif_line);
                    if (!elif_inline) {
                        consume_expected(lines, count, idx, "then");
                    }
                    end = execute_block_until(term, ctx, lines, count, idx, 0,
                                              (1 << END_ELSE) | (1 << END_ELIF) | (1 << END_FI));
                } else if (end.type == END_ELSE) {
                    end = execute_block_until(term, ctx, lines, count, idx, 0,
                                              (1 << END_FI));
                } else {
                    break;
                }
            }
        }
    } else {
        end = execute_block_until(term, ctx, lines, count, idx, 0,
                                  (1 << END_ELSE) | (1 << END_ELIF) | (1 << END_FI));
        while (end.type == END_ELIF) {
            char *elif_line = end.line;
            end.line = NULL;
            char *elif_cond = NULL;
            int elif_inline = 0;
            if (!parse_if_line(elif_line, "elif", &elif_cond, &elif_inline)) {
                free(elif_line);
                break;
            }
            int elif_res = execute_condition(term, ctx, elif_cond);
            free(elif_cond);
            free(elif_line);
            if (!elif_inline) {
                if (!consume_expected(lines, count, idx, "then")) {
                    shell_error(term, "run: missing then");
                    break;
                }
            }
            if (elif_res == SHELL_OK) {
                end = execute_block_until(term, ctx, lines, count, idx, 1,
                                          (1 << END_ELSE) | (1 << END_ELIF) | (1 << END_FI));
                if (end.type == END_ELIF || end.type == END_ELSE) {
                    while (end.type != END_FI && *idx < count) {
                        if (end.type == END_ELIF && end.line != NULL) {
                            char *skip_line = end.line;
                            end.line = NULL;
                            char *skip_cond = NULL;
                            int skip_inline = 0;
                            if (parse_if_line(skip_line, "elif", &skip_cond, &skip_inline)) {
                                free(skip_cond);
                            }
                            free(skip_line);
                            if (!skip_inline) {
                                consume_expected(lines, count, idx, "then");
                            }
                            end = execute_block_until(term, ctx, lines, count, idx, 0,
                                                      (1 << END_ELSE) | (1 << END_ELIF) | (1 << END_FI));
                        } else if (end.type == END_ELSE) {
                            end = execute_block_until(term, ctx, lines, count, idx, 0,
                                                      (1 << END_FI));
                        } else {
                            break;
                        }
                    }
                }
                return end;
            }
            end = execute_block_until(term, ctx, lines, count, idx, 0,
                                      (1 << END_ELSE) | (1 << END_ELIF) | (1 << END_FI));
        }
        if (end.type == END_ELSE) {
            end = execute_block_until(term, ctx, lines, count, idx, 1,
                                      (1 << END_FI));
        }
    }
    if (end.line != NULL) {
        free(end.line);
        end.line = NULL;
    }
    return end;
}

static void skip_if(terminal_state *term, script_ctx_t *ctx,
                    char **lines, size_t count, size_t *idx,
                    const char *line) {
    char *cond = NULL;
    int inline_then = 0;
    if (!parse_if_line(line, "if", &cond, &inline_then)) {
        free(cond);
        return;
    }
    free(cond);
    if (!inline_then) {
        if (!consume_expected(lines, count, idx, "then")) {
            return;
        }
    }
    block_end_t end = execute_block_until(term, ctx, lines, count, idx, 0,
                                          (1 << END_ELSE) | (1 << END_ELIF) | (1 << END_FI));
    while (end.type == END_ELIF) {
        char *elif_line = end.line;
        end.line = NULL;
        char *elif_cond = NULL;
        int elif_inline = 0;
        if (parse_if_line(elif_line, "elif", &elif_cond, &elif_inline)) {
            free(elif_cond);
        }
        free(elif_line);
        if (!elif_inline) {
            consume_expected(lines, count, idx, "then");
        }
        end = execute_block_until(term, ctx, lines, count, idx, 0,
                                  (1 << END_ELSE) | (1 << END_ELIF) | (1 << END_FI));
    }
    if (end.type == END_ELSE) {
        end = execute_block_until(term, ctx, lines, count, idx, 0, (1 << END_FI));
    }
    if (end.line != NULL) {
        free(end.line);
    }
}

static block_end_t execute_while(terminal_state *term, script_ctx_t *ctx,
                                 char **lines, size_t count, size_t *idx,
                                 const char *line) {
    block_end_t end = {END_NONE, NULL};
    char *cond = NULL;
    int inline_do = 0;
    if (!parse_while_line(line, &cond, &inline_do)) {
        shell_error(term, "run: malformed while");
        free(cond);
        return end;
    }
    if (!inline_do) {
        if (!consume_expected(lines, count, idx, "do")) {
            shell_error(term, "run: missing do");
            free(cond);
            return end;
        }
    }
    size_t body_start = *idx;
    int body_end = find_matching_done(lines, count, body_start);
    if (body_end < 0) {
        shell_error(term, "run: missing done");
        free(cond);
        return end;
    }
    ctx->loop_depth++;
    while (execute_condition(term, ctx, cond) == SHELL_OK) {
        size_t inner_idx = body_start;
        execute_block_until(term, ctx, lines, (size_t)body_end, &inner_idx, 1, 0);
        if (ctx->break_requested) {
            ctx->break_requested = 0;
            break;
        }
        if (ctx->continue_requested) {
            ctx->continue_requested = 0;
            continue;
        }
    }
    ctx->loop_depth--;
    free(cond);
    *idx = (size_t)body_end + 1;
    return end;
}

static void skip_while(terminal_state *term, script_ctx_t *ctx,
                       char **lines, size_t count, size_t *idx,
                       const char *line) {
    char *cond = NULL;
    int inline_do = 0;
    if (!parse_while_line(line, &cond, &inline_do)) {
        free(cond);
        return;
    }
    free(cond);
    if (!inline_do) {
        if (!consume_expected(lines, count, idx, "do")) {
            return;
        }
    }
    block_end_t end = execute_block_until(term, ctx, lines, count, idx, 0,
                                          (1 << END_DONE));
    if (end.line != NULL) {
        free(end.line);
    }
}

static block_end_t execute_for(terminal_state *term, script_ctx_t *ctx,
                               char **lines, size_t count, size_t *idx,
                               const char *line) {
    block_end_t end = {END_NONE, NULL};
    char *var = NULL;
    char **items = NULL;
    int item_count = 0;
    int inline_do = 0;
    if (!parse_for_line(line, &var, &items, &item_count, &inline_do)) {
        shell_error(term, "run: malformed for");
        free(var);
        free_words(items, item_count);
        return end;
    }
    if (!inline_do) {
        if (!consume_expected(lines, count, idx, "do")) {
            shell_error(term, "run: missing do");
            free(var);
            free_words(items, item_count);
            return end;
        }
    }
    size_t body_start = *idx;
    int body_end = find_matching_done(lines, count, body_start);
    if (body_end < 0) {
        shell_error(term, "run: missing done");
        free(var);
        free_words(items, item_count);
        return end;
    }
    ctx->loop_depth++;
    for (int i = 0; i < item_count; i++) {
        char *expanded = expand_vars(ctx, items[i]);
        ctx_set_var(ctx, var, expanded ? expanded : items[i]);
        free(expanded);
        size_t inner_idx = body_start;
        execute_block_until(term, ctx, lines, (size_t)body_end, &inner_idx, 1, 0);
        if (ctx->break_requested) {
            ctx->break_requested = 0;
            break;
        }
        if (ctx->continue_requested) {
            ctx->continue_requested = 0;
            continue;
        }
    }
    ctx->loop_depth--;
    free(var);
    free_words(items, item_count);
    *idx = (size_t)body_end + 1;
    return end;
}

static void skip_for(terminal_state *term, script_ctx_t *ctx,
                     char **lines, size_t count, size_t *idx,
                     const char *line) {
    char *var = NULL;
    char **items = NULL;
    int item_count = 0;
    int inline_do = 0;
    if (!parse_for_line(line, &var, &items, &item_count, &inline_do)) {
        free(var);
        free_words(items, item_count);
        return;
    }
    free(var);
    free_words(items, item_count);
    if (!inline_do) {
        if (!consume_expected(lines, count, idx, "do")) {
            return;
        }
    }
    block_end_t end = execute_block_until(term, ctx, lines, count, idx, 0,
                                          (1 << END_DONE));
    if (end.line != NULL) {
        free(end.line);
    }
}

static block_end_t execute_block_until(terminal_state *term, script_ctx_t *ctx,
                                       char **lines, size_t count, size_t *idx,
                                       int execute, int stop_mask) {
    block_end_t end = {END_NONE, NULL};
    while (*idx < count) {
        if (ctx->break_requested || ctx->continue_requested) {
            return end;
        }
        char *line = strip_comments_and_trim(lines[*idx]);
        (*idx)++;
        if (line == NULL) {
            break;
        }
        if (line[0] == '\0') {
            free(line);
            continue;
        }
        if (line_is_keyword(line, "else")) {
            if (stop_mask & (1 << END_ELSE)) {
                end.type = END_ELSE;
                end.line = line;
                return end;
            }
        } else if (line_starts_with(line, "elif", NULL)) {
            if (stop_mask & (1 << END_ELIF)) {
                end.type = END_ELIF;
                end.line = line;
                return end;
            }
        } else if (line_is_keyword(line, "fi")) {
            if (stop_mask & (1 << END_FI)) {
                end.type = END_FI;
                end.line = line;
                return end;
            }
        } else if (line_is_keyword(line, "done")) {
            if (stop_mask & (1 << END_DONE)) {
                end.type = END_DONE;
                end.line = line;
                return end;
            }
        } else if (line_starts_with(line, "if", NULL)) {
            if (execute) {
                execute_if(term, ctx, lines, count, idx, line);
            } else {
                skip_if(term, ctx, lines, count, idx, line);
            }
        } else if (line_starts_with(line, "while", NULL)) {
            if (execute) {
                execute_while(term, ctx, lines, count, idx, line);
            } else {
                skip_while(term, ctx, lines, count, idx, line);
            }
        } else if (line_starts_with(line, "for", NULL)) {
            if (execute) {
                execute_for(term, ctx, lines, count, idx, line);
            } else {
                skip_for(term, ctx, lines, count, idx, line);
            }
        } else {
            if (execute) {
                execute_simple_line(term, ctx, line);
            }
        }
        free(line);
    }
    return end;
}

static int load_script_lines(vfs_file_t *file, script_lines_t *lines_out) {
    line_accum_t acc = {0};
    char buffer[128];
    while (1) {
        ssize_t read_bytes = vfs_read(file, buffer, sizeof(buffer));
        if (read_bytes < 0) {
            free(acc.buf);
            return 0;
        }
        if (read_bytes == 0) {
            break;
        }
        for (ssize_t i = 0; i < read_bytes; i++) {
            char c = buffer[i];
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                if (!line_accum_append(&acc, '\0')) {
                    free(acc.buf);
                    return 0;
                }
                char *line = strdup(acc.buf ? acc.buf : "");
                if (line == NULL || !script_lines_append(lines_out, line)) {
                    free(line);
                    free(acc.buf);
                    return 0;
                }
                acc.len = 0;
            } else {
                if (!line_accum_append(&acc, c)) {
                    free(acc.buf);
                    return 0;
                }
            }
        }
    }
    if (acc.len > 0 || lines_out->count == 0) {
        if (!line_accum_append(&acc, '\0')) {
            free(acc.buf);
            return 0;
        }
        char *line = strdup(acc.buf ? acc.buf : "");
        if (line == NULL || !script_lines_append(lines_out, line)) {
            free(line);
            free(acc.buf);
            return 0;
        }
    }
    free(acc.buf);
    return 1;
}

int cmd_run(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    if (argc < 2) {
        shell_error(term, "run: missing program name");
        return SHELL_EINVAL;
    }
    if (argc > 2) {
        shell_error(term, "run: too many arguments");
        return SHELL_EINVAL;
    }
    const char *path = argv[1];
    if (path == NULL) {
        return SHELL_ERR;
    }
    vfs_node_t *node = vfs_resolve_at(term->cwd, path);
    if (node == NULL) {
        shell_error(term, "run: %s: no such file or directory", path);
        return SHELL_ENOENT;
    }
    if (node->type != VFS_NODE_FILE) {
        shell_error(term, "run: %s: not a file", path);
        vfs_node_release(node);
        return SHELL_EINVAL;
    }
    vfs_file_t *file = vfs_open_node(node, VFS_O_READ);
    vfs_node_release(node);
    if (file == NULL) {
        shell_error(term, "run: %s: unable to open", path);
        return SHELL_ERR;
    }
    script_lines_t lines = {0};
    if (!load_script_lines(file, &lines)) {
        vfs_close(file);
        shell_error(term, "run: %s: read error", path);
        return SHELL_ERR;
    }
    vfs_close(file);
    script_ctx_t ctx = {0};
    size_t idx = 0;
    while (idx < lines.count) {
        char *line = strip_comments_and_trim(lines.lines[idx]);
        idx++;
        if (line == NULL) {
            break;
        }
        if (line[0] == '\0') {
            free(line);
            continue;
        }
        if (idx == 1 && line[0] == '#' && line[1] == '!') {
            free(line);
            continue;
        }
        if (line_starts_with(line, "if", NULL)) {
            execute_if(term, &ctx, lines.lines, lines.count, &idx, line);
        } else if (line_starts_with(line, "while", NULL)) {
            execute_while(term, &ctx, lines.lines, lines.count, &idx, line);
        } else if (line_starts_with(line, "for", NULL)) {
            execute_for(term, &ctx, lines.lines, lines.count, &idx, line);
        } else if (line_is_keyword(line, "else") || line_is_keyword(line, "fi") ||
                   line_is_keyword(line, "done") || line_starts_with(line, "elif", NULL)) {
            shell_error(term, "run: unexpected control keyword");
        } else {
            execute_simple_line(term, &ctx, line);
        }
        free(line);
    }
    ctx_free(&ctx);
    for (size_t i = 0; i < lines.count; i++) {
        free(lines.lines[i]);
    }
    free(lines.lines);
    return SHELL_OK;
}

