#pragma once
#include<cstdint>
#include <SDL3/SDL.h>


constexpr auto AUDIO_SAMPLE_SIZE = 4000;
constexpr auto SAMPLE_RATE = 48000;
constexpr auto CHANNELS = 2;
constexpr auto BYTES_PER_FRAME = (sizeof(float) * CHANNELS);
constexpr auto TARGET_MS = 30;
constexpr auto TARGET_QUEUE_BYTES = ((SAMPLE_RATE * TARGET_MS / 1000) * BYTES_PER_FRAME);

struct SDL_Display {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    const char* title;
    int width;
    int height;
};

struct Audio_Output {
    SDL_AudioDeviceID device;
    SDL_AudioStream* stream;
};

enum class Waveform {
    Square,
    Sine,
    Saw,
    Triangle
};

struct Oscillator {
    int samples_per_second;
    float frequency;
    float volume;
    float phase; // 0.0 to 1.0
    Waveform waveform;
};


bool initialize_display(SDL_Display* display);
bool initialize_audio();
bool create_audio_stream(int frequency, int channels, Audio_Output* ao);
float next_sample(Oscillator *osc);
void fill_audio_buffer(Oscillator *osc, float* audio_buffer);
void play_tone(SDL_AudioStream* stream, Oscillator *osc, float* audio_buffer);
void pause_tone(SDL_AudioStream* stream);
bool processing_events(void (*key_callback)(int,bool));