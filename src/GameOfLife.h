#ifndef GAME_OF_LIFE_H
#define GAME_OF_LIFE_H

#include <Arduino.h>
#include <Adafruit_GFX_Pixel.hpp>

class GameOfLife {
public:
    static const int WIDTH = 84;
    static const int HEIGHT = 16;
    
    uint8_t grid[WIDTH][HEIGHT];
    uint8_t nextGrid[WIDTH][HEIGHT];
    uint32_t lastChange = 0;
    int generationCount = 0;

    void init() {
        randomSeed(analogRead(0) + millis());
        for (int x = 0; x < WIDTH; x++) {
            for (int y = 0; y < HEIGHT; y++) {
                grid[x][y] = (random(100) < 30) ? 1 : 0;
            }
        }
        generationCount = 0;
        lastChange = millis();
    }

    bool update() {
        bool changed = false;
        int aliveCount = 0;
        
        for (int x = 0; x < WIDTH; x++) {
            for (int y = 0; y < HEIGHT; y++) {
                int neighbors = countNeighbors(x, y);
                uint8_t currentState = grid[x][y];
                uint8_t nextState = currentState;

                if (currentState == 1) {
                    if (neighbors < 2 || neighbors > 3) nextState = 0;
                } else {
                    if (neighbors == 3) nextState = 1;
                }

                if (nextState != currentState) changed = true;
                nextGrid[x][y] = nextState;
                if (nextState == 1) aliveCount++;
            }
        }

        // Copy back
        memcpy(grid, nextGrid, sizeof(grid));
        generationCount++;

        // If no change or everyone is dead or too many generations, re-seed
        if (!changed || aliveCount == 0 || generationCount > 200) {
            init();
            return true;
        }
        return false;
    }

    void render(Adafruit_Pixel& gfx) {
        for (int x = 0; x < WIDTH; x++) {
            for (int y = 0; y < HEIGHT; y++) {
                if (grid[x][y]) gfx.drawPixel(x, y, 1);
            }
        }
    }

private:
    int countNeighbors(int x, int y) {
        int count = 0;
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if (i == 0 && j == 0) continue;
                int ni = (x + i + WIDTH) % WIDTH;
                int nj = (y + j + HEIGHT) % HEIGHT;
                if (grid[ni][nj]) count++;
            }
        }
        return count;
    }
};

#endif
