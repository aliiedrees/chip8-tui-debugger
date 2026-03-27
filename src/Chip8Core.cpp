#include "../include/Chip8.h"
#include "../include/Exceptions.h"
#include <fstream>
#include <string>
#include <random>
#include <format>
#include <cstdint>

using std::ifstream;
using std::string;

// =================================================================================================
// SECTION: Helper Functions
// =================================================================================================

/**
 * @brief Print the usage message 
 * @param os is the output stream
 * @param arg is the compiled file which is going to be argv[0]
 */
static void PrintUsageMessage(std::ostream& os, const char* arg){
    os << "Usage: " << arg << " <ROM_PATH> [OPTIONS]\n\n"
        << "A high-performance Chip-8 interpreter with configurable hardware quirks.\n\n"
        << "CORE OPTIONS:\n"
        << "  -h, --help                Display this help message and exit\n"
        << "  -l, --log <PATH>          Enable instruction logging to file (while running you need to enable the logging)\n"
        << "  -t, --ticks <N>           Instructions per 60Hz frame [Default: 11] N in [1,5000]\n\n"
        << "EMULATION QUIRKS (Modern SCHIP by default; Flags enable 1977 logic):\n"
        << "  -s, --shift               Apply Modern Shift (VX = VY before shift)\n"
        << "  -i, --increment           Apply Modern Load/Store (I = I + X + 1)\n"
        << "  -j, --jump                Apply Modern Jump (Jump to NNN + V0)\n"
        << "  -f, --vf-preserve         Apply Modern Logic (AND/OR/XOR reset VF to 0)\n"
        << "  -w, --wrap                Apply Modern Display (Sprites wrap screen edges)\n"
        << std::endl;
}

/**
 * @brief decimal to hex convertion 
 */
static std::string toHex(uint16_t val, int width) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%0*X", width, val);
    return std::string(buf);
}


/**
 *  @brief parse argument to flag 
 * */
Flag ParseFlag(const char* arg){
    string flag = string(arg);
    if (flag == "-h" || flag == "--help") return Flag::Help;
    if (flag == "-l" || flag == "--log") return Flag::Log;
    if (flag == "-t" || flag == "--ticks") return Flag::Ticks;
    if (flag == "-s" || flag == "--shift") return Flag::Shift;
    if (flag == "-i" || flag == "--increment") return Flag::Increment;
    if (flag == "-j" || flag == "--jump") return Flag::Jump;
    if (flag == "-f" || flag == "--vf-preserve") return Flag::VfPreserve;
    if (flag == "-w" || flag == "--wrap") return Flag::Wrap;
    
    return Flag::Unkown;  
}

/** 
 * @brief Parse all the args in the input 
 * @note COULD THROW EXCEPTION 
 * */
Chip8::Settings Chip8::Settings::ParseArgs(int argc ,const char** argv){
    if (argc < 2) throw WrongUsageException(argv[0]);
    
    Chip8::Settings settings;
    if (ParseFlag(argv[1]) == Flag::Help){
        PrintUsageMessage(std::cout, argv[0]); 
        exit(0);
    }
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
            case Flag::VfPreserve : settings.vfPreserve = true; break;
            case Flag::Wrap : settings.wrap = true; break;
            default:
                throw WrongUsageException(argv[0]);
            }
    }
    return settings;
}


/** 
 * @brief to get specific nibble from the istruction
 *  @note each nibble is 4 bits
 *  @note instruction is seperated to 0x[N1][N2][N3][N4]
*/
uint8_t Chip8::GetNibble(int i) const{
    switch (i){
        case 1: return (ir & 0xF000) >> 12;
        case 2: return (ir & 0x0F00) >> 8;
        case 3: return (ir & 0x00F0) >> 4;
        case 4: return (ir & 0x000F);
        default: throw Chip8Exception("ERROR: only 4 nibbles in instruction you tried to get nibble number " + i);
    }
}

/**
 * @brief get the kk section from the instruction
 * @note kk is the last 8 bits in the instruction
 */
uint8_t Chip8::GetKK() const{
    return (GetNibble(3) << 4) + GetNibble(4);
}

/**
 * @brief get the nnn section from the instruction
 * @note nnn is the last 12 bits in the instruction
 */
uint16_t Chip8::GetNNN() const{
    return ir & 0xFFF;
}
/**
 * @brief get the instruction type
 * @return type from the enum class OpType
 */
OpType Chip8::GetOpType() const{
    return static_cast<OpType>((ir & 0xF000) >> 12);
}

/** 
 * @brief to get the VX in the instruction
 * @return a refrence to the VX;
 * */
uint8_t& Chip8::GetVX(){
    return V[GetNibble(2)];
}

