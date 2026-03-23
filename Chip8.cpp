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
    stack[++sp] = pc + 2;
    pc = ir & 0x0FFF;
}

// SE VX, byte (skip next instruction if VX == KK)
void Chip8::IN_3XKK(){
    uint8_t& vx = GetVX();
    uint8_t byte = ir & 0x00FF;
    
    if (vx == byte){
        pc += 4;
    } else {
        pc += 2;
    }
}

// SNE VX, byte (skip next instruction if VX != KK)
void Chip8::IN_4XKK(){
    uint8_t& vx = GetVX();
    uint8_t byte = ir & 0x00FF;
    
    if (vx != byte){
        pc += 4;
    } else {
        pc += 2;
    }
}

// SE VX, VY (skip next instruction if VX == VY)
void Chip8::IN_5XY0(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();

    if (vx == vy){
        pc += 4;
    } else {
        pc += 2;
    }
}

// LD VX, byte (set vx = kk)
void Chip8::IN_6XKK(){
    uint8_t& vx = GetVX();
    vx = ir & 0x00FF;
    pc += 2;
}

// ADD VX, byte
void Chip8::IN_7XKK(){
    uint8_t& vx = GetVX();
    vx += ir & 0x00FF;
    pc += 2;
}

// LD VX, VY (vx = vy)
void Chip8::IN_8XY0(){
    uint8_t& vy = GetVY();
    uint8_t& vx = GetVX();
    vx = vy;
    pc += 2;
}

// OR VX, VY (vx = vx | vy)
void Chip8::IN_8XY1(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();
    vx = vx | vy;
    pc += 2;
}

// AND VX, VY
void Chip8::IN_8XY2(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();
    vx = vx & vy;
    pc += 2;
}

// XOR VX, VY
void Chip8::IN_8XY3(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();
    vx = vx ^ vy;
    pc += 2;
}

// ADD VX, VY
void Chip8::IN_8XY4(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();
    int res = vx + vy;
    V[F]= (res > 255) ? 1 : 0;
    vx = res & 0xFF;
    pc += 2;
}

// SUB VX, VY
void Chip8::IN_8XY5(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();
    
    V[F] = (vx > vy) ? 1 : 0;
    vx = vx - vy;
    pc += 2;
}

// SHR VX, {, Vy} (SRL)
void Chip8::IN_8XY6(){
    uint8_t& vx = GetVX();
    
    V[F] = vx & 0x1;
    vx >>=1;
    pc += 2;
}

// SUBN VX, VY
void Chip8::IN_8XY7(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();
    
    V[F] = (vx < vy) ? 1 : 0;
    vx = vy - vx;
    pc += 2;
}

// SHL VX, {, VY}
void Chip8::IN_8XYE(){
    uint8_t& vx = GetVX();
    V[F] = (vx >> 7) ? 1 : 0;
    vx <<= 1;
    pc += 2;
}

// SNE VX, VY (skip next inst if vx != vy)
void Chip8::IN_9XY0(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();

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
    uint8_t& vx = GetVX();
    uint8_t kk = ir & 0xFF;
    uint8_t rnd = dist(generator);
    vx = rnd & kk;
    pc += 2;
}

// DRW VX, VY, nibble
void Chip8::IN_DXYN(){
    uint8_t vx = GetVX() % DISPLAY_WIDTH;
    uint8_t vy = GetVY() % DISPLAY_HEIGHT;
    uint8_t height = ir & 0xF;
    V[F] = 0;
    for (int row = 0; row < height; ++row) {
        uint8_t spriteByte = memory[I + row];

        for (int col = 0; col < BYTE; ++col) {
            uint8_t spritePixel = spriteByte & (0x80 >> col);

            int screenX = (vx + col) % DISPLAY_WIDTH;
            int screenY = (vy + row) % DISPLAY_HEIGHT;
            int index = screenX + (screenY * DISPLAY_WIDTH);

            uint8_t& screenPixel = display[index];

            if (spritePixel) {
                if (screenPixel == 1) {
                    V[F] = 1;
                }
                screenPixel ^= 1;
            }
        }
    }
    shouldRender = true;
    pc += 2;
}

// SKP VX
void Chip8::IN_EX9E(){
    uint8_t vx = GetVX();
    if (vx > 0xFF){
        return;
    }
    if(keyboard[vx]){
        pc += 4;
    } else {
        pc += 2;
    }
}

// SKNP VX
void Chip8::IN_EXA1(){
    uint8_t vx = GetVX();
    if (vx > 0xFF){
        return;
    }
    if(!keyboard[vx]){
        pc += 4;
    } else {
        pc += 2;
    }
}

// LS VX, DT
void Chip8::IN_FX07(){
    GetVX() = dt;
    pc += 2;
}

