#include "Chip8.h"
#include "Exceptions.h"
#include <fstream>
#include <string>
#include <random>
#include <format>
#include <cstdint>

using std::ifstream;
using std::string;

enum Flag {Unkown, Help, Log, Ticks, Shift, Increment, Jump, VfReset, Wrap};
// helper functions
static void PrintUsageMessage(std::ostream& os, const char* arg){
    os << "Usage: " << arg << " <ROM_PATH> [OPTIONS]\n\n"
        << "A high-performance Chip-8 interpreter with configurable hardware quirks.\n\n"
        << "CORE OPTIONS:\n"
        << "  -h, --help                Display this help message and exit\n"
        << "  -l, --log <PATH>          Enable instruction logging to file\n"
        << "  -t, --ticks <N>           Instructions per 60Hz frame [Default: 11] N in [1,5000]\n\n"
        << "EMULATION QUIRKS (Modern SCHIP by default; Flags enable 1977 logic):\n"
        << "  -s, --shift               Original Shift (VX = VY before shift)\n"
        << "  -i, --increment           Original Load/Store (I = I + X + 1)\n"
        << "  -j, --jump                Original Jump (Jump to NNN + V0)\n"
        << "  -f, --vf-reset            Original Logic (AND/OR/XOR reset VF to 0)\n"
        << "  -w, --wrap                Original Display (Sprites wrap screen edges)\n"
        << std::endl;
}
static std::string toHex(uint16_t val, int width) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%0*X", width, val);
    return std::string(buf);
}
Flag ParseFlag(const char* arg){
    string flag = string(arg);
    if (flag == "-h" || flag == "--help") return Flag::Help;
    if (flag == "-l" || flag == "--log") return Flag::Log;
    if (flag == "-t" || flag == "--ticks") return Flag::Ticks;
    if (flag == "-s" || flag == "--shift") return Flag::Shift;
    if (flag == "-i" || flag == "--increment") return Flag::Increment;
    if (flag == "-j" || flag == "--jump") return Flag::Jump;
    if (flag == "-f" || flag == "--vf-reset") return Flag::VfReset;
    if (flag == "-w" || flag == "--wrap") return Flag::Wrap;
    
    return Flag::Unkown;  
}

/* Chip8::Settings THROWS EXCEPTION */
Chip8::Settings Chip8::Settings::ParseArgs(int argc ,const char** argv){
    if (argc < 2) throw WrongUsageException(argv[0]);
    
    Chip8::Settings settings;
    settings.romPath = argv[1];
    for (int i = 2; i < argc; i++){
        Flag flag = ParseFlag(argv[i]);
        switch (flag){
            case Flag::Unkown : throw WrongUsageException(argv[0]);
            case Flag::Help : PrintUsageMessage(std::cout, argv[0]); exit(0);
            case Flag::Log :
                settings.logFlag = true;
                if (i + 1 < argc && ParseFlag(argv[i+1]) == Flag::Unkown){
                    settings.logPath = argv[++i];
                }
                break;
            case Flag::Ticks : 
                if (i + 1 < argc && ParseFlag(argv[i+1]) == Flag::Unkown){
                    try {
                        int ticks = std::stoi(argv[++i]);
                        if(ticks <= 0 || ticks > 15000){
                            throw TicksOutOfRangeException();
                        }
                        settings.ticksPerFrame = ticks; break;
                    } catch (const std::invalid_argument&) {
                        throw TicksNotIntException(argv[i]);
                    } catch (const std::out_of_range&) {
                        throw TicksOutOfRangeException();
                    }                  
                } else {
                    throw WrongUsageException(argv[0]);
                }
            case Flag::Shift : settings.shift = true; break;
            case Flag::Increment : settings.increment = true; break;
            case Flag::Jump : settings.jump = true; break;
            case Flag::VfReset : settings.vfReset = true; break;
            case Flag::Wrap : settings.wrap = true; break;
            default:
                throw WrongUsageException(argv[0]);
            }
    }
    return settings;
}

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
static constexpr int FONTSET_SIZE = 80;

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
Chip8::Chip8(Settings& settings)
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
      keyboard{},
      settings(settings)
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
    if(settings.vfReset) V[F] = 0;
    vx = vx | vy;
    pc += 2;
}

// AND VX, VY
void Chip8::IN_8XY2(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();
    if(settings.vfReset) V[F] = 0;
    vx = vx & vy;
    pc += 2;
}

// XOR VX, VY
void Chip8::IN_8XY3(){
    uint8_t& vx = GetVX();
    uint8_t& vy = GetVY();
    if(settings.vfReset) V[F] = 0;
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
    
    if (settings.shift){// original
        uint8_t& vy = GetVY();
        V[F] = vy & 0x1;
        vx = vy >> 1;
    } else {// modern
        V[F] = vx & 0x1;
        vx = vx >> 1;
    }
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
    
    if (settings.shift){// original
        uint8_t& vy = GetVY();
        V[F] = vy & 0x1;
        vx = vy >> 1;
    } else {// modern
        V[F] = vx & 0x1;
        vx = vx >> 1;
    }
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
    pc += 2;
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
    pc += 2;
}

