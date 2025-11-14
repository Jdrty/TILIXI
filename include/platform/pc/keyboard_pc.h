#include <SDL2/SDL.h>
#include <unistd.h>
#include <termios.h>
#include "keyboard_core.h"
#include "platform/pc/keymap_pc.h"
#include <debug_helper.h>

void sdlread(void);
