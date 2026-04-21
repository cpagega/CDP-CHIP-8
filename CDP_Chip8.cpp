#include "CDP_Chip8.h"

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

uint8_t memory[4096] = { 0 };


void CDP_Chip8_reset(CDP_CHIP8_CPU* cpu, CDP_CHIP8_GUI* gui) {
    srand(time(NULL));
    cpu->PC = 0x200;
    cpu->I = 0;
    cpu->SP = 0;
    cpu->delay_timer = 0;
    cpu->sound_timer = 0;
    memset(&memory, 0, sizeof(memory));
    memcpy(&memory[FONT_ADDRESS], font, sizeof(font));
    memset(&cpu->registers, 0, sizeof(cpu->registers));
    memset(&cpu->stack, 0, sizeof(cpu->stack));
    CDP_Chip8_clear_screen(gui);
    CDP_Chip8_load_rom();
}

void CDP_Chip8_cycle(CDP_CHIP8_CPU* cpu, CDP_CHIP8_GUI* gui, CDP_CHIP8_Keypad* keypad) {
    //fetch
    uint8_t hi_byte = memory[cpu->PC++];
    uint8_t lo_byte = memory[cpu->PC++];
    //decode
    uint16_t opcode = ((uint16_t)hi_byte << 8 | lo_byte);
    uint8_t kind = (opcode & 0xF000) >> 12;
    uint8_t VX = (opcode & 0x0F00) >> 8;
    uint8_t VY = (opcode & 0x00F0) >> 4;
    uint8_t N = (opcode & 0x000F);
    uint8_t NN = (opcode & 0x00FF);
    uint16_t NNN = (opcode & 0x0FFF);
    //execute
   // printf("Opcode: 0x%04X | kind: %X, X: %X, Y: %X, N: 0x%04X, NN: 0x%04X, NNN: 0x%04X\n", opcode, kind, VX, VY, N, NN, NNN);
    switch (kind) {
    case 0x0:
        // clear screen
        if (opcode == 0x00E0) {
            CDP_Chip8_clear_screen(gui);
        }
        // return from function
        if (opcode == 0x00EE) {
            cpu->PC = cpu->stack[--cpu->SP];
        }
        break;
        // Jump to NNN
    case 0x1:
        cpu->PC = NNN;
        break;
    case 0x2:
        // Call
        cpu->stack[cpu->SP++] = cpu->PC;
        cpu->PC = NNN;
        break;
        // Skip next instruction
    case 0x3:
        if (cpu->registers[VX] == NN) {
            cpu->PC += 2;
        }
        break;
        // Skip next instruction
    case 0x4:
        if (cpu->registers[VX] != NN) {
            cpu->PC += 2;
        }
        break;
        // Skip next instruction
    case 0x5:
        if (cpu->registers[VX] == cpu->registers[VY]) {
            cpu->PC += 2;
        }
        break;
        // Set VX
    case 0x6:
        cpu->registers[VX] = NN;
        break;
        // Add to VX
    case 0x7:
        cpu->registers[VX] += NN;
        break;
    case 0x8:
        // Logical and Arithmetic instructions
        switch (N) {
            // Set assign VY to VX
        case 0x0:
            cpu->registers[VX] = cpu->registers[VY];
            cpu->registers[0xF] = 0;
            break;
            // OR
        case 0x1:
            cpu->registers[VX] |= cpu->registers[VY];
            cpu->registers[0xF] = 0;
            break;
            // AND
        case 0x2:
            cpu->registers[VX] &= cpu->registers[VY];
            cpu->registers[0xF] = 0;
            break;
            // XOR
        case 0x3:
            cpu->registers[VX] ^= cpu->registers[VY];
            cpu->registers[0xF] = 0;
            break;
            // VX + VY
        case 0x4:
        {
            uint8_t a = cpu->registers[VX];
            uint8_t b = cpu->registers[VY];
            uint8_t sum = a + b;
            uint8_t carry = (sum < a) ? 1 : 0;
            cpu->registers[VX] = sum;
            cpu->registers[0xF] = carry;
        }
        break;
            // VX - VY 
        case 0x5:
        {

            uint8_t a = cpu->registers[VX];
            uint8_t b = cpu->registers[VY];
            uint8_t diff = a - b;
            uint8_t carry = (a >= b) ? 1 : 0;
            cpu->registers[VX] = diff;
            cpu->registers[0xF] = carry;
        }
        break;
            // RSH
        case 0x6:
        {
            uint8_t a = cpu->registers[VY];
            cpu->registers[VX] = a >> 1;
            cpu->registers[0xF] = a & 0x1;

        }
        break;
            // VY - VX
        case 0x7:
        {
            uint8_t a = cpu->registers[VY];
            uint8_t b = cpu->registers[VX];
            uint8_t diff = a - b;
            uint8_t carry = (a >= b) ? 1 : 0;
            cpu->registers[VX] = diff;
            cpu->registers[0xF] = carry;
        }
        break;
            // LSH
        case 0xE:
        {
            uint8_t a = cpu->registers[VY];
            cpu->registers[VX] = a << 1;
            cpu->registers[0xF] = (a >> 7) & 1;

        }
        break;
        default:
            printf("Invalid opcode. Sucks to be you.\n");
            printf("Opcode: 0x%04X | kind: %X, X: %X, Y: %X, N: 0x%04X, NN: 0x%04X, NNN: 0x%04X\n", opcode, kind, VX, VY, N, NN, NNN);
            break;
        }
        break;
        // Skip
    case 0x9:
        if (cpu->registers[VX] != cpu->registers[VY]) {
            cpu->PC += 2;
        }
        break;
        // Set I to address
    case 0xA:
        cpu->I = NNN;
        break;
        // Jump
    case 0xB:
        cpu->PC = NNN + cpu->registers[0];
        break;
        // Rand
    case 0xC:
    {
        int random_number = rand();
        cpu->registers[VX] = random_number & NN;
    }
    break;
    // Draw
    case 0xD:
        if (!cpu->display) {
            cpu->PC -= 2;
            return;
        }
        cpu->display = false;
    {
        uint8_t x = cpu->registers[VX] % SCREEN_WIDTH;
        uint8_t y = cpu->registers[VY] % SCREEN_HEIGHT;
        cpu->registers[0xF] = 0;

        for (uint8_t row = 0; row < N; row++) {
            uint8_t sprite_row = memory[cpu->I + row];

            for (uint8_t bit = 0; bit < 8; bit++) {
                uint8_t px = x + bit;
                uint8_t py = y + row;

                if (px >= SCREEN_WIDTH || py >= SCREEN_HEIGHT) {
                    continue;
                }

                if ((sprite_row & (0x80 >> bit)) == 0) {
                    continue;
                }

                uint16_t index = py * SCREEN_WIDTH + px;

                if (gui->display_buffer[index] == 0xFFFFFFFF) {
                    cpu->registers[0xF] = 1;
                    gui->display_buffer[index] = 0xFF000000;
                } else {
                    gui->display_buffer[index] = 0xFFFFFFFF;
                }
            }
        }
    }
    break;
    // Skip
    case 0xE:
    {
        uint8_t key = cpu->registers[VX];
        // Skip if key pressed
        if (NN == 0x9E && keypad->keys[key]) {
            cpu->PC += 2;
        }
        // Skip if key not pressed
        if (NN == 0xA1 && !keypad->keys[key]) {
            cpu->PC += 2;
        }
    }
    break;

    case 0xF:
        switch (NN) {
        case 0x07:
            // Get the delay timer value and assign to VX
            cpu->registers[VX] = cpu->delay_timer;
            break;
            // Set the delay timer to VX
        case 0x15:
            cpu->delay_timer = cpu->registers[VX];
            break;
            // Set the sound timer to VX
        case 0x18:
            cpu->sound_timer = cpu->registers[VX];
            break;
            // Increment I by VX
        case 0x1E:
            cpu->I += cpu->registers[VX];
            if (cpu->I > 0x0FFF) {
                cpu->registers[0xF] = 1;
            }
            break;
            // Get Key press. this is a blocking call
        case 0x0A:
            // TODO: this may not be exactly to chip8 spec, need to revisit
            if (keypad->keydown) {
                cpu->registers[VX] = keypad->key;
            } else {
                cpu->PC -= 2;
            }
            break;
            // Set I to the location of the FONT sprite in memory
        case 0x29:
            cpu->I = FONT_ADDRESS + cpu->registers[VX];
            break;
            // Load the binary-coded decimal value of VX to memory
        case 0x33:
        {
            uint8_t number = cpu->registers[VX];
            memory[cpu->I + 2] = number % 10;
            number /= 10;
            memory[cpu->I + 1] = number % 10;
            number /= 10;
            memory[cpu->I] = number % 10;
        }
        break;
        case 0x55: // Store
            for (uint8_t reg = 0; reg <= VX; reg++) {
                memory[cpu->I + reg] = cpu->registers[reg];
            }
            cpu->I += VX + 1;
            break;
        case 0x65: // Load
            for (uint8_t reg = 0; reg <= VX; reg++) {
                cpu->registers[reg] = memory[cpu->I + reg];
            }
            cpu->I += VX + 1;
            break;
        default:
            printf("Invalid opcode. Sucks to be you.\n");
            printf("Opcode: 0x%04X | kind: %X, X: %X, Y: %X, N: 0x%04X, NN: 0x%04X, NNN: 0x%04X\n", opcode, kind, VX, VY, N, NN, NNN);
            break;
        }
        break;
    default:
        printf("Invalid opcode. Sucks to be you.\n");
        printf("Opcode: 0x%04X | kind: %X, X: %X, Y: %X, N: 0x%04X, NN: 0x%04X, NNN: 0x%04X\n", opcode, kind, VX, VY, N, NN, NNN);
        break;
    }
}

