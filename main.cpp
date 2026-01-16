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

class Vector2 {
public:
    float x, y;
    Vector2(float px = 0, float py = 0) : x(px), y(py) {}
    float magnitude() const { return std::hypot(x, y); }
    Vector2 normalize() const {
        float mag = magnitude();
        return mag > 0 ? Vector2(x / mag, y / mag) : Vector2();
    }
    Vector2 operator+(const Vector2& other) const { return Vector2(x + other.x, y + other.y); }
    Vector2 operator-(const Vector2& other) const { return Vector2(x - other.x, y - other.y); }
    Vector2 operator*(float scalar) const { return Vector2(x * scalar, y * scalar); }
};

class Entity {
public:
    Vector2 position;
    Vector2 velocity;
    bool active = false;
    Game* game = nullptr;
    Entity(Game* g = nullptr) : game(g) {}
    virtual ~Entity() = default;
    virtual void update() {}
    virtual void render(SDL_Renderer* renderer) const {}
};

class Ship : public Entity {
public:
    float angle = 0.0f;
    float fuel = 1000.0f;

    Ship(Game* g) : Entity (g) {
        position = Vector2(WINDOW_W / 2.0f, WINDOW_H / 2.0f);
        angle = -M_PI / 2;
    }

    void update(const Uint8* keys, float danger_level);
    void render(SDL_Renderer* renderer) const;
};

class Game {
public:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    void init();
};

void Game::init() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

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