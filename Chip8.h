#pragma once

#include <cstdint> // to access 16/8 bit types
#include <string>
#include <random>
#include <iostream>
using std::string;
using std::ofstream;

// to save snapshots
struct Chip8State{
    uint8_t V[16];
    uint16_t I;
    uint16_t pc;
    uint16_t ir;
    uint8_t sp;
    uint8_t dt;
    uint8_t st;
};

/* CHIP8 QUIRKS
    as I was testing and debugging the chip
    I realised that there is no "right" way
        to implement the instructions and 
        there is a modern way and original way
    so I came up with the idea to have a flags
        that trigger to which version of the 
        instruction we want to use

    HERE CAME THE IDEA OF THE QUIRKS
*/




class Chip8 {
public:
    /* Settings
        I will be handling the settings and the flags 
        using this struct which eventually will be 
        a member in the CHIP8 class.
        in short the settings will be triggered using
        flags and paths in the arguments
    */
    struct Settings { 
        string romPath = "";
        string logPath = "";
        bool log = false;
        bool modernShift = false;
        bool incrementI = false;

        static Chip8::Settings& ParseArgs(int argc ,const char** argv);
    };

    Chip8(Settings& settings); // c'tor
    void LoadROM(const string& path); 
    void Cycle(); // fetch decode execute
    void SetKey(uint8_t key, bool isPressed);
    const uint8_t* GetDisplay() const {return display;}
    void ActivateDT();
    void ActivateST();
    bool ShouldRender() const {return shouldRender;}
    void DeActivateRender() {shouldRender = false;}
    Chip8State GetState() const;
    void LoadState(const Chip8State& state);
    void ToggleLog(const char* path, ofstream& logFile, bool& running); // i may later use exception handling

    
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

    int waitingForKeyIndex = -1; // -1 means we aren't waiting
    bool logEnabled = false;

    Settings settings;

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