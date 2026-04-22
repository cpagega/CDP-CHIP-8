#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <time.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>


#define FONT_ADDRESS 0x50
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define AUDIO_SAMPLE_SIZE 4000
#define NO_KEY 0xFF

#define SAMPLE_RATE 48000
#define CHANNELS 2
#define BYTES_PER_FRAME (sizeof(float) * CHANNELS)
#define TARGET_MS 30
#define TARGET_QUEUE_BYTES ((SAMPLE_RATE * TARGET_MS / 1000) * BYTES_PER_FRAME)

extern uint8_t font[80];
extern uint8_t memory[4096];
extern float audio_buffer[AUDIO_SAMPLE_SIZE];

struct CDP_CHIP8_CPU {
    bool display;
    uint16_t PC;
    uint16_t I;
    uint16_t stack[16];
    uint8_t SP;
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint8_t registers[16];
};

struct CDP_CHIP8_Keypad {
    uint8_t key;
    uint8_t keys[16];
    bool keydown;
};

struct CDP_CHIP8_GUI {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    uint32_t display_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];
};

struct CDP_CHIP8_SoundOutput {
    uint16_t tone_hz;
    uint16_t tone_volume;
    uint16_t samples_per_second;
    uint16_t square_wave_period;
    uint16_t half_square_wave_period;
    uint32_t wave_position;
};


void CDP_Chip8_reset(CDP_CHIP8_CPU* cpu, CDP_CHIP8_GUI* gui);
void CDP_Chip8_load_rom();
void CDP_Chip8_clear_screen(CDP_CHIP8_GUI* gui);
void CDP_Chip8_cycle(CDP_CHIP8_CPU* cpu, CDP_CHIP8_GUI* gui, CDP_CHIP8_Keypad* key);
void CDP_Chip8_render(CDP_CHIP8_GUI* gui);
uint8_t CDP_Chip8_map_hexkey(int scancode);
SDL_Window* CDP_Chip8_init_window();