void CDP_Chip8_render(CDP_CHIP8_GUI* gui) {
    SDL_UpdateTexture(gui->texture, NULL, gui->display_buffer, 256);
    SDL_RenderTexture(gui->renderer, gui->texture, NULL, NULL);
    SDL_RenderPresent(gui->renderer);
}
void CDP_Chip8_load_rom() {
    FILE* rom;
    fopen_s(&rom, "Space Intercept [Joseph Weisbecker, 1978].ch8", "rb");
    if (rom == NULL) {
        printf("Failed to read file\n");
        return;
    }
    fseek(rom, 0, SEEK_END);
    long rom_size = ftell(rom);
    fseek(rom, 0, SEEK_SET);
    fread(&memory[0x200], 1, rom_size, rom);
}

void CDP_Chip8_clear_screen(CDP_CHIP8_GUI* gui) {
    SDL_RenderClear(gui->renderer);
    SDL_RenderPresent(gui->renderer);
    memset(&gui->display_buffer, 0, sizeof(gui->display_buffer));
}

SDL_Window* CDP_Chip8_init_window() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL could not initialize: %s", SDL_GetError());
        return {};
    }
    SDL_Window* window = SDL_CreateWindow("CDP-CHIP-8", 1920, 1080, 0);
    if (!window) {
        SDL_Log("Window could not be created: %s", SDL_GetError());
        SDL_Quit();
        return {};
    }
    return window;
}

uint8_t CDP_Chip8_map_hexkey(int scancode) {
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
    return 0xFF;
}