// SKP VX
void Chip8::IN_EX9E(){
    uint8_t vx = GetVX();
    /*if (vx > (uint8_t)0xFF){
        return;
    }*/
    if(keyboard[vx]){
        pc += 4;
    } else {
        pc += 2;
    }
}

// SKNP VX
void Chip8::IN_EXA1(){
    uint8_t vx = GetVX();
    /*if (vx > (uint8_t)0xFF){
        return;
    }*/
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
        pc += 2;                 // Finally move to next instruction
    } else {
        // If no key has been pressed yet, or the key is still being held:
        // Stay on this instruction (don't increment PC)
        return; 
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
    int sum = I + GetVX();
    I = sum & 0xFFF;
    //V[F] = sum > 0xFFF ? 1 : 0;
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
    if(settings.increment) I += x + 1;
    pc += 2;
}

// LD VX, [I]
void Chip8::IN_FX65(){
    uint8_t x = (ir & 0x0F00) >> 8;
    for(int i = 0; i <= x; ++i){
        V[i] = memory[I + i];
    }
    if(settings.increment) I += x + 1;
    pc += 2;
}

/* loadROM THROWS EXCEPTION */
void Chip8::LoadROM(){
    ifstream file(settings.romPath, std::ios::ate | std::ios::binary);

    if (file.is_open()){
        int max_size = MEMORY_SIZE - START_OF_PROGRAM;
        std::streampos size = file.tellg(); // we are at the end (ate) so tellg will return size
        if (size > max_size){ // check ROM size
            throw RomFileTooBigException(settings.romPath, max_size);
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
        std::ofstream logFile(settings.logPath, std::ios::app);
        logFile << "ROM loaded successfully. size of ROM is " << size << " bytes." << std::endl;
        logFile.close();
    } else {
        throw InvalidRomPathException(settings.romPath);
    }
}

void Chip8::LogCycle() const{
    if (settings.logFlag && settings.log){
        string instruction = Disassemble(ir);
        ofstream logFile(settings.logPath, std::ios::app);
        if(!logFile.is_open()){
            throw InvalidLogPathException(settings.logPath);
        }
        logFile << "IR: " << instruction << " | "
                << "I: 0x" << toHex(I, 3) << " | "
                << "SP: " << int(sp) << " | V: ";
        for (int i = 0; i <= 0xF; i++){
            logFile << toHex(V[i], 2) << " ";
        } 
        logFile << std::endl;
        logFile.close();
    }
}

/* Cycle THROWS EXCEPTION */
void Chip8::Cycle(bool& running) {
    // check the pc
    if (pc < START_OF_PROGRAM || pc >= MEMORY_SIZE) {
        throw PcOutOfBondsException(pc);
    }
    
    // fetch
    ir = (memory[pc] << 8) | memory[pc + 1];

    // decode & execute
    switch (ir & 0xF000) {
        case 0x0000:
            switch (ir & 0x00FF) {
                case 0xE0: IN_00E0(); break; // CLS
                case 0xEE: IN_00EE(); break; // RET
                case 0x00: running = false; break;
                default: throw UnknownOpcodeException(ir);
            }
            break;
        case 0x1000: IN_1NNN(); break; // JP addr
        case 0x2000: IN_2NNN(); break; // CALL addr
        case 0x3000: IN_3XKK(); break; // SE Vx, byte
        case 0x4000: IN_4XKK(); break; // SNE Vx, byte   
        case 0x5000:
            if ((ir & 0x000F) == 0) {// SE Vx, Vy
                IN_5XY0(); break; 
            }
            throw UnknownOpcodeException(ir);
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
                default: throw UnknownOpcodeException(ir);
            }
            break;
        case 0x9000:
            if ((ir & 0x000F) == 0) {// SNE Vx, Vy
                IN_9XY0(); 
                break;
            }
            throw UnknownOpcodeException(ir);
        case 0xA000: IN_ANNN(); break; // LD I, addr
        case 0xB000: IN_BNNN(); break; // JP V0, addr
        case 0xC000: IN_CXKK(); break; // RND Vx, byte
        case 0xD000: IN_DXYN(); break; // DRW Vx, Vy, nibble
        case 0xE000:
            switch (ir & 0x00FF) {
                case 0x9E: IN_EX9E(); break; // SKP Vx
                case 0xA1: IN_EXA1(); break; // SKNP Vx
                default: throw UnknownOpcodeException(ir);
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
                default: throw UnknownOpcodeException(ir);
            }
            break;
        default:
            throw UnknownOpcodeException(ir);
    }
    LogCycle();
}

void Chip8::SetKey(uint8_t key, bool isPressed){
    keyboard[key] = isPressed;
}

void Chip8::ActivateDT(){
    if (dt > 0) --dt;  
}

void Chip8::ActivateST(){
    if (st > 0) --st; 
}

