#pragma once

#include <stdexcept>
#include <string>
#include <cstdint>

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
class ShowUsageException : public Chip8Exception {
    ShowUsageException(const char* arg)
        : Chip8Exception("Usage: " + std::string(arg) + " <ROM_PATH> [OPTIONS]\n"
                       + "OPTIONS:\n"
                       + "\t[-l] or [--log] <LOG_PATH>(Optional) /*NECESSARY TO ENABLE LOGGING*/\n"
                       + "\t[-m] or [--modern]                   /*TO ENABLE MODERN SHIFT QUIRK (8XY6/8XYE)*/\n"
                       + "\t[-i] or [--no-inc]                   /*DISABLE I-INCREMENT(FX55/FX65)*/\n"
                       + "\t[-h] or [--help]                     /*TO GET THIS HELP MESSAGE*/"){}
};






