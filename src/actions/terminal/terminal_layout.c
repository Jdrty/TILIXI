#include "terminal.h"
#include "vfs.h"
#include "debug_helper.h"
#include <string.h>
#include <limits.h>

#ifdef ARDUINO
    #include "boot_splash.h"
    extern void boot_tft_fill_screen(uint16_t color);
    extern void boot_tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    extern int16_t boot_tft_get_width(void);
    extern int16_t boot_tft_get_height(void);
    #define COLOR_WHITE  0xFFFF
#endif

// external terminal state (defined in terminal.c)
extern terminal_state terminals[max_windows];
extern uint8_t active_terminal;
extern uint8_t window_count;
extern uint8_t selected_terminal;

#define MAX_HORIZONTAL_TERMINALS 4  // maximum terminals side-by-side (like Hyprland)

#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

// count how many terminals are horizontally aligned at a given Y position
static uint8_t UNUSED count_horizontal_terminals_at_y(int16_t y, int16_t tolerance) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < max_windows; i++) {
        if (terminals[i].active) {
            // check if terminal's Y position is within tolerance
            if (terminals[i].y >= (y - tolerance) && terminals[i].y <= (y + tolerance)) {
                count++;
            }
        }
    }
    return count;
}

// validate terminal geometry - check for large gaps or invalid positions
static int UNUSED validate_terminal_layout(void) {
#ifdef ARDUINO
    int16_t screen_width = boot_tft_get_width();
    int16_t screen_height = boot_tft_get_height();
    int16_t margin = 5;
    
    // some sanity checks (⌒_⌒;)
    // check each active terminal
    for (uint8_t i = 0; i < max_windows; i++) {
        if (!terminals[i].active) continue;
        
        terminal_state *term = &terminals[i];
        
        // is terminal within screen bounds?
        if (term->x < 0 || term->y < 0 || 
            term->x + term->width > screen_width ||
            term->y + term->height > screen_height) {
            DEBUG_PRINT("[SANITY] Terminal %d out of bounds!\n", i);
            return 0;  // invalid
        }
        
        // terminal has reasonable size?
        if (term->width < 50 || term->height < 50) {
            DEBUG_PRINT("[SANITY] Terminal %d too small! w=%d h=%d\n", i, term->width, term->height);
            return 0;  // invalid
        }
    }
    
    // check for large gaps (unfilled areas)
    // ensure terminals cover most of the screen
    // these checks are redundant, its a bandage for a bug that I'm clueless onto where it is
    int16_t total_area = 0;
    for (uint8_t i = 0; i < max_windows; i++) {
        if (terminals[i].active) {
            total_area += terminals[i].width * terminals[i].height;
        }
    }
    
    int16_t screen_area = (screen_width - margin * 2) * (screen_height - margin * 2);
    int16_t coverage = (total_area * 100) / screen_area;
    
    // if coverage is less than 70%, we likely have large gaps
    if (coverage < 70 && window_count > 1) {
        DEBUG_PRINT("[SANITY] Low coverage: %d%% (likely gaps)\n", coverage);
        return 0;  // invalid - large gaps detected
    }

#endif
    return 1;  // valid
}

static int set_cwd_from_passwd(terminal_state *term) {
    if (term == NULL) {
        return 0;
    }
    vfs_file_t *file = vfs_open("/etc/passwd", VFS_O_READ);
    if (file == NULL) {
        return 0;
    }
    char buf[128];
    ssize_t read_bytes = vfs_read(file, buf, sizeof(buf) - 1);
    vfs_close(file);
    if (read_bytes <= 0) {
        return 0;
    }
    buf[read_bytes] = '\0';
    char *newline = strchr(buf, '\n');
    if (newline) {
        *newline = '\0';
    }
    char *colon = strchr(buf, ':');
    if (colon) {
        *colon = '\0';
    }
    if (buf[0] == '\0') {
        return 0;
    }
    char path[256];
    snprintf(path, sizeof(path), "/home/%s", buf);
    vfs_node_t *home = vfs_resolve(path);
    if (home == NULL || home->type != VFS_NODE_DIR) {
        if (home != NULL) {
            vfs_node_release(home);
        }
        return 0;
    }
    if (term->cwd != NULL) {
        vfs_node_release(term->cwd);
    }
    term->cwd = home;
    return 1;
}

