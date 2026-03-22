#pragma once

#include <cstdint> // to access 16/8 bit types
#include <string>
using std::string;

class Chip8 {
private:
    static constexpr int MEMORY_SIZE = 4096;
    static constexpr int F = 16;
public:
    uint8_t memory[MEMORY_SIZE]; //4KB RAM
    uint8_t V[F]; // 16 registers (V0-VF)
    uint16_t I; // index register (addresses)
    uint16_t pc; // program counter 

    //stack
    uint8_t sp; // stack pointer
    uint16_t stack[F]; //stack size 16

    //delay & timer
    uint8_t dt; // delay timer
    uint8_t st; // sound timer

    uint8_t display[64 * 32]; // display in pixles
    uint8_t keyboard[F]; // keyboard 0-F (in hex)

    Chip8(); // c'tor
    void LoadROM(string path);
};