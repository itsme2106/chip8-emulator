#include "chip8.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <cstring>
#include <random>

uint8_t chip8_fontset[80] = {
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

Chip8::Chip8(){
    initialise();
}

void Chip8::initialise(){
    pc = 0x200;
    opcode = 0;
    index = 0;
    sp = 0;

    memset(display, 0, sizeof(display));
    memset(stack, 0, sizeof(stack));
    memset(v, 0, sizeof(v));
    memset(memory, 0, sizeof(memory));
    memset(key, 0, sizeof(key));

    load_fonts();

    delay_timer = 0;
    sound_timer = 0;
    draw_flag = false;
}

void Chip8::load_fonts(){
    for(int i = 0; i < 80; i++)
        memory[i] = chip8_fontset[i];
}

void Chip8::load_rom(const std::string& filename){
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    if(!file.is_open()){
        std::cerr << "Failed to open ROM: " << filename << std::endl;
        return;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if(size > (4096 - 512)){
        std::cerr << "ROM too large to fit in memory" << std::endl;
        return;
    }

    file.read((char*)(memory + 512), size);
    file.close();

    std::cout << "Loaded ROM: " << filename << std::endl;
}

void Chip8::emulate_cycle(){

    // FETCH
    opcode = memory[pc] << 8 | memory[pc + 1];

    // DECODE + EXECUTE
    switch(opcode & 0xF000){

        // 0XXX
        case 0x0000:

            switch(opcode & 0x00FF){

                // 00E0 = Clear screen
                case 0x00E0:
                    std::memset(display, 0, sizeof(display));
                    draw_flag = true;
                    pc += 2;
                    break;

                // 00EE = Return from subroutine
                case 0x00EE:
                    sp--;
                    pc = stack[sp] + 2;
                    break;

                default:
                    std::cerr << "Unknown opcode: 0x"
                              << std::hex << opcode << std::endl;
                    pc += 2;
                    break;
            }

            break;


        // 1XXX = Jump
        case 0x1000:
            pc = opcode & 0x0FFF;
            break;


        // 2XXX = Call subroutine
        case 0x2000:
            stack[sp] = pc;
            sp++;
            pc = opcode & 0x0FFF;
            break;


        // 3XNN = Skip if VX == NN
        case 0x3000:
            if(v[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF))
                pc += 4;
            else
                pc += 2;
            break;


        // 4XNN = Skip if VX != NN
        case 0x4000:
            if(v[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF))
                pc += 4;
            else
                pc += 2;
            break;


        // 5XY0 = Skip if VX == VY
        case 0x5000:
            if((opcode & 0x000F) != 0){
                std::cerr << "Unknown opcode: 0x"
                          << std::hex << opcode << std::endl;
                pc += 2;
                break;
            }

            if(v[(opcode & 0x0F00) >> 8] ==
               v[(opcode & 0x00F0) >> 4])
                pc += 4;
            else
                pc += 2;

            break;


        // 6XNN = VX = NN
        case 0x6000:
            v[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
            pc += 2;
            break;


        // 7XNN = VX += NN
        case 0x7000:
            v[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
            pc += 2;
            break;


        // 8XYN = Arithmetic
        case 0x8000:

            switch(opcode & 0x000F){

                // 8XY0 = VX = VY
                case 0x0000:
                    v[(opcode & 0x0F00) >> 8] =
                        v[(opcode & 0x00F0) >> 4];
                    pc += 2;
                    break;


                // 8XY1 = VX |= VY
                case 0x0001:
                    v[(opcode & 0x0F00) >> 8] |=
                        v[(opcode & 0x00F0) >> 4];
                    pc += 2;
                    break;


                // 8XY2 = VX &= VY
                case 0x0002:
                    v[(opcode & 0x0F00) >> 8] &=
                        v[(opcode & 0x00F0) >> 4];
                    pc += 2;
                    break;


                // 8XY3 = VX ^= VY
                case 0x0003:
                    v[(opcode & 0x0F00) >> 8] ^=
                        v[(opcode & 0x00F0) >> 4];
                    pc += 2;
                    break;


                // 8XY4 = VX += VY, VF = carry
                case 0x0004:{
                    uint16_t sum =
                        v[(opcode & 0x0F00) >> 8] +
                        v[(opcode & 0x00F0) >> 4];

                    v[0xF] = (sum > 0xFF) ? 1 : 0;

                    v[(opcode & 0x0F00) >> 8] =
                        sum & 0xFF;

                    pc += 2;
                    break;
                }


                // 8XY5 = VX -= VY, VF = NOT borrow
                case 0x0005:{
                    uint8_t x = (opcode & 0x0F00) >> 8;
                    uint8_t y = (opcode & 0x00F0) >> 4;

                    v[0xF] = (v[x] >= v[y]) ? 1 : 0;
                    v[x] -= v[y];

                    pc += 2;
                    break;
                }


                // 8XY6 = VX >>= 1, VF = LSB
                case 0x0006:{
                    uint8_t x = (opcode & 0x0F00) >> 8;

                    v[0xF] = v[x] & 0x1;
                    v[x] >>= 1;

                    pc += 2;
                    break;
                }


                // 8XY7 = VX = VY - VX, VF = NOT borrow
                case 0x0007:{
                    uint8_t x = (opcode & 0x0F00) >> 8;
                    uint8_t y = (opcode & 0x00F0) >> 4;

                    v[0xF] = (v[y] >= v[x]) ? 1 : 0;
                    v[x] = v[y] - v[x];

                    pc += 2;
                    break;
                }


                // 8XYE = VX <<= 1, VF = MSB
                case 0x000E:{
                    uint8_t x = (opcode & 0x0F00) >> 8;

                    v[0xF] = v[x] >> 7;
                    v[x] <<= 1;

                    pc += 2;
                    break;
                }


                default:
                    std::cerr << "Unknown opcode: 0x"
                              << std::hex << opcode << std::endl;
                    pc += 2;
                    break;
            }

            break;


        // 9XY0 = Skip if VX != VY
        case 0x9000:

            if((opcode & 0x000F) != 0){
                std::cerr << "Unknown opcode: 0x"
                          << std::hex << opcode << std::endl;
                pc += 2;
                break;
            }

            if(v[(opcode & 0x0F00) >> 8] !=
               v[(opcode & 0x00F0) >> 4])
                pc += 4;
            else
                pc += 2;

            break;


        // ANNN = I = NNN
        case 0xA000:
            index = opcode & 0x0FFF;
            pc += 2;
            break;


        // BNNN = Jump to NNN + V0
        case 0xB000:
            pc = (opcode & 0x0FFF) + v[0];
            break;


        // CXNN = VX = random & NN
        case 0xC000:{
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<> dist(0, 255);

            v[(opcode & 0x0F00) >> 8] =
                dist(gen) & (opcode & 0x00FF);

            pc += 2;
            break;
        }


        // DXYN = Draw sprite
        case 0xD000:{
            uint8_t x = v[(opcode & 0x0F00) >> 8];
            uint8_t y = v[(opcode & 0x00F0) >> 4];
            uint8_t height = opcode & 0x000F;

            v[0xF] = 0;

            for(int y_line = 0; y_line < height; y_line++){

                uint8_t pixel = memory[index + y_line];

                for(int x_line = 0; x_line < 8; x_line++){

                    if((pixel & (0x80 >> x_line)) != 0){

                        // Wrap around screen
                        int screen_x =
                            (x + x_line) % 64;

                        int screen_y =
                            (y + y_line) % 32;

                        int screen_index =
                            screen_x + (screen_y * 64);

                        if(display[screen_index] == 1)
                            v[0xF] = 1;

                        display[screen_index] ^= 1;
                    }
                }
            }

            draw_flag = true;
            pc += 2;

            break;
        }


        // EXXX = Keyboard
        case 0xE000:

            switch(opcode & 0x00FF){

                // EX9E = Skip if key VX pressed
                case 0x009E:
                    if(key[v[(opcode & 0x0F00) >> 8]] != 0)
                        pc += 4;
                    else
                        pc += 2;
                    break;


                // EXA1 = Skip if key VX not pressed
                case 0x00A1:
                    if(key[v[(opcode & 0x0F00) >> 8]] == 0)
                        pc += 4;
                    else
                        pc += 2;
                    break;


                default:
                    std::cerr << "Unknown opcode: 0x"
                              << std::hex << opcode << std::endl;
                    pc += 2;
                    break;
            }

            break;


        // FXXX
        case 0xF000:

            switch(opcode & 0x00FF){

                // FX07 = VX = delay timer
                case 0x0007:
                    v[(opcode & 0x0F00) >> 8] = delay_timer;
                    pc += 2;
                    break;


                // FX0A = Wait for key press
                case 0x000A:{
                    bool key_pressed = false;

                    for(int i = 0; i < 16; i++){

                        if(key[i] != 0){

                            v[(opcode & 0x0F00) >> 8] = i;
                            key_pressed = true;
                            break;
                        }
                    }

                    // IMPORTANT:
                    // Do NOT advance PC until a key is pressed
                    if(key_pressed)
                        pc += 2;

                    break;
                }


                // FX15 = delay timer = VX
                case 0x0015:
                    delay_timer =
                        v[(opcode & 0x0F00) >> 8];

                    pc += 2;
                    break;


                // FX18 = sound timer = VX
                case 0x0018:
                    sound_timer =
                        v[(opcode & 0x0F00) >> 8];

                    pc += 2;
                    break;


                // FX1E = I += VX
                case 0x001E:
                    index += v[(opcode & 0x0F00) >> 8];
                    pc += 2;
                    break;


                // FX29 = I = location of font digit VX
                case 0x0029:
                    index =
                        v[(opcode & 0x0F00) >> 8] * 5;

                    pc += 2;
                    break;


                // FX33 = BCD
                case 0x0033:{
                    uint8_t value =
                        v[(opcode & 0x0F00) >> 8];

                    memory[index] =
                        value / 100;

                    memory[index + 1] =
                        (value / 10) % 10;

                    memory[index + 2] =
                        value % 10;

                    pc += 2;
                    break;
                }


                // FX55 = Store V0 through VX
                case 0x0055:{
                    uint8_t x =
                        (opcode & 0x0F00) >> 8;

                    for(int i = 0; i <= x; i++)
                        memory[index + i] = v[i];

                    pc += 2;
                    break;
                }


                // FX65 = Load V0 through VX
                case 0x0065:{
                    uint8_t x =
                        (opcode & 0x0F00) >> 8;

                    for(int i = 0; i <= x; i++)
                        v[i] = memory[index + i];

                    pc += 2;
                    break;
                }


                default:
                    std::cerr << "Unknown opcode: 0x"
                              << std::hex << opcode << std::endl;

                    pc += 2;
                    break;
            }

            break;


        default:
            std::cerr << "Unknown opcode: 0x"
                      << std::hex << opcode << std::endl;

            pc += 2;
            break;
    }


    // Timer update
    if(delay_timer > 0)
        delay_timer--;

    if(sound_timer > 0){

        if(sound_timer == 1)
            std::cout << "BEEP!" << std::endl;

        sound_timer--;
    }
}