void new_terminal(void) {
    if (window_count >= max_windows) {
        DEBUG_PRINT("[LIMIT] Max terminal count\n");
        return;
    }
    
    // find free terminal slot
    uint8_t new_idx = max_windows;
    for (uint8_t i = 0; i < max_windows; i++) {
        if (!terminals[i].active) {
            new_idx = i;
            break;
        }
    }
    
    if (new_idx >= max_windows) {
        DEBUG_PRINT("[ERROR] No free terminal slot\n");
        return;
    }
    
    terminal_state *new_term = &terminals[new_idx];
    
    // initialize terminal state
    new_term->active = 1;
    new_term->cursor_row = 0;
    new_term->cursor_col = 0;
    new_term->input_pos = 0;
    new_term->input_len = 0;
    new_term->history_count = 0;
    new_term->history_pos = 0;
    new_term->pipe_input = NULL;
    new_term->pipe_input_len = 0;
    new_term->fastfetch_image_active = 0;
    new_term->fastfetch_image_path[0] = '\0';
    new_term->fastfetch_image_pixels = NULL;
    new_term->fastfetch_image_w = 0;
    new_term->fastfetch_image_h = 0;
    new_term->fastfetch_start_row = 0;
    new_term->fastfetch_line_count = 0;
    
    // set initial working directory to root, then try user home from /etc/passwd
    new_term->cwd = vfs_resolve("/");
    set_cwd_from_passwd(new_term);
    memset(new_term->buffer, ' ', terminal_buffer_size);
    memset(new_term->input_line, 0, terminal_cols);
    
#ifdef ARDUINO
    int16_t screen_width = boot_tft_get_width();
    int16_t screen_height = boot_tft_get_height();
    int16_t margin = 5;
    int16_t border = 2;
    
    // variables for rendering (need to be accessible in rendering block)
    int16_t orig_x = 0, orig_y = 0, orig_width = 0, orig_height = 0;
    terminal_state *selected = NULL;
    
    if (window_count == 0) {
        // first terminal: fullscreen
        new_term->x = margin;
        new_term->y = margin;
        new_term->width = screen_width - (margin * 2);
        new_term->height = screen_height - (margin * 2);
        new_term->split_dir = SPLIT_NONE;
    } else {
        // split the selected terminal (last opened one)
        selected = &terminals[selected_terminal];
        if (!selected->active) {
            // fallback: find any active terminal
            for (uint8_t i = 0; i < max_windows; i++) {
                if (terminals[i].active) {
                    selected = &terminals[i];
                    selected_terminal = i;
                    break;
                }
            }
        }
        
        // save original geometry for clearing
        orig_x = selected->x;
        orig_y = selected->y;
        orig_width = selected->width;
        orig_height = selected->height;
        
        // determine split direction:
        // 1. If we already have 4 terminals horizontally at this Y position, force vertical split
        // 2. Otherwise, if width > height, split vertically; else horizontally
        // i should make a better algorithim later since this can end up producing some pretty weird terminal configs TODO
        uint8_t horizontal_count = count_horizontal_terminals_at_y(selected->y, 5);
        split_direction_t split_dir;
        
        if (horizontal_count >= MAX_HORIZONTAL_TERMINALS) {
            // already at max horizontal terminals - must split vertically
            split_dir = SPLIT_HORIZONTAL;
            DEBUG_PRINT("[SPLIT] Max horizontal terminals reached (%d), forcing vertical split\n", horizontal_count);
        } else {
            // normal split logic: if width > height, split vertically; else horizontally
            split_dir = (selected->width > selected->height) ? SPLIT_VERTICAL : SPLIT_HORIZONTAL;
        }
        
        if (split_dir == SPLIT_VERTICAL) {
            // split left/right
            int16_t half_width = orig_width / 2 - border;
            
            // sanity check: ensure minimum width
            if (half_width < 50) {
                DEBUG_PRINT("[SANITY] Terminal too narrow for vertical split, forcing horizontal\n");
                split_dir = SPLIT_HORIZONTAL;
                // will fall through to horizontal split below
            } else {
                new_term->x = orig_x;
                new_term->y = orig_y;
                new_term->width = half_width;
                new_term->height = orig_height;
                new_term->split_dir = SPLIT_VERTICAL;
                
                // resize selected terminal to right half
                selected->x = orig_x + half_width + (border * 2);
                selected->width = half_width;
                selected->split_dir = SPLIT_VERTICAL;
            }
        }
        
        // handle horizontal split (either originally or as fallback from vertical)
        if (split_dir == SPLIT_HORIZONTAL) {
            // split top/bottom
            int16_t half_height = orig_height / 2 - border;
            
            // sanity check: ensure minimum height
            if (half_height < 50) {
                DEBUG_PRINT("[SANITY] Terminal too short for any split, aborting\n");
                // can't split - terminal too small, clean up
                new_term->active = 0;
                memset(new_term->buffer, ' ', terminal_buffer_size);
                memset(new_term->input_line, 0, terminal_cols);
                return;  // exit early, window_count not incremented yet
            }
            
            new_term->x = orig_x;
            new_term->y = orig_y;
            new_term->width = orig_width;
            new_term->height = half_height;
            new_term->split_dir = SPLIT_HORIZONTAL;
            
            // resize selected terminal to bottom half
            selected->y = orig_y + half_height + (border * 2);
            selected->height = half_height;
            selected->split_dir = SPLIT_HORIZONTAL;
        }
    }
#else
    // PC: no geometry needed
    new_term->x = 0;
    new_term->y = 0;
    new_term->width = 0;
    new_term->height = 0;
    new_term->split_dir = SPLIT_NONE;
#endif
    
    active_terminal = new_idx;
    selected_terminal = new_idx;  // newly opened terminal becomes selected
    window_count++;
    
    // write initial message (need to include terminal_io for this)
    extern void terminal_write_line(terminal_state *term, const char *str);
    extern void terminal_write_string(terminal_state *term, const char *str);
    terminal_write_line(new_term, "TILIXI Terminal v1.0");
    terminal_write_string(new_term, "$ ");
    
#ifdef ARDUINO
    extern void terminal_render_all(void);
    extern void terminal_render_window(terminal_state *term);
    
    if (window_count == 1) {
        // first terminal: full screen render
        terminal_render_all();
    } else {
        // split case: clear the original area, then render both terminals
        // we saved the original geometry before modifying it
        boot_tft_fill_rect(orig_x, orig_y, orig_width, orig_height, COLOR_WHITE);
        
        // render both the resized selected terminal and the new one
        terminal_render_window(selected);
        terminal_render_window(new_term);
        
        // validate layout after split
        if (!validate_terminal_layout()) {
            DEBUG_PRINT("[SANITY] Layout validation failed after split, forcing full rebuild\n");
            terminal_render_all();
        }
    }
#endif
    
    DEBUG_PRINT("[ACTION] Terminal opened count is %d\n", window_count);
}