Chip8::State Chip8::GetState() const {
    Chip8::State state;
    for (int i = 0; i < 16; ++i){
        state.V[i] = V[i];
        state.stack[i] = stack[i];
    }
    for (int i = 0; i <= 0xFFF; i++){
        state.memory[i] = memory[i];
    }
    state.pc = pc;
    state.ir = ir;
    state.dt = dt;
    state.sp = sp;
    state.st = st;
    state.I = I;
    state.settings = settings;
    return state;
}
 
void Chip8::LoadState(const Chip8::State& state){
    for (int i = 0; i < 16; ++i){
        V[i] = state.V[i];
        stack[i] = state.stack[i];
    }
    for (int i = 0; i <= 0xFFF; i++){
        memory[i] = state.memory[i];
    }
    pc = state.pc;
    ir = state.ir;
    dt = state.dt;
    sp = state.sp;
    st = state.st;
    I = state.I;
    settings = state.settings;
}

/* THROWS EXCEPTION*/
void Chip8::ToggleLog(ofstream& logFile){
    if (settings.logFlag){
        settings.log = !settings.log;
        if (settings.log){
            logFile.open(settings.logPath, std::ios::app);
            if(!logFile){
                throw InvalidLogPathException(settings.logPath);
            }
            logFile << "---LOG START---" << std::endl;
        } else {
            if (logFile.is_open()){
                logFile << "---LOG END---" << std::endl;
                logFile.close();
            }
        }
    }
}

std::string Chip8::Disassemble(uint16_t ir) {
    uint8_t n1 = (ir & 0xF000) >> 12;
    uint8_t n2 = (ir & 0x0F00) >> 8;
    uint8_t n3 = (ir & 0x00F0) >> 4;
    uint8_t n4 = (ir & 0x000F);
    uint16_t nnn = ir & 0x0FFF;
    uint8_t kk = ir & 0x00FF;

    switch (n1) {
        case 0x0:
            if (kk == 0xE0) return "CLS";
            if (kk == 0xEE) return "RET";
            if (nnn == 0x000) return "--- END OF PROGRAM ---";
            else throw UnknownOpcodeException(ir);
        case 0x1: return "JP " + toHex(nnn, 3);
        case 0x2: return "CALL " + toHex(nnn, 3);
        case 0x3: return "SE V" + toHex(n2, 1) + ", " + toHex(kk, 2);
        case 0x4: return "SNE V" + toHex(n2, 1) + ", " + toHex(kk, 2);
        case 0x5: return "SE V" + toHex(n2, 1) + ", V" + toHex(n3, 1);
        case 0x6: return "LD V" + toHex(n2, 1) + ", " + toHex(kk, 2);
        case 0x7: return "ADD V" + toHex(n2, 1) + ", " + toHex(kk, 2);
        case 0x8:
            switch (n4) {
                case 0x0: return "LD V" + toHex(n2, 1) + ", V" + toHex(n3, 1);
                case 0x1: return "OR V" + toHex(n2, 1) + ", V" + toHex(n3, 1);
                case 0x2: return "AND V" + toHex(n2, 1) + ", V" + toHex(n3, 1);
                case 0x3: return "XOR V" + toHex(n2, 1) + ", V" + toHex(n3, 1);
                case 0x4: return "ADD V" + toHex(n2, 1) + ", V" + toHex(n3, 1);
                case 0x5: return "SUB V" + toHex(n2, 1) + ", V" + toHex(n3, 1);
                case 0x6: return "SHR V" + toHex(n2, 1);
                case 0x7: return "SUBN V" + toHex(n2, 1) + ", V" + toHex(n3, 1);
                case 0xE: return "SHL V" + toHex(n2, 1);
            }
            break;
        case 0x9: return "SNE V" + toHex(n2, 1) + ", V" + toHex(n3, 1);
        case 0xA: return "LD I, " + toHex(nnn, 3);
        case 0xB: return "JP V0, " + toHex(nnn, 3);
        case 0xC: return "RND V" + toHex(n2, 1) + ", " + toHex(kk, 2);
        case 0xD: return "DRW V" + toHex(n2, 1) + ", V" + toHex(n3, 1) + ", " + toHex(n4, 1);
        case 0xE:
            if (kk == 0x9E) return "SKP V" + toHex(n2, 1);
            if (kk == 0xA1) return "SKNP V" + toHex(n2, 1);
            break;
        case 0xF:
            switch (kk) {
                case 0x07: return "LD V" + toHex(n2, 1) + ", DT";
                case 0x0A: return "LD V" + toHex(n2, 1) + ", K";
                case 0x15: return "LD DT, V" + toHex(n2, 1);
                case 0x18: return "LD ST, V" + toHex(n2, 1);
                case 0x1E: return "ADD I, V" + toHex(n2, 1);
                case 0x29: return "LD F, V" + toHex(n2, 1);
                case 0x33: return "LD B, V" + toHex(n2, 1);
                case 0x55: return "LD [I], V" + toHex(n2, 1);
                case 0x65: return "LD V" + toHex(n2, 1) + ", [I]";
            }
            break;
    }
    throw UnknownOpcodeException(ir);
}


