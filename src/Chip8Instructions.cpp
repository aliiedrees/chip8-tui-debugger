#include "../include/Chip8.h"
#include "../include/Exceptions.h"

// chip8 instructions

// CLS (clear display)
void Chip8::IN_00E0(){
    for (int i = 0 ; i < DISPLAY_HEIGHT * DISPLAY_WIDTH; i++){
        display[i] = 0;
    }
    shouldRender = true;
}

// RET (return from a subroutine)
void Chip8::IN_00EE(){
    sp--;
    pc = stack[sp];
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
    stack[sp] = pc + 2;
    sp++;
    pc = ir & 0x0FFF;
}

// SE VX, byte (skip next instruction if VX == KK)
void Chip8::IN_3XKK(){
    uint8_t& vx = GetVX();
    uint8_t byte = ir & 0x00FF;
    
    if (vx == byte) pc += 2;
}

// SNE VX, byte (skip next instruction if VX != KK)
void Chip8::IN_4XKK(){
    uint8_t& vx = GetVX();
    uint8_t byte = ir & 0x00FF;
    
    if (vx != byte) pc += 2;
}

// SE VX, VY (skip next instruction if VX == VY)
void Chip8::IN_5XY0(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();

    if (vx == vy) pc += 2;
}

// LD VX, byte (set vx = kk)
void Chip8::IN_6XKK(){
    uint8_t& vx = GetVX();
    vx = ir & 0x00FF;
}

// ADD VX, byte
void Chip8::IN_7XKK(){
    uint8_t& vx = GetVX();
    vx += ir & 0x00FF;
}

// LD VX, VY (vx = vy)
void Chip8::IN_8XY0(){
    uint8_t& vy = GetVY();
    uint8_t& vx = GetVX();
    vx = vy;
}

// OR VX, VY (vx = vx | vy)
void Chip8::IN_8XY1(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();
    if(settings.vfReset) V[F] = 0;
    vx = vx | vy;
}

// AND VX, VY
void Chip8::IN_8XY2(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();
    if(settings.vfReset) V[F] = 0;
    vx = vx & vy;
}

// XOR VX, VY
void Chip8::IN_8XY3(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();
    if(settings.vfReset) V[F] = 0;
    vx = vx ^ vy;
}

// ADD VX, VY
void Chip8::IN_8XY4(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();
    int res = vx + vy;
    V[F]= (res > 255) ? 1 : 0;
    vx = res & 0xFF;
}

// SUB VX, VY
void Chip8::IN_8XY5(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();
    
    V[F] = (vx > vy) ? 1 : 0;
    vx = vx - vy;
}

// SHR VX, {, Vy} (SRL)
void Chip8::IN_8XY6(){
    uint8_t& vx = GetVX();
    
    if (settings.shift){// original
        uint8_t& vy = GetVY();
        V[F] = vy & 0x1;
        vx = vy >> 1;
    } else {// modern
        V[F] = vx & 0x1;
        vx = vx >> 1;
    }
}

// SUBN VX, VY
void Chip8::IN_8XY7(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();
    
    V[F] = (vx < vy) ? 1 : 0;
    vx = vy - vx;
}

// SHL VX, {, VY}
void Chip8::IN_8XYE() {
    uint8_t& vx = GetVX();
    
    if (settings.shift) { // original logic
        uint8_t& vy = GetVY();
        V[F] = (vy & 0x80) >> 7; 
        vx = vy << 1;
    } else { // modern logic
        V[F] = (vx & 0x80) >> 7; 
        vx = vx << 1;
    }
}

// SNE VX, VY (skip next inst if vx != vy)
void Chip8::IN_9XY0(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();

    if (vx != vy) pc += 2;
}

// LD I, addr
void Chip8::IN_ANNN(){
    I = ir & 0x0FFF;
}

// JP V0, addr 
void  Chip8::IN_BNNN(){
    if (settings.jump) pc = V[0] + (ir & 0xFFF);
    else pc = GetVX() + (ir & 0xFFF);
}

