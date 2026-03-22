#include "Chip8.h"
#include <fstream>
#include <iostream>
#include <string>
using std::ifstream;

// font array
static constexpr unsigned int FONTSET_SIZE = 80;

static const uint8_t FONTSET[FONTSET_SIZE] = {
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

// c'tor
Chip8::Chip8()
    : pc(START_OF_PROGRAM),
      I(0),
      sp(0),
      dt(0),
      st(0),
      memory{},
      stack{},
      V{},
      display{},
      keyboard{}
{
    for (int i = 0; i < FONTSET_SIZE; i++){
        memory[i] = FONTSET[i];
    }
}

//   loadROM
void Chip8::LoadROM(const string& path){
    ifstream file(path, std::ios::ate | std::ios::binary);

    if (file.is_open()){
        int max_size = MEMORY_SIZE - START_OF_PROGRAM;
        std::streampos size = file.tellg(); // we are at the end (ate) so tellg will return size
        if (size > max_size){ // check ROM size
            std::cerr << "ROM file is too big! *max size of ROM is" << max_size << "*" << std::endl;
            return;
        }
        //return to the beginnnig of file
        file.seekg(0, std::ios::beg);

        char c;
        int address = START_OF_PROGRAM;
        while (file.get(c)){
            //no need to check we know size is good
            memory[address] = static_cast<uint8_t>(c);
            address++;
        }
        file.close();
        std::cout << "ROM loaded successfully. size of ROM is " << size << " bytes." << std::endl;
    } else {
        std::cerr << "FAILED to open ROM: " << path << std::endl;
    }
}