// LD VX, K
void Chip8::IN_FX0A(){
    bool key_pressed = false;
    for(int i = 0; i < 16; i++){
        if(keyboard[i]){
            GetVX() = i;
            key_pressed = true;
            break;
        }
    }

    if(!key_pressed){
        pc -= 2;
    }
}

// LD DT, VX
void Chip8::IN_FX15(){
    dt = GetVX();
    pc += 2;
}

// LD ST, VX
void Chip8::IN_FX18(){
    st = GetVX();
    pc += 2;
}

// ADD I, VX
void Chip8::IN_FX1E(){
    I += GetVX();
    pc += 2;
}

// LD F, VX
void Chip8::IN_FX29(){
    I = GetVX() * 5;
    pc += 2;
}

// LD B, VX
void Chip8::IN_FX33(){
    uint8_t& vx = GetVX();
    memory[I] = vx / 100;
    memory[I + 1] = (vx / 10) % 10;
    memory[I + 2] = vx % 10;

    pc += 2;
}

// LD [I], VX
void Chip8::IN_FX55(){
    uint8_t x = (ir & 0x0F00) >> 8;
    for(int i = 0; i <= x; ++i){
        memory[I + i] = V[i];
    }
    pc += 2;
}

// LD VX, [I]
void Chip8::IN_FX65(){
    uint8_t x = (ir & 0x0F00) >> 8;
    for(int i = 0; i <= x; ++i){
        V[i] = memory[I + i];
    }
    pc += 2;
}


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
void Chip8::UnknownInstruction(const uint16_t& instruction) {
    std::printf("Unknown Instruction: 0x%X at PC: 0x%X\n", instruction, pc - 2);
}

void Chip8::Cycle() {
    // fetch
    ir = (memory[pc] << 8) | memory[pc + 1];

    // decode & execute
    switch (ir & 0xF000) {
        case 0x0000:
            switch (ir & 0x00FF) {
                case 0xE0: IN_00E0(); break; // CLS
                case 0xEE: IN_00EE(); break; // RET
                default: UnknownInstruction(ir); break;
            }
            break;

        case 0x1000: IN_1NNN(); break; // JP addr
        case 0x2000: IN_2NNN(); break; // CALL addr
        case 0x3000: IN_3XKK(); break; // SE Vx, byte
        case 0x4000: IN_4XKK(); break; // SNE Vx, byte
        
        case 0x5000:
            if ((ir & 0x000F) == 0) IN_5XY0(); // SE Vx, Vy
            else UnknownInstruction(ir);
            break;

        case 0x6000: IN_6XKK(); break; // LD Vx, byte
        case 0x7000: IN_7XKK(); break; // ADD Vx, byte

        case 0x8000:
            switch (ir & 0x000F) {
                case 0x0: IN_8XY0(); break; // LD Vx, Vy
                case 0x1: IN_8XY1(); break; // OR Vx, Vy
                case 0x2: IN_8XY2(); break; // AND Vx, Vy
                case 0x3: IN_8XY3(); break; // XOR Vx, Vy
                case 0x4: IN_8XY4(); break; // ADD Vx, Vy
                case 0x5: IN_8XY5(); break; // SUB Vx, Vy
                case 0x6: IN_8XY6(); break; // SHR Vx
                case 0x7: IN_8XY7(); break; // SUBN Vx, Vy
                case 0xE: IN_8XYE(); break; // SHL Vx
                default: UnknownInstruction(ir); break;
            }
            break;

        case 0x9000:
            if ((ir & 0x000F) == 0) IN_9XY0(); // SNE Vx, Vy
            else UnknownInstruction(ir);
            break;

        case 0xA000: IN_ANNN(); break; // LD I, addr
        case 0xB000: IN_BNNN(); break; // JP V0, addr
        case 0xC000: IN_CXKK(); break; // RND Vx, byte
        case 0xD000: IN_DXYN(); break; // DRW Vx, Vy, nibble

        case 0xE000:
            switch (ir & 0x00FF) {
                case 0x9E: IN_EX9E(); break; // SKP Vx
                case 0xA1: IN_EXA1(); break; // SKNP Vx
                default: UnknownInstruction(ir); break;
            }
            break;

        case 0xF000:
            switch (ir & 0x00FF) {
                case 0x07: IN_FX07(); break; // LD Vx, DT
                case 0x0A: IN_FX0A(); break; // LD Vx, K
                case 0x15: IN_FX15(); break; // LD DT, Vx
                case 0x18: IN_FX18(); break; // LD ST, Vx
                case 0x1E: IN_FX1E(); break; // ADD I, Vx
                case 0x29: IN_FX29(); break; // LD F, Vx
                case 0x33: IN_FX33(); break; // LD B, Vx
                case 0x55: IN_FX55(); break; // LD [I], Vx
                case 0x65: IN_FX65(); break; // LD Vx, [I]
                default: UnknownInstruction(ir); break;
            }
            break;

        default:
            UnknownInstruction(ir);
            break;
    }


}
