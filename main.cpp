#include <windows.h>
#include <SDL3/SDL_main.h>
#include "CDP_Chip8.h"

float audio_buffer[AUDIO_SAMPLE_SIZE];
int main(int argc, char* argv[]) {
    SDL_Window* window = CDP_Chip8_init_window();
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    SDL_Texture* sdl_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, 64, 32);
    SDL_SetTextureScaleMode(sdl_texture, SDL_SCALEMODE_NEAREST);

    CDP_CHIP8_SoundOutput sound = {};
    sound.samples_per_second = 48000;
    sound.tone_hz = 256;
    sound.tone_volume = 3000;
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

    CDP_CHIP8_GUI gui = {};
    gui.window = window;
    gui.texture = sdl_texture;
    gui.renderer = renderer;

    CDP_CHIP8_Keypad keypad = {};
    CDP_CHIP8_CPU cpu = {};
 
    CDP_Chip8_reset(&cpu, &gui);
    /* High resolution timer for setting clock speed and other timed events */
    LARGE_INTEGER frequency, last_500Hz, last_60Hz, now;
    QueryPerformanceFrequency(&frequency);
    uint64_t interval_500Hz = frequency.QuadPart / 700;
    uint64_t interval_60Hz = frequency.QuadPart / 60;
    QueryPerformanceCounter(&last_500Hz);
    last_60Hz = last_500Hz;

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
                uint8_t key = CDP_Chip8_map_hexkey(event.key.scancode);
                if (key != NO_KEY) {
                    keypad.key = key;
                    keypad.keys[key] = 1;
                    keypad.keydown = true;
                }
            }
            break;
            case(SDL_EVENT_KEY_UP):
            {
                uint8_t key = CDP_Chip8_map_hexkey(event.key.scancode);
                if (key != NO_KEY) {
                    keypad.key = key;
                    keypad.keys[key] = 0;
                    keypad.keydown = false;
                }
            }
            break;
            }
        }

        // 60Hz Block (every ~16.67ms)
        if (now.QuadPart - last_60Hz.QuadPart >= interval_60Hz) {
            if (cpu.delay_timer > 0) cpu.delay_timer--;
            if (cpu.sound_timer > 0) {
                int queued = SDL_GetAudioStreamQueued(stream);
                if (queued < 0) {
                    SDL_Log("SDL_GetAudioStreamQueued failed: %s", SDL_GetError());
                } else if (queued < TARGET_QUEUE_BYTES && cpu.sound_timer > 0) {
                    for (int32_t sample_index = 0; sample_index < AUDIO_SAMPLE_SIZE - 1; sample_index += 2) {
                        float sample = ((sound.wave_position++ / sound.half_square_wave_period) % 2) ? 0.25f : -0.25f;
                        audio_buffer[sample_index] = sample;
                        audio_buffer[sample_index + 1] = sample;
                    }

                    if (!SDL_PutAudioStreamData(stream, audio_buffer, AUDIO_SAMPLE_SIZE * sizeof(float))) {
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
            CDP_Chip8_render(&gui);
            
            last_60Hz.QuadPart += interval_60Hz;
        }

        // 500Hz Block (every ~2ms)
        if (now.QuadPart - last_500Hz.QuadPart >= interval_500Hz) {
            CDP_Chip8_cycle(&cpu, &gui, &keypad);
            last_500Hz.QuadPart += interval_500Hz;
        } 
    }
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}


