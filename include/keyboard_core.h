#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "action_manager.h"

// long list of keys ill use
typedef enum __attribute__((packed)) {
    key_none,
    key_esc,
    key_tilde,
    key_one,
    key_two,
    key_three,
    key_four,
    key_five,
    key_six,
    key_seven,
    key_eight,
    key_nine,
    key_zero,
    key_dash,
    key_equals,
    key_backspace,
    key_tab,
    key_q,
    key_w,
    key_e,
    key_r,
    key_tee,
    key_y,
    key_u,
    key_i,
    key_o,
    key_p,
    key_openbracket,
    key_closebracket,
    key_backslash,
    key_capslock,
    key_a,
    key_s,
    key_d,
    key_f,
    key_g,
    key_h,
    key_j,
    key_k,
    key_l,
    key_colon,
    key_quote,
    key_enter,
    key_z,
    key_x,
    key_c,
    key_v,
    key_b,
    key_n,
    key_m,
    key_comma,
    key_period,
    key_slash,
    key_space,
    key_left,
    key_right,
    key_up,
    key_down
} key_code;

// key and modifier combo to be defined
typedef struct __attribute__((packed)) {
    key_code key;
    uint8_t modifiers;  // will be bitmask for easier use
} key_event;

// modifications to the byte for difference modifier keys
#define mod_shift (1 << 0)
#define mod_ctrl  (1 << 1)
#define mod_super (1 << 2)

// what kind of events can exist
typedef enum __attribute__ ((packed)) {
    event_none,
    event_keypressed,
    event_hotkey
} event_type;

// define event characteristics
typedef struct {
    event_type event;
    key_code key;
    uint8_t modifiers;
    const char *action;
} keyboard_event;

// functions
void process(key_event evt);
keyboard_event get_next_event(void);
void register_key(uint8_t modifiers, key_code key, const char *action);
