#define _USE_MATH_DEFINES
#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cstdbool>
#include <string>

#define WINDOW_W 1200
#define WINDOW_H 675

class Game;

class Game {
public:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
};

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    Game game;
    game.window = SDL_CreateWindow("Nebula Harvester", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, 0);
    SDL_SetRenderDrawBlendMode(game.renderer, SDL_BLENDMODE_BLEND);

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
        }
    }
    
    SDL_DestroyRenderer(game.renderer);
    SDL_DestroyWindow(game.window);
    SDL_Quit();
    return 0;
}