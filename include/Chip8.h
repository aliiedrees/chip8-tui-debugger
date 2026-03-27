#pragma once

#include <cstdint> // to access 16/8 bit types
#include <string>
#include <random>
#include <iostream>
#include <array>

using std::string;
using std::ofstream;
using std::array;

/* CHIP8 QUIRKS
    as I was testing and debugging the chip
    I realised that there is no "right" way
        to implement the instructions and 
        there is a modern way and original way
    so I came up with the idea to have flags
        that trigger to which version of the 
        instruction we want to use

    HERE CAME THE IDEA OF THE QUIRKS
*/

/* Flags used in the command line + unkown incase
*/
enum class Flag {
    Unkown,
    Help, 
    Log, 
    Ticks, 
    Shift, 
    Increment, 
    Jump, 
    VfPreserve, 
    Wrap
};

/* Primary instruction groups decoded from the most significant 4 bits of the 16-bit opcode.
*/
enum class OpType{
    SYSTEM = 0x0,
    JUMP = 0x1,
    CALL = 0x2,
    SKIP_EQ_BYTE = 0x3,
    SKIP_NE_BYTE = 0x4,
    SKIP_EQ_REG = 0x5,
    SET_REG_BYTE = 0x6,
    ADD_REG_BYTE = 0x7,
    ARITHMETIC = 0x8,
    SKIP_NE_REG = 0x9,
    SET_INDEX = 0xA,
    JUMP_REL = 0xB,
    RANDOM = 0xC,
    DISPLAY = 0xD,
    INPUT = 0xE,
    UTILITY = 0xF
};

class Chip8 {
private:
    static constexpr int MEMORY_SIZE = 4096;
    static constexpr int F = 15;
    static constexpr int START_OF_PROGRAM = 0x200;
    static constexpr int DISPLAY_WIDTH = 64;
    static constexpr int DISPLAY_HEIGHT = 32;
    static constexpr int BYTE = 8;

    using OpFunc = void (Chip8::*)();
public:
    /** 
        * @struct Settings
        * @brief Handles emulator configuration and quirk flags.
        * * This struct is a member of the CHIP8 class. Settings are 
        * typically toggled via command-line arguments or config files.
        *
        * @note if the flag is not triggered the modern quirk is set by default.
        */
    struct Settings { 
        string romPath = "";
        string logPath = "trace.log"; // default
        bool log = false;
        bool logFlag = false;
        bool shift = false;
        bool increment = false;
        int ticksPerFrame = 11; // default
        bool jump = false;
        bool vfPreserve = false;
        bool wrap = false;
       
        // run time settings
        bool running = true;
        bool paused = true;
        bool restart = false;
        static Chip8::Settings ParseArgs(int argc ,const char** argv);
    };
    
    /** 
        * @struct State
        * @brief state is used to take a snapshot of the current state of the chip
        * @note will be useful later when implementing the backwards feature.
    */
    struct State{
        uint8_t V[16];
        uint16_t I;
        uint16_t pc;
        uint16_t ir;
        uint8_t sp;
        uint8_t dt;
        uint8_t st;
        uint16_t stack[0xF + 1];
        uint8_t memory[MEMORY_SIZE];
        Chip8::Settings settings;
    };

    explicit Chip8(const Settings& settings);
    void LoadROM();
    /**
     * @brief runs a single instruction and logs it.
     * @details runs a single instruction through the three stages fetch decode execute
     * + logs the instruction in the log in case log is enabled.
     * @note COULD THROW EXCEPTION
     *  */ 
    void Cycle();
    /**
     * @brief Restarts the program
     */
    void Restart();
    void SetKey(uint8_t key, bool isPressed);
    const uint8_t* GetDisplay() const {return display;}
    void ActivateDT();
    void ActivateST();
    bool ShouldRender() const {return shouldRender;}
    void ToggleRender() {shouldRender = !shouldRender;}
    Chip8::State GetState() const;
    void LoadState(const Chip8::State& state);
    /**
     * @brief Turn logging on/off
     * @note COULD THROW EXCEPTION
     */
    void ToggleLog(ofstream& logFile);
    bool IsLogging() const {return settings.log;}
    bool LogFlag() const {return settings.logFlag;}
    bool IsRunning() const {return settings.running;}
    void Stop() {settings.running = false;}
    bool IsPaused() const {return settings.paused;}
    void TogglePaused() { settings.paused = !settings.paused;}
    bool ShouldRestart() const {return settings.restart;}
    void ToggleRestart() { settings.restart = !settings.restart;}


