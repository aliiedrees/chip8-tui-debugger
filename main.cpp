#include "Chip8.h"
#include "Exceptions.h"
#include <ncurses.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <locale.h>
#include <fstream>


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
void drawSettings(WINDOW* win, const Chip8::Settings& settings);
void drawStack(WINDOW *win, const Chip8::State &state);
void drawDissassembly(WINDOW* win, const Chip8::State& state);


int main(int argc, const char** argv) {
    try{
        // this can give excpetion
        Chip8::Settings settings = Chip8::Settings::ParseArgs(argc, argv);
        ofstream logFile;
        setlocale(LC_ALL, "");
        
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

        WINDOW* displayWin = newwin(34, 66, 0, 0);
        WINDOW* regWin = newwin(15, 26, 0, 66);
        WINDOW* stackWin = newwin(12, 26, 15, 66);
        WINDOW* settingsWin = newwin(7, 26, 27, 66);
        WINDOW* disWin = newwin(14, 50, 34, 0);
        WINDOW* memWin = newwin(14, 50, 34, 50);
        
        wnoutrefresh(displayWin);
        wnoutrefresh(regWin);
        wnoutrefresh(stackWin);
        wnoutrefresh(settingsWin);
        wnoutrefresh(disWin);
        wnoutrefresh(memWin);
        box(displayWin, 0, 0); 
        wnoutrefresh(displayWin);

        bool running = true;
        bool paused = true;
        bool reset = false;
        int key_timer[16] = {0}; // acts as a latch for the key
        
        while (running){
            auto startTime = std::chrono::high_resolution_clock::now();
            int c = getch();

            if (c == 27) {
                running = false; 
                break;
            } else if (c == KEY_F(6)) paused = !paused;
            else if (c == KEY_F(5) && paused) chip8.Cycle(running);
            else if (c == KEY_F(8)){ // reset
                chip8 = Chip8(settings);
                chip8.LoadROM();
                reset = true;
            }else if (c == KEY_F(9)){
                chip8.ToggleLog(logFile);
            }else if (c != ERR) {
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

            if (!paused && !reset){ // ~480 hz by default 
                for (int i = 0; i < settings.ticksPerFrame; ++i) {
                    chip8.Cycle(running);
                    if (!running) break;
                }
                chip8.ActivateDT();
                chip8.ActivateST();
            }
            
            drawRegisters(regWin, chip8.GetState());
            drawStack(stackWin, chip8.GetState());
            drawSettings(settingsWin, chip8.GetState().settings);
            drawDissassembly(disWin, chip8.GetState());

            if (chip8.ShouldRender() || reset){
                box(displayWin, 0, 0); 
                const uint8_t *display = chip8.GetDisplay();
                for (int y = 0; y < 32; ++y){
                    std::string row = ""; // build the row string
                    for (int x = 0; x < 64; ++x){
                        // add a block or a space
                        row += (display[y * 64 + x] ? "█" : " ");
                    }
                    // Draw the entire row at once (starting at col 1 to avoid the border)
                    mvwaddstr(displayWin, y + 1, 1, row.c_str());
                }
                chip8.ToggleRender();
                reset = false;
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
            if(chip8.Logging()){
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
    catch (const Chip8Exception& e){
        endwin();
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (const std::exception& e){
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

void drawSettings(WINDOW* win, const Chip8::Settings& settings){
    werase(win);
    box(win, 0, 0);

    mvwprintw(win, 1, 4, " --- SETTINGS ---");
    string shiftStatus = (settings.shift) ? "ORG" : "MOD";
    string incrementStatus = (settings.increment) ? "ORG" : "MOD";
    string jumpStatus = (settings.jump) ? "ORG" : "MOD";
    string logStatus = (settings.logFlag) ? ((settings.log) ? "ON" : "OFF") : "DISABLED";
    string vfStatus = (settings.vfReset) ? "ORG" : "MOD";
    string wrapStatus = (settings.wrap) ? "ORG" : "MOD";

    mvwprintw(win, 3, 4, "S: %s", shiftStatus.c_str());
    mvwprintw(win, 4, 4, "I: %s", incrementStatus.c_str());
    mvwprintw(win, 5, 4, "J: %s", jumpStatus.c_str());
    mvwprintw(win, 3, 14, "L: %s", logStatus.c_str());
    mvwprintw(win, 4, 14, "VF: %s", vfStatus.c_str());
    mvwprintw(win, 5, 14, "W: %s", wrapStatus.c_str());

    wnoutrefresh(win);
}

void drawDissassembly(WINDOW* win, const Chip8::State& state){
    box(win ,0 ,0);
    mvwprintw(win, 1 , 2, "--- DISASSEMBLY ---");
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

