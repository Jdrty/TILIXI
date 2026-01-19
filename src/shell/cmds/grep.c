#include "builtins.h"
#include "terminal.h"
#include "vfs.h"
#include "shell_codes.h"
#include "shell_error.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

int cmd_grep(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_grep_def = {
    .name = "grep",
    .handler = cmd_grep,
    .help = "Search for PATTERN in files"
};

static int parse_flags(int argc, char **argv, int *ignore_case, int *invert,
                       int *show_line, int *first_pattern) {
    *ignore_case = 0;
    *invert = 0;
    *show_line = 0;
    *first_pattern = 1;
    
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (arg == NULL) {
            return SHELL_ERR;
        }
        if (arg[0] != '-') {
            *first_pattern = i;
            return SHELL_OK;
        }
        if (strcmp(arg, "--") == 0) {
            *first_pattern = i + 1;
            return SHELL_OK;
        }
        if (arg[1] == '\0') {
            *first_pattern = i;
            return SHELL_OK;
        }
        for (int j = 1; arg[j] != '\0'; j++) {
            if (arg[j] == 'i') {
                *ignore_case = 1;
            } else if (arg[j] == 'v') {
                *invert = 1;
            } else if (arg[j] == 'n') {
                *show_line = 1;
            } else {
                return SHELL_EINVAL;
            }
        }
    }
    
    *first_pattern = argc;
    return SHELL_OK;
}

static int line_contains(const char *line, size_t len, const char *pattern,
                         int ignore_case) {
    if (pattern == NULL) {
        return 0;
    }
    size_t pat_len = strlen(pattern);
    if (pat_len == 0) {
        return 1;
    }
    if (pat_len > len) {
        return 0;
    }
    for (size_t i = 0; i + pat_len <= len; i++) {
        int match = 1;
        for (size_t j = 0; j < pat_len; j++) {
            char a = line[i + j];
            char b = pattern[j];
            if (ignore_case) {
                a = (char)tolower((unsigned char)a);
                b = (char)tolower((unsigned char)b);
            }
            if (a != b) {
                match = 0;
                break;
            }
        }
        if (match) {
            return 1;
        }
    }
    return 0;
}

static void grep_output_line(terminal_state *term, const char *filename,
                             int show_filename, int show_line,
                             size_t line_no, const char *line, size_t len) {
    if (show_filename && filename != NULL) {
        terminal_write_string(term, filename);
        terminal_write_char(term, ':');
    }
    if (show_line) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lu:", (unsigned long)line_no);
        terminal_write_string(term, buf);
    }
    for (size_t i = 0; i < len; i++) {
        terminal_write_char(term, line[i]);
    }
    terminal_newline(term);
}

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    size_t line_no;
} line_accum_t;

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

static int grep_process_line(terminal_state *term, const char *line, size_t len,
                             const char *pattern, int ignore_case, int invert,
                             int show_line, const char *filename,
                             int show_filename, size_t line_no) {
    int match = line_contains(line, len, pattern, ignore_case);
    if (invert) {
        match = !match;
    }
    if (match) {
        grep_output_line(term, filename, show_filename, show_line, line_no, line, len);
    }
    return 1;
}

static int grep_process_stream(terminal_state *term, const char *pattern,
                               int ignore_case, int invert, int show_line,
                               const char *filename, int show_filename,
                               const char *data, size_t data_len,
                               vfs_file_t *file) {
    line_accum_t acc = {0};
    acc.line_no = 1;
    char buffer[128];
    
    while (1) {
        ssize_t read_bytes = 0;
        if (file != NULL) {
            read_bytes = vfs_read(file, buffer, sizeof(buffer));
            if (read_bytes < 0) {
                free(acc.buf);
                return 0;
            }
            if (read_bytes == 0) {
                break;
            }
        } else {
            if (data_len == 0) {
                break;
            }
            size_t chunk = data_len < sizeof(buffer) ? data_len : sizeof(buffer);
            memcpy(buffer, data, chunk);
            data += chunk;
            data_len -= chunk;
            read_bytes = (ssize_t)chunk;
        }
        
        for (ssize_t i = 0; i < read_bytes; i++) {
            char c = buffer[i];
            if (c == '\n') {
                if (!grep_process_line(term, acc.buf ? acc.buf : "",
                                       acc.len, pattern, ignore_case, invert,
                                       show_line, filename, show_filename, acc.line_no)) {
                    free(acc.buf);
                    return 0;
                }
                acc.len = 0;
                acc.line_no++;
            } else {
                if (!line_accum_append(&acc, c)) {
                    free(acc.buf);
                    return 0;
                }
            }
        }
    }
    
    if (acc.len > 0) {
        if (!grep_process_line(term, acc.buf ? acc.buf : "",
                               acc.len, pattern, ignore_case, invert,
                               show_line, filename, show_filename, acc.line_no)) {
            free(acc.buf);
            return 0;
        }
    }
    
    free(acc.buf);
    return 1;
}

int cmd_grep(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    if (argc < 2) {
        shell_error(term, "grep: missing pattern");
        return SHELL_EINVAL;
    }
    
    int ignore_case = 0;
    int invert = 0;
    int show_line = 0;
    int first_pattern = 1;
    int parse_res = parse_flags(argc, argv, &ignore_case, &invert,
                                &show_line, &first_pattern);
    if (parse_res == SHELL_EINVAL) {
        shell_error(term, "grep: invalid option");
        return SHELL_EINVAL;
    }
    if (parse_res != SHELL_OK) {
        return parse_res;
    }
    if (first_pattern >= argc || argv[first_pattern] == NULL) {
        shell_error(term, "grep: missing pattern");
        return SHELL_EINVAL;
    }
    
    const char *pattern = argv[first_pattern];
    int first_path = first_pattern + 1;
    
    if (first_path >= argc) {
        if (term->pipe_input != NULL && term->pipe_input_len > 0) {
            if (!grep_process_stream(term, pattern, ignore_case, invert, show_line,
                                     NULL, 0, term->pipe_input, term->pipe_input_len, NULL)) {
                shell_error(term, "grep: read error");
                return SHELL_ERR;
            }
            return SHELL_OK;
        }
        shell_error(term, "grep: missing file operand");
        return SHELL_EINVAL;
    }
    
    int file_count = argc - first_path;
    for (int i = first_path; i < argc; i++) {
        const char *path = argv[i];
        if (path == NULL) {
            return SHELL_ERR;
        }
        vfs_node_t *node = vfs_resolve_at(term->cwd, path);
        if (node == NULL) {
            shell_error(term, "grep: %s: no such file or directory", path);
            return SHELL_ENOENT;
        }
        if (node->type != VFS_NODE_FILE) {
            shell_error(term, "grep: %s: not a file", path);
            vfs_node_release(node);
            return SHELL_EINVAL;
        }
        
        vfs_file_t *file = vfs_open_node(node, VFS_O_READ);
        vfs_node_release(node);
        if (file == NULL) {
            shell_error(term, "grep: %s: unable to open", path);
            return SHELL_ERR;
        }
        
        int ok = grep_process_stream(term, pattern, ignore_case, invert, show_line,
                                     path, file_count > 1, NULL, 0, file);
        vfs_close(file);
        if (!ok) {
            shell_error(term, "grep: %s: read error", path);
            return SHELL_ERR;
        }
    }
    
    return SHELL_OK;
}







