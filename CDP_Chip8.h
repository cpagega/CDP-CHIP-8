#pragma once
#include <stdint.h>
#include <atomic>
namespace CH8
{
    constexpr auto MEMORY_SIZE = 0x1000;
    constexpr auto PROGRAM_START_ADDR = 0x200;
    constexpr auto FONT_ADDR = 0x50;
    constexpr auto SCREEN_WIDTH = 64;
    constexpr auto SCREEN_HEIGHT = 32;
    constexpr auto NO_KEY = 0xFF;

    extern uint8_t font[80];
    extern uint8_t memory[MEMORY_SIZE];
    extern uint32_t display_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

    struct CPU {
        std::atomic<bool> display;
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

    void reset();
    void load_rom();
    void clear_screen();
    void cycle();
    void decrement_delay_timer();
    bool decrement_sound_timer();
    void set_display_flag();
    uint8_t map_hexkey(int scancode);
    Keypad* get_keypad();
    void set_keypad(int scancode,bool key_pressed);
    bool get_display_flag();
}