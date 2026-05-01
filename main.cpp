#include <windows.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include "CDP_Chip8.h"

float audio_buffer[CH8::AUDIO_SAMPLE_SIZE];



int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL could not initialize: %s", SDL_GetError());
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("CDP-CHIP-8", 1920, 1080, 0);
    if (!window) {
        SDL_Log("Window could not be created: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    SDL_Texture* sdl_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, 64, 32);
    SDL_SetTextureScaleMode(sdl_texture, SDL_SCALEMODE_NEAREST);

    CH8::SoundOutput sound = {};
    sound.samples_per_second = 48000;
    sound.tone_hz = 350;
    sound.tone_volume = 5.0f/100;
    sound.square_wave_period = sound.samples_per_second / sound.tone_hz;
    sound.half_square_wave_period = sound.square_wave_period / 2;

    SDL_AudioSpec audio_spec = {};
    audio_spec.format = SDL_AUDIO_F32;
    audio_spec.freq = sound.samples_per_second;
    audio_spec.channels = 2;

    int count = 0;
    SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);
    SDL_AudioDeviceID device = 0;
    if (count > 0) {
        device = devices[0]; 
        device = SDL_OpenAudioDevice(device, &audio_spec);
    }
    SDL_free(devices);

    SDL_AudioStream* stream = SDL_CreateAudioStream(&audio_spec, &audio_spec);

    if (!stream) {
        SDL_Log("CreateAudioStream failed: %s", SDL_GetError());
        SDL_CloseAudioDevice(device);
        return 1;
    }

    if (!SDL_BindAudioStream(device, stream)) {
        SDL_Log("BindAudioStream failed: %s", SDL_GetError());
        SDL_DestroyAudioStream(stream);
        SDL_CloseAudioDevice(device);
        return 1;
    }

    CH8::Keypad keypad = {};
    CH8::CPU cpu = {};
 
    CH8::reset(&cpu);
    /* High resolution timer for setting clock speed and other timed events */
    LARGE_INTEGER frequency, last_cycle, last_60Hz, now;
    QueryPerformanceFrequency(&frequency);
    uint64_t clock_cycle = frequency.QuadPart / 700;
    uint64_t interval_60Hz = frequency.QuadPart / 60;
    QueryPerformanceCounter(&last_cycle);
    last_60Hz = last_cycle;

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
                    keypad.key = key;
                    keypad.keys[key] = 1;
                    keypad.keydown = true;
                }
            }
            break;
            case(SDL_EVENT_KEY_UP):
            {
                uint8_t key = CH8::map_hexkey(event.key.scancode);
                if (key != CH8::NO_KEY) {
                    keypad.key = key;
                    keypad.keys[key] = 0;
                    keypad.keydown = false;
                }
            }
            break;
            }
        }

        // 60Hz Block (every ~16.67ms)
        while (now.QuadPart - last_60Hz.QuadPart >= interval_60Hz) {
            if (cpu.delay_timer > 0) cpu.delay_timer--;
            if (cpu.sound_timer > 0) {
                int queued = SDL_GetAudioStreamQueued(stream);
                if (queued < 0) {
                    SDL_Log("SDL_GetAudioStreamQueued failed: %s", SDL_GetError());
                } else if (queued < CH8::TARGET_QUEUE_BYTES && cpu.sound_timer > 0) {
                    for (int32_t sample_index = 0; sample_index < CH8::AUDIO_SAMPLE_SIZE - 1; sample_index += 2) {
                        float sample = ((sound.wave_position++ / sound.half_square_wave_period) % 2) ? sound.tone_volume : -sound.tone_volume;
                        audio_buffer[sample_index] = sample;
                        audio_buffer[sample_index + 1] = sample;
                    }

                    if (!SDL_PutAudioStreamData(stream, audio_buffer, CH8::AUDIO_SAMPLE_SIZE * sizeof(float))) {
                        SDL_Log("Error adding audio data to stream\n%s", SDL_GetError());
                        return 1;
                    }
                }
                SDL_ResumeAudioStreamDevice(stream);
                cpu.sound_timer--;
            } else {
                SDL_PauseAudioStreamDevice(stream);
            }
            cpu.display = true;
            SDL_UpdateTexture(sdl_texture, NULL, CH8::display_buffer, 256);
            SDL_RenderTexture(renderer, sdl_texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            last_60Hz.QuadPart += interval_60Hz;
        }

        // 500Hz Block (every ~2ms)
        int cycles = 0;
        while (now.QuadPart - last_cycle.QuadPart >= clock_cycle) {
            CH8::cycle(&cpu, &keypad);
            last_cycle.QuadPart += clock_cycle;
            cycles++;
        } 
    }
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}


