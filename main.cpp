#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include <SDL3/SDL_main.h>
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

#include "Platform.h"
#include "CDP_Chip8.h"

float audio_buffer[AUDIO_SAMPLE_SIZE] = {};
void cycle_thread();
void timer_thread(Audio_Output audio_output, Oscillator* square_wave);
bool process_events(bool* show_ui);
void render_ui(char* rom_path, std::size_t rom_path_size, std::string* status_message);

std::atomic<bool> running = false;

int main(int argc, char* argv[]) {
    SDL_Display display = {};
    display.title = "CDP-CHIP-8 (Cursor experiment)";
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
        return 1;
    }

    if (!initialize_audio()) {
        printf("Audio initialization failed. Continuing without audio.\n");
    }

    Audio_Output audio_output = {};
    if (!create_audio_stream(SAMPLE_RATE, CHANNELS, &audio_output)) {
        printf("Audio stream creation failed. Continuing without audio.\n");
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(display.window, display.renderer);
    ImGui_ImplSDLRenderer3_Init(display.renderer);

    CH8::initialize();

    bool show_ui = true;
    std::array<char, 512> rom_path = {};
    std::string status_message = "Load a ROM to start execution.";
    std::array<uint32_t, CH8::SCREEN_WIDTH * CH8::SCREEN_HEIGHT> frame_buffer = {};

    running = true;
    std::thread cycle(cycle_thread);
    std::thread timer(timer_thread, audio_output, &square_wave);

    while (process_events(&show_ui)) {
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (show_ui) {
            render_ui(rom_path.data(), rom_path.size(), &status_message);
        }

        ImGui::Render();

        CH8::copy_display_buffer(frame_buffer.data(), frame_buffer.size());
        SDL_UpdateTexture(display.texture, NULL, frame_buffer.data(), CH8::SCREEN_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(display.renderer);
        SDL_RenderTexture(display.renderer, display.texture, NULL, NULL);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), display.renderer);
        SDL_RenderPresent(display.renderer);
    }

    running = false;
    cycle.join();
    timer.join();

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (audio_output.stream) {
        SDL_DestroyAudioStream(audio_output.stream);
    }
    if (audio_output.device) {
        SDL_CloseAudioDevice(audio_output.device);
    }
    SDL_DestroyTexture(display.texture);
    SDL_DestroyRenderer(display.renderer);
    SDL_DestroyWindow(display.window);
    SDL_Quit();
    return 0;
}

bool process_events(bool* show_ui) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }

        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.scancode == SDL_SCANCODE_GRAVE) {
            *show_ui = !*show_ui;
            continue;
        }

        if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
            ImGuiIO& io = ImGui::GetIO();
            bool key_pressed = event.type == SDL_EVENT_KEY_DOWN;
            if (!io.WantCaptureKeyboard || !key_pressed) {
                CH8::set_keypad(event.key.scancode, key_pressed);
            }
        }
    }

    return true;
}

void render_ui(char* rom_path, std::size_t rom_path_size, std::string* status_message) {
    CH8::Debug_State debug_state = CH8::get_debug_state();
    std::array<uint8_t, CH8::MEMORY_SIZE> memory = {};
    CH8::copy_memory(memory.data(), memory.size());

    ImGui::Begin("CDP-CHIP-8");
    ImGui::TextUnformatted("Press ~ to show or hide this UI.");
    ImGui::Separator();

    bool enter_pressed = ImGui::InputTextWithHint(
        "ROM path",
        "roms/Space Intercept [Joseph Weisbecker, 1978].ch8",
        rom_path,
        rom_path_size,
        ImGuiInputTextFlags_EnterReturnsTrue);

    bool load_clicked = ImGui::Button("Load ROM");
    if (enter_pressed || load_clicked) {
        if (rom_path[0] == '\0') {
            *status_message = "Enter a ROM path before loading.";
        } else if (CH8::load_rom(rom_path)) {
            *status_message = std::string("Loaded ROM: ") + rom_path;
        } else {
            *status_message = std::string("Failed to load ROM: ") + rom_path;
        }
    }

    ImGui::TextWrapped("%s", status_message->c_str());
    ImGui::Text("Execution: %s", debug_state.rom_loaded ? "running" : "waiting for ROM");

    ImGui::SeparatorText("Registers");
    ImGui::Text("PC: 0x%03X  I: 0x%03X  SP: 0x%02X  DT: 0x%02X  ST: 0x%02X",
        debug_state.PC,
        debug_state.I,
        debug_state.SP,
        debug_state.delay_timer,
        debug_state.sound_timer);

    if (ImGui::BeginTable("register-table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        for (int index = 0; index < 16; index++) {
            ImGui::TableNextColumn();
            ImGui::Text("V%X: 0x%02X", index, debug_state.registers[index]);
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Memory");
    static int memory_start = CH8::PROGRAM_START_ADDR;
    static int memory_bytes = 256;
    ImGui::SliderInt("Start", &memory_start, 0, CH8::MEMORY_SIZE - 16, "0x%03X");
    ImGui::SliderInt("Bytes", &memory_bytes, 16, 512, "%d");

    memory_start = (memory_start < 0) ? 0 : memory_start;
    memory_start = (memory_start > CH8::MEMORY_SIZE - 1) ? CH8::MEMORY_SIZE - 1 : memory_start;
    memory_bytes = (memory_bytes < 16) ? 16 : memory_bytes;
    memory_bytes = (memory_bytes > CH8::MEMORY_SIZE - memory_start) ? CH8::MEMORY_SIZE - memory_start : memory_bytes;

    if (ImGui::BeginChild("memory-view", ImVec2(0, 260), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        int memory_end = memory_start + memory_bytes;
        for (int address = memory_start; address < memory_end; address += 16) {
            char line[96] = {};
            int written = snprintf(line, sizeof(line), "0x%03X: ", address);
            for (int offset = 0; offset < 16 && address + offset < memory_end; offset++) {
                written += snprintf(line + written, sizeof(line) - written, "%02X ", memory[address + offset]);
            }
            ImGui::TextUnformatted(line);
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

void cycle_thread() {
    using clock = std::chrono::steady_clock;
    auto next_cycle = clock::now();
    while (running) {
        if (CH8::is_rom_loaded()) {
            CH8::cycle();
        }

        next_cycle += std::chrono::microseconds(1428); // ~700Hz
        std::this_thread::sleep_until(next_cycle);
        if (clock::now() > next_cycle + std::chrono::milliseconds(16)) {
            next_cycle = clock::now();
        }
    }
}

void timer_thread(Audio_Output audio_output, Oscillator* square_wave) {
    using clock = std::chrono::steady_clock;
    auto next_tick = clock::now();
    while (running) {
        bool sound_ready = false;
        if (CH8::is_rom_loaded()) {
            CH8::decrement_delay_timer();
            sound_ready = CH8::decrement_sound_timer();
            CH8::set_display_flag();
        }

        if (audio_output.stream) {
            if (sound_ready) {
                play_tone(audio_output.stream, square_wave, audio_buffer);
            } else {
                pause_tone(audio_output.stream);
            }
        }

        next_tick += std::chrono::microseconds(16666); // ~60Hz
        std::this_thread::sleep_until(next_tick);
        if (clock::now() > next_tick + std::chrono::milliseconds(16)) {
            next_tick = clock::now();
        }
    }
}
