#include "../include/Chip8.h"
#include "../include/Exceptions.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <locale.h>
#include <fstream>
#include <ncurses.h>
#include <clocale>

uint8_t keymap[16] = {
    'x',
    '1',
    '2',
    '3', // 0, 1, 2, 3
    'q',
    'w',
    'e',
    'a', // 4, 5, 6, 7
    's',
    'd',
    'z',
    'c', // 8, 9, A, B
    '4',
    'r',
    'f',
    'v', // C, D, E, F
};

void drawRegisters(WINDOW *win, const Chip8::State &state);
void drawSettings(WINDOW* win, const Chip8::Settings& settings, int historyCount);
void drawStack(WINDOW *win, const Chip8::State &state);
void drawDissassembly(WINDOW* win, const Chip8::State& state);
void drawHeatmap(WINDOW* win, const Chip8::State& state);
void drawKeys(WINDOW* win);

int main(int argc, const char** argv) {
    try{
        // this can give excpetion
        Chip8::Settings settings = Chip8::Settings::ParseArgs(argc, argv);
        ofstream logFile;
        std::setlocale(LC_ALL, "");
        
        const int TICKS_PER_SECOND = 60;
        const int FRAME_DURATION_US = 1000000 / TICKS_PER_SECOND;
        Chip8 chip8(settings);

        // this can give excpetion
        chip8.LoadROM();

        initscr();
        noecho();
        cbreak();
        curs_set(0);
        nodelay(stdscr, true);
        keypad(stdscr, true);
        start_color();
        use_default_colors();
        init_pair(1, COLOR_BLUE,    -1);  // cold  (1–5 hits)
        init_pair(2, COLOR_CYAN,    -1);  // warm  (6–20 hits)
        init_pair(3, COLOR_YELLOW,  -1);  // hot   (21–100 hits)
        init_pair(4, COLOR_RED,     -1);  // fire  (100+ hits)

        WINDOW* displayWin = newwin(34, 66, 0, 0);
        WINDOW* regWin = newwin(15, 26, 0, 66);
        WINDOW* stackWin = newwin(12, 26, 15, 66);
        WINDOW* settingsWin = newwin(7, 26, 27, 66);
        WINDOW* disWin = newwin(14, 32, 34, 0);
        WINDOW* memWin = newwin(14, 60, 34, 32);
        WINDOW* keysWin = newwin(3, 92, 48, 0);
        refresh();
        
        wnoutrefresh(displayWin);
        wnoutrefresh(regWin);
        wnoutrefresh(stackWin);
        wnoutrefresh(settingsWin);
        wnoutrefresh(disWin);
        wnoutrefresh(memWin);
        wnoutrefresh(keysWin);
        box(displayWin, 0, 0); 
        box(memWin, 0, 0);

        doupdate();
        int key_timer[16] = {0}; // acts as a latch for the key
        
        while (chip8.IsRunning()){
            auto startTime = std::chrono::high_resolution_clock::now();
            int c = getch();

            if (c == 27) {
                chip8.Stop(); 
                break;
            } else if (c == KEY_F(6)) chip8.TogglePaused();
            else if (c == KEY_F(7) && chip8.IsPaused()) chip8.Cycle();
            else if (c == KEY_F(5) && chip8.IsPaused()) chip8.StepBack(); // 
            else if (c == KEY_F(8)) chip8.Restart();
            else if (c == KEY_F(9)) chip8.ToggleLog(logFile);
            else if (c != ERR) {
                for (int i = 0; i < 16; ++i) { // handling the input
                    if (c == keymap[i]) {
                        chip8.SetKey(i, 1);
                        key_timer[i] = 10;
                    } 
                }
            } else {
                // If getch() returns ERR, no keys are being pressed
                for (int i = 0; i < 16; ++i) {
                    if (key_timer[i] > 0) {
                        key_timer[i]--;
                        if (key_timer[i] == 0) {
                            chip8.SetKey(i, 0); // Finally release the key
                        }
                    }
                }
            }

            if (!chip8.IsPaused() && !chip8.ShouldRestart()){ // ~480 hz by default 
                for (int i = 0; i < settings.ticksPerFrame; ++i) {
                    chip8.Cycle();
                    if (!chip8.IsRunning()) break;
                }
                // @fixme i think the logic is wrong in term of when do decrement the timers
                // they must decrease at exactly 60hz
                chip8.ActivateDT();
                chip8.ActivateST();
            }
            
            drawRegisters(regWin, chip8.GetState());
            drawStack(stackWin, chip8.GetState());
            drawSettings(settingsWin, chip8.GetState().settings, chip8.HistoryCount());
            drawDissassembly(disWin, chip8.GetState());
            drawHeatmap(memWin, chip8.GetState());
            drawKeys(keysWin);
            if (chip8.ShouldRender() || chip8.ShouldRestart()){
                box(displayWin, 0, 0); 
                const uint8_t *display = chip8.GetDisplay();
                for (int y = 0; y < 32; ++y){
                    std::string row = ""; // build the row string
                    for (int x = 0; x < 64; ++x){
                        // add a block or a space
                        row += (display[y * 64 + x] ? "\u2588" : " ");
                    }
                    // Draw the entire row at once (starting at col 1 to avoid the border)
                    mvwaddstr(displayWin, y + 1, 1, row.c_str());
                }
                if (chip8.ShouldRender()) chip8.ToggleRender();
                if (chip8.ShouldRestart()) chip8.ToggleRestart();
            }
            wnoutrefresh(displayWin);
            doupdate();

            auto endTime = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
            if (elapsed < FRAME_DURATION_US){
                std::this_thread::sleep_for(std::chrono::microseconds(FRAME_DURATION_US - elapsed));
            }
        }
        endwin();
        if(chip8.LogFlag()){
            if(chip8.IsLogging()){
                logFile << "---LOG END---" << std::endl;
                logFile << "Emulator Terminated Safely." << std::endl;
                logFile.close();
            } else {
                logFile.open(settings.logPath, std::ios::app);
                if(!logFile){
                    throw InvalidLogPathException(settings.logPath);
                }            
                logFile << "Emulator Terminated Safely." << std::endl;
                logFile.close();
            }
        }
        std::cout << "Emulator Terminated Safely." << std::endl;
        return EXIT_SUCCESS;
    } 
    catch (const std::exception& e){
        erase();
        endwin();
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
}

void drawRegisters(WINDOW *win, const Chip8::State &state)
{
    werase(win);
    box(win, 0, 0);

    mvwprintw(win, 1, 2, "  --- REGISTERS ---");
    for (int i = 0; i < 8; ++i)
    {
        mvwprintw(win, 3 + i, 3, "V%X: 0x%02X", i, state.V[i]);
    }

    for (int i = 8; i < 16; ++i)
    {
        mvwprintw(win, 3 + (i - 8), 14, "V%X: 0x%02X", i, state.V[i]);
    }

    mvwprintw(win, 11, 3, "--------------------");
    mvwprintw(win, 12, 2, "PC: 0x%04X", state.pc);
    mvwprintw(win, 13, 2, "I: 0x%04X", state.I);
    mvwprintw(win, 12, 14, "HZ: %d", state.settings.ticksPerFrame * 60);
    mvwprintw(win, 13, 14, "IR: 0x%04X", state.ir);

    wnoutrefresh(win);
}

void drawStack(WINDOW *win, const Chip8::State &state){
    werase(win);
    box(win, 0, 0);

    mvwprintw(win, 1, 4, "  --- STACK ---");
    for (int i = 0; i < 8; ++i)
    {
        if (!has_colors()){
            mvwprintw(win, 3 + i, 3, "[%X]: 0x%02X", i, state.stack[i]);
        } else {
            bool isActive = (state.sp > 0 && i == state.sp - 1);
            if (isActive) wattron(win, A_REVERSE); 

            mvwprintw(win, 3 + i, 3, "[%X]: 0x%03X", i, state.stack[i]);

            if (isActive) wattroff(win, A_REVERSE);
        }
    }

    for (int i = 8; i < 16; ++i)
    {
        mvwprintw(win, 3 + (i - 8), 14, "[%X]: 0x%02X", i, state.stack[i]);
    }

    wnoutrefresh(win);
}

void drawSettings(WINDOW* win, const Chip8::Settings& settings, int historyCount){
    werase(win);
    box(win, 0, 0);

    mvwprintw(win, 1, 4, " --- SETTINGS ---");
    string shiftStatus = (settings.shift) ? "MOD" : "LEG";
    string incrementStatus = (settings.increment) ? "MOD" : "LEG";
    string jumpStatus = (settings.jump) ? "MOD" : "LEG";
    string logStatus = (settings.logFlag) ? ((settings.log) ? "MOD" : "LEG") : "DISABLED";
    string vfStatus = (settings.vfPreserve) ? "MOD" : "LEG";
    string wrapStatus = (settings.wrap) ? "MOD" : "LEG";

    mvwprintw(win, 3, 4, "S: %s", shiftStatus.c_str());
    mvwprintw(win, 4, 4, "I: %s", incrementStatus.c_str());
    mvwprintw(win, 5, 4, "J: %s", jumpStatus.c_str());
    mvwprintw(win, 3, 14, "L: %s", logStatus.c_str());
    mvwprintw(win, 4, 14, "VF: %s", vfStatus.c_str());
    mvwprintw(win, 5, 14, "W: %s", wrapStatus.c_str());
    mvwprintw(win, 2, 8, "HIST: %d/600", historyCount);
    wnoutrefresh(win);
}

void drawDissassembly(WINDOW* win, const Chip8::State& state){
    werase(win);
    box(win ,0 ,0);
    mvwprintw(win, 1 , 6, "--- DISASSEMBLY ---");
    int y = 3;
    for (int i = -2; i <= 7; i++){
        uint16_t addr = state.pc + i * 2;

        if(addr >= 0x200 && addr < 0xFFF){
            uint16_t instruction = (state.memory[addr] << 8) | state.memory[addr + 1];
            string disassembled = Chip8::Disassemble(instruction);
            if (i == 0){
                wattron(win, A_REVERSE);
                mvwprintw(win, y, 1, "-> 0x%03X: %04X %-15s", addr, instruction, disassembled.c_str());
                wattroff(win, A_REVERSE);
            } else {
                mvwprintw(win, y, 1, "   0x%03X: %04X %-15s", addr, instruction, disassembled.c_str());
            }
        } else{
            mvwprintw(win, y, 1, "                          ");
        }
        ++y;
    }
    wnoutrefresh(win);
}

void drawHeatmap(WINDOW* win, const Chip8::State& state) {
    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 1, 18, "--- HEATMAP (0x200+) ---");

    const int ROWS = 11;
    const int COLS = 53;

    for (int row = 0; row < ROWS; row++) {
        uint16_t rowAddr = 0x200 + row * COLS * 2;
        mvwprintw(win, row + 2, 1, "%03X:", rowAddr);

        for (int col = 0; col < COLS; col++) {
            uint16_t addr = 0x200 + (row * COLS + col) * 2;
            if (addr + 1 >= 0xFFF) break;

            uint32_t heat = state.heatmap[addr];

            char symbol;
            int colorPair;
            int attr = A_NORMAL;

            if (heat == 0) {
                symbol = '.';
                colorPair = 0;
                attr = A_DIM;
            } else if (heat <= 5) {
                symbol = '+';
                colorPair = 1;
            } else if (heat <= 20) {
                symbol = '#';
                colorPair = 2;
            } else if (heat <= 100) {
                symbol = '%';
                colorPair = 3;
            } else {
                symbol = '@';
                colorPair = 4;
                attr = A_BOLD;
            }

            if (colorPair > 0) wattron(win, COLOR_PAIR(colorPair) | attr);
            else wattron(win, attr);

            mvwaddch(win, row + 2, col + 6, symbol);

            if (colorPair > 0) wattroff(win, COLOR_PAIR(colorPair) | attr);
            else wattroff(win, attr);
        }
    }

    wattron(win, COLOR_PAIR(1)); mvwprintw(win, 13, 2,  "+ cold"); wattroff(win, COLOR_PAIR(1));
    wattron(win, COLOR_PAIR(2)); mvwprintw(win, 13, 10, "# warm"); wattroff(win, COLOR_PAIR(2));
    wattron(win, COLOR_PAIR(3)); mvwprintw(win, 13, 18, "%% hot");  wattroff(win, COLOR_PAIR(3));
    wattron(win, COLOR_PAIR(4) | A_BOLD); mvwprintw(win, 13, 26, "@ fire"); wattroff(win, COLOR_PAIR(4) | A_BOLD);

    wnoutrefresh(win);
}

void drawKeys(WINDOW* win) {
    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 1, 2,  "[F5] Step Back");
    mvwprintw(win, 1, 17, "[F6] Pause/Play");
    mvwprintw(win, 1, 34, "[F7] Step Fwd");
    mvwprintw(win, 1, 50, "[F8] Reset");
    mvwprintw(win, 1, 62, "[F9] Toggle Log");
    mvwprintw(win, 1, 79, "[ESC] Quit");
    wnoutrefresh(win);
}