    static std::string Disassemble(const uint16_t ir); ///
private:
    void LogCycle() const;

    /* --- Instructions ---*/

    void IN_0000(); void IN_00E0(); void IN_00EE(); void IN_1NNN(); 
    void IN_2NNN(); void IN_3XKK(); void IN_4XKK(); void IN_5XY0(); 
    void IN_6XKK(); void IN_7XKK(); void IN_8XY0(); void IN_8XY1(); 
    void IN_8XY2(); void IN_8XY3(); void IN_8XY4(); void IN_8XY5(); 
    void IN_8XY6(); void IN_8XY7(); void IN_8XYE(); void IN_9XY0();
    void IN_ANNN(); void IN_BNNN(); void IN_CXKK(); void IN_DXYN();
    void IN_EX9E(); void IN_EXA1(); void IN_FX07(); void IN_FX0A(); 
    void IN_FX15(); void IN_FX18(); void IN_FX1E(); void IN_FX29();
    void IN_FX33(); void IN_FX55(); void IN_FX65(); void IN_NULL();
    
    void Fetch();
    void DecodeExecute();
 
    void InitializeDispatches();
    void InitializeMainDispatch();
    void InitializeDispatch0();
    void InitializeDispatch5();
    void InitializeDispatch8();
    void InitializeDispatch9();
    void InitializeDispatchE();
    void InitializeDispatchF();

    void Dispatch0();
    void Dispatch5();
    void Dispatch8();
    void Dispatch9();
    void DispatchE();
    void DispatchF();
    

    uint8_t& GetVX();
    uint8_t& GetVY();
    uint8_t GetNibble(int i) const;
    uint8_t GetKK() const;
    uint16_t GetNNN() const;
    OpType GetOpType() const;
    
    /* Main Dispatch
    */
    array<OpFunc, 0xF + 1> dispatch;
    /* Sub Dispatch for the SYSTEM type
    */
    array<OpFunc, 0xFF + 1> dispatch0;
    /* Sub Dispatch to control unknown ops in 0x5 type*/
    array<OpFunc, 0xF + 1> dispatch5;
    /* Sub Dispatch for the ARITHMETIC type
    */    
    array<OpFunc, 0xF + 1> dispatch8;
    /* Sub Dispatch to control unknown ops in 0x9 type*/
    array<OpFunc, 0xF + 1> dispatch9;
    /* Sub Dispatch for the INPUT type
    */
    array<OpFunc, 0xFF + 1> dispatchE;
    /* Sub Dispatch for the UTILITY type
    */
    array<OpFunc, 0xFF + 1> dispatchF;
    
    // random generator
    std::mt19937 generator;
    std::uniform_int_distribution<uint8_t> dist;

    uint8_t memory[MEMORY_SIZE]; //4KB RAM
    uint8_t V[F+1]; // 16 registers (V0-VF)
    uint16_t I; // index register (addresses)
    uint16_t pc; // program counter 
    uint16_t ir; // usually called IR register holds the current instruction
    uint8_t sp; // stack pointer
    uint16_t stack[F+1]; //stack size 16

    //delay & timer
    uint8_t dt; // delay timer
    uint8_t st; // sound timer

    bool shouldRender;
    uint8_t display[DISPLAY_HEIGHT * DISPLAY_WIDTH]; // display in pixles
    uint8_t keyboard[0xF+1]; // keyboard 0-F (in hex)

    int waitingForKeyIndex = -1; // -1 means we aren't waiting

    Settings settings;


};