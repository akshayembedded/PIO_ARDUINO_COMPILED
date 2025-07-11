#ifndef PIEZO32_H
#define PIEZO32_H

#include "Global/global.h"
#include <stdbool.h>

// Structure to represent a tone
typedef struct {
    int frequency;
    int duration;
    int pause;
    int repetitions;
} Tone;

// C Interface Functions
bool piezo_init_c(void);
bool piezo_play_tone_c(int freq, int duration, bool blocking= false);
bool piezo_play_sequence_c(Tone* tone, bool blocking= false);
bool piezo_play_music_c(const char* music_str, bool blocking= false);
void piezo_force_stop_c(void);
void piezo_set_speed_c(int new_speed);
// Module registration function for use with require()
int luaopen_piezo32(lua_State* L);

// Function to register the piezo module globally
// Call this during Lua state initialization
// void register_piezo32_global(lua_State* L);

#endif // PIEZO32_H