static void terminal_select_direction(int dx, int dy) {
    if (window_count == 0) {
        return;
    }
    
    terminal_state *current = &terminals[selected_terminal];
    int16_t left = current->x;
    int16_t right = current->x + current->width;
    int16_t top = current->y;
    int16_t bottom = current->y + current->height;
    
    int best_idx = -1;
    int best_overlap = -1;
    int best_primary = INT_MAX;
    int best_secondary = INT_MAX;
    
    for (uint8_t i = 0; i < max_windows; i++) {
        if (!terminals[i].active || i == selected_terminal) {
            continue;
        }
        terminal_state *cand = &terminals[i];
        int16_t c_left = cand->x;
        int16_t c_right = cand->x + cand->width;
        int16_t c_top = cand->y;
        int16_t c_bottom = cand->y + cand->height;
        
        int primary = INT_MAX;
        int secondary = INT_MAX;
        int overlap = 0;
        
        if (dx < 0) {
            if (c_right > left) {
                continue;
            }
            primary = left - c_right;
            overlap = !(c_bottom <= top || c_top >= bottom);
            if (!overlap) {
                secondary = (c_bottom <= top) ? (top - c_bottom) : (c_top - bottom);
            } else {
                secondary = 0;
            }
        } else if (dx > 0) {
            if (c_left < right) {
                continue;
            }
            primary = c_left - right;
            overlap = !(c_bottom <= top || c_top >= bottom);
            if (!overlap) {
                secondary = (c_bottom <= top) ? (top - c_bottom) : (c_top - bottom);
            } else {
                secondary = 0;
            }
        } else if (dy < 0) {
            if (c_bottom > top) {
                continue;
            }
            primary = top - c_bottom;
            overlap = !(c_right <= left || c_left >= right);
            if (!overlap) {
                secondary = (c_right <= left) ? (left - c_right) : (c_left - right);
            } else {
                secondary = 0;
            }
        } else if (dy > 0) {
            if (c_top < bottom) {
                continue;
            }
            primary = c_top - bottom;
            overlap = !(c_right <= left || c_left >= right);
            if (!overlap) {
                secondary = (c_right <= left) ? (left - c_right) : (c_left - right);
            } else {
                secondary = 0;
            }
        }
        
        if (primary < 0) {
            continue;
        }
        
        if (overlap > best_overlap ||
            (overlap == best_overlap && primary < best_primary) ||
            (overlap == best_overlap && primary == best_primary && secondary < best_secondary)) {
            best_overlap = overlap;
            best_primary = primary;
            best_secondary = secondary;
            best_idx = i;
        }
    }
    
    if (best_idx >= 0) {
        selected_terminal = best_idx;
        active_terminal = best_idx;
#ifdef ARDUINO
        extern void terminal_render_all(void);
        terminal_render_all();
#endif
    }
}

