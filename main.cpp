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

#define MAX_CREATURES 38
#define MAX_PROJECTILES 50

#define SHIP_ROT_SPEED 0.10f
#define SHIP_THRUST 0.12f
#define FUEL_CONSUMPTION 0.085f
#define OVERHEAT_MAX 300.0f

// Overheat tuning
#define HEAT_GAIN_PER_THRUST 1.1f
#define HEAT_DECAY_NORMAL 0.7f
#define HEAT_DECAY_CRITICAL 0.35f
#define OVERHEAT_WARNING_THRESHOLD 0.80f
#define OVERHEAT_CRITICAL_THRESHOLD 1.00f
#define OVERHEAT_DAMAGE_PER_SEC 1.6f
#define OVERHEAT_THRUST_PENALTY 0.30f
#define OVERHEAT_DRAG_MULTIPLIER 0.94f

#define PROJECTILE_SPEED 8.0f
#define PROJECTILE_LIFE 120
#define SHOOT_COOLDOWN 10

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
    float fuel = 1000.0f;
    float heat = 0.0f;
    float thrusting = false;
    float overheat_damage_accumulator = 0.0f;
    int shoot_timer = 0;

    Ship(Game* g) : Entity (g) {
        position = Vector2(WINDOW_W / 2.0f, WINDOW_H / 2.0f);
        angle = -M_PI / 2;
    }

    void update(const Uint8* keys);
    void render(SDL_Renderer* renderer) const;
    bool is_critical_overheat() const { return heat >= OVERHEAT_MAX * OVERHEAT_CRITICAL_THRESHOLD; }
    bool is_overheat_warning() const { return heat >= OVERHEAT_MAX * OVERHEAT_WARNING_THRESHOLD; }
};

class NebulaCreature : public Entity {
public:
    float angle;
    float wiggle;
    float size;
    float hunt_phase;
    float patrol_phase;
    Vector2 target;
    int type;
    Uint32 color;
    int health = 1;

    NebulaCreature(Game* g) : Entity(g) {}
    void spawn(Game* g, const Ship& ship);
    void update(const Ship& ship);
    void render(SDL_Renderer* renderer) const;
};

class Projectile : public Entity {
public: 
    float life = PROJECTILE_LIFE;
    Uint32 color = 0xFFAA00FF;

    Projectile(Game* g = nullptr) : Entity(g) {}
    void update();
    void render(SDL_Renderer* renderer) const;
};

class Game {
public:
    Ship ship;
    std::vector<NebulaCreature> creatures;
    std::vector<Projectile> projectiles;


    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    int frame = 0;

    Game() : ship(this) {
        creatures.reserve(MAX_CREATURES);
        projectiles.reserve(MAX_PROJECTILES);
    }

    void init();
    void update();
    void render();
    void wrap(Vector2& pos);
};

void Game::wrap(Vector2& pos) {
    pos.x = std::fmod(pos.x + WINDOW_W * 10, WINDOW_W);
    pos.y = std::fmod(pos.y + WINDOW_H * 10, WINDOW_H);
}

void Projectile::update() {
    position = position + velocity;
    life -= 1.0f;
}

void Projectile::render(SDL_Renderer* renderer) const {
    SDL_SetRenderDrawColor(renderer, (color>>16)&255, (color>>8)&255, color&255, 255);
    int px = static_cast<int>(position.x), py = static_cast<int>(position.y);
    for (int i = -2; i <= 2; i++) {
        SDL_RenderDrawPoint(renderer, px + i, py);
        SDL_RenderDrawPoint(renderer, px, py + i);
    }
}