// RND VX, byte 
void Chip8::IN_CXKK(){
    uint8_t& vx = GetVX();
    uint8_t kk = ir & 0xFF;
    uint8_t rnd = dist(generator);
    vx = rnd & kk;
}

// DRW VX, VY, nibble
void Chip8::IN_DXYN() {
    uint8_t vx = GetVX() % DISPLAY_WIDTH; 
    uint8_t vy = GetVY() % DISPLAY_HEIGHT;
    uint8_t height = ir & 0x000F; 
    
    V[0xF] = 0; 

    for (int row = 0; row < height; ++row) {
        uint8_t spriteByte = memory[I + row];
        
        for (int col = 0; col < 8; ++col) {
            if ((spriteByte & (0x80 >> col)) != 0) {
                
                int screenX = vx + col;
                int screenY = vy + row;

                if (settings.wrap) {
                    screenX %= DISPLAY_WIDTH;
                    screenY %= DISPLAY_HEIGHT;
                } else {
                    if (screenX >= DISPLAY_WIDTH || screenY >= DISPLAY_HEIGHT) {
                        continue; 
                    }
                }

                int index = screenX + (screenY * DISPLAY_WIDTH);

                if (display[index] == 1) {
                    V[0xF] = 1; 
                }
                
                display[index] ^= 1;
            }
        }
    }
    
    shouldRender = true;
}

// SKP VX
void Chip8::IN_EX9E(){
    uint8_t vx = GetVX();
    /*if (vx > (uint8_t)0xFF){
        return;
    }*/
    if(keyboard[vx]) pc += 2;
}

// SKNP VX
void Chip8::IN_EXA1(){
    uint8_t vx = GetVX();
    /*if (vx > (uint8_t)0xFF){
        return;
    }*/
    if(!keyboard[vx]) pc += 2;
}

// LS VX, DT
void Chip8::IN_FX07(){
    GetVX() = dt;
}

// LD VX, K
void Chip8::IN_FX0A() {
    bool key_pressed = false;

    for (int i = 0; i < 16; i++) {
        if (keyboard[i]) {
            // A key is being held down. 
            // We store it, but we DON'T advance the PC yet.
            waitingForKeyIndex = i; 
            key_pressed = true;
            break;
        }
    }

    // If a key was being held but is NOW released:
    if (!key_pressed && waitingForKeyIndex != -1) {
        GetVX() = (uint8_t)waitingForKeyIndex;
        waitingForKeyIndex = -1; // Reset for next time
    } else {
        pc -= 2;
    }
}

// LD DT, VX
void Chip8::IN_FX15(){
    dt = GetVX();
}

// LD ST, VX
void Chip8::IN_FX18(){
    st = GetVX();
}

// ADD I, VX
void Chip8::IN_FX1E(){
    int sum = I + GetVX();
    I = sum & 0xFFF;
    //V[F] = sum > 0xFFF ? 1 : 0;
}

// LD F, VX
void Chip8::IN_FX29(){
    I = GetVX() * 5;
}

// LD B, VX
void Chip8::IN_FX33(){
    uint8_t& vx = GetVX();
    memory[I] = vx / 100;
    memory[I + 1] = (vx / 10) % 10;
    memory[I + 2] = vx % 10;
}

// LD [I], VX
void Chip8::IN_FX55(){
    uint8_t x = (ir & 0x0F00) >> 8;
    for(int i = 0; i <= x; ++i){
        memory[I + i] = V[i];
    }
    if(settings.increment) I += x + 1;
}

// LD VX, [I]
void Chip8::IN_FX65(){
    uint8_t x = (ir & 0x0F00) >> 8;
    for(int i = 0; i <= x; ++i){
        V[i] = memory[I + i];
    }
    if(settings.increment) I += x + 1;
}

// throw unkown isntruction
void Chip8::IN_NULL(){
    throw UnknownOpcodeException(ir);
}