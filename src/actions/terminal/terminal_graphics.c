#include "terminal.h"
#include "debug_helper.h"
#include <string.h>

extern int nano_is_active(void);
extern int firstboot_is_active(void);
extern int passwd_is_active(void);

#ifdef ARDUINO
    #include "boot_splash.h"
    extern void boot_tft_set_cursor(int16_t x, int16_t y);
    extern void boot_tft_set_text_size(uint8_t size);
    extern void boot_tft_set_text_color(uint16_t color, uint16_t bg);
    extern void boot_tft_print(const char *str);
    extern void boot_tft_fill_screen(uint16_t color);
    extern void boot_tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    extern void boot_tft_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    #define COLOR_BLACK   0x0000
    #define COLOR_WHITE  0xFFFF

    // external terminal state (defined in terminal.c)
    extern terminal_state terminals[max_windows];
    extern uint8_t selected_terminal;

    // render all active terminal windows
    // clears screen first to ensure clean rendering
    void terminal_render_all(void) {
        // clear screen first
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
        int16_t font_size = 1;
        int16_t char_width = 6 * font_size;
        int16_t char_height = 8 * font_size;
        
        // draw window border (highlight selected without new color)
        uint16_t border_color = COLOR_BLACK;
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
        
        int16_t image_cols = 0;
        const int16_t image_padding_cols = 2;
        if (term->fastfetch_image_active && term->fastfetch_image_pixels != NULL &&
            term->fastfetch_image_w > 0 && term->fastfetch_image_h > 0 &&
            term->fastfetch_line_count > 0) {
            image_cols = (term->fastfetch_image_w + char_width - 1) / char_width;
            image_cols += image_padding_cols;
        }
        
        // set text properties
        boot_tft_set_text_size(font_size);
        boot_tft_set_text_color(COLOR_BLACK, COLOR_WHITE);
        
        // calculate how many characters fit in window
        int16_t base_text_x = term->x + border + padding;
        int16_t text_y = term->y + border + padding;
        int16_t base_max_cols = (term->width - (border * 2) - (padding * 2)) / char_width;
        int16_t max_rows = (term->height - (border * 2) - (padding * 2)) / char_height;
        
        // render terminal buffer (simplified - just show visible portion)
        int16_t start_row = 0;
        if (term->cursor_row >= max_rows) {
            start_row = term->cursor_row - max_rows + 1;
        }

        if (term->fastfetch_image_active && term->fastfetch_image_pixels != NULL &&
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
                boot_tft_draw_rgb565(term->x + border + padding,
                                     term->y + border + padding + y_offset,
                                     term->fastfetch_image_pixels,
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
                !passwd_is_active() && current_row == term->cursor_row) {
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
                
                // render the line from buffer (which now has the correct content)
                char line[terminal_cols + 1];
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
                
                if (row_max_cols > 0) {
                    char display_line[row_max_cols + 1];
                    strncpy(display_line, line, row_max_cols);
                    display_line[row_max_cols] = '\0';
                    boot_tft_print(display_line);
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
            !passwd_is_active() &&
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
        
        if (term == &terminals[selected_terminal]) {
            int16_t cursor_x = base_text_x + (cursor_col_display * char_width);
            int16_t cursor_y = text_y + ((term->cursor_row - start_row) * char_height);
            if (cursor_y >= text_y && cursor_y < text_y + (max_rows * char_height) && 
                cursor_x >= base_text_x && cursor_x < base_text_x + (base_max_cols * char_width)) {
                // draw a thin underline cursor without overwriting the character
                boot_tft_fill_rect(cursor_x, cursor_y + char_height - 1, char_width, 1, COLOR_BLACK);
            }
        }
    }
#endif


