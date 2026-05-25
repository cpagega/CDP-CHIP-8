#include <SDL3/SDL_audio.h>
#include <cmath>
#include <cstddef>
#include "Platform.h"


bool initialize_display(SDL_Display* display) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL could not initialize: %s", SDL_GetError());
        return false;
    }

    display->window = SDL_CreateWindow(
        display->title,
        display->width,
        display->height,
        0
    );

    if (!display->window) {
        SDL_Log("Window could not be created: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    display->renderer = SDL_CreateRenderer(display->window, NULL);

    if (!display->renderer) {
        SDL_Log("Renderer could not be created: %s", SDL_GetError());
        SDL_DestroyWindow(display->window);
        SDL_Quit();
        return false;
    }

    display->texture = SDL_CreateTexture(
        display->renderer,
        SDL_PIXELFORMAT_XRGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        64,
        32
    );

    if (!display->texture) {
        SDL_Log("Texture could not be created: %s", SDL_GetError());
        SDL_DestroyRenderer(display->renderer);
        SDL_DestroyWindow(display->window);
        SDL_Quit();
        return false;
    }

    SDL_SetTextureScaleMode(display->texture, SDL_SCALEMODE_NEAREST);

    return true;
}

bool initialize_audio() {
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        SDL_Log("SDL could not initialize: %s", SDL_GetError());
        return false;
    }
    return true;
}


bool create_audio_stream(int frequency, int channels, Audio_Output* ao) {
    SDL_AudioSpec spec = {};
    spec.format = SDL_AUDIO_F32;
    spec.freq = frequency;
    spec.channels = channels;
    int count = 0;
    SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);

    if (count == 0) {
        SDL_free(devices);
        return false;
    }
    SDL_AudioDeviceID device = 0;
    device = devices[0];
    ao->device = SDL_OpenAudioDevice(device, &spec);
    ao->stream = SDL_CreateAudioStream(&spec, &spec);
    SDL_free(devices);

    if (!ao->stream) {
        SDL_Log("CreateAudioStream failed: %s", SDL_GetError());
        SDL_CloseAudioDevice(ao->device);
        return false;
    }

    if (!SDL_BindAudioStream(ao->device, ao->stream)) {
        SDL_Log("BindAudioStream failed: %s", SDL_GetError());
        SDL_DestroyAudioStream(ao->stream);
        SDL_CloseAudioDevice(ao->device);
        return false;
    }
    return true;
}


float next_sample(Oscillator* osc) {
    float sample = 0.0f;

    switch (osc->waveform) {
    case Waveform::Square:
        sample = osc->phase < 0.5f ? -1.0f : 1.0f;
        break;

    case Waveform::Sine:
        sample = sinf(osc->phase * 2.0f * 3.14159265f);
        break;

    case Waveform::Saw:
        sample = (2.0f * osc->phase) - 1.0f;
        break;

    case Waveform::Triangle:
        sample = 4.0f * fabsf(osc->phase - 0.5f) - 1.0f;
        break;
    }

    osc->phase += osc->frequency / (float)osc->samples_per_second;
    if (osc->phase >= 1.0f) {
        osc->phase -= 1.0f;
    }

    return sample * osc->volume;
}

void fill_audio_buffer(Oscillator *osc, float *audio_buffer) {
    for (int32_t sample_index = 0; sample_index < AUDIO_SAMPLE_SIZE - 1; sample_index += 2) {
        float sample = next_sample(osc);
        audio_buffer[sample_index] = sample;
        audio_buffer[sample_index + 1] = sample;
    }
}

void play_tone(SDL_AudioStream *stream, Oscillator *osc, float *audio_buffer) {
    int queued = SDL_GetAudioStreamQueued(stream);
    if (queued < 0) {
        SDL_Log("SDL_GetAudioStreamQueued failed: %s", SDL_GetError());
    } else if (static_cast<std::size_t>(queued) < TARGET_QUEUE_BYTES) {
        fill_audio_buffer(osc, audio_buffer);
        if (!SDL_PutAudioStreamData(stream, audio_buffer, AUDIO_SAMPLE_SIZE * sizeof(float))) {
            SDL_Log("Error adding audio data to stream\n%s", SDL_GetError());
            return;
        }
    }
    SDL_ResumeAudioStreamDevice(stream);
}

void pause_tone(SDL_AudioStream* stream) {
    SDL_PauseAudioStreamDevice(stream);
}

bool processing_events(void (*key_callback)(int, bool)) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case(SDL_EVENT_QUIT):
            return false;
            break;
        case(SDL_EVENT_KEY_DOWN):
        {
            key_callback(event.key.scancode, true);
        }
        break;
        case(SDL_EVENT_KEY_UP):
        {
            key_callback(event.key.scancode, false);
        }
        break;
        }
    }
    return true;
}



/*


*/