/** 
 * @brief to get the VY in the instruction
 * @return a refrence to the VY;
 * */
uint8_t& Chip8::GetVY(){
    return V[GetNibble(3)];
}

// font array
static constexpr int FONTSET_SIZE = 80;

/** 
 * @brief FONT SET
 *  @note DIDNT USE THEM YET BUT HTEY ARE IN THE CHIP MEMORY
 * */ 
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

// =================================================================================================
// SECTION: Constructor + Object prepare
// =================================================================================================

Chip8::Chip8(const Settings& settings)
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
    InitializeDispatches();
    for (int i = 0; i < FONTSET_SIZE; i++){
        memory[i] = FONTSET[i];
    }
    std::random_device rd;
    generator.seed(rd());
}

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

// =================================================================================================
// SECTION: Cycle execution + inner functions in cycle
// =================================================================================================

void Chip8::Cycle() {
    // check the pc
    if (pc < START_OF_PROGRAM || pc >= MEMORY_SIZE) {
        throw PcOutOfBondsException(pc);
    }
    LogCycle();
    Fetch();
    DecodeExecute();   
}

/**
 * @brief the logic behind the logging of a single isntruction
 * @note COULD THROW InvalidLogPathException
 */
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

/**
 * @brief Fetch phase logic
 */
void Chip8::Fetch() {
    ir = (memory[pc] << 8) | memory[pc + 1];
    pc += 2;
}

/**
 * @brief Decode & Execute phases logic
 */
void Chip8::DecodeExecute() {
    OpType opType = GetOpType();
    (this->*dispatch[static_cast<size_t>(opType)])();
}

/** 
 * @brief Initialize the dispatch arrays 
*/
void Chip8::InitializeDispatches(){
    InitializeMainDispatch();
    InitializeDispatch0();
    InitializeDispatch5();
    InitializeDispatch8();
    InitializeDispatch9();
    InitializeDispatchE();
    InitializeDispatchF();
}

/** 
 * @brief Initialize Main Dispatch
*/
void Chip8::InitializeMainDispatch(){
    dispatch[static_cast<size_t>(OpType::SYSTEM)] = &Chip8::Dispatch0;
    dispatch[static_cast<size_t>(OpType::JUMP)] = &Chip8::IN_1NNN;
    dispatch[static_cast<size_t>(OpType::CALL)] = &Chip8::IN_2NNN;
    dispatch[static_cast<size_t>(OpType::SKIP_EQ_BYTE)] = &Chip8::IN_3XKK;
    dispatch[static_cast<size_t>(OpType::SKIP_NE_BYTE)] = &Chip8::IN_4XKK;
    dispatch[static_cast<size_t>(OpType::SKIP_EQ_REG)] = &Chip8::Dispatch5;
    dispatch[static_cast<size_t>(OpType::SET_REG_BYTE)] = &Chip8::IN_6XKK;
    dispatch[static_cast<size_t>(OpType::ADD_REG_BYTE)] = &Chip8::IN_7XKK;
    dispatch[static_cast<size_t>(OpType::ARITHMETIC)] = &Chip8::Dispatch8;
    dispatch[static_cast<size_t>(OpType::SKIP_NE_REG)] = &Chip8::Dispatch9;
    dispatch[static_cast<size_t>(OpType::SET_INDEX)] = &Chip8::IN_ANNN;
    dispatch[static_cast<size_t>(OpType::JUMP_REL)] = &Chip8::IN_BNNN;
    dispatch[static_cast<size_t>(OpType::RANDOM)] = &Chip8::IN_CXKK;
    dispatch[static_cast<size_t>(OpType::DISPLAY)] = &Chip8::IN_DXYN;
    dispatch[static_cast<size_t>(OpType::INPUT)] = &Chip8::DispatchE;
    dispatch[static_cast<size_t>(OpType::UTILITY)] = &Chip8::DispatchF;
}

/** 
 * @brief Initialize SYSTEM Dispatch 
*/
void Chip8::InitializeDispatch0(){
    dispatch0.fill(&Chip8::IN_NULL);
    dispatch0[0x00] = &Chip8::IN_0000;
    dispatch0[0xE0] = &Chip8::IN_00E0;
    dispatch0[0xEE] = &Chip8::IN_00EE;
}

