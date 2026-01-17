#include "builtins.h"
#include "terminal.h"
#include "vfs.h"
#include "shell_codes.h"
#include "shell_error.h"
#include <string.h>
#include <stdlib.h>

int cmd_wc(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_wc_def = {
    .name = "wc",
    .handler = cmd_wc,
    .help = "Count lines, words, and bytes"
};

static int is_whitespace(char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}

static int parse_flags(int argc, char **argv, int *show_lines, int *show_words,
                       int *show_bytes, int *first_path) {
    *show_lines = 0;
    *show_words = 0;
    *show_bytes = 0;
    *first_path = 1;
    
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (arg == NULL) {
            return SHELL_ERR;
        }
        if (arg[0] != '-') {
            *first_path = i;
            return SHELL_OK;
        }
        if (strcmp(arg, "--") == 0) {
            *first_path = i + 1;
            return SHELL_OK;
        }
        for (int j = 1; arg[j] != '\0'; j++) {
            if (arg[j] == 'l') {
                *show_lines = 1;
            } else if (arg[j] == 'w') {
                *show_words = 1;
            } else if (arg[j] == 'c') {
                *show_bytes = 1;
            } else {
                return SHELL_EINVAL;
            }
        }
    }
    
    *first_path = argc;
    return SHELL_OK;
}

static void format_counts(char *out, size_t out_size, size_t lines, size_t words,
                          size_t bytes, const char *path,
                          int show_lines, int show_words, int show_bytes) {
    int use_lines = show_lines || (!show_lines && !show_words && !show_bytes);
    int use_words = show_words || (!show_lines && !show_words && !show_bytes);
    int use_bytes = show_bytes || (!show_lines && !show_words && !show_bytes);
    
    char buffer[128];
    buffer[0] = '\0';
    if (use_lines) {
        char part[32];
        snprintf(part, sizeof(part), "%lu", (unsigned long)lines);
        strncat(buffer, part, sizeof(buffer) - strlen(buffer) - 1);
    }
    if (use_words) {
        char part[32];
        snprintf(part, sizeof(part), "%s%lu",
                 buffer[0] ? " " : "", (unsigned long)words);
        strncat(buffer, part, sizeof(buffer) - strlen(buffer) - 1);
    }
    if (use_bytes) {
        char part[32];
        snprintf(part, sizeof(part), "%s%lu",
                 buffer[0] ? " " : "", (unsigned long)bytes);
        strncat(buffer, part, sizeof(buffer) - strlen(buffer) - 1);
    }
    
    if (path != NULL) {
        snprintf(out, out_size, "%s %s\n", buffer, path);
    } else {
        snprintf(out, out_size, "%s\n", buffer);
    }
}

int cmd_wc(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    if (argc < 2) {
        shell_error(term, "wc: missing file operand");
        return SHELL_EINVAL;
    }
    
    int show_lines = 0;
    int show_words = 0;
    int show_bytes = 0;
    int first_path = 1;
    int parse_res = parse_flags(argc, argv, &show_lines, &show_words, &show_bytes, &first_path);
    if (parse_res == SHELL_EINVAL) {
        shell_error(term, "wc: invalid option");
        return SHELL_EINVAL;
    }
    if (parse_res != SHELL_OK) {
        return parse_res;
    }
    if (first_path >= argc) {
        if (term->pipe_input != NULL && term->pipe_input_len > 0) {
            size_t lines = 0;
            size_t words = 0;
            size_t bytes = term->pipe_input_len;
            int in_word = 0;
            for (size_t i = 0; i < term->pipe_input_len; i++) {
                char c = term->pipe_input[i];
                if (c == '\n') {
                    lines++;
                }
                if (is_whitespace(c)) {
                    in_word = 0;
                } else if (!in_word) {
                    in_word = 1;
                    words++;
                }
            }
            char line_out[128];
            format_counts(line_out, sizeof(line_out), lines, words, bytes, NULL,
                          show_lines, show_words, show_bytes);
            terminal_write_string(term, line_out);
            return SHELL_OK;
        }
        shell_error(term, "wc: missing file operand");
        return SHELL_EINVAL;
    }
    
    size_t total_lines = 0;
    size_t total_words = 0;
    size_t total_bytes = 0;
    int file_count = 0;
    
    char buffer[128];
    for (int i = first_path; i < argc; i++) {
        const char *path = argv[i];
        if (path == NULL) {
            return SHELL_ERR;
        }
        vfs_node_t *node = vfs_resolve_at(term->cwd, path);
        if (node == NULL) {
            shell_error(term, "wc: %s: no such file or directory", path);
            return SHELL_ENOENT;
        }
        if (node->type != VFS_NODE_FILE) {
            shell_error(term, "wc: %s: not a file", path);
            vfs_node_release(node);
            return SHELL_EINVAL;
        }
        
        vfs_file_t *file = vfs_open_node(node, VFS_O_READ);
        vfs_node_release(node);
        if (file == NULL) {
            shell_error(term, "wc: %s: unable to open", path);
            return SHELL_ERR;
        }
        
        size_t lines = 0;
        size_t words = 0;
        size_t bytes = 0;
        int in_word = 0;
        
        while (1) {
            ssize_t read_bytes = vfs_read(file, buffer, sizeof(buffer));
            if (read_bytes < 0) {
                vfs_close(file);
                shell_error(term, "wc: %s: read error", path);
                return SHELL_ERR;
            }
            if (read_bytes == 0) {
                break;
            }
            
            bytes += (size_t)read_bytes;
            for (ssize_t j = 0; j < read_bytes; j++) {
                char c = buffer[j];
                if (c == '\n') {
                    lines++;
                }
                if (is_whitespace(c)) {
                    in_word = 0;
                } else if (!in_word) {
                    in_word = 1;
                    words++;
                }
            }
        }
        
        vfs_close(file);
        
        total_lines += lines;
        total_words += words;
        total_bytes += bytes;
        file_count++;
        
        char line_out[128];
        format_counts(line_out, sizeof(line_out), lines, words, bytes, path,
                      show_lines, show_words, show_bytes);
        terminal_write_string(term, line_out);
    }
    
    if (file_count > 1) {
        char total_out[128];
        format_counts(total_out, sizeof(total_out), total_lines, total_words, total_bytes,
                      "total", show_lines, show_words, show_bytes);
        terminal_write_string(term, total_out);
    }
    
    return SHELL_OK;
}

