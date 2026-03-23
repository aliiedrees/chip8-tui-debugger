#pragma once

#include <cstdint> // to access 16/8 bit types
#include <string>
#include <random>
using std::string;

class Chip8 {
private:
    static constexpr int MEMORY_SIZE = 4096;
    static constexpr int F = 15;
    static constexpr int START_OF_PROGRAM = 0x200;
    static constexpr int DISPLAY_WIDTH = 64;
    static constexpr int DISPLAY_HEIGHT = 32;
    static constexpr int BYTE = 8;

    void IN_00E0();
    void IN_00EE();
    void IN_1NNN();
    void IN_2NNN();
    void IN_3XKK();
    void IN_4XKK();
    void IN_5XY0();
    void IN_6XKK();
    void IN_7XKK();
    void IN_8XY0();
    void IN_8XY1();
    void IN_8XY2();
    void IN_8XY3();
    void IN_8XY4();
    void IN_8XY5();
    void IN_8XY6();
    void IN_8XY7();
    void IN_8XYE();
    void IN_9XY0();
    void IN_ANNN();
    void IN_BNNN();
    void IN_CXKK();
    void IN_DXYN();
    void IN_EX9E();
    void IN_EXA1();
    void IN_FX07();
    void IN_FX0A();
    void IN_FX15();
    void IN_FX18();
    void IN_FX1E();
    void IN_FX29();
    void IN_FX33();
    void IN_FX55();
    void IN_FX65();

    uint8_t& GetVX();
    const uint8_t& GetVX() const; 
    uint8_t& GetVY();
    const uint8_t& GetVY() const;

    void UnknownInstruction(const uint16_t& instruction);


    // random generator
    std::mt19937 generator;
    std::uniform_int_distribution<uint8_t> dist;
public:
    uint8_t memory[MEMORY_SIZE]; //4KB RAM
    uint8_t V[F+1]; // 16 registers (V0-VF)
    uint16_t I; // index register (addresses)
    uint16_t pc; // program counter 
    uint16_t ir; // usually called IR register holds the current instruction
    //stack
    uint8_t sp; // stack pointer
    uint16_t stack[F+1]; //stack size 16

    //delay & timer
    uint8_t dt; // delay timer
    uint8_t st; // sound timer

    bool shouldRender;
    uint8_t display[DISPLAY_HEIGHT * DISPLAY_WIDTH]; // display in pixles
    uint8_t keyboard[F+1]; // keyboard 0-F (in hex)

    Chip8(); // c'tor
    void LoadROM(const string& path); 
    void Cycle(); // fetch decode execute
};
/* display
(0,0)                                              (63,0)
 ................................................................
 ..........................########..............................
 ..........................#......#..............................
 ..........................#......#..............................
 ..........................########..............................
 ................................................................
(0,31)                                             (63,31)
*/