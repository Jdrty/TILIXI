#define _POSIX_C_SOURCE 200809L

#ifndef ARDUINO
#include <time.h>

static inline void delay_ms(unsigned int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
#endif

#include "boot_sequence.h"
#include "boot_splash.h"
#include "debug_helper.h"
#include <string.h>
#include <stdio.h>

#ifdef ARDUINO
    #include <Arduino.h>
    #include "ino_helper.h"
    
    void boot_tft_set_cursor(int16_t x, int16_t y);
    void boot_tft_set_text_size(uint8_t size);
    void boot_tft_set_text_color(uint16_t color, uint16_t bg);
    void boot_tft_print(const char *str);
    void boot_tft_fill_screen(uint16_t color);
    void boot_tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void boot_tft_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    int16_t boot_tft_get_width(void);
    int16_t boot_tft_get_height(void);
    
    // note: colors are inverted on this display
    #define COLOR_BLACK   0x0000  // appears white on display
    #define COLOR_WHITE  0xFFFF   // appears black on display
    #define COLOR_GREEN  0x07E0
    #define COLOR_RED    0xF800
#else
    #include <stdlib.h>
    #define COLOR_BLACK   0
    #define COLOR_WHITE   1
    #define COLOR_GREEN   2
    #define COLOR_RED     3
#endif

// terminal display constants
#define tty_margin_x 10
#define tty_margin_y 10
#define tty_padding 5
#define tty_font_size 2
#define tty_line_height 16
#define tty_prompt "$ "
#define tty_border_thickness 3

// render terminal interface on TFT display
void boot_render_terminal(void) {
#ifdef ARDUINO
    int16_t screen_width = boot_tft_get_width();
    int16_t screen_height = boot_tft_get_height();
    
    boot_tft_fill_screen(COLOR_WHITE);
    
    // draw thick terminal window border
    // draw multiple rectangles to make it thicker, there's probably a better way to do this
    // but i'm too lazy to read documentations ¯\_(ᐛ )_/¯
    for (int i = 0; i < tty_border_thickness; i++) {
        boot_tft_draw_rect(tty_margin_x + i, tty_margin_y + i, 
                          screen_width - (tty_margin_x * 2) - (i * 2), 
                          screen_height - (tty_margin_y * 2) - (i * 2), 
                          COLOR_BLACK);
    }
    
    // calculate terminal area
    int16_t term_x = tty_margin_x + tty_padding + tty_border_thickness;
    int16_t term_y = tty_margin_y + tty_padding + tty_border_thickness;
    int16_t term_width = screen_width - (tty_margin_x * 2) - (tty_padding * 2) - (tty_border_thickness * 2);
    int16_t term_height = screen_height - (tty_margin_y * 2) - (tty_padding * 2) - (tty_border_thickness * 2);
    
    boot_tft_fill_rect(term_x, term_y, term_width, term_height, COLOR_WHITE);
    
    // set text properties for terminal
    boot_tft_set_text_size(tty_font_size);
    boot_tft_set_text_color(COLOR_BLACK, COLOR_WHITE);
    
    // draw command prompt at top
    int16_t prompt_y = term_y + tty_line_height;
    boot_tft_set_cursor(term_x, prompt_y);
    boot_tft_print(tty_prompt);
    
    // draw cursor (underscore)
    int16_t cursor_x = term_x + (strlen(tty_prompt) * 6 * tty_font_size);  // approximate char width
    boot_tft_set_cursor(cursor_x, prompt_y);
    boot_tft_print("_");
    
    DEBUG_PRINT("[BOOT] Terminal interface rendered\n");
#endif
}

