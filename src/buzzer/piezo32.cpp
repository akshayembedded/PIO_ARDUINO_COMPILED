#include "piezo32.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#ifndef PIEZO_PIN
#define PIEZO_PIN 47 // Default GPIO pin for piezo speaker
#endif
#ifndef PIEZO_CHANNEL
#define PIEZO_CHANNEL 4 // Default LEDC channel
#endif

#define PWM_RESOLUTION 8     // PWM resolution (8-bit: 0-255)
#define DEFAULT_DUTY 127     // Default duty cycle (50% of 255 for 8-bit)
#define TASK_STACK_SIZE 2048 // Stack size for background task
#define QUEUE_SIZE 10        // Size of the command queue

// Music note frequencies
#include "notes.h" // Using the provided notes.h

// Command types for task queue
typedef enum
{
    CMD_PLAY,
    CMD_STOP,
    CMD_SUCCESS,
    CMD_ERROR,
    CMD_MUSIC,
    CMD_FORCE_STOP
} CommandType;

// Command execution mode
typedef enum
{
    MODE_BACKGROUND, // Return immediately
    MODE_BLOCKING    // Wait for completion
} ExecutionMode;

// Structure for command queue
typedef struct
{
    CommandType type;
    Tone tone;
    char *music;                      // For storing music string
    ExecutionMode mode;               // Execution mode
    SemaphoreHandle_t completion_sem; // For blocking mode
} Command;

// Global state
static TaskHandle_t piezo_task_handle = NULL;
static QueueHandle_t command_queue = NULL;
static lua_State *L_global = NULL;
static int callback_ref_step = LUA_NOREF;
static int callback_ref_done = LUA_NOREF;
static bool is_playing = false;
static bool is_setup = false;
static int speed = 80; // ms i think

// Forward declarations
static void piezo_task(void *pvParameters);
static int l_piezo_play(lua_State *L);
static int l_piezo_stop(lua_State *L);
static int l_piezo_success(lua_State *L);
static int l_piezo_error(lua_State *L);
static int l_piezo_play_music(lua_State *L);
static int l_piezo_is_playing(lua_State *L);
static int l_piezo_set_callback(lua_State *L);
static int l_piezo_set_speed(lua_State *L);
static int l_piezo_force_stop(lua_State *L);
static void play_tone(int frequency, int duration);
static void play_sequence(Tone *tone);
static void play_music(const char *music_str);

// C Interface Implementation
bool piezo_init_c(void)
{
    if (is_setup)
    {
        return true; // Already initialized
    }

    // Initialize LEDC for PWM
    
    // ledcSetup(PIEZO_CHANNEL, 5000, PWM_RESOLUTION);

    // ledcAttachPin(PIEZO_PIN, PIEZO_CHANNEL);
    ledcAttachChannel(PIEZO_PIN, 5000, PWM_RESOLUTION, PIEZO_CHANNEL );
    ledcWrite(PIEZO_PIN, 0);

    // Create command queue
    if (command_queue == NULL)
    {
        command_queue = xQueueCreate(QUEUE_SIZE, sizeof(Command));
        if (command_queue == NULL)
        {
            return false;
        }
    }

    // Create background task
    if (piezo_task_handle == NULL)
    {
        BaseType_t res = xTaskCreate(
            piezo_task,
            "piezo_task",
            TASK_STACK_SIZE,
            NULL,
            1,
            &piezo_task_handle);

        if (res != pdPASS)
        {
            return false;
        }
    }

    is_setup = true;
    return true;
}

