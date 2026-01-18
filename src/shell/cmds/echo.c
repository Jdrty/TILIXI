#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"
#include <ctype.h>

int cmd_echo(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_echo_def = {
    .name = "echo",
    .handler = cmd_echo,
    .help = "Echo arguments"
};

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

static const char *parse_octal(const char *p, int *out) {
    int value = 0;
    int count = 0;
    while (*p >= '0' && *p <= '7' && count < 3) {
        value = (value * 8) + (*p - '0');
        p++;
        count++;
    }
    *out = value;
    return p;
}

static void terminal_write_escaped(terminal_state *term, const char *text,
                                   int *stop_output, int *print_newline) {
    const char *p = text;
    while (*p != '\0') {
        if (*p != '\\') {
            terminal_write_char(term, *p++);
            continue;
        }
        p++;
        if (*p == '\0') {
            terminal_write_char(term, '\\');
            break;
        }
        switch (*p) {
            case 'a': terminal_write_char(term, '\a'); p++; break;
            case 'b': terminal_write_char(term, '\b'); p++; break;
            case 'f': terminal_write_char(term, '\f'); p++; break;
            case 'n': terminal_write_char(term, '\n'); p++; break;
            case 'r': terminal_write_char(term, '\r'); p++; break;
            case 't': terminal_write_char(term, '\t'); p++; break;
            case 'v': terminal_write_char(term, '\v'); p++; break;
            case '\\': terminal_write_char(term, '\\'); p++; break;
            case 'c':
                *stop_output = 1;
                *print_newline = 0;
                return;
            case '0': {
                int value = 0;
                const char *next = parse_octal(p, &value);
                terminal_write_char(term, (char)value);
                p = next;
                break;
            }
            case 'x': {
                int value = 0;
                int digits = 0;
                p++;
                while (digits < 2) {
                    int hv = hex_value(*p);
                    if (hv < 0) {
                        break;
                    }
                    value = (value << 4) | hv;
                    digits++;
                    p++;
                }
                if (digits == 0) {
                    terminal_write_char(term, 'x');
                } else {
                    terminal_write_char(term, (char)value);
                }
                break;
            }
            default:
                terminal_write_char(term, *p);
                p++;
                break;
        }
    }
}

int cmd_echo(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    int print_newline = 1;
    int interpret_escapes = 0;
    int arg_start = 1;
    
    while (arg_start < argc && argv[arg_start] != NULL) {
        const char *opt = argv[arg_start];
        if (opt[0] != '-' || opt[1] == '\0') {
            break;
        }
        if (strcmp(opt, "--") == 0) {
            arg_start++;
            break;
        }
        int valid = 1;
        for (const char *c = opt + 1; *c != '\0'; c++) {
            if (*c == 'n') {
                print_newline = 0;
            } else if (*c == 'e') {
                interpret_escapes = 1;
            } else if (*c == 'E') {
                interpret_escapes = 0;
            } else {
                valid = 0;
                break;
            }
        }
        if (!valid) {
            break;
        }
        arg_start++;
    }
    
    int stop_output = 0;
    for (int i = arg_start; i < argc; i++) {
        if (argv[i] != NULL) {
            if (interpret_escapes) {
                terminal_write_escaped(term, argv[i], &stop_output, &print_newline);
                if (stop_output) {
                    break;
                }
            } else {
                terminal_write_string(term, argv[i]);
            }
            if (i < argc - 1 && !stop_output) {
                terminal_write_char(term, ' ');
            }
        }
    }
    if (print_newline) {
        terminal_newline(term);
    }
    return SHELL_OK;
}

