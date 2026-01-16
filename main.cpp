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

#define SHIP_ROT_SPEED 0.10f
#define SHIP_THRUST 0.12f

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

void thick_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int thickness) {
    if (thickness <= 1) {
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        return;
    }
    int dx = x2 - x1, dy = y2 - y1;
    float len = std::hypot(dx, dy);
    if (len < 1.0f) return;
    float nx = -dy / len, ny = dx / len;
    for (int t = -thickness/2; t <= thickness/2; t++) {
        int ox = static_cast<int>(nx * t), oy = static_cast<int>(ny * t);
        SDL_RenderDrawLine(renderer, x1 + ox, y1 + oy, x2 + ox, y2 + oy);

    }
}

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
    float thrusting = false;

    Ship(Game* g) : Entity (g) {
        position = Vector2(WINDOW_W / 2.0f, WINDOW_H / 2.0f);
        angle = -M_PI / 2;
    }

    void update(const Uint8* keys);
    void render(SDL_Renderer* renderer) const;
};

void Ship::update(const Uint8* keys) {
    thrusting = false;
    int left = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
    int right = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
    int thrust = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];

    if (left) angle -= SHIP_ROT_SPEED;
    if (right) angle += SHIP_ROT_SPEED;

    if (thrust) {
        velocity = velocity + Vector2(std::cos(angle), std::sin(angle)) * SHIP_THRUST;
        thrusting = true;
    }

    position = position + velocity;
}

void Ship::render(SDL_Renderer* renderer) const {
    Uint8 r = 255;
    Uint8 g = 255;
    Uint8 b = 255;
    Uint8 alpha = 255;

    SDL_SetRenderDrawColor(renderer, r, g, b, alpha);

    Vector2 nose = position + Vector2(std::cos(angle) * 30, std::sin(angle) * 30);
    Vector2 left = position + Vector2(std::cos(angle + 2.4f) * 24, std::sin(angle + 2.4f) * 24);
    Vector2 right = position + Vector2(std::cos(angle - 2.4f) * 24, std::sin(angle - 2.4f) * 24);
    Vector2 core = position + Vector2(std::cos(angle) * 14, std::sin(angle) * 14);

    thick_line(renderer, static_cast<int>(nose.x), static_cast<int>(nose.y), static_cast<int>(left.x), static_cast<int>(left.y), 7);
    thick_line(renderer, static_cast<int>(left.x), static_cast<int>(left.y), static_cast<int>(core.x), static_cast<int>(core.y), 7);
    thick_line(renderer, static_cast<int>(core.x), static_cast<int>(core.y), static_cast<int>(right.x), static_cast<int>(right.y), 7);
    thick_line(renderer, static_cast<int>(right.x), static_cast<int>(right.y), static_cast<int>(nose.x), static_cast<int>(nose.y), 7);
}

class Game {
public:
    Ship ship;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    Game() : ship(this) {}
    void init();
    void update();
    void render();
};


void Game::init() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    ship = Ship(this);
}

void Game::render() {
    SDL_SetRenderDrawColor(renderer, 3, 3, 12, 255);
    SDL_RenderClear(renderer);

    ship.render(renderer);

    SDL_RenderPresent(renderer);
}

void Game::update() {
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    ship.update(keys);
    // TODO: Code thrust_fame()
}



int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    Game game;
    game.window = SDL_CreateWindow("Nebula Harvester", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, 0);
    game.renderer = SDL_CreateRenderer(game.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawBlendMode(game.renderer, SDL_BLENDMODE_BLEND);

    game.init();

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
        }

        game.update();
        game.render();
        SDL_Delay(16);
    }
    
    SDL_DestroyRenderer(game.renderer);
    SDL_DestroyWindow(game.window);
    SDL_Quit();
    return 0;
}