bool piezo_play_tone_c(int freq, int duration, bool blocking)
{
    if (!is_setup && !piezo_init_c())
    {
        return false;
    }

    Tone tone = {
        .frequency = freq,
        .duration = duration,
        .pause = 0,
        .repetitions = 1};

    Command cmd = {
        .type = CMD_PLAY,
        .tone = tone,
        .music = NULL,
        .mode = blocking ? MODE_BLOCKING : MODE_BACKGROUND,
        .completion_sem = NULL};

    if (blocking)
    {
        cmd.completion_sem = xSemaphoreCreateBinary();
        if (cmd.completion_sem == NULL)
        {
            return false;
        }
    }

    if (xQueueSend(command_queue, &cmd, portMAX_DELAY) != pdPASS)
    {
        if (blocking && cmd.completion_sem != NULL)
        {
            vSemaphoreDelete(cmd.completion_sem);
        }
        return false;
    }

    if (blocking)
    {
        xSemaphoreTake(cmd.completion_sem, portMAX_DELAY);
        vSemaphoreDelete(cmd.completion_sem);
    }

    return true;
}

bool piezo_play_sequence_c(Tone *tone, bool blocking)
{
    if (!is_setup && !piezo_init_c())
    {
        return false;
    }

    Command cmd = {
        .type = CMD_PLAY,
        .tone = *tone,
        .music = NULL,
        .mode = blocking ? MODE_BLOCKING : MODE_BACKGROUND,
        .completion_sem = NULL};

    if (blocking)
    {
        cmd.completion_sem = xSemaphoreCreateBinary();
        if (cmd.completion_sem == NULL)
        {
            return false;
        }
    }

    if (xQueueSend(command_queue, &cmd, portMAX_DELAY) != pdPASS)
    {
        if (blocking && cmd.completion_sem != NULL)
        {
            vSemaphoreDelete(cmd.completion_sem);
        }
        return false;
    }

    if (blocking)
    {
        xSemaphoreTake(cmd.completion_sem, portMAX_DELAY);
        vSemaphoreDelete(cmd.completion_sem);
    }

    return true;
}

bool piezo_play_music_c(const char *music_str, bool blocking)
{
    if (!is_setup && !piezo_init_c())
    {
        return false;
    }

    if (strlen(music_str) < 2)
    {
        return false;
    }

    Command cmd = {
        .type = CMD_MUSIC,
        .music = strdup(music_str),
        .mode = blocking ? MODE_BLOCKING : MODE_BACKGROUND,
        .completion_sem = NULL};

    if (cmd.music == NULL)
    {
        return false;
    }

    if (blocking)
    {
        cmd.completion_sem = xSemaphoreCreateBinary();
        if (cmd.completion_sem == NULL)
        {
            free(cmd.music);
            return false;
        }
    }

    if (xQueueSend(command_queue, &cmd, portMAX_DELAY) != pdPASS)
    {
        free(cmd.music);
        if (blocking && cmd.completion_sem != NULL)
        {
            vSemaphoreDelete(cmd.completion_sem);
        }
        return false;
    }

    if (blocking)
    {
        xSemaphoreTake(cmd.completion_sem, portMAX_DELAY);
        vSemaphoreDelete(cmd.completion_sem);
    }

    return true;
}

void piezo_force_stop_c(void)
{
    if (!is_setup)
    {
        return;
    }

    Command cmd = {
        .type = CMD_FORCE_STOP,
        .mode = MODE_BLOCKING,
        .completion_sem = xSemaphoreCreateBinary()};

    if (cmd.completion_sem == NULL)
    {
        return;
    }

    xQueueSend(command_queue, &cmd, portMAX_DELAY);
    xSemaphoreTake(cmd.completion_sem, portMAX_DELAY);
    vSemaphoreDelete(cmd.completion_sem);
}

void piezo_set_speed_c(int new_speed)
{
    if (new_speed < 1)
    {
        new_speed = 1; // Minimum speed
    }
    speed = new_speed;
}

