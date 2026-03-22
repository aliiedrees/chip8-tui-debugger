#include "Chip8.h"
#include <fstream>
#include <iostream>
#include <string>
#include <random>
using std::ifstream;


uint8_t& Chip8::GetVX(){
    return V[(ir & 0x0F00) >> 8];
}
const uint8_t& Chip8::GetVX() const {
    return V[(ir & 0x0F00) >> 8];
}
uint8_t& Chip8::GetVY(){
    return V[(ir & 0x00F0) >> 4];
}
const uint8_t& Chip8::GetVY() const {
    return V[(ir & 0x00F0) >> 4];
}

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
    : generator(std::random_device{}()),
      dist(0, 255),
      memory{},
      V{},
      I(0),
      pc(START_OF_PROGRAM),
      sp(0),
      stack{},
      dt(0),
      st(0),
      shouldRender(false),
      display{},
      keyboard{}
{
    for (int i = 0; i < FONTSET_SIZE; i++){
        memory[i] = FONTSET[i];
    }
    std::random_device rd;
    generator.seed(rd());
}

// chip8 instructions

// CLS (clear display)
void Chip8::IN_00E0(){
    for (int i = 0 ; i < DISPLAY_HEIGHT * DISPLAY_WIDTH; i++){
        display[i] = 0;
    }
    shouldRender = true;
    pc += 2;
}

// RET (return from a subroutine)
void Chip8::IN_00EE(){
    pc = stack[sp--];
}

// JP addr (jump to address NNN)
void Chip8::IN_1NNN(){
    pc = ir & 0xFFF;
}

// CALL addr (call subroutine at NNN)
void Chip8::IN_2NNN(){
    if (sp >= F){
        std::cerr << "ERROR: Stack Overflow! Too many nested calls." << std::endl;
    }
    stack[++sp] = pc;
    pc += 2;
}

// SE VX, byte (skip next instruction if VX == KK)
void Chip8::IN_3XKK(){
    uint8_t vx = GetVX();
    uint8_t byte = ir & 0x00FF;
    
    if (vx == byte){
        pc += 4;
    } else {
        pc += 2;
    }
}

// SNE VX, byte (skip next instruction if VX != KK)
void Chip8::IN_4XKK(){
    uint8_t vx = GetVX();
    uint8_t byte = ir & 0x00FF;
    
    if (vx != byte){
        pc += 4;
    } else {
        pc += 2;
    }
}

// SE VX, VY (skip next instruction if VX == VY)
void Chip8::IN_5XY0(){
    uint8_t vx = GetVX();
    uint8_t vy = GetVY();

    if (vx == vy){
        pc += 4;
    } else {
        pc += 2;
    }
}

// LD VX, byte (set vx = kk)
void Chip8::IN_6XKK(){
    uint8_t vx = GetVX();
    vx = ir & 0x00FF;
    pc += 2;
}

// ADD VX, byte
void Chip8::IN_7XKK(){
    uint8_t vx = GetVX();
    vx += ir & 0x00FF;
    pc += 2;
}

// LD VX, VY (vx = vy)
void Chip8::IN_8XY0(){
    uint8_t vy = GetVY();
    uint8_t vx = GetVX();
    vx = vy;
    pc += 2;
}

// OR VX, VY (vx = vx | vy)
void Chip8::IN_8XY1(){
    uint8_t vx = GetVX();
    uint8_t vy = GetVY();
    vx = vx | vy;
    pc += 2;
}

// AND VX, VY
void Chip8::IN_8XY2(){
    uint8_t vx = GetVX();
    uint8_t vy = GetVY();
    vx = vx & vy;
    pc += 2;
}

// XOR VX, VY
void Chip8::IN_8XY3(){
    uint8_t vx = GetVX();
    uint8_t vy = GetVY();
    vx = vx ^ vy;
    pc += 2;
}

// ADD VX, VY
void Chip8::IN_8XY4(){
    uint8_t vx = GetVX();
    uint8_t vy = GetVY();
    int res = vx + vy;
    vy = (res > 255) ? 1 : 0;
    vx = res & 0xFF;
    pc += 2;
}

// SUB VX, VY
void Chip8::IN_8XY5(){
    uint8_t vx = GetVX();
    uint8_t vy = GetVY();
    
    V[F] = (vx > vy) ? 1 : 0;
    vx = vx - vy;
    pc += 2;
}

// SHR VX, {, Vy} (SRL)
void Chip8::IN_8XY6(){
    uint8_t vx = GetVX();
    
    V[F] = vx & 0x1;
    vx >>=1;
    pc += 2;
}

// SUBN VX, VY
void Chip8::IN_8XY7(){
    uint8_t vx = GetVX();
    uint8_t vy = GetVY();
    
    V[F] = (vx < vy) ? 1 : 0;
    vx = vy - vx;
    pc += 2;
}

// SHL VX, {, VY}
void Chip8::IN_8XYE(){
    uint8_t vx = GetVX();
    V[F] = (vx >> 7) ? 1 : 0;
    vx <<= 2;
    pc += 2;
}

// SNE VX, VY (skip next inst if vx != vy)
void Chip8::IN_9XY0(){
    uint8_t vx = GetVX();
    uint8_t vy = GetVY();

    if (vx != vy){
        pc += 4;
    } else {
        pc += 2;
    }
}

// LD I, addr
void Chip8::IN_ANNN(){
    I = ir & 0x0FFF;
    pc += 2;
}

// JP V0, addr (jump to v0 + nnn)
void  Chip8::IN_BNNN(){
    pc = V[0] + (ir & 0xFFF);
}

// RND VX, byte 
void Chip8::IN_CXKK(){
    uint8_t vx = GetVX();
    uint8_t kk = ir & 0xFF;
    uint8_t rnd = dist(generator);
    vx = rnd & kk;
    pc += 2;
}

// DRW VX, VY, nibble



// loadROM
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

// Cycle
void Chip8::Cycle(){
    // fetch
    ir = (memory[pc] << 8) | memory[pc + 1];
    uint16_t opcode = ir && 0xF000;
    // decode
    switch (opcode){
        case 0x0000:
            switch (ir){
                case 0x00E0: //CLS
                    
            }
        case 0x1000: // jump
            pc = ir && 0xFFF;
        case 0x6000: // Vx = nn
            V[(ir & 0x0F00) >> 8] = ir & 0x00FF;
            pc += 2;
        case 0x7000: // Vx += nn
            V[(ir & 0x0F00) >> 8] += ir & 0x00FF;
            pc += 2;
        
    }

}
