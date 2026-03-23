#include "Chip8.h"
#include <ncurses.h>
#include <chrono>
#include <iostream>
#include <thread>

uint8_t keymap[17] = {
    'x', '1', '2', '3', // 0, 1, 2, 3
    'q', 'w', 'e', 'a', // 4, 5, 6, 7
    's', 'd', 'z', 'c', // 8, 9, A, B
    '4', 'r', 'f', 'v',  // C, D, E, F
};

int main(int argc, char** argv){
    if(argc != 2){
        std::cerr << "Usage: " << argv[0] << " <ROM_PATH>" << std::endl;
        return 1;
    }

    const int TICKS_PER_SECOND = 60;
    const int TICKS_PER_FRAME = 8;
    const int FRAME_DURATION_MS = 1000 / TICKS_PER_SECOND;

    Chip8 chip8;
    chip8.LoadROM(argv[1]);

    initscr();
    noecho();
    curs_set(0);
    nodelay(stdscr, true);
    keypad(stdscr, true);

    bool quit = false;

    while (!quit){
        auto startTime = std::chrono::high_resolution_clock::now();
        int c = getch();
        if (c == 27){
            quit = true;
        }

        for (int i = 0; i < 16; ++i){
            chip8.keyboard[i] = 0;
        }

        for (int i = 0; i < 16; ++i){
            if (c == keymap[i]){
                chip8.keyboard[i] = 1;
                break;
            }
        }
        for(int i = 0; i < TICKS_PER_FRAME; ++i){
            chip8.Cycle();
        }

        if (chip8.dt > 0) --chip8.dt;
        if (chip8.st > 0) --chip8.st;

        if (chip8.shouldRender){
            for (int y = 0; y < 32; ++y){
                for (int x = 0; x < 64; ++x){
                    if (chip8.display[y * 64 + x]) {
                        mvaddch(y, x, '#'); 
                    } else {
                        mvaddch(y, x, ' ');
                    }
                }
            }
            refresh();
            chip8.shouldRender = false;
        }
        
        
    auto endTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

    if (elapsed < FRAME_DURATION_MS) {
        std::this_thread::sleep_for(std::chrono::microseconds(FRAME_DURATION_MS - elapsed));
    }    }

    endwin();
    std::cout << "Emulator Terminated Safely." << std::endl;
    return 0;
}