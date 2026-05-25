#include "CDP_Chip8.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>
namespace CH8
{
    std::mutex keypad_mutex;
    std::mutex state_mutex;

    uint8_t font[80] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };

    uint8_t memory[MEMORY_SIZE] = { 0 };
    uint32_t display_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

    CPU cpu = {};
    Keypad keypad = {};
    std::atomic<bool> rom_loaded = false;

    const uint8_t* glyph_for(char c) {
        static const uint8_t space[5] = { 0b000, 0b000, 0b000, 0b000, 0b000 };
        static const uint8_t dash[5] = { 0b000, 0b000, 0b111, 0b000, 0b000 };
        static const uint8_t dot[5] = { 0b000, 0b000, 0b000, 0b000, 0b010 };
        static const uint8_t zero[5] = { 0b111, 0b101, 0b101, 0b101, 0b111 };
        static const uint8_t eight[5] = { 0b111, 0b101, 0b111, 0b101, 0b111 };
        static const uint8_t a[5] = { 0b010, 0b101, 0b111, 0b101, 0b101 };
        static const uint8_t b[5] = { 0b110, 0b101, 0b110, 0b101, 0b110 };
        static const uint8_t glyph_c[5] = { 0b111, 0b100, 0b100, 0b100, 0b111 };
        static const uint8_t d[5] = { 0b110, 0b101, 0b101, 0b101, 0b110 };
        static const uint8_t e[5] = { 0b111, 0b100, 0b110, 0b100, 0b111 };
        static const uint8_t h[5] = { 0b101, 0b101, 0b111, 0b101, 0b101 };
        static const uint8_t i[5] = { 0b111, 0b010, 0b010, 0b010, 0b111 };
        static const uint8_t l[5] = { 0b100, 0b100, 0b100, 0b100, 0b111 };
        static const uint8_t m[5] = { 0b101, 0b111, 0b111, 0b101, 0b101 };
        static const uint8_t n[5] = { 0b110, 0b101, 0b101, 0b101, 0b101 };
        static const uint8_t o[5] = { 0b111, 0b101, 0b101, 0b101, 0b111 };
        static const uint8_t p[5] = { 0b110, 0b101, 0b110, 0b100, 0b100 };
        static const uint8_t r[5] = { 0b110, 0b101, 0b110, 0b101, 0b101 };
        static const uint8_t t[5] = { 0b111, 0b010, 0b010, 0b010, 0b010 };
        static const uint8_t u[5] = { 0b101, 0b101, 0b101, 0b101, 0b111 };
        static const uint8_t v[5] = { 0b101, 0b101, 0b101, 0b101, 0b010 };

        if (c >= 'a' && c <= 'z') {
            c = c - ('a' - 'A');
        }

        switch (c) {
        case ' ': return space;
        case '-': return dash;
        case '.': return dot;
        case '0': return zero;
        case '8': return eight;
        case 'A': return a;
        case 'B': return b;
        case 'C': return glyph_c;
        case 'D': return d;
        case 'E': return e;
        case 'H': return h;
        case 'I': return i;
        case 'L': return l;
        case 'M': return m;
        case 'N': return n;
        case 'O': return o;
        case 'P': return p;
        case 'R': return r;
        case 'T': return t;
        case 'U': return u;
        case 'V': return v;
        default: return space;
        }
    }

    int text_width(const char* text) {
        return (int)strlen(text) * 4 - 1;
    }

    void draw_text_unlocked(int x, int y, const char* text, uint32_t color) {
        for (int character = 0; text[character] != '\0'; character++) {
            const uint8_t* glyph = glyph_for(text[character]);
            int glyph_x = x + character * 4;
            for (int row = 0; row < 5; row++) {
                for (int column = 0; column < 3; column++) {
                    if ((glyph[row] & (0b100 >> column)) == 0) {
                        continue;
                    }

                    int px = glyph_x + column;
                    int py = y + row;
                    if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                        display_buffer[py * SCREEN_WIDTH + px] = color;
                    }
                }
            }
        }
    }

    void clear_screen_unlocked() {
        memset(&display_buffer, 0, sizeof(display_buffer));
    }

    void draw_splash_screen_unlocked() {
        clear_screen_unlocked();
        const uint32_t white = 0xFFFFFFFF;
        const uint32_t dim = 0xFFAAAAAA;
        const char* title = "CDP-CHIP-8";
        const char* line1 = "Load a rom";
        const char* line2 = "to continue.";

        draw_text_unlocked((SCREEN_WIDTH - text_width(title)) / 2, 7, title, white);
        draw_text_unlocked((SCREEN_WIDTH - text_width(line1)) / 2, 18, line1, dim);
        draw_text_unlocked((SCREEN_WIDTH - text_width(line2)) / 2, 25, line2, dim);
    }

    void initialize_unlocked(bool show_splash) {
        auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        srand((unsigned int)seed);
        cpu.display = true;
        cpu.PC = PROGRAM_START_ADDR;
        cpu.I = PROGRAM_START_ADDR;
        cpu.SP = 0;
        cpu.delay_timer = 0;
        cpu.sound_timer = 0;
        memset(&cpu.registers, 0, sizeof(cpu.registers));
        memset(&cpu.stack, 0, sizeof(cpu.stack));
        memset(&keypad, 0, sizeof(keypad));
        keypad.key = NO_KEY;
        memcpy(&memory[FONT_ADDR], font, sizeof(font));
        rom_loaded = false;

        if (show_splash) {
            draw_splash_screen_unlocked();
        } else {
            clear_screen_unlocked();
        }
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(state_mutex);
        initialize_unlocked(true);
    }

    void draw_splash_screen() {
        std::lock_guard<std::mutex> lock(state_mutex);
        draw_splash_screen_unlocked();
        rom_loaded = false;
    }

    struct Decoded_Opcode {
        uint16_t raw;
        uint8_t kind;
        uint8_t x;
        uint8_t y;
        uint8_t n;
        uint8_t nn;
        uint16_t nnn;
    };

    Decoded_Opcode decode_opcode(uint16_t opcode) {
        Decoded_Opcode decoded = {};
        decoded.raw = opcode;
        decoded.kind = (opcode & 0xF000) >> 12;
        decoded.x = (opcode & 0x0F00) >> 8;
        decoded.y = (opcode & 0x00F0) >> 4;
        decoded.n = opcode & 0x000F;
        decoded.nn = opcode & 0x00FF;
        decoded.nnn = opcode & 0x0FFF;
        return decoded;
    }

    Decoded_Opcode fetch_opcode() {
        uint8_t hi_byte = memory[cpu.PC++];
        uint8_t lo_byte = memory[cpu.PC++];
        return decode_opcode(((uint16_t)hi_byte << 8) | lo_byte);
    }

    void invalid_opcode(const Decoded_Opcode& opcode) {
        printf("Invalid opcode. Sucks to be you.\n");
        printf("Opcode: 0x%04X | kind: %X, X: %X, Y: %X, N: 0x%04X, NN: 0x%04X, NNN: 0x%04X\n",
            opcode.raw,
            opcode.kind,
            opcode.x,
            opcode.y,
            opcode.n,
            opcode.nn,
            opcode.nnn);
    }

    void skip_next_instruction() {
        cpu.PC += 2;
    }

    void execute_system_instruction(const Decoded_Opcode& opcode) {
        if (opcode.raw == 0x00E0) {
            clear_screen_unlocked();
        }

        if (opcode.raw == 0x00EE) {
            cpu.PC = cpu.stack[--cpu.SP];
        }
    }

    void execute_arithmetic_instruction(const Decoded_Opcode& opcode) {
        uint8_t& vx = cpu.registers[opcode.x];
        uint8_t& vy = cpu.registers[opcode.y];

        switch (opcode.n) {
        case 0x0:
            vx = vy;
            cpu.registers[0xF] = 0;
            break;
        case 0x1:
            vx |= vy;
            cpu.registers[0xF] = 0;
            break;
        case 0x2:
            vx &= vy;
            cpu.registers[0xF] = 0;
            break;
        case 0x3:
            vx ^= vy;
            cpu.registers[0xF] = 0;
            break;
        case 0x4:
        {
            uint8_t a = vx;
            uint8_t b = vy;
            uint8_t sum = a + b;
            uint8_t carry = (sum < a) ? 1 : 0;
            vx = sum;
            cpu.registers[0xF] = carry;
        }
        break;
        case 0x5:
        {
            uint8_t a = vx;
            uint8_t b = vy;
            uint8_t diff = a - b;
            uint8_t carry = (a >= b) ? 1 : 0;
            vx = diff;
            cpu.registers[0xF] = carry;
        }
        break;
        case 0x6:
        {
            uint8_t a = vy;
            vx = a >> 1;
            cpu.registers[0xF] = a & 0x1;
        }
        break;
        case 0x7:
        {
            uint8_t a = vy;
            uint8_t b = vx;
            uint8_t diff = a - b;
            uint8_t carry = (a >= b) ? 1 : 0;
            vx = diff;
            cpu.registers[0xF] = carry;
        }
        break;
        case 0xE:
        {
            uint8_t a = vy;
            vx = a << 1;
            cpu.registers[0xF] = (a >> 7) & 1;
        }
        break;
        default:
            invalid_opcode(opcode);
            break;
        }
    }

    bool execute_draw_instruction(const Decoded_Opcode& opcode) {
        // Drawing waits for the render thread to present the previous frame.
        if (!cpu.display) {
            cpu.PC -= 2;
            return false;
        }

        cpu.display = false;
        uint8_t x = cpu.registers[opcode.x] % SCREEN_WIDTH;
        uint8_t y = cpu.registers[opcode.y] % SCREEN_HEIGHT;
        cpu.registers[0xF] = 0;

        for (uint8_t row = 0; row < opcode.n; row++) {
            uint8_t sprite_row = memory[cpu.I + row];
            for (uint8_t bit = 0; bit < 8; bit++) {
                uint8_t px = x + bit;
                uint8_t py = y + row;
                if (px >= SCREEN_WIDTH || py >= SCREEN_HEIGHT) continue;
                if ((sprite_row & (0x80 >> bit)) == 0) continue;

                uint16_t index = py * SCREEN_WIDTH + px;
                if (display_buffer[index] == 0xFFFFFFFF) {
                    cpu.registers[0xF] = 1;
                    display_buffer[index] = 0xFF000000;
                } else {
                    display_buffer[index] = 0xFFFFFFFF;
                }
            }
        }

        return true;
    }

    void execute_key_skip_instruction(const Decoded_Opcode& opcode) {
        uint8_t key = cpu.registers[opcode.x];
        std::lock_guard<std::mutex> read_key_press(keypad_mutex);

        if (opcode.nn == 0x9E && keypad.keys[key]) {
            skip_next_instruction();
        }

        if (opcode.nn == 0xA1 && !keypad.keys[key]) {
            skip_next_instruction();
        }

        keypad.key = NO_KEY;
    }

    void wait_for_key_release(uint8_t register_index) {
        std::lock_guard<std::mutex> get_key(keypad_mutex);
        if (keypad.key == NO_KEY) {
            cpu.PC -= 2;
        } else if (keypad.keydown) {
            cpu.PC -= 2;
        } else {
            cpu.registers[register_index] = keypad.key;
            keypad.key = NO_KEY;
        }
    }

    void store_bcd(uint8_t register_index) {
        uint8_t number = cpu.registers[register_index];
        memory[cpu.I + 2] = number % 10;
        number /= 10;
        memory[cpu.I + 1] = number % 10;
        number /= 10;
        memory[cpu.I] = number % 10;
    }

    void store_registers(uint8_t last_register) {
        for (uint8_t reg = 0; reg <= last_register; reg++) {
            memory[cpu.I + reg] = cpu.registers[reg];
        }
        cpu.I += last_register + 1;
    }

    void load_registers(uint8_t last_register) {
        for (uint8_t reg = 0; reg <= last_register; reg++) {
            cpu.registers[reg] = memory[cpu.I + reg];
        }
        cpu.I += last_register + 1;
    }

    void execute_misc_instruction(const Decoded_Opcode& opcode) {
        switch (opcode.nn) {
        case 0x07:
            cpu.registers[opcode.x] = cpu.delay_timer;
            break;
        case 0x15:
            cpu.delay_timer = cpu.registers[opcode.x];
            break;
        case 0x18:
            cpu.sound_timer = cpu.registers[opcode.x];
            break;
        case 0x1E:
            cpu.I += cpu.registers[opcode.x];
            if (cpu.I > 0x0FFF) {
                cpu.registers[0xF] = 1;
            }
            break;
        case 0x0A:
            wait_for_key_release(opcode.x);
            break;
        case 0x29:
            cpu.I = FONT_ADDR + cpu.registers[opcode.x];
            break;
        case 0x33:
            store_bcd(opcode.x);
            break;
        case 0x55:
            store_registers(opcode.x);
            break;
        case 0x65:
            load_registers(opcode.x);
            break;
        default:
            invalid_opcode(opcode);
            break;
        }
    }

    void execute_opcode(const Decoded_Opcode& opcode) {
        switch (opcode.kind) {
        case 0x0:
            execute_system_instruction(opcode);
            break;
        case 0x1:
            cpu.PC = opcode.nnn;
            break;
        case 0x2:
            cpu.stack[cpu.SP++] = cpu.PC;
            cpu.PC = opcode.nnn;
            break;
        case 0x3:
            if (cpu.registers[opcode.x] == opcode.nn) {
                skip_next_instruction();
            }
            break;
        case 0x4:
            if (cpu.registers[opcode.x] != opcode.nn) {
                skip_next_instruction();
            }
            break;
        case 0x5:
            if (cpu.registers[opcode.x] == cpu.registers[opcode.y]) {
                skip_next_instruction();
            }
            break;
        case 0x6:
            cpu.registers[opcode.x] = opcode.nn;
            break;
        case 0x7:
            cpu.registers[opcode.x] += opcode.nn;
            break;
        case 0x8:
            execute_arithmetic_instruction(opcode);
            break;
        case 0x9:
            if (cpu.registers[opcode.x] != cpu.registers[opcode.y]) {
                skip_next_instruction();
            }
            break;
        case 0xA:
            cpu.I = opcode.nnn;
            break;
        case 0xB:
            cpu.PC = opcode.nnn + cpu.registers[0];
            break;
        case 0xC:
            cpu.registers[opcode.x] = rand() & opcode.nn;
            break;
        case 0xD:
            execute_draw_instruction(opcode);
            break;
        case 0xE:
            execute_key_skip_instruction(opcode);
            break;
        case 0xF:
            execute_misc_instruction(opcode);
            break;
        default:
            invalid_opcode(opcode);
            break;
        }
    }

    void cycle() {
        if (!rom_loaded.load()) {
            return;
        }

        std::lock_guard<std::mutex> lock(state_mutex);
        execute_opcode(fetch_opcode());
    }

    bool load_rom(const char* rom_path) {
        if (rom_path == nullptr || rom_path[0] == '\0') {
            printf("Failed to load rom. No file path provided.\n");
            return false;
        }

        FILE* rom = fopen(rom_path, "rb");
        if (rom == NULL) {
            printf("Failed to read file: %s\n", rom_path);
            return false;
        }

        fseek(rom, 0, SEEK_END);
        long rom_size = ftell(rom);
        fseek(rom, 0, SEEK_SET);

        if (rom_size <= 0 || rom_size > (MEMORY_SIZE - PROGRAM_START_ADDR)) {
            printf("Failed to load rom. File is empty or too large: %s\n", rom_path);
            fclose(rom);
            return false;
        }

        std::vector<uint8_t> rom_data((std::size_t)rom_size);
        std::size_t bytes_read = fread(rom_data.data(), 1, rom_data.size(), rom);
        fclose(rom);

        if (bytes_read != rom_data.size()) {
            printf("Failed to read complete rom: %s\n", rom_path);
            return false;
        }

        std::lock_guard<std::mutex> lock(state_mutex);
        initialize_unlocked(false);
        memcpy(&memory[PROGRAM_START_ADDR], rom_data.data(), rom_data.size());
        cpu.PC = PROGRAM_START_ADDR;
        cpu.I = PROGRAM_START_ADDR;
        rom_loaded = true;
        cpu.display = true;
        return true;
    }

    bool is_rom_loaded() {
        return rom_loaded.load();
    }

    Debug_State get_debug_state() {
        std::lock_guard<std::mutex> lock(state_mutex);
        Debug_State state = {};
        state.display = cpu.display.load();
        state.rom_loaded = rom_loaded.load();
        state.PC = cpu.PC;
        state.I = cpu.I;
        state.SP = cpu.SP;
        state.delay_timer = cpu.delay_timer;
        state.sound_timer = cpu.sound_timer;
        memcpy(&state.registers, &cpu.registers, sizeof(state.registers));
        memcpy(&state.stack, &cpu.stack, sizeof(state.stack));
        return state;
    }

    void copy_memory(uint8_t* destination, std::size_t count) {
        if (destination == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(state_mutex);
        memcpy(destination, memory, std::min(count, (std::size_t)MEMORY_SIZE));
    }

    void copy_display_buffer(uint32_t* destination, std::size_t count) {
        if (destination == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(state_mutex);
        memcpy(destination, display_buffer, std::min(count, (std::size_t)(SCREEN_WIDTH * SCREEN_HEIGHT)) * sizeof(uint32_t));
    }

    void clear_screen() {
        std::lock_guard<std::mutex> lock(state_mutex);
        clear_screen_unlocked();
    }

    uint8_t map_hexkey(int scancode) {
        switch (scancode) {
        case 30: // 1 key
            return 0x1;
        case 31: // 2 key
            return 0x2;
        case 32: // 3 key
            return 0x3;
        case 33: // 4 key
            return 0xC;
        case 20: // Q
            return 0x4;
        case 26: // W
            return 0x5;
        case 8:  // E
            return 0x6;
        case 21: // R
            return 0xD;
        case 4:  // A
            return 0x7;
        case 22: // S
            return 0x8;
        case 7:  // D
            return 0x9;
        case 9:  // F
            return 0xE;
        case 29: // Z
            return 0xA;
        case 27: // X
            return 0x0;
        case 6:  // C
            return 0xB;
        case 25: // V
            return 0xF;
        }
        return NO_KEY;
    }

    Keypad* get_keypad() {
        return &keypad;
    }

    void set_keypad(int scancode, bool key_pressed) {
        uint8_t key = CH8::map_hexkey(scancode);
        if (key != CH8::NO_KEY) {
            std::lock_guard<std::mutex> set_key(keypad_mutex);
            keypad.key = key;
            keypad.keys[key] = key_pressed ? 1 : 0;
            keypad.keydown = key_pressed;
        }
    }

    void decrement_delay_timer() {
        std::lock_guard<std::mutex> lock(state_mutex);
        if (cpu.delay_timer > 0) {
            cpu.delay_timer--;
        }
    }

    bool decrement_sound_timer() {
        std::lock_guard<std::mutex> lock(state_mutex);
        if (cpu.sound_timer > 0) {
            cpu.sound_timer--;
            return true;
        }
        return false;
    }

    void set_display_flag() {
        cpu.display = true;
    }

    bool get_display_flag() {
        return cpu.display;
    }
}