/** 
 * @brief Initialize SKIP_EQ_REG Dispatch
*/
void Chip8::InitializeDispatch5(){
    dispatch5.fill(&Chip8::IN_NULL);
    dispatch5[0x0] = &Chip8::IN_5XY0;
}
/** 
 * @brief Initialize ARITHMETIC Dispatch
*/
void Chip8::InitializeDispatch8(){
    dispatch8.fill(&Chip8::IN_NULL);
    dispatch8[0x0] = &Chip8::IN_8XY0;
    dispatch8[0x1] = &Chip8::IN_8XY1;
    dispatch8[0x2] = &Chip8::IN_8XY2;
    dispatch8[0x3] = &Chip8::IN_8XY3;
    dispatch8[0x4] = &Chip8::IN_8XY4;
    dispatch8[0x5] = &Chip8::IN_8XY5;
    dispatch8[0x6] = &Chip8::IN_8XY6;
    dispatch8[0x7] = &Chip8::IN_8XY7;
    dispatch8[0xE] = &Chip8::IN_8XYE;
}
/** 
 * @brief Initialize SKIP_NE_REG Dispatch
*/
void Chip8::InitializeDispatch9(){
    dispatch9.fill(&Chip8::IN_NULL);
    dispatch9[0x0] = &Chip8::IN_9XY0;
}
/** 
 * @brief Initialize INPUT Dispatch
*/
void Chip8::InitializeDispatchE(){
    dispatchE.fill(&Chip8::IN_NULL);
    dispatchE[0x9E] = &Chip8::IN_EX9E;
    dispatchE[0xA1] = &Chip8::IN_EXA1;
}

/** 
 * @brief Initialize ARITHMETIC Dispatch 
*/
void Chip8::InitializeDispatchF(){
    dispatchF.fill(&Chip8::IN_NULL);
    dispatchF[0x07] = &Chip8::IN_FX07;
    dispatchF[0x0A] = &Chip8::IN_FX0A;
    dispatchF[0x15] = &Chip8::IN_FX15;
    dispatchF[0x18] = &Chip8::IN_FX18;
    dispatchF[0x1E] = &Chip8::IN_FX1E;
    dispatchF[0x29] = &Chip8::IN_FX29;
    dispatchF[0x33] = &Chip8::IN_FX33;
    dispatchF[0x55] = &Chip8::IN_FX55;
    dispatchF[0x65] = &Chip8::IN_FX65;
}

/** 
 *  @brief SYSTEM Dispatch 
 *  executes the proper instruction from the SYSTEM type
*/
void Chip8::Dispatch0(){
    (this->*dispatch0[Chip8::GetKK()])();
}

/** 
 * @brief SKIP_EQ_REG Dispatch 
 * executes the proper instruction from the SKIP_EQ_REG type
*/
void Chip8::Dispatch5(){
    (this->*dispatch5[Chip8::GetNibble(4)])();
}

/** 
 * @brief ARITHMETIC Dispatch 
 * executes the proper instruction from the ARITHMETIC type
*/
void Chip8::Dispatch8(){
    (this->*dispatch8[Chip8::GetNibble(4)])();
}

/** 
 * @brief SKIP_NE_REG Dispatch 
 * executes the proper instruction from the SKIP_NE_REG type
*/
void Chip8::Dispatch9(){
    (this->*dispatch9[Chip8::GetNibble(4)])();
}

/** 
 * @brief INPUT Dispatch 
 *  executes the proper instruction from the INPUT type
*/
void Chip8::DispatchE(){
    (this->*dispatchE[Chip8::GetKK()])();
}

/**
 *  @brief UTILITY Dispatch 
 *  executes the proper instruction from the UTILITY type
 */
void Chip8::DispatchF(){
    (this->*dispatchF[Chip8::GetKK()])();
}

// =================================================================================================
// SECTION: Rest of UI
// =================================================================================================

void Chip8::Restart() {
    *this = Chip8(settings);
    this->LoadROM();
    this->settings.restart = true;
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
            else return "UNKNOWN OPCODE";
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
                default: return "UNKNOWN OPCODE";
            }
        case 0x9: return "SNE V" + toHex(n2, 1) + ", V" + toHex(n3, 1);
        case 0xA: return "LD I, " + toHex(nnn, 3);
        case 0xB: return "JP V0, " + toHex(nnn, 3);
        case 0xC: return "RND V" + toHex(n2, 1) + ", " + toHex(kk, 2);
        case 0xD: return "DRW V" + toHex(n2, 1) + ", V" + toHex(n3, 1) + ", " + toHex(n4, 1);
        case 0xE:
            if (kk == 0x9E) return "SKP V" + toHex(n2, 1);
            if (kk == 0xA1) return "SKNP V" + toHex(n2, 1);
            else return "UNKNOWN OPCODE";
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
                default: return "UNKNOWN OPCODE";
            }
    }
    throw UnknownOpcodeException(ir); // shouldn't reach this line since all the cases 0x0-0xF are in switch
}