// Initialize the piezo module
static int l_piezo_init(lua_State *L)
{
    L_global = L;

    // Initialize piezo system using C interface
    if (!piezo_init_c())
    {
        return luaL_error(L, "Failed to initialize piezo system");
    }

    // Return the piezo table
    lua_newtable(L);

    // Register functions
    lua_pushcfunction(L, l_piezo_play);
    lua_setfield(L, -2, "play");

    lua_pushcfunction(L, l_piezo_stop);
    lua_setfield(L, -2, "stop");

    lua_pushcfunction(L, l_piezo_force_stop);
    lua_setfield(L, -2, "force_stop");

    lua_pushcfunction(L, l_piezo_success);
    lua_setfield(L, -2, "success");

    lua_pushcfunction(L, l_piezo_error);
    lua_setfield(L, -2, "error");

    lua_pushcfunction(L, l_piezo_play_music);
    lua_setfield(L, -2, "play_music");

    lua_pushcfunction(L, l_piezo_is_playing);
    lua_setfield(L, -2, "is_playing");

    lua_pushcfunction(L, l_piezo_set_callback);
    lua_setfield(L, -2, "set_callback");

    lua_pushcfunction(L, l_piezo_set_speed);
    lua_setfield(L, -2, "set_speed");
    // Create notes table
    lua_newtable(L);

    // Add all notes to the table
    // Example for a few notes - you can add more from notes.h
    lua_pushinteger(L, NOTE_C4);
    lua_setfield(L, -2, "C4");

    lua_pushinteger(L, NOTE_D4);
    lua_setfield(L, -2, "D4");

    lua_pushinteger(L, NOTE_E4);
    lua_setfield(L, -2, "E4");

    lua_pushinteger(L, NOTE_F4);
    lua_setfield(L, -2, "F4");

    lua_pushinteger(L, NOTE_G4);
    lua_setfield(L, -2, "G4");

    lua_pushinteger(L, NOTE_A4);
    lua_setfield(L, -2, "A4");

    lua_pushinteger(L, NOTE_B4);
    lua_setfield(L, -2, "B4");

    // Add all octaves
    for (int octave = 1; octave <= 8; octave++)
    {
        char note_name[4];

        sprintf(note_name, "C%d", octave);
        lua_pushinteger(L, NOTE_C1 * (1 << (octave - 1)));
        lua_setfield(L, -2, note_name);

        sprintf(note_name, "D%d", octave);
        lua_pushinteger(L, NOTE_D1 * (1 << (octave - 1)));
        lua_setfield(L, -2, note_name);

        sprintf(note_name, "E%d", octave);
        lua_pushinteger(L, NOTE_E1 * (1 << (octave - 1)));
        lua_setfield(L, -2, note_name);

        sprintf(note_name, "F%d", octave);
        lua_pushinteger(L, NOTE_F1 * (1 << (octave - 1)));
        lua_setfield(L, -2, note_name);

        sprintf(note_name, "G%d", octave);
        lua_pushinteger(L, NOTE_G1 * (1 << (octave - 1)));
        lua_setfield(L, -2, note_name);

        sprintf(note_name, "A%d", octave);
        lua_pushinteger(L, NOTE_A1 * (1 << (octave - 1)));
        lua_setfield(L, -2, note_name);

        sprintf(note_name, "B%d", octave);
        lua_pushinteger(L, NOTE_B1 * (1 << (octave - 1)));
        lua_setfield(L, -2, note_name);
    }

    // Add notes table to piezo table
    lua_setfield(L, -2, "notes");

    return 1;
}

// Background task for playing sounds
static void piezo_task(void *pvParameters)
{
    Command cmd;

    for (;;)
    {
        if (xQueueReceive(command_queue, &cmd, portMAX_DELAY))
        {
            is_playing = true;

            switch (cmd.type)
            {
            case CMD_FORCE_STOP:
                // Force stop overrides everything
                ledcWrite(PIEZO_PIN, 0);
                is_playing = false;
                if (cmd.mode == MODE_BLOCKING && cmd.completion_sem != NULL)
                {
                    xSemaphoreGive(cmd.completion_sem);
                }
                continue; // Skip normal completion handling

            case CMD_PLAY:
                play_sequence(&cmd.tone);
                break;

            case CMD_STOP:
                ledcWrite(PIEZO_CHANNEL, 0);
                break;

            case CMD_SUCCESS:
                play_sequence(&cmd.tone);
                break;

            case CMD_ERROR:
                play_sequence(&cmd.tone);
                break;

            case CMD_MUSIC:
                if (cmd.music != NULL)
                {
                    play_music(cmd.music);
                    free(cmd.music); // Free the allocated string
                }
                break;
            }

            is_playing = false;

            // Signal completion for blocking mode
            if (cmd.mode == MODE_BLOCKING && cmd.completion_sem != NULL)
            {
                xSemaphoreGive(cmd.completion_sem);
            }
        }
    }
}

