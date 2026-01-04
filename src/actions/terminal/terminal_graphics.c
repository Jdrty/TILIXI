#include "terminal.h"
#include "debug_helper.h"
#include <string.h>

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
        
        // draw window border
        for (int i = 0; i < border; i++) {
            boot_tft_draw_rect(term->x + i, term->y + i, 
                              term->width - (i * 2), 
                              term->height - (i * 2), 
                              COLOR_BLACK);
        }
        
        // fill window background
        boot_tft_fill_rect(term->x + border, term->y + border, 
                          term->width - (border * 2), 
                          term->height - (border * 2), 
                          COLOR_WHITE);
        
        // set text properties
        boot_tft_set_text_size(font_size);
        boot_tft_set_text_color(COLOR_BLACK, COLOR_WHITE);
        
        // calculate how many characters fit in window
        int16_t text_x = term->x + border + padding;
        int16_t text_y = term->y + border + padding;
        int16_t max_cols = (term->width - (border * 2) - (padding * 2)) / char_width;
        int16_t max_rows = (term->height - (border * 2) - (padding * 2)) / char_height;
        
        // render terminal buffer (simplified - just show visible portion)
        int16_t start_row = 0;
        if (term->cursor_row >= max_rows) {
            start_row = term->cursor_row - max_rows + 1;
        }
        
        for (int16_t row = 0; row < max_rows && (start_row + row) < terminal_rows; row++) {
            int16_t y_pos = text_y + (row * char_height);
            boot_tft_set_cursor(text_x, y_pos);
            
            // render one line of text
            char line[terminal_cols + 1];
            int16_t line_start = (start_row + row) * terminal_cols;
            int16_t chars_to_show = max_cols;
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
            char display_line[max_cols + 1];
            strncpy(display_line, line, max_cols);
            display_line[max_cols] = '\0';
            boot_tft_print(display_line);
        }
        
        // draw cursor at current position
        int16_t cursor_x = text_x + (term->cursor_col * char_width);
        int16_t cursor_y = text_y + ((term->cursor_row - start_row) * char_height);
        if (cursor_y >= text_y && cursor_y < text_y + (max_rows * char_height)) {
            boot_tft_set_cursor(cursor_x, cursor_y);
            boot_tft_print("_");
        }
    }
#endif



