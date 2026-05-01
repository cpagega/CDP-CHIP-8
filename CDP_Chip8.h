#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <time.h>


namespace CH8
{

    constexpr auto MEMORY_SIZE = 0x1000;
    constexpr auto PROGRAM_START_ADDR = 0x200;
    constexpr auto FONT_ADDR = 0x50;
    constexpr auto SCREEN_WIDTH = 64;
    constexpr auto SCREEN_HEIGHT = 32;
    constexpr auto AUDIO_SAMPLE_SIZE = 4000;
    constexpr auto NO_KEY = 0xFF;

    constexpr auto SAMPLE_RATE = 48000;
    constexpr auto CHANNELS = 2;
    constexpr auto BYTES_PER_FRAME = (sizeof(float) * CHANNELS);
    constexpr auto TARGET_MS = 30;
    constexpr auto TARGET_QUEUE_BYTES = ((SAMPLE_RATE * TARGET_MS / 1000) * BYTES_PER_FRAME);

    extern uint8_t font[80];
    extern uint8_t memory[MEMORY_SIZE];
    extern float audio_buffer[AUDIO_SAMPLE_SIZE];
    extern uint32_t display_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

    struct CPU {
        bool display;
        uint16_t PC;
        uint16_t I;
        uint16_t stack[16];
        uint8_t SP;
        uint8_t delay_timer;
        uint8_t sound_timer;
        uint8_t registers[16];
    };

    struct Keypad {
        uint8_t key;
        uint8_t keys[16];
        bool keydown;
    };


    struct SoundOutput {
        uint16_t tone_hz;
        float    tone_volume;
        uint16_t samples_per_second;
        uint16_t square_wave_period;
        uint16_t half_square_wave_period;
        uint32_t wave_position;
    };

    void reset(CPU* cpu);
    void load_rom();
    void clear_screen();
    void cycle(CPU* cpu, Keypad* key);
    uint8_t map_hexkey(int scancode);
}