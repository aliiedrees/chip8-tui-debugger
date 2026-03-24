#include "Chip8.h"
#include <ncurses.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <locale.h>


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

void drawRegisters(WINDOW *win, const Chip8State &state);
int main(int argc, const char **argv)
{
    Chip8::Settings settings = Chip8::Settings::ParseArgs(argc, argv);
    setlocale(LC_ALL, "");
    
    const int TICKS_PER_SECOND = 60;
    const int TICKS_PER_FRAME = 11;
    const int FRAME_DURATION_US = 1000000 / TICKS_PER_SECOND;
    Chip8 chip8(settings);
    chip8.LoadROM(argv[1]);

    initscr();
    noecho();
    cbreak();
    curs_set(0);
    nodelay(stdscr, true);
    keypad(stdscr, true);

    WINDOW *displayWin = newwin(34, 66, 0, 0);
    box(displayWin, 0, 0);

    WINDOW *regWin = newwin(34, 30, 0, 67);
    box(regWin, 0, 0);

    wnoutrefresh(displayWin);
    wnoutrefresh(regWin);

    bool running = true;
    bool paused = false;
    int key_timer[16] = {0}; // acts as a latch for the key
    
    while (running)
    {
        auto startTime = std::chrono::high_resolution_clock::now();
        int c = getch();

        if (c == 27) running = false;
        if (c == ' ') paused = !paused;
        if (c == 'n' && paused) chip8.Cycle();

        if (c != ERR) {
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

        if (!paused){ // ~480 hz 
            for (int i = 0; i < TICKS_PER_FRAME; ++i) {
                chip8.Cycle();
            }
            chip8.ActivateDT();
            chip8.ActivateST();
        }
        
        drawRegisters(regWin, chip8.GetState());


       if (chip8.ShouldRender())
        {
            box(displayWin, 0, 0); 
            const uint8_t *display = chip8.GetDisplay();

            for (int y = 0; y < 32; ++y)
            {
                std::string row = ""; // build the row string
                for (int x = 0; x < 64; ++x)
                {
                    // add a block or a space
                    row += (display[y * 64 + x] ? "█" : " ");
                }
                // Draw the entire row at once (starting at col 1 to avoid the border)
                mvwaddstr(displayWin, y + 1, 1, row.c_str());
            }
            chip8.DeActivateRender();
        }
        wnoutrefresh(displayWin);
        doupdate();


        auto endTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        if (elapsed < FRAME_DURATION_US)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(FRAME_DURATION_US - elapsed));
        }
    }

    endwin();
    std::cout << "Emulator Terminated Safely." << std::endl;
    return 0;
}

void drawRegisters(WINDOW *win, const Chip8State &state)
{
    werase(win);
    box(win, 0, 0);

    mvwprintw(win, 1, 2, "--- REGISTERS ---");
    for (int i = 0; i < 8; ++i)
    {
        mvwprintw(win, 3 + i, 2, "V%X: 0x%02X", i, state.V[i]);
    }

    for (int i = 8; i < 16; ++i)
    {
        mvwprintw(win, 3 + (i - 8), 15, "V%X: 0x%02X", i, state.V[i]);
    }

    mvwprintw(win, 12, 2, "--------------");
    mvwprintw(win, 14, 2, "PC: 0x%04X", state.pc);
    mvwprintw(win, 15, 2, "I: 0x%04X", state.I);
    mvwprintw(win, 16, 2, "SP: 0x%04X", state.sp);
    mvwprintw(win, 17, 2, "IR: 0x%04X", state.ir);

    wnoutrefresh(win);
}