void Ship::update(const Uint8* keys) {
    thrusting = false;
    int left = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
    int right = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
    int thrust = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
    int shoot = keys[SDL_SCANCODE_J];

    if (left) angle -= SHIP_ROT_SPEED;
    if (right) angle += SHIP_ROT_SPEED;

    float effective_thrust = SHIP_THRUST;
    if (is_critical_overheat()) effective_thrust *= OVERHEAT_THRUST_PENALTY;

    if (thrust && fuel > 5.0f) {
        velocity = velocity + Vector2(std::cos(angle), std::sin(angle)) * SHIP_THRUST;
        fuel -= FUEL_CONSUMPTION;
        heat += HEAT_GAIN_PER_THRUST;
        thrusting = true;
    }

    if (shoot && shoot_timer <= 0) {
        Projectile p(game);
        p.position = position + Vector2(std::cos(angle) * 30, std::sin(angle) * 30);
        p.velocity = Vector2(std::cos(angle), std::sin(angle)) * PROJECTILE_SPEED + velocity * 0.5f;
        game->projectiles.push_back(p);
        shoot_timer = SHOOT_COOLDOWN;
    }
    shoot_timer--;

    float decay = is_critical_overheat() ? HEAT_DECAY_CRITICAL : HEAT_DECAY_NORMAL;
    heat = std::fmax(0.0f, heat - decay);

    position = position + velocity;

    if (is_critical_overheat()) {
        velocity = velocity * OVERHEAT_DRAG_MULTIPLIER;
    } else {
        velocity = velocity * 0.985f;
    }
    fuel = std::fmin(1000.0f, fuel + 0.35f);
}

void Ship::render(SDL_Renderer* renderer) const {
    float heat_ratio = heat / OVERHEAT_MAX;
    float heat_glow = std::fmin(heat_ratio, 1.3f);

    Uint8 r = 255;
    Uint8 g = static_cast<Uint8>(255 - heat_glow * 160);
    Uint8 b = static_cast<Uint8>(120 + heat_glow * 40);
    Uint8 alpha = 220 + static_cast<Uint8>(35 * std::sin(game->frame * 0.25f));
    
    if (is_critical_overheat()) {
        if ((game->frame / 5) % 2 == 0) {
            r = 255; g = 50; b = 30;
            alpha = 255;
        } else {
            r = 230; g = 90; b = 50;
            alpha = 200;
        }
    } else if (is_overheat_warning()) {
        g = static_cast<Uint8>(g * 0.5f + 100);
        b = static_cast<Uint8>(b * 0.3f + 30);
    }

    SDL_SetRenderDrawColor(renderer, r, g, b, alpha);

    Vector2 nose = position + Vector2(std::cos(angle) * 30, std::sin(angle) * 30);
    Vector2 left = position + Vector2(std::cos(angle + 2.4f) * 24, std::sin(angle + 2.4f) * 24);
    Vector2 right = position + Vector2(std::cos(angle - 2.4f) * 24, std::sin(angle - 2.4f) * 24);
    Vector2 core = position + Vector2(std::cos(angle) * 14, std::sin(angle) * 14);

    thick_line(renderer, static_cast<int>(nose.x), static_cast<int>(nose.y), static_cast<int>(left.x), static_cast<int>(left.y), 7);
    thick_line(renderer, static_cast<int>(left.x), static_cast<int>(left.y), static_cast<int>(core.x), static_cast<int>(core.y), 7);
    thick_line(renderer, static_cast<int>(core.x), static_cast<int>(core.y), static_cast<int>(right.x), static_cast<int>(right.y), 7);
    thick_line(renderer, static_cast<int>(right.x), static_cast<int>(right.y), static_cast<int>(nose.x), static_cast<int>(nose.y), 7);

    SDL_SetRenderDrawColor(renderer, 255, 240 - static_cast<int>(heat_glow * 120), 180, 200);
    for (int rad = 0; rad < 12; rad++) {
        SDL_RenderDrawLine(renderer, static_cast<int>(position.x - rad), static_cast<int>(position.y), static_cast<int>(position.x + rad), static_cast<int>(position.y));
        SDL_RenderDrawLine(renderer, static_cast<int>(position.x), static_cast<int>(position.y - rad), static_cast<int>(position.x), static_cast<int>(position.y + rad));
    }
}


void Game::init() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    projectiles.clear();
    frame = 0;
    ship = Ship(this);
}

void Game::render() {
    SDL_SetRenderDrawColor(renderer, 3, 3, 12, 255);
    SDL_RenderClear(renderer);

    for (const auto& proj : projectiles) proj.render(renderer);
    ship.render(renderer);

    SDL_RenderPresent(renderer);
}

void Game::update() {
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    frame++;
    ship.update(keys);
    wrap(ship.position);
    // TODO: Code thrust_fame()

    for (auto it = projectiles.begin(); it != projectiles.end(); ) {
        it->update();
        if (it->life <= 0) {
            it = projectiles.erase(it);
            continue;
        }
        ++it;
        // TODO: Hit detection with creatures
    }
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