// Play a single tone
static void play_tone(int frequency, int duration)
{
    // Call step callback with playing=true
    if (callback_ref_step != LUA_NOREF && L_global != NULL)
    {
        lua_rawgeti(L_global, LUA_REGISTRYINDEX, callback_ref_step);
        lua_newtable(L_global);
        lua_pushboolean(L_global, 1); // true
        lua_setfield(L_global, -2, "playing");
        lua_pushinteger(L_global, frequency);
        lua_setfield(L_global, -2, "freq");
        lua_pcall(L_global, 1, 0, 0);
    }

    // cheack if frequency is valid
    if (frequency <= 200 || frequency > 20000)
    {
        frequency = 3000; // Default frequency if invalid
    }

    // Play the tone
    ledcAttachChannel(PIEZO_PIN, frequency, PWM_RESOLUTION, PIEZO_CHANNEL);
    ledcWrite(PIEZO_CHANNEL, DEFAULT_DUTY / 3);

    // Wait for duration
    vTaskDelay(duration / portTICK_PERIOD_MS);

    // Turn off the tone
    ledcWrite(PIEZO_CHANNEL, 0);

    // Call step callback with playing=false
    if (callback_ref_step != LUA_NOREF && L_global != NULL)
    {
        lua_rawgeti(L_global, LUA_REGISTRYINDEX, callback_ref_step);
        lua_newtable(L_global);
        lua_pushboolean(L_global, 0); // false
        lua_setfield(L_global, -2, "playing");
        lua_pcall(L_global, 1, 0, 0);
    }
}

// Play a sequence of tones
static void play_sequence(Tone *tone)
{
    for (int i = 0; i < tone->repetitions; i++)
    {
        play_tone(tone->frequency, tone->duration);

        // Pause between tones if needed
        if (tone->pause > 0)
        {
            vTaskDelay(tone->pause / portTICK_PERIOD_MS);
        }
    }

    // Call done callback
    if (callback_ref_done != LUA_NOREF && L_global != NULL)
    {
        lua_rawgeti(L_global, LUA_REGISTRYINDEX, callback_ref_done);
        lua_pcall(L_global, 0, 0, 0);
    }
}

// Play a music string (e.g., "C41D42E43")
static void play_music(const char *music_str)
{
    int len = strlen(music_str);
    int i = 0;

    while (i < len)
    {
        // Extract note
        char note = music_str[i++];
        if (i >= len)
            break;

        // Extract duration factor
        char duration_char = music_str[i++];
        int duration_factor = duration_char - '0';

        // Find the note frequency
        int frequency = 0;
        switch (note)
        {
        case 'C':
            frequency = NOTE_C4;
            break;
        case 'D':
            frequency = NOTE_D4;
            break;
        case 'E':
            frequency = NOTE_E4;
            break;
        case 'F':
            frequency = NOTE_F4;
            break;
        case 'G':
            frequency = NOTE_G4;
            break;
        case 'A':
            frequency = NOTE_A4;
            break;
        case 'B':
            frequency = NOTE_B4;
            break;
        default:
            continue; // Skip invalid notes
        }

        // Play the note
        play_tone(frequency, speed * duration_factor);
        // Serial.print("Playing speed: ");
        // Serial.println(speed);
        // Pause between notes
        vTaskDelay(int(speed / 6.5) / portTICK_PERIOD_MS);
    }

    // Call done callback
    if (callback_ref_done != LUA_NOREF && L_global != NULL)
    {
        lua_rawgeti(L_global, LUA_REGISTRYINDEX, callback_ref_done);
        lua_pcall(L_global, 0, 0, 0);
    }
}