void terminal_select_left(void) { terminal_select_direction(-1, 0); }
void terminal_select_right(void) { terminal_select_direction(1, 0); }
void terminal_select_up(void) { terminal_select_direction(0, -1); }
void terminal_select_down(void) { terminal_select_direction(0, 1); }

void close_terminal(void) {
    if (window_count == 0) {
        DEBUG_PRINT("[ERROR] No terminals to close\n");
        return;
    }
    
    // close the selected terminal (last opened one)
    terminal_state *to_close = &terminals[selected_terminal];
    if (!to_close->active) {
        // fallback: close active_terminal
        to_close = &terminals[active_terminal];
        selected_terminal = active_terminal;
    }
    
    if (!to_close->active) {
        DEBUG_PRINT("[ERROR] Selected terminal not active\n");
        return;
    }
    
    // save geometry before closing for clearing
    int16_t closed_x = to_close->x;
    int16_t closed_y = to_close->y;
    int16_t closed_width = to_close->width;
    int16_t closed_height = to_close->height;
    
    to_close->active = 0;
    window_count--;
    
    // find next active terminal for selection
    uint8_t new_selected = max_windows;
    for (uint8_t i = 0; i < max_windows; i++) {
        if (terminals[i].active) {
            new_selected = i;
            break;
        }
    }
    
    if (new_selected < max_windows) {
        selected_terminal = new_selected;
        active_terminal = new_selected;
    } else {
        selected_terminal = 0;
        active_terminal = 0;
    }
    
#ifdef ARDUINO
    // rebuild layout for remaining terminals to fill gaps
    if (window_count > 0) {
        int16_t screen_width = boot_tft_get_width();
        int16_t screen_height = boot_tft_get_height();
        int16_t margin = 5;
        int16_t border = 2;
        
        // collect all active terminals
        uint8_t active_indices[max_windows];
        uint8_t active_count = 0;
        for (uint8_t i = 0; i < max_windows; i++) {
            if (terminals[i].active) {
                active_indices[active_count++] = i;
            }
        }
        
        if (active_count == 0) {
            // no terminals left
            return;
        } else if (active_count == 1) {
            // only one terminal left: make it fullscreen
            uint8_t idx = active_indices[0];
            terminals[idx].x = margin;
            terminals[idx].y = margin;
            terminals[idx].width = screen_width - (margin * 2);
            terminals[idx].height = screen_height - (margin * 2);
            terminals[idx].split_dir = SPLIT_NONE;
        } else {
            // multiple terminals: rebuild layout respecting max horizontal limit
            // calculate grid dimensions with max 4 columns
            uint8_t cols = active_count;
            if (cols > MAX_HORIZONTAL_TERMINALS) {
                cols = MAX_HORIZONTAL_TERMINALS;
            }
            uint8_t rows = (active_count + cols - 1) / cols;  // ceiling division
            
            int16_t cell_width = (screen_width - (margin * 2) - (border * (cols - 1))) / cols;
            int16_t cell_height = (screen_height - (margin * 2) - (border * (rows - 1))) / rows;
            
            // ensure minimum sizes
            if (cell_width < 50) cell_width = 50;
            if (cell_height < 50) cell_height = 50;
            
            // assign positions to active terminals
            for (uint8_t i = 0; i < active_count; i++) {
                uint8_t col = i % cols;
                uint8_t row = i / cols;
                uint8_t idx = active_indices[i];
                
                terminals[idx].x = margin + col * (cell_width + border);
                terminals[idx].y = margin + row * (cell_height + border);
                terminals[idx].width = cell_width;
                terminals[idx].height = cell_height;
                terminals[idx].split_dir = (cols > 1) ? SPLIT_VERTICAL : SPLIT_HORIZONTAL;
            }
        }
        
        // clear the closed terminal's area
        boot_tft_fill_rect(closed_x, closed_y, closed_width, closed_height, COLOR_WHITE);
        
        // validate layout after rebuilding
        extern void terminal_render_all(void);
        if (!validate_terminal_layout()) {
            DEBUG_PRINT("[SANITY] Layout validation failed after close, forcing full rebuild\n");
            // force full screen clear and rebuild
            boot_tft_fill_screen(COLOR_WHITE);
            terminal_render_all();
        } else {
            // redraw all remaining terminals (they've been repositioned)
            terminal_render_all();
        }
    } else {
        // no terminals left - clear screen
        boot_tft_fill_screen(COLOR_WHITE);
    }
#endif
    
    DEBUG_PRINT("[ACTION] Terminal closed count is %d\n", window_count);
}

