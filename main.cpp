#include <thread>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <SDL3/SDL_main.h>
#include "Platform.h"
#include "CDP_Chip8.h"


float audio_buffer[AUDIO_SAMPLE_SIZE] = {};
void cycle_thread();
void timer_thread(Audio_Output audio_output, Oscillator* square_wave);
std::atomic<bool> running;
int main(int argc, char* argv[]) {

    SDL_Display display = {};
    display.title = "CDP-CHIP-8";
    display.width = 1920;
    display.height = 1080;

    Oscillator square_wave = {};
    square_wave.frequency = 350.0f;
    square_wave.samples_per_second = SAMPLE_RATE;
    square_wave.volume = 0.1f;
    square_wave.phase = 0.0f;
    square_wave.waveform = Waveform::Square;

    if (!initialize_display(&display)) {
        printf("Display initialization failed.\n");
    }

    if (!initialize_audio()) {
        printf("Audio initialization failed.\n");
    }

    Audio_Output audio_output;
    if (!create_audio_stream(SAMPLE_RATE, CHANNELS, &audio_output)) {
        printf("Audio stream creation failed.\n");
    }
    
    CH8::reset();
    std::thread cycle(cycle_thread);
    std::thread timer(timer_thread, audio_output, &square_wave);

    running = true;
    while (processing_events(CH8::set_keypad)) {
        if (CH8::get_display_flag()) {
            SDL_UpdateTexture(display.texture, NULL, CH8::display_buffer, 256);
            SDL_RenderTexture(display.renderer, display.texture, NULL, NULL);
            SDL_RenderPresent(display.renderer);
        }
    }

    running = false;
    SDL_DestroyWindow(display.window);
    SDL_Quit();
    cycle.join();
    timer.join();
    return 0;
}

void cycle_thread() {
    while (running) {
        CH8::cycle();
        std::this_thread::sleep_for(std::chrono::microseconds(1428)); // ~700Hz
    }
}

void timer_thread(Audio_Output audio_output, Oscillator *square_wave) {
    while (running) {
        CH8::decrement_delay_timer();
        bool sound_ready = CH8::decrement_sound_timer();
        if (sound_ready) {
            play_tone(audio_output.stream, square_wave, audio_buffer);
        } else {
            pause_tone(audio_output.stream);
        }
        CH8::set_display_flag();
        std::this_thread::sleep_for(std::chrono::microseconds(16666)); // ~60Hz
    }
}