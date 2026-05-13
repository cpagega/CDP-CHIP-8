#include <windows.h>
#include <stdio.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_audio.h>
#include "CDP_Chip8.h"
#include "Platform.h"


float audio_buffer[AUDIO_SAMPLE_SIZE] = {};


int main(int argc, char* argv[]) {

    SDL_Display display = {};
    display.title = "CDP-CHIP-8";
    display.width = 1920;
    display.height = 1080;

    if (!initialize_display(&display)) {
        printf("Display initialization failed.\n");
    }

    if (!initialize_audio()) {
        printf("Audio initialization failed.\n");
    }

    Audio_Output audio_output;
    if (!create_audio_stream(SAMPLE_RATE,CHANNELS,&audio_output)) {
        printf("Audio stream creation failed.\n");
    }
 
    Oscillator square_wave = {};
    square_wave.frequency = 350.0f;
    square_wave.samples_per_second = SAMPLE_RATE;
    square_wave.volume = 0.1f;
    square_wave.phase = 0.0f;
    square_wave.waveform = Waveform::Square;

    CH8::reset();

    /* High resolution timer for setting clock speed and other timed events */
    LARGE_INTEGER frequency, last_cycle, last_60Hz, now;
    QueryPerformanceFrequency(&frequency);
    uint64_t clock_cycle = frequency.QuadPart / 700;
    uint64_t interval_60Hz = frequency.QuadPart / 60;
    QueryPerformanceCounter(&last_cycle);
    last_60Hz = last_cycle;
    CH8::Keypad* keypad = CH8::get_keypad();
    bool running = true;
    while (running) {
        QueryPerformanceCounter(&now);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case(SDL_EVENT_QUIT):
                running = false;
                break;
            case(SDL_EVENT_KEY_DOWN):
            {
                uint8_t key = CH8::map_hexkey(event.key.scancode);
                if (key != CH8::NO_KEY) {
                    keypad->key = key;
                    keypad->keys[key] = 1;
                    keypad->keydown = true;
                }
            }
            break;
            case(SDL_EVENT_KEY_UP):
            {
                uint8_t key = CH8::map_hexkey(event.key.scancode);
                if (key != CH8::NO_KEY) {
                    keypad->key = key;
                    keypad->keys[key] = 0;
                    keypad->keydown = false;
                }
            }
            break;
            }
        }

        // 60Hz Block (every ~16.67ms)
        while (now.QuadPart - last_60Hz.QuadPart >= interval_60Hz) {
            CH8::decrement_delay_timer();
            bool sound_ready = CH8::decrement_sound_timer();
            if (sound_ready) {
                play_tone(audio_output.stream, &square_wave, audio_buffer);
            } else {
                pause_tone(audio_output.stream);
            }
            CH8::set_display_flag();
            SDL_UpdateTexture(display.texture, NULL, CH8::display_buffer, 256);
            SDL_RenderTexture(display.renderer, display.texture, NULL, NULL);
            SDL_RenderPresent(display.renderer);
            last_60Hz.QuadPart += interval_60Hz;
        }

        // 500Hz Block (every ~2ms)
        int cycles = 0;
        while (now.QuadPart - last_cycle.QuadPart >= clock_cycle) {
            CH8::cycle();
            last_cycle.QuadPart += clock_cycle;
            cycles++;
        } 
    }
    SDL_DestroyWindow(display.window);
    SDL_Quit();

    return 0;
}


