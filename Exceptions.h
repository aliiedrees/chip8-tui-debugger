#pragma once

#include <stdexcept>
#include <string>
#include <cstdint>
#include <format>

class Chip8Exception : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

// 1. Invalid ROM
class InvalidRomPathException : public Chip8Exception {
public:
    InvalidRomPathException(const std::string& path) 
        : Chip8Exception("ROM Error: Could not find or open file at [" + path + "]") {}
};

// 2. Invalid Flag
class InvalidArgumentException : public Chip8Exception {
public:
    InvalidArgumentException(const std::string& flag) 
        : Chip8Exception("CLI Error: Unsupported or malformed flag [" + flag + "]") {}
};

// 3. Unkown instruction
class UnknownOpcodeException : public Chip8Exception {
public:
    UnknownOpcodeException(uint16_t opcode) 
        : Chip8Exception("CPU Fault: Attempted to execute unknown instruction [0x" + toHex(opcode) + "]") {}
private:
    static std::string toHex(uint16_t val) {
        char buf[5];
        sprintf(buf, "%04X", val);
        return std::string(buf);
    }
};

// 4. Wrong Usage
class WrongUsageException : public Chip8Exception {
public:
    WrongUsageException(const char* arg)
        : Chip8Exception("Usage: " + string(arg) + " <ROM_PATH> [OPTIONS]\n\n"
        + "A high-performance Chip-8 interpreter with configurable hardware quirks.\n\n"
        +"CORE OPTIONS:\n"
        + "  -h, --help                Display this help message and exit\n"
        + "  -l, --log <PATH>          Enable instruction logging to file\n"
        + "  -t, --ticks <N>           Instructions per 60Hz frame [Default: 11] N in [1,15000]\n\n"
        + "EMULATION QUIRKS (Modern SCHIP by default; Flags enable 1977 logic):\n"
        + "  -s, --shift               Original Shift (VX = VY before shift)\n"
        + "  -i, --increment           Original Load/Store (I = I + X + 1)\n"
        + "  -j, --jump                Original Jump (Jump to NNN + V0)\n"
        + "  -f, --vf-reset            Original Logic (AND/OR/XOR reset VF to 0)\n"
        + "  -w, --wrap                Original Display (Sprites wrap screen edges)\n"){}
};

// 5. Ticks was not a number
class TicksNotIntException : public Chip8Exception {
public:
    TicksNotIntException(const char* arg)
        : Chip8Exception("Error: " + string(arg) +" is not a valid number for ticks."){}
};

// 6. Ticks out of range
class TicksOutOfRangeException : public Chip8Exception {
public:
    TicksOutOfRangeException()
        : Chip8Exception("Error: ticks range is [1, 15000]."){};
};

// 7. Rom File too big
class RomFileTooBigException : public Chip8Exception {
public:
    RomFileTooBigException(const string& file, int maxsize)
        : Chip8Exception("ROM Error: File [" + file + "] exceeds memory limits. " 
                         "Max allowed size is " + std::to_string(maxsize) + " bytes."){}
};

// 8. PC out of bonds
class PcOutOfBondsException : public Chip8Exception {
public:
    explicit PcOutOfBondsException(uint16_t pc)
        : Chip8Exception("PC out of bounds! PC: 0x"+ toHex(pc)){}
private:
    static std::string toHex(uint16_t val) {
        char buf[5];
        sprintf(buf, "%04X", val);
        return std::string(buf);
    }
};

// 9. Invalid Log File
class InvalidLogPathException : public Chip8Exception {
public:
    explicit InvalidLogPathException(const std::string& path) 
        : Chip8Exception("LOG Error: Could not find or open file at [" + path + "]") {}
};





