#include "terminal.h"
#include "debug_helper.h"
#include "login.h"
#include <string.h>

extern int nano_is_active(void);
extern int firstboot_is_active(void);
extern int passwd_is_active(void);

#ifdef ARDUINO
    #include "boot_splash.h"
    #include "vfs.h"
    #include <stdlib.h>
    extern void boot_tft_set_cursor(int16_t x, int16_t y);
    extern void boot_tft_set_text_size(uint8_t size);
    extern void boot_tft_set_text_color(uint16_t color, uint16_t bg);
    extern void boot_tft_print(const char *str);
    extern void boot_tft_fill_screen(uint16_t color);
    extern void boot_tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    extern void boot_tft_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    #define COLOR_BLACK   0x0000
    #define COLOR_WHITE  0xFFFF
    #define COLOR_GRAY   0xC618

    // external terminal state (defined in terminal.c)
    extern terminal_state terminals[max_windows];
    extern uint8_t selected_terminal;

    static uint16_t *image_cache_pixels[max_windows] = {0};
    static uint16_t image_cache_w[max_windows] = {0};
    static uint16_t image_cache_h[max_windows] = {0};
    static char image_cache_path[max_windows][256] = {{0}};
    static uint16_t *image_src_pixels[max_windows] = {0};
    static char image_src_path[max_windows][256] = {{0}};
    static uint16_t *fastfetch_tint_pixels[max_windows] = {0};
    static uint16_t fastfetch_tint_w[max_windows] = {0};
    static uint16_t fastfetch_tint_h[max_windows] = {0};
    static uint16_t fastfetch_tint_color[max_windows] = {0};

    static int terminal_index(terminal_state *term) {
        if (term == NULL) {
            return -1;
        }
        for (uint8_t i = 0; i < max_windows; i++) {
            if (&terminals[i] == term) {
                return (int)i;
            }
        }
        return -1;
    }

    static void tint_black_to_color(uint16_t *dst, const uint16_t *src,
                                    size_t count, uint16_t active_color) {
        if (dst == NULL || src == NULL) {
            return;
        }
        for (size_t i = 0; i < count; i++) {
            uint16_t pix = src[i];
            dst[i] = (pix == 0x0000) ? active_color : pix;
        }
    }

    void terminal_image_view_release(terminal_state *term) {
        int idx = terminal_index(term);
        if (idx < 0) {
            return;
        }
        if (image_cache_pixels[idx] != NULL) {
            free(image_cache_pixels[idx]);
            image_cache_pixels[idx] = NULL;
        }
        image_cache_w[idx] = 0;
        image_cache_h[idx] = 0;
        image_cache_path[idx][0] = '\0';
        if (image_src_pixels[idx] != NULL) {
            free(image_src_pixels[idx]);
            image_src_pixels[idx] = NULL;
        }
        image_src_path[idx][0] = '\0';
    }

    static uint16_t *load_rgb565_source_vfs(const char *path) {
        if (path == NULL || path[0] == '\0') {
            return NULL;
        }
        const int16_t src_w = 480;
        const int16_t src_h = 320;
        const size_t expected = (size_t)src_w * (size_t)src_h * 2;
        ssize_t file_size = vfs_size(path);
        if (file_size >= 0 && (size_t)file_size < expected) {
            return NULL;
        }
        vfs_file_t *file = vfs_open(path, VFS_O_READ);
        if (file == NULL) {
            return NULL;
        }
        uint16_t *src = (uint16_t*)malloc(expected);
        if (src == NULL) {
            vfs_close(file);
            return NULL;
        }
        const int16_t chunk_lines = 20;
        size_t row_bytes = (size_t)src_w * 2;
        int16_t y = 0;
        while (y < src_h) {
            int16_t lines = chunk_lines;
            if (y + lines > src_h) {
                lines = src_h - y;
            }
            size_t bytes = (size_t)lines * row_bytes;
            uint8_t *dest_bytes = (uint8_t*)src + (size_t)y * row_bytes;
            ssize_t read_bytes = vfs_read(file, dest_bytes, bytes);
            if (read_bytes != (ssize_t)bytes) {
                free(src);
                vfs_close(file);
                return NULL;
            }
            y += lines;
        }
        vfs_close(file);
        return src;
    }

    static uint16_t *scale_rgb565_from_source(const uint16_t *src, int16_t width, int16_t height) {
        if (src == NULL || width <= 0 || height <= 0) {
            return NULL;
        }
        const int16_t src_w = 480;
        const int16_t src_h = 320;
        uint16_t *dest = (uint16_t*)malloc((size_t)width * (size_t)height * 2);
        if (dest == NULL) {
            return NULL;
        }
        uint16_t *x_map = (uint16_t*)malloc((size_t)width * sizeof(uint16_t));
        uint16_t *dest_sy = (uint16_t*)malloc((size_t)height * sizeof(uint16_t));
        if (x_map == NULL || dest_sy == NULL) {
            if (x_map) free(x_map);
            if (dest_sy) free(dest_sy);
            free(dest);
            return NULL;
        }
        for (int16_t dx = 0; dx < width; dx++) {
            x_map[dx] = (uint16_t)((dx * src_w) / width);
        }
        for (int16_t dy = 0; dy < height; dy++) {
            dest_sy[dy] = (uint16_t)((dy * src_h) / height);
        }
        for (int16_t dy = 0; dy < height; dy++) {
            const uint16_t *src_row = src + (size_t)dest_sy[dy] * (size_t)src_w;
            uint16_t *out_row = dest + (size_t)dy * (size_t)width;
            for (int16_t dx = 0; dx < width; dx++) {
                out_row[dx] = src_row[x_map[dx]];
            }
        }
        free(x_map);
        free(dest_sy);
        return dest;
    }

    static uint16_t *load_rgb565_scaled_vfs(const char *path, int16_t width, int16_t height) {
        if (path == NULL || path[0] == '\0' || width <= 0 || height <= 0) {
            return NULL;
        }
        const int16_t src_w = 480;
        const int16_t src_h = 320;
        const size_t expected = (size_t)src_w * (size_t)src_h * 2;
        ssize_t file_size = vfs_size(path);
        if (file_size >= 0 && (size_t)file_size < expected) {
            return NULL;
        }
        
        vfs_file_t *file = vfs_open(path, VFS_O_READ);
        if (file == NULL) {
            return NULL;
        }
        
        size_t src_row_bytes = (size_t)src_w * 2;
        uint16_t *dest = (uint16_t*)malloc((size_t)width * (size_t)height * 2);
        if (dest == NULL) {
            vfs_close(file);
            return NULL;
        }

        if (width == src_w && height == src_h) {
            const int16_t chunk_lines = 20;
            int16_t y = 0;
            while (y < src_h) {
                int16_t lines = chunk_lines;
                if (y + lines > src_h) {
                    lines = src_h - y;
                }
                size_t bytes = (size_t)lines * src_row_bytes;
                uint8_t *dest_bytes = (uint8_t*)dest + (size_t)y * src_row_bytes;
                ssize_t read_bytes = vfs_read(file, dest_bytes, bytes);
                if (read_bytes != (ssize_t)bytes) {
                    free(dest);
                    vfs_close(file);
                    return NULL;
                }
                y += lines;
            }
            vfs_close(file);
            return dest;
        }

        const int16_t chunk_lines = 16;
        size_t chunk_bytes = (size_t)chunk_lines * src_row_bytes;
        uint8_t *src_chunk = (uint8_t*)malloc(chunk_bytes);
        uint16_t *x_map = (uint16_t*)malloc((size_t)width * sizeof(uint16_t));
        uint16_t *dest_sy = (uint16_t*)malloc((size_t)height * sizeof(uint16_t));
        if (src_chunk == NULL || x_map == NULL || dest_sy == NULL) {
            if (src_chunk) free(src_chunk);
            if (x_map) free(x_map);
            if (dest_sy) free(dest_sy);
            free(dest);
            vfs_close(file);
            return NULL;
        }

        for (int16_t dx = 0; dx < width; dx++) {
            x_map[dx] = (uint16_t)((dx * src_w) / width);
        }
        for (int16_t dy = 0; dy < height; dy++) {
            dest_sy[dy] = (uint16_t)((dy * src_h) / height);
        }
        
        for (int16_t sy_base = 0; sy_base < src_h; sy_base += chunk_lines) {
            int16_t lines = chunk_lines;
            if (sy_base + lines > src_h) {
                lines = src_h - sy_base;
            }
            size_t bytes = (size_t)lines * src_row_bytes;
            ssize_t read_bytes = vfs_read(file, src_chunk, bytes);
            if (read_bytes != (ssize_t)bytes) {
                free(src_chunk);
                free(x_map);
                free(dest_sy);
                free(dest);
                vfs_close(file);
                return NULL;
            }
            
            for (int16_t dy = 0; dy < height; dy++) {
                int16_t sy = (int16_t)dest_sy[dy];
                if (sy < sy_base || sy >= sy_base + lines) {
                    continue;
                }
                uint8_t *src_row = src_chunk + (size_t)(sy - sy_base) * src_row_bytes;
                uint16_t *out_row = dest + (size_t)dy * (size_t)width;
                for (int16_t dx = 0; dx < width; dx++) {
                    size_t idx = (size_t)x_map[dx] * 2;
                    uint16_t pix = (uint16_t)src_row[idx] | ((uint16_t)src_row[idx + 1] << 8);
                    out_row[dx] = pix;
                }
            }
        }
        
        free(src_chunk);
        free(x_map);
        free(dest_sy);
        vfs_close(file);
        return dest;
    }

    // render all active terminal windows
    // clears screen first to ensure clean rendering
    void terminal_render_all(void) {
        uint16_t active_color = terminal_get_active_color();
        // clear screen first (background stays black)
        boot_tft_fill_screen(COLOR_WHITE);
        
        // render each active terminal
        for (uint8_t i = 0; i < max_windows; i++) {
            if (terminals[i].active) {
                terminal_render_window(&terminals[i]);
            }
        }
    }

    // render only a specific terminal (for incremental updates)
    void terminal_render_window_only(terminal_state *term) {
        if (term == NULL || !term->active) return;
        terminal_render_window(term);
    }

    // render a single terminal window
    void terminal_render_window(terminal_state *term) {
        if (term == NULL || !term->active) return;
        
        int16_t border = 2;
        int16_t padding = 3;
        uint16_t active_color = terminal_get_active_color();
        int16_t font_size = terminal_get_zoom();
        if (font_size < 1) font_size = 1;
        int16_t char_width = 6 * font_size;
        int16_t char_height = 8 * font_size;
        
        // draw window border (highlight selected without new color)
        uint16_t border_color = active_color;
        for (int i = 0; i < border; i++) {
            boot_tft_draw_rect(term->x + i, term->y + i, 
                              term->width - (i * 2), 
                              term->height - (i * 2), 
                              border_color);
        }
        // selection is indicated by cursor only
        
        // fill window background
        boot_tft_fill_rect(term->x + border, term->y + border, 
                          term->width - (border * 2), 
                          term->height - (border * 2), 
                          COLOR_WHITE);
        
        int idx = terminal_index(term);
        if (idx >= 0 && !term->fastfetch_image_active && fastfetch_tint_pixels[idx] != NULL) {
            free(fastfetch_tint_pixels[idx]);
            fastfetch_tint_pixels[idx] = NULL;
            fastfetch_tint_w[idx] = 0;
            fastfetch_tint_h[idx] = 0;
            fastfetch_tint_color[idx] = 0;
        }
        
        if (term->image_view_active && term->image_view_path[0] != '\0') {
            int16_t image_x = term->x + border;
            int16_t image_y = term->y + border;
            int16_t image_w = term->width - (border * 2);
            int16_t image_h = term->height - (border * 2);
            if (image_w > 0 && image_h > 0) {
                if (idx >= 0) {
                    uint8_t path_match = (image_cache_path[idx][0] != '\0' &&
                                          strcmp(image_cache_path[idx], term->image_view_path) == 0);
                    if (image_cache_pixels[idx] == NULL ||
                        image_cache_w[idx] != (uint16_t)image_w ||
                        image_cache_h[idx] != (uint16_t)image_h ||
                        !path_match) {
                        if (image_cache_pixels[idx] != NULL) {
                            free(image_cache_pixels[idx]);
                            image_cache_pixels[idx] = NULL;
                        }
                        image_cache_w[idx] = 0;
                        image_cache_h[idx] = 0;
                        image_cache_path[idx][0] = '\0';
                        if (!path_match && image_src_pixels[idx] != NULL) {
                            free(image_src_pixels[idx]);
                            image_src_pixels[idx] = NULL;
                            image_src_path[idx][0] = '\0';
                        }
                        if (image_src_pixels[idx] == NULL) {
                            image_src_pixels[idx] = load_rgb565_source_vfs(term->image_view_path);
                            if (image_src_pixels[idx] != NULL) {
                                strncpy(image_src_path[idx], term->image_view_path,
                                        sizeof(image_src_path[idx]) - 1);
                                image_src_path[idx][sizeof(image_src_path[idx]) - 1] = '\0';
                            }
                        }
                        if (image_src_pixels[idx] != NULL) {
                            image_cache_pixels[idx] = scale_rgb565_from_source(
                                image_src_pixels[idx], image_w, image_h);
                        } else {
                            image_cache_pixels[idx] = load_rgb565_scaled_vfs(
                                term->image_view_path, image_w, image_h);
                        }
                        if (image_cache_pixels[idx] != NULL) {
                            image_cache_w[idx] = (uint16_t)image_w;
                            image_cache_h[idx] = (uint16_t)image_h;
                            strncpy(image_cache_path[idx], term->image_view_path,
                                    sizeof(image_cache_path[idx]) - 1);
                            image_cache_path[idx][sizeof(image_cache_path[idx]) - 1] = '\0';
                        }
                    }
                    if (image_cache_pixels[idx] != NULL) {
                        boot_tft_draw_rgb565(image_x, image_y, image_cache_pixels[idx],
                                             (int16_t)image_cache_w[idx],
                                             (int16_t)image_cache_h[idx]);
                    } else {
                        boot_draw_rgb565_scaled(term->image_view_path, image_x, image_y, image_w, image_h);
                    }
                } else {
                    boot_draw_rgb565_scaled(term->image_view_path, image_x, image_y, image_w, image_h);
                }
            }
            return;
        }
        
        int16_t image_cols = 0;
        const int16_t image_padding_cols = 2;
        if (!nano_is_active() &&
            term->fastfetch_image_active && term->fastfetch_image_pixels != NULL &&
            term->fastfetch_image_w > 0 && term->fastfetch_image_h > 0 &&
            term->fastfetch_line_count > 0) {
            image_cols = (term->fastfetch_image_w + char_width - 1) / char_width;
            image_cols += image_padding_cols;
        }
        
        // set text properties
        boot_tft_set_text_size(font_size);
        boot_tft_set_text_color(active_color, COLOR_WHITE);
        
        // calculate how many characters fit in window
        int16_t base_text_x = term->x + border + padding;
        int16_t text_y = term->y + border + padding;
        int16_t base_max_cols = (term->width - (border * 2) - (padding * 2)) / char_width;
        int16_t max_rows = (term->height - (border * 2) - (padding * 2)) / char_height;
        
        // render terminal buffer (simplified - just show visible portion)
        int16_t start_row = 0;
        if (!nano_is_active()) {
            if (term->cursor_row >= max_rows) {
                start_row = term->cursor_row - max_rows + 1;
            }
        }

            if (!nano_is_active() &&
                term->fastfetch_image_active && term->fastfetch_image_pixels != NULL &&
            term->fastfetch_image_w > 0 && term->fastfetch_image_h > 0 &&
            term->fastfetch_line_count > 0) {
            int16_t ff_start = term->fastfetch_start_row;
            int16_t ff_end = ff_start + term->fastfetch_line_count;
            if (ff_end > start_row && ff_start < start_row + max_rows) {
                int16_t visible_start = ff_start;
                if (visible_start < start_row) {
                    visible_start = start_row;
                }
                int16_t y_offset = (visible_start - start_row) * char_height;
                const uint16_t *src_pixels = term->fastfetch_image_pixels;
                uint16_t *draw_pixels = term->fastfetch_image_pixels;
                if (idx >= 0) {
                    size_t pixel_count = (size_t)term->fastfetch_image_w * (size_t)term->fastfetch_image_h;
                    if (fastfetch_tint_pixels[idx] == NULL ||
                        fastfetch_tint_w[idx] != term->fastfetch_image_w ||
                        fastfetch_tint_h[idx] != term->fastfetch_image_h) {
                        free(fastfetch_tint_pixels[idx]);
                        fastfetch_tint_pixels[idx] = (uint16_t*)malloc(pixel_count * sizeof(uint16_t));
                        fastfetch_tint_w[idx] = term->fastfetch_image_w;
                        fastfetch_tint_h[idx] = term->fastfetch_image_h;
                        fastfetch_tint_color[idx] = 0;
                    }
                    if (fastfetch_tint_pixels[idx] != NULL &&
                        fastfetch_tint_color[idx] != active_color) {
                        tint_black_to_color(fastfetch_tint_pixels[idx], src_pixels,
                                            pixel_count, active_color);
                        fastfetch_tint_color[idx] = active_color;
                    }
                    if (fastfetch_tint_pixels[idx] != NULL &&
                        fastfetch_tint_color[idx] == active_color) {
                        draw_pixels = fastfetch_tint_pixels[idx];
                    }
                }
                boot_tft_draw_rgb565(term->x + border + padding,
                                     term->y + border + padding + y_offset,
                                     draw_pixels,
                                     term->fastfetch_image_w,
                                     term->fastfetch_image_h);
            }
        }
        
        for (int16_t row = 0; row < max_rows && (start_row + row) < terminal_rows; row++) {
            int16_t y_pos = text_y + (row * char_height);
            int16_t current_row = start_row + row;
            uint8_t in_fastfetch = 0;
            if (term->fastfetch_image_active && image_cols > 0 &&
                term->fastfetch_line_count > 0) {
                uint8_t ff_start = term->fastfetch_start_row;
                uint8_t ff_end = ff_start + term->fastfetch_line_count;
                if (current_row >= ff_start && current_row < ff_end) {
                    in_fastfetch = 1;
                }
            }
            int16_t row_text_x = base_text_x + (in_fastfetch ? (image_cols * char_width) : 0);
            int16_t row_max_cols = base_max_cols;
            if (in_fastfetch && image_cols > 0) {
                if (row_max_cols > image_cols) {
                    row_max_cols -= image_cols;
                } else {
                    row_max_cols = 0;
                }
            }
            boot_tft_set_cursor(row_text_x, y_pos);
            
            // check if this is the current input line (where cursor is)
            if (!nano_is_active() && !firstboot_is_active() &&
                !passwd_is_active() && !login_is_active() &&
                current_row == term->cursor_row) {
                // render prompt + input_line for current input line
                // this ensures the current input line shows exactly what's being typed
                // first, clear the buffer positions for this line to remove old characters
                int16_t line_start = current_row * terminal_cols;
                for (int16_t col = 0; col < terminal_cols; col++) {
                    if (line_start + col < terminal_buffer_size) {
                        term->buffer[line_start + col] = ' ';
                    }
                }
                
                // now write the prompt and input_line to the buffer
                if (line_start + 0 < terminal_buffer_size) term->buffer[line_start + 0] = '$';
                if (line_start + 1 < terminal_buffer_size) term->buffer[line_start + 1] = ' ';
                for (int16_t i = 0; i < term->input_len && (2 + i) < terminal_cols; i++) {
                    int16_t buf_pos = line_start + 2 + i;
                    if (buf_pos < terminal_buffer_size) {
                        term->buffer[buf_pos] = term->input_line[i];
                    }
                }
                
                // render prompt + input, then autocomplete suffix in gray
                int16_t chars_to_show = row_max_cols;
                if (chars_to_show > terminal_cols) chars_to_show = terminal_cols;
                if (chars_to_show > 0) {
                    char base_line[terminal_cols + 1];
                    int16_t base_len = 0;
                    base_line[base_len++] = '$';
                    if (base_len < chars_to_show) {
                        base_line[base_len++] = ' ';
                    }
                    for (int16_t i = 0;
                         i < term->input_len && base_len < chars_to_show;
                         i++) {
                        char c = term->input_line[i];
                        if (c < 32 || c >= 127) c = ' ';
                        base_line[base_len++] = c;
                    }
                    base_line[base_len] = '\0';
                    
                    boot_tft_set_text_color(active_color, COLOR_WHITE);
                    boot_tft_print(base_line);
                    
                    int16_t suffix_len = 0;
                    if (!term->autocomplete_applied &&
                        term->input_pos == term->input_len &&
                        term->autocomplete_len > 0 &&
                        base_len < chars_to_show) {
                        int16_t max_suffix = chars_to_show - base_len;
                        suffix_len = term->autocomplete_len;
                        if (suffix_len > max_suffix) {
                            suffix_len = max_suffix;
                        }
                        if (suffix_len > 0) {
                            char suffix[terminal_cols + 1];
                            memcpy(suffix, term->autocomplete_suffix, (size_t)suffix_len);
                            suffix[suffix_len] = '\0';
                            boot_tft_set_cursor(row_text_x + (base_len * char_width), y_pos);
                            boot_tft_set_text_color(COLOR_GRAY, COLOR_WHITE);
                            boot_tft_print(suffix);
                            boot_tft_set_text_color(active_color, COLOR_WHITE);
                        }
                    }
                    
                    int16_t remaining = chars_to_show - base_len - suffix_len;
                    if (remaining > 0) {
                        char spaces[terminal_cols + 1];
                        if (remaining > terminal_cols) {
                            remaining = terminal_cols;
                        }
                        memset(spaces, ' ', (size_t)remaining);
                        spaces[remaining] = '\0';
                        boot_tft_set_cursor(row_text_x + ((base_len + suffix_len) * char_width), y_pos);
                        boot_tft_set_text_color(active_color, COLOR_WHITE);
                        boot_tft_print(spaces);
                    }
                }
            } else {
                // render normal buffer line
                char line[terminal_cols + 1];
                int16_t line_start = current_row * terminal_cols;
                int16_t chars_to_show = row_max_cols;
                if (chars_to_show > terminal_cols) chars_to_show = terminal_cols;
                
                for (int16_t col = 0; col < chars_to_show; col++) {
                    if (line_start + col < terminal_buffer_size) {
                        char c = term->buffer[line_start + col];
                        if (c < 32 || c >= 127) c = ' ';  // sanitize
                        line[col] = c;
                    } else {
                        line[col] = ' ';
                    }
                }
                line[chars_to_show] = '\0';
                
                // print line (truncate if needed)
                if (row_max_cols > 0) {
                    char display_line[row_max_cols + 1];
                    strncpy(display_line, line, row_max_cols);
                    display_line[row_max_cols] = '\0';
                    boot_tft_print(display_line);
                }
            }
        }
        
        // draw cursor at current position
        // for the input line, cursor should be after prompt "$ " (2 chars) plus input cursor
        int16_t cursor_col_display = term->cursor_col;
        
        // if cursor is on the current input line (within visible range), calculate based on input_pos
        if (!nano_is_active() && !firstboot_is_active() &&
            !passwd_is_active() && !login_is_active() &&
            term->cursor_row >= start_row && term->cursor_row < start_row + max_rows) {
            // for simplicity, if cursor_row is the last row or matches the input line, use input_pos
            cursor_col_display = 2 + term->input_pos;  // "$ " is 2 chars, then input_pos
        }
        if (term->fastfetch_image_active && image_cols > 0 &&
            term->fastfetch_line_count > 0) {
            uint8_t ff_start = term->fastfetch_start_row;
            uint8_t ff_end = ff_start + term->fastfetch_line_count;
            if (term->cursor_row >= ff_start && term->cursor_row < ff_end) {
                cursor_col_display += image_cols;
            }
        }
        
        if (term == &terminals[selected_terminal] && !login_is_active()) {
            int16_t cursor_x = base_text_x + (cursor_col_display * char_width);
            int16_t cursor_y = text_y + ((term->cursor_row - start_row) * char_height);
            if (cursor_y >= text_y && cursor_y < text_y + (max_rows * char_height) && 
                cursor_x >= base_text_x && cursor_x < base_text_x + (base_max_cols * char_width)) {
                // draw a thin underline cursor without overwriting the character
                boot_tft_fill_rect(cursor_x, cursor_y + char_height - 1, char_width, 1, active_color);
            }
        }
    }
#endif