static int l_piezo_set_speed(lua_State *L)
{
    int _speed = luaL_checkinteger(L, 1);
    Serial.print("Playing __speed: ");
    Serial.print(_speed);
    // speed = constrain(speed, 0, 10000);
    speed = _speed;

    Serial.print("Playing After assign speed: ");
    Serial.println(speed);
    return 0;
}

// Lua: piezo.play(options)
static int l_piezo_play(lua_State *L)
{
    // Default values
    Tone tone = {
        .frequency = 3000, // Default frequency
        .duration = 1000,  // Default duration in ms
        .pause = 0,        // Default pause in ms
        .repetitions = 1   // Default repetitions
    };

    bool blocking = false; // Default to non-blocking mode

    // Parse options table if provided
    if (lua_istable(L, 1))
    {
        // Get frequency
        lua_getfield(L, 1, "freq");
        if (lua_isnumber(L, -1))
        {
            tone.frequency = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        // Get play duration
        lua_getfield(L, 1, "play_duration");
        if (lua_isnumber(L, -1))
        {
            tone.duration = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        // Get pause duration
        lua_getfield(L, 1, "pause_duration");
        if (lua_isnumber(L, -1))
        {
            tone.pause = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        // Get repetitions
        lua_getfield(L, 1, "times");
        if (lua_isnumber(L, -1))
        {
            tone.repetitions = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        // Get blocking mode
        lua_getfield(L, 1, "blocking");
        if (lua_isboolean(L, -1))
        {
            blocking = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        // Get callbacks
        lua_getfield(L, 1, "on_step");
        if (lua_isfunction(L, -1))
        {
            // Replace old callback if exists
            if (callback_ref_step != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_step);
            }

            // Store callback reference
            callback_ref_step = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else
        {
            lua_pop(L, 1);
        }

        lua_getfield(L, 1, "on_done");
        if (lua_isfunction(L, -1))
        {
            // Replace old callback if exists
            if (callback_ref_done != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_done);
            }

            // Store callback reference
            callback_ref_done = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else
        {
            lua_pop(L, 1);
        }
    }

    if (!piezo_play_sequence_c(&tone, blocking))
    {
        return luaL_error(L, "Failed to play tone");
    }

    return 0;
}

// Lua: piezo.stop()
static int l_piezo_stop(lua_State *L)
{
    Command cmd = {
        .type = CMD_STOP,
        .mode = MODE_BLOCKING,
        .completion_sem = xSemaphoreCreateBinary()};

    if (cmd.completion_sem == NULL)
    {
        return luaL_error(L, "Failed to create semaphore");
    }

    if (xQueueSend(command_queue, &cmd, portMAX_DELAY) != pdPASS)
    {
        vSemaphoreDelete(cmd.completion_sem);
        return luaL_error(L, "Failed to send stop command");
    }

    xSemaphoreTake(cmd.completion_sem, portMAX_DELAY);
    vSemaphoreDelete(cmd.completion_sem);
    return 0;
}

// Lua: piezo.force_stop()
static int l_piezo_force_stop(lua_State *L)
{
    piezo_force_stop_c();
    return 0;
}

// Lua: piezo.success(options)
static int l_piezo_success(lua_State *L)
{
    // Default success tone
    Tone tone = {
        .frequency = 2700, // Higher frequency for success
        .duration = 100,   // Short beeps
        .pause = 25,       // Short pause
        .repetitions = 2   // Two beeps
    };

    bool blocking = false; // Default to non-blocking mode

    // Parse options table if provided
    if (lua_istable(L, 1))
    {
        lua_getfield(L, 1, "freq");
        if (lua_isnumber(L, -1))
        {
            tone.frequency = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "play_duration");
        if (lua_isnumber(L, -1))
        {
            tone.duration = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "pause_duration");
        if (lua_isnumber(L, -1))
        {
            tone.pause = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "times");
        if (lua_isnumber(L, -1))
        {
            tone.repetitions = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "blocking");
        if (lua_isboolean(L, -1))
        {
            blocking = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "on_step");
        if (lua_isfunction(L, -1))
        {
            if (callback_ref_step != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_step);
            }
            callback_ref_step = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else
        {
            lua_pop(L, 1);
        }

        lua_getfield(L, 1, "on_done");
        if (lua_isfunction(L, -1))
        {
            if (callback_ref_done != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_done);
            }
            callback_ref_done = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else
        {
            lua_pop(L, 1);
        }
    }

    if (!piezo_play_sequence_c(&tone, blocking))
    {
        return luaL_error(L, "Failed to play success tone");
    }

    return 0;
}

// Lua: piezo.error(options)
static int l_piezo_error(lua_State *L)
{
    // Default error tone
    Tone tone = {
        .frequency = 200, // Low frequency for error
        .duration = 1000, // Long beep
        .pause = 0,       // No pause
        .repetitions = 1  // One beep
    };

    bool blocking = false; // Default to non-blocking mode

    // Parse options table if provided
    if (lua_istable(L, 1))
    {
        lua_getfield(L, 1, "freq");
        if (lua_isnumber(L, -1))
        {
            tone.frequency = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "play_duration");
        if (lua_isnumber(L, -1))
        {
            tone.duration = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "times");
        if (lua_isnumber(L, -1))
        {
            tone.repetitions = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "blocking");
        if (lua_isboolean(L, -1))
        {
            blocking = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "on_step");
        if (lua_isfunction(L, -1))
        {
            if (callback_ref_step != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_step);
            }
            callback_ref_step = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else
        {
            lua_pop(L, 1);
        }

        lua_getfield(L, 1, "on_done");
        if (lua_isfunction(L, -1))
        {
            if (callback_ref_done != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_done);
            }
            callback_ref_done = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else
        {
            lua_pop(L, 1);
        }
    }

    if (!piezo_play_sequence_c(&tone, blocking))
    {
        return luaL_error(L, "Failed to play error tone");
    }

    return 0;
}

// Lua: piezo.play_music(music_str, blocking)
static int l_piezo_play_music(lua_State *L)
{
    const char *music_str = luaL_checkstring(L, 1);
    bool blocking = lua_isboolean(L, 2) ? lua_toboolean(L, 2) : false;

    if (!piezo_play_music_c(music_str, blocking))
    {
        return luaL_error(L, "Failed to play music");
    }

    return 0;
}

// Lua: piezo.is_playing()
static int l_piezo_is_playing(lua_State *L)
{
    lua_pushboolean(L, is_playing);
    return 1;
}

// Lua: piezo.set_callback("step"|"done", function)
static int l_piezo_set_callback(lua_State *L)
{
    const char *type = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    if (strcmp(type, "step") == 0)
    {
        // Replace old callback if exists
        if (callback_ref_step != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_step);
        }

        // Store callback reference
        lua_pushvalue(L, 2); // Copy the function to the top
        callback_ref_step = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else if (strcmp(type, "done") == 0)
    {
        // Replace old callback if exists
        if (callback_ref_done != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_done);
        }

        // Store callback reference
        lua_pushvalue(L, 2); // Copy the function to the top
        callback_ref_done = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else
    {
        return luaL_error(L, "Invalid callback type: %s", type);
    }

    return 0;
}

// Module registration function
int luaopen_piezo32(lua_State *L)
{
    // Create piezo table directly in the global environment
    l_piezo_init(L);

    // Set the returned table in the global environment as "piezo"
    lua_setglobal(L, "piezo");

    // Return nothing (we've already set it as a global)
    return 0;
}
