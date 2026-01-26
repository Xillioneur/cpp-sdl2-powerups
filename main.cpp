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

#define MAX_CLOUDS    120
#define MAX_PARTICLES 700
#define MAX_CREATURES 38
#define MAX_NEBULAE   12
#define NUM_STARS     600
#define NUM_DEBRIS    300
#define NUM_PLANETS   6
#define MAX_POWERUPS  10  // New for episode 6
#define MAX_PROJECTILES 50  // New for episode 6: shooting

#define SHIP_ROT_SPEED    0.10f
#define SHIP_THRUST       0.12f
#define HARVEST_RANGE     65.0f
#define CREATURE_DANGER_DIST 140.0f
#define FUEL_CONSUMPTION  0.085f
#define OVERHEAT_MAX      300.0f

// Overheat tuning
#define HEAT_GAIN_PER_THRUST      1.1f
#define HEAT_DECAY_NORMAL         0.7f
#define HEAT_DECAY_CRITICAL       0.35f
#define OVERHEAT_WARNING_THRESHOLD  0.80f
#define OVERHEAT_CRITICAL_THRESHOLD 1.00f
#define OVERHEAT_DAMAGE_PER_SEC     1.6f
#define OVERHEAT_THRUST_PENALTY     0.30f
#define OVERHEAT_DRAG_MULTIPLIER    0.94f

#define TRACTOR_RANGE     220.0f
#define COMBO_BOOST_THRESHOLD 8
#define COMBO_BOOST_DURATION 600

#define CLOUDS_PER_WAVE_BASE 45
#define WAVE_CREATURE_BONUS 4

// New constants for episode 6 advancements
#define POWERUP_SPAWN_CHANCE 0.05f  // 5% chance per frame after wave 2
#define POWERUP_DURATION 900  // 15 seconds at 60 FPS
#define POWERUP_TYPES 3  // 0: Fuel Boost, 1: Heat Reduction, 2: Score Multiplier

// Shooting constants
#define PROJECTILE_SPEED 8.0f
#define PROJECTILE_LIFE 120
#define SHOOT_COOLDOWN 10
#define PROJECTILE_DAMAGE 1  // Creatures have health now

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

float distance(const Vector2& a, const Vector2& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    if (std::fabs(dx) > WINDOW_W / 2) dx -= (dx > 0 ? WINDOW_W : -WINDOW_W);
    if (std::fabs(dy) > WINDOW_H / 2) dy -= (dy > 0 ? WINDOW_H : -WINDOW_H);
    return std::hypot(dx, dy);
}

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

const bool digit_segments[10][7] = {
    {1,1,1,0,1,1,1}, // 0
    {0,0,1,0,0,1,0}, // 1
    {1,0,1,1,1,0,1}, // 2
    {1,0,1,1,0,1,1}, // 3
    {0,1,1,1,0,1,0}, // 4
    {1,1,0,1,0,1,1}, // 5
    {1,1,0,1,1,1,1}, // 6
    {1,0,1,0,0,1,0}, // 7
    {1,1,1,1,1,1,1}, // 8
    {1,1,1,1,0,1,1}  // 9
};

void draw_7segment_digit(SDL_Renderer* renderer, int bx, int by, int digit) {
    if (digit < 0 || digit > 9) return;
    
    int w = 20;
    int h = 32;
    int thick = 4;
    
    if (digit_segments[digit][0]) thick_line(renderer, bx, by, bx + w, by, thick);
    if (digit_segments[digit][1]) thick_line(renderer, bx, by, bx, by + h/2, thick);
    if (digit_segments[digit][2]) thick_line(renderer, bx + w, by, bx + w, by + h/2, thick);
    if (digit_segments[digit][3]) thick_line(renderer, bx, by + h/2, bx + w, by + h/2, thick);
    if (digit_segments[digit][4]) thick_line(renderer, bx, by + h/2, bx, by + h, thick);
    if (digit_segments[digit][5]) thick_line(renderer, bx + w, by + h/2, bx + w, by + h, thick);
    if (digit_segments[digit][6]) thick_line(renderer, bx, by + h, bx + w, by + h, thick);
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
    int score = 0;
    int combo = 0;
    int lives = 3;
    bool tractor_active = false;
    float tractor_charge = 0.0f;
    bool combo_boost_active = false;
    int combo_boost_timer = 0;
    float overheat_damage_accumulator = 0.0f;
    bool thrusting = false;
    float damage_this_frame = 0.0f;
    int shoot_timer = 0;
    
    // New for episode 6: Power-up states
    int active_powerup = -1;  // -1: none, 0: fuel, 1: heat, 2: score
    int powerup_timer = 0;
    float score_multiplier = 1.0f;

    Ship(Game* g) : Entity(g) {
        position = Vector2(WINDOW_W / 2.0f, WINDOW_H / 2.0f);
        angle = -M_PI / 2;
    }

    void update(const Uint8* keys, float danger_level);
    void render(SDL_Renderer* renderer) const;
    bool is_critical_overheat() const { return heat >= OVERHEAT_MAX * OVERHEAT_CRITICAL_THRESHOLD; }
    bool is_overheat_warning() const { return heat >= OVERHEAT_MAX * OVERHEAT_WARNING_THRESHOLD; }
};

class GasCloud : public Entity {
public:
    float size;
    float density;
    float phase;
    float pull_strength;
    int value;
    Uint32 color;

    GasCloud(Game* g) : Entity(g) {}
    void spawn(Game* g);
    void update(const Ship& ship);
    void render(SDL_Renderer* renderer) const;
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
    int health = 1;  // New: creatures have health

    NebulaCreature(Game* g) : Entity(g) {}
    void spawn(Game* g, const Ship& ship);
    void update(const Ship& ship);
    void render(SDL_Renderer* renderer) const;
};

class Nebula : public Entity {
public:
    float radius;
    float density;
    float swirl;
    float pulse;
    Uint32 color;

    Nebula(Game* g) : Entity(g) {}
    void spawn(int idx);
    void update();
    void render(SDL_Renderer* renderer) const;
};

class Particle : public Entity {
public:
    float life;
    Uint32 color;

    Particle(Game* g = nullptr) : Entity(g) {}
    void update();
    void render(SDL_Renderer* renderer) const;
};

class Projectile : public Entity {
public:
    float life = PROJECTILE_LIFE;
    Uint32 color = 0xFFAA00FF;

    Projectile(Game* g) : Entity(g) {}
    void update();
    void render(SDL_Renderer* renderer) const;
};

class Star {
public:
    float base_x, base_y;
    int brightness, phase, size;
};

class Debris {
public:
    float base_x, base_y;
    float vx;
    int size;
};

class Planet {
public:
    float base_x, base_y;
    float radius;
    Uint32 color;
    float spin;
};

class Sun {
public:
    float base_x, base_y;
    float radius;
    float pulse_phase;
};

// New for episode 6: Power-up class
class PowerUp : public Entity {
public:
    int type;  // 0: Fuel, 1: Heat, 2: Score
    float size = 20.0f;
    Uint32 color;

    PowerUp(Game* g) : Entity(g) {}
    void spawn(Game* g);
    void update(Ship& ship);
    void render(SDL_Renderer* renderer) const;
};

class Game {
public:
    Ship ship;
    std::vector<GasCloud> clouds;
    std::vector<NebulaCreature> creatures;
    std::vector<Nebula> nebulas;
    std::vector<Particle> particles;
    std::vector<Projectile> projectiles;  // New for shooting
    std::vector<Star> stars;
    std::vector<Debris> debris;
    std::vector<Planet> planets;
    std::vector<PowerUp> powerups;  // New for episode 6
    Sun sun;

    int frame = 0;
    float scrollX = 0.0f;
    float danger_level = 0.0f;
    int combo_timer = 0;

    int wave = 1;
    int clouds_collected_this_wave = 0;
    int clouds_needed_for_next_wave = CLOUDS_PER_WAVE_BASE;
    int wave_flash_timer = 0;
    int current_wave_display_timer = 0;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    Game() : ship(this) {
        clouds.reserve(MAX_CLOUDS);
        creatures.reserve(MAX_CREATURES);
        nebulas.reserve(MAX_NEBULAE);
        particles.reserve(MAX_PARTICLES);
        projectiles.reserve(MAX_PROJECTILES);
        stars.resize(NUM_STARS);
        debris.resize(NUM_DEBRIS);
        planets.resize(NUM_PLANETS);
        powerups.reserve(MAX_POWERUPS);
    }

    void init();
    void update();
    void render();
    void spawn_particle(const Vector2& pos, const Vector2& vel, Uint32 color, float life);
    void harvest_effect(const Vector2& pos, int intensity);
    void tractor_beam_effect(const Vector2& from, const Vector2& to);
    void danger_trail(const Vector2& pos);
    void critical_overheat_effect();
    void thrust_flame();
    void trail_emit();
    void wrap(Vector2& pos);
    void creature_destroy_effect(const Vector2& pos);
};

void Game::creature_destroy_effect(const Vector2& pos) {
    for (int i = 0; i < 50; i++) {
        float ang = static_cast<float>(i) / 50 * 2 * M_PI;
        float speed = 4.0f + (rand() % 100) / 20.0f;
        Uint32 c = 0xFF8800AA | ((rand() % 120 + 135) << 24);
        spawn_particle(pos, Vector2(std::cos(ang) * speed, std::sin(ang) * speed), c, 80 + rand() % 60);
    }
}

void Game::wrap(Vector2& pos) {
    pos.x = std::fmod(pos.x + WINDOW_W * 10, WINDOW_W);
    pos.y = std::fmod(pos.y + WINDOW_H * 10, WINDOW_H);
}

void Game::spawn_particle(const Vector2& pos, const Vector2& vel, Uint32 color, float life) {
    if (particles.size() >= MAX_PARTICLES) return;
    Particle p(this);
    p.position = pos;
    p.velocity = vel;
    p.life = life;
    p.color = color;
    p.active = true;
    particles.push_back(p);
}

void Game::harvest_effect(const Vector2& pos, int intensity) {
    for (int i = 0; i < 30 + intensity * 15; i++) {
        float ang = static_cast<float>(i) / (30 + intensity * 15) * 2 * M_PI;
        float speed = 3.5f + (rand() % 90) / 30.0f;
        Uint32 c = 0xAAEEFFAA | ((rand() % 120 + 135) << 24);
        spawn_particle(pos, Vector2(std::cos(ang) * speed, std::sin(ang) * speed * 0.7f), c, 60 + rand() % 50);
    }
}

void Game::tractor_beam_effect(const Vector2& from, const Vector2& to) {
    for (int i = 0; i < 18; i++) {
        float t = static_cast<float>(i) / 18.0f + (rand() % 20)/1000.0f;
        Vector2 p = from + (to - from) * t;
        float jitter_x = (rand() % 40 - 20) * 0.15f;
        float jitter_y = (rand() % 40 - 20) * 0.15f;
        Uint32 c = 0xCCEEFFFF | ((200 + static_cast<int>(std::sin(frame * 0.5f + i) * 55)) << 24);
        spawn_particle(p + Vector2(jitter_x, jitter_y), Vector2((rand() % 40 - 20) * 0.2f, (rand() % 40 - 20) * 0.2f), c, 40 + rand() % 20);
    }
}

void Game::danger_trail(const Vector2& pos) {
    for (int i = 0; i < 8; i++) {
        float ang = (rand() % 360) * M_PI / 180.0f;
        float speed = 5.0f + (rand() % 50) / 10.0f;
        Uint32 c = 0xFF4444FF | ((rand() % 100 + 140) << 24);
        spawn_particle(pos, Vector2(std::cos(ang) * speed, std::sin(ang) * speed), c, 30 + rand() % 25);
    }
}

void Game::critical_overheat_effect() {
    float rear = ship.angle + M_PI;
    Vector2 rear_pos = ship.position + Vector2(std::cos(rear) * 20, std::sin(rear) * 20);
    for (int i = 0; i < 8; i++) {
        float ang = rear + (rand() % 100 - 50) * 0.018f;
        float spd = 3.5f + (rand() % 60)/10.0f;
        Uint32 c = 0xAA444444 | ((90 + rand() % 80) << 24);
        spawn_particle(rear_pos, Vector2(std::cos(ang)*spd + ship.velocity.x*0.3f, std::sin(ang)*spd + ship.velocity.y*0.3f), c, 60 + rand() % 50);
    }
    if (frame % 4 == 0) {
        for (int i = 0; i < 5; i++) {
            float ang = static_cast<float>(rand()) * 2 * M_PI / RAND_MAX;
            float spd = 4.5f + (rand() % 60)/10.0f;
            Uint32 c = 0xFFFF8800 | ((180 + rand() % 75) << 24);
            spawn_particle(ship.position, Vector2(std::cos(ang)*spd, std::sin(ang)*spd), c, 30 + rand() % 25);
        }
    }
}

void Game::thrust_flame() {
    float rear = ship.angle + M_PI;
    Vector2 px = ship.position + Vector2(std::cos(rear) * 22, std::sin(rear) * 22);
    for (int i = 0; i < 14; i++) {
        float ang = rear + (rand() % 120 - 60) * 0.015f;
        float spd = 7.0f + (rand() % 70) / 10.0f;
        Uint32 c = (rand() % 3 == 0) ? 0xFFAA88FF : 0xEEFFCCFF;
        spawn_particle(px, Vector2(std::cos(ang) * spd + ship.velocity.x * 0.25f, std::sin(ang) * spd + ship.velocity.y * 0.25f), c, 30 + rand() % 25);
    }
}

void Game::trail_emit() {
    float speed = ship.velocity.magnitude();
    if (speed < 3.5f || frame % 3 != 0) return;
    float rear = std::atan2(ship.velocity.y, ship.velocity.x) + M_PI;
    Vector2 px = ship.position + Vector2(std::cos(rear) * 20, std::sin(rear) * 20);
    for (int i = 0; i < 5; i++) {
        float ang = rear + (rand() % 100 - 50) * 0.012f;
        spawn_particle(px, Vector2(std::cos(ang) * (2.0f + speed * 0.3f), std::sin(ang) * (2.0f + speed * 0.3f)), 0x66DDFFFF, 40 + rand() % 35);
    }
}

void GasCloud::spawn(Game* g) {
    game = g;
    active = true;
    size = 18 + (rand() % 32);
    density = 0.65f + (rand() % 35) / 100.0f;
    phase = rand() * 2 * M_PI / RAND_MAX;
    pull_strength = 0.14f + (rand() % 70) / 1000.0f;
    value = 6 + (rand() % 10);
    
    int tries = 0;
    do {
        position.x = rand() % WINDOW_W;
        position.y = rand() % WINDOW_H;
    } while (distance(position, game->ship.position) < 180 && ++tries < 50);
    
    float dir = rand() * 2 * M_PI / RAND_MAX;
    float speed = 0.4f + (rand() % 50) / 100.0f;
    velocity = Vector2(std::cos(dir) * speed, std::sin(dir) * speed);
    
    int hue = 140 + rand() % 100;
    float sat = 0.9f + (rand() % 10)/100.0f;
    float val = 1.0f;
    float cmax = val * sat;
    float hp = hue / 60.0f;
    float x = cmax * (1 - std::fabs(std::fmod(hp, 2) - 1));
    Uint8 r, green, b;
    if (hp < 1) { r = static_cast<Uint8>(cmax*255); green = static_cast<Uint8>(x*255); b = 0; }
    else if (hp < 2) { r = static_cast<Uint8>(x*255); green = static_cast<Uint8>(cmax*255); b = 0; }
    else if (hp < 3) { r = 0; green = static_cast<Uint8>(cmax*255); b = static_cast<Uint8>(x*255); }
    else if (hp < 4) { r = 0; green = static_cast<Uint8>(x*255); b = static_cast<Uint8>(cmax*255); }
    else if (hp < 5) { r = static_cast<Uint8>(x*255); green = 0; b = static_cast<Uint8>(cmax*255); }
    else { r = static_cast<Uint8>(cmax*255); green = 0; b = static_cast<Uint8>(x*255); }
    color = (r << 16) | (green << 8) | b | 0xFF;
}

void NebulaCreature::spawn(Game* g, const Ship& ship) {
    game = g;
    active = true;
    size = 16 + rand() % 26;
    hunt_phase = 0;
    wiggle = rand() * 2 * M_PI / RAND_MAX;
    patrol_phase = rand() * 2 * M_PI / RAND_MAX;
    
    type = rand() % 3;
    health = type + 1;  // Different health based on type
    
    int tries = 0;
    do {
        float side = rand() % 4;
        if (side == 0) { position.x = -100; position.y = rand() % WINDOW_H; }
        else if (side == 1) { position.x = WINDOW_W + 100; position.y = rand() % WINDOW_H; }
        else if (side == 2) { position.y = -100; position.x = rand() % WINDOW_W; }
        else { position.y = WINDOW_H + 100; position.x = rand() % WINDOW_W; }
    } while (distance(position, ship.position) < 300 && ++tries < 80);
    
    float dir_to_ship = std::atan2(ship.position.y - position.y, ship.position.x - position.x);
    float offset = (rand() % 100 - 50) / 100.0f * M_PI / 2;
    float target_dir = dir_to_ship + offset;
    float target_dist = 300 + rand() % 400;
    target = position + Vector2(std::cos(target_dir), std::sin(target_dir)) * target_dist;
    
    float dir = rand() * 2 * M_PI / RAND_MAX;
    float base_speed = (type == 0) ? 0.8f : (type == 1) ? 1.4f : 1.0f;
    velocity = Vector2(std::cos(dir) * base_speed, std::sin(dir) * base_speed);
    angle = dir;
    
    if (type == 0) color = 0x88BBFFFF | ((170 + rand() % 50) << 24);
    else if (type == 1) color = 0xFF8888FF | ((140 + rand() % 60) << 24);
    else color = 0xCC88FFFF | ((130 + rand() % 70) << 24);
}

void Nebula::spawn(int idx) {
    active = true;
    radius = 180 + rand() % 120;
    density = 0.4f + (rand() % 40)/100.0f;
    swirl = rand() * 2 * M_PI / RAND_MAX;
    pulse = 0.0f;
    position.x = WINDOW_W * (0.2f + (rand() % 1000)/10000.0f * 0.6f);
    position.y = 100 + rand() % 400;
    
    int hue = 220 + rand() % 40;
    float sat = 0.35f + (rand() % 15)/100.0f;
    float val = 0.45f + (rand() % 15)/100.0f;
    float cmax = val * sat;
    float hp = hue / 60.0f;
    float x = cmax * (1 - std::fabs(std::fmod(hp, 2) - 1));
    Uint8 r, green, b;
    if (hp < 1) { r = static_cast<Uint8>(cmax*255); green = static_cast<Uint8>(x*255); b = 0; }
    else if (hp < 2) { r = static_cast<Uint8>(x*255); green = static_cast<Uint8>(cmax*255); b = 0; }
    else if (hp < 3) { r = 0; green = static_cast<Uint8>(cmax*255); b = static_cast<Uint8>(x*255); }
    else if (hp < 4) { r = 0; green = static_cast<Uint8>(x*255); b = static_cast<Uint8>(cmax*255); }
    else if (hp < 5) { r = static_cast<Uint8>(x*255); green = 0; b = static_cast<Uint8>(cmax*255); }
    else { r = static_cast<Uint8>(cmax*255); green = 0; b = static_cast<Uint8>(x*255); }
    color = (r << 16) | (green << 8) | b | 0x88;
}

void PowerUp::spawn(Game* g) {
    game = g;
    active = true;
    type = rand() % POWERUP_TYPES;
    position.x = rand() % WINDOW_W;
    position.y = rand() % WINDOW_H;
    velocity = Vector2(0.5f - static_cast<float>(rand()) / RAND_MAX, 0.5f - static_cast<float>(rand()) / RAND_MAX);
    
    if (type == 0) color = 0x00FF00FF;  // Green for fuel
    else if (type == 1) color = 0x0000FFFF;  // Blue for heat
    else color = 0xFFFF00FF;  // Yellow for score
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

void Ship::update(const Uint8* keys, float danger_level) {
    thrusting = false;
    damage_this_frame = 0.0f;
    int left = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
    int right = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
    int thrust = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
    int shoot = keys[SDL_SCANCODE_S];
    static bool prev_tractor = false;
    bool tractor = keys[SDL_SCANCODE_SPACE];
    
    if (tractor && !prev_tractor) tractor_active = true;
    if (tractor) {
        tractor_charge += 0.25f;
        if (!combo_boost_active) fuel -= 0.12f;
    } else {
        tractor_active = false;
        tractor_charge = std::fmax(0.0f, tractor_charge - 0.4f);
    }
    prev_tractor = tractor;
    
    if (combo >= COMBO_BOOST_THRESHOLD && !combo_boost_active) {
        combo_boost_active = true;
        combo_boost_timer = COMBO_BOOST_DURATION;
    }
    if (combo_boost_active) {
        combo_boost_timer--;
        if (combo_boost_timer <= 0) {
            combo_boost_active = false;
        }
    }
    
    if (left) angle -= SHIP_ROT_SPEED;
    if (right) angle += SHIP_ROT_SPEED;
    
    float effective_thrust = SHIP_THRUST;
    if (is_critical_overheat()) effective_thrust *= OVERHEAT_THRUST_PENALTY;
    
    if (thrust && fuel > 5.0f) {
        velocity = velocity + Vector2(std::cos(angle), std::sin(angle)) * effective_thrust;
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
        overheat_damage_accumulator += OVERHEAT_DAMAGE_PER_SEC / 60.0f;
        if (overheat_damage_accumulator >= 1.0f) {
            int damage = static_cast<int>(overheat_damage_accumulator);
            lives -= damage;
            overheat_damage_accumulator -= damage;
            damage_this_frame = static_cast<float>(damage);
            if (lives <= 0) {
                std::cout << "Game Over! (Overheated to death) Final Score: " << score << std::endl;
                game->init();
            }
        }
    } else {
        velocity = velocity * 0.985f;
        overheat_damage_accumulator = std::fmax(0.0f, overheat_damage_accumulator - 0.4f);
    }
    
    fuel = std::fmin(1000.0f, fuel + 0.35f);
    
    // New for episode 6: Power-up update
    if (active_powerup >= 0) {
        powerup_timer--;
        if (powerup_timer <= 0) {
            active_powerup = -1;
            score_multiplier = 1.0f;
        } else if (active_powerup == 0) {
            fuel = std::fmin(1000.0f, fuel + 1.0f);  // Extra fuel regen
        } else if (active_powerup == 1) {
            heat = std::fmax(0.0f, heat - 1.5f);  // Faster heat decay
        } else if (active_powerup == 2) {
            score_multiplier = 1.5f;
        }
    }
}

void GasCloud::update(const Ship& ship) {
    float current_range = ship.combo_boost_active ? TRACTOR_RANGE * 1.6f : TRACTOR_RANGE;
    float current_pull = ship.combo_boost_active ? 1.5f : 1.0f;
    
    if (ship.tractor_active && distance(position, ship.position) < current_range) {
        Vector2 delta = ship.position - position;
        float dist = delta.magnitude();
        if (dist > 0) {
            float pull = pull_strength * std::fmin(ship.tractor_charge * 0.02f, current_pull);
            velocity = velocity + delta.normalize() * pull;
            game->tractor_beam_effect(ship.position, position);
        }
    }
    
    position = position + velocity;
    phase += 0.08f;
    velocity = velocity * 0.97f;
}

void NebulaCreature::update(const Ship& ship) {
    hunt_phase += 0.04f;
    wiggle += 0.09f;
    patrol_phase += 0.025f;
    
    float dist_to_ship = distance(position, ship.position);
    
    if (game->frame % 200 == 0) {
        if (dist_to_ship > 600.0f) {
            float dir_to_ship = std::atan2(ship.position.y - position.y, ship.position.x - position.x);
            float offset = (rand() % 100 - 50) / 100.0f * M_PI / 2;
            float target_dir = dir_to_ship + offset;
            float target_dist = 300 + rand() % 400;
            target = position + Vector2(std::cos(target_dir), std::sin(target_dir)) * target_dist;
        } else {
            float random_dir = static_cast<float>(rand()) * 2 * M_PI / RAND_MAX;
            float target_dist = 200 + rand() % 300;
            target = position + Vector2(std::cos(random_dir), std::sin(random_dir)) * target_dist;
        }
    }
    
    Vector2 delta = ship.position - position;
    if (std::fabs(delta.x) > WINDOW_W / 2) delta.x -= (delta.x > 0 ? WINDOW_W : -WINDOW_W);
    if (std::fabs(delta.y) > WINDOW_H / 2) delta.y -= (delta.y > 0 ? WINDOW_H : -WINDOW_H);
    float dir = std::atan2(delta.y, delta.x);
    
    if (type == 0) {
        float accel = (dist_to_ship < 420) ? 0.028f : 0.03f;
        velocity = velocity + Vector2(std::cos(dir), std::sin(dir)) * accel;
    } else if (type == 1) {
        float accel = (dist_to_ship < 500) ? 0.045f : 0.035f;
        velocity = velocity + Vector2(std::cos(dir), std::sin(dir)) * accel + Vector2(std::sin(wiggle), std::cos(wiggle)) * (dist_to_ship < 500 ? 0.06f : 0.03f);
    } else {
        if (dist_to_ship < 380) {
            float offset = (dist_to_ship < 180) ? -M_PI/2 : M_PI/2;
            dir += offset + std::sin(wiggle)*0.3f;
            velocity = velocity + Vector2(std::cos(dir), std::sin(dir)) * 0.036f;
        } else {
            velocity = velocity + Vector2(std::cos(dir), std::sin(dir)) * 0.03f;
        }
    }
    
    angle = std::atan2(velocity.y, velocity.x);
    position = position + velocity;
    velocity = velocity * 0.975f;
}

void Nebula::update() {
    swirl += 0.003f;
    pulse += 0.012f;
}

void Particle::update() {
    position = position + velocity;
    velocity.y += 0.06f * ((color >> 24) / 255.0f);
    velocity.x *= 0.98f;
    life -= 1.2f;
}

void PowerUp::update(Ship& ship) {
    position = position + velocity;
    
    if (distance(position, ship.position) < size + 20.0f) {
        ship.active_powerup = type;
        ship.powerup_timer = POWERUP_DURATION;
        active = false;
        game->harvest_effect(position, 10);
    }
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
    
    if (tractor_active) {
        float pulse = std::sin(game->frame * 0.3f) * 0.4f + 0.6f;
        Uint8 beam_a = static_cast<Uint8>(180 + 75 * pulse);
        SDL_SetRenderDrawColor(renderer, 120, 240, 255, beam_a);
        for (int rad = 0; rad < 16; rad += 3) {
            SDL_RenderDrawLine(renderer, static_cast<int>(position.x - rad*1.4f), static_cast<int>(position.y), 
                                     static_cast<int>(position.x + rad*1.4f), static_cast<int>(position.y));
        }
    }
    
    if (combo > 0) {
        float pulse = std::sin(game->frame * 0.25f) * 0.5f + 0.5f;
        Uint8 aura_a = static_cast<Uint8>(140 + 115 * pulse);
        SDL_SetRenderDrawColor(renderer, 140, 255, 220, aura_a);
        for (int rad = 0; rad < 14; rad += 2) {
            SDL_RenderDrawLine(renderer, static_cast<int>(position.x - rad), static_cast<int>(position.y - rad), 
                                     static_cast<int>(position.x + rad), static_cast<int>(position.y - rad));
            SDL_RenderDrawLine(renderer, static_cast<int>(position.x - rad), static_cast<int>(position.y + rad), 
                                     static_cast<int>(position.x + rad), static_cast<int>(position.y + rad));
        }
    }

    if (is_overheat_warning()) {
        Uint8 glow_a = static_cast<Uint8>(80 + 120 * std::sin(game->frame * 0.45f));
        SDL_SetRenderDrawColor(renderer, 255, 140, 40, glow_a);
        for (int rad = 0; rad < 22; rad += 4) {
            SDL_RenderDrawLine(renderer, static_cast<int>(position.x - rad*1.6f), static_cast<int>(position.y),
                                     static_cast<int>(position.x + rad*1.6f), static_cast<int>(position.y));
        }
    }

    // New for episode 6: Render power-up aura
    if (active_powerup >= 0) {
        Uint32 aura_color = (active_powerup == 0) ? 0x00FF00AA : (active_powerup == 1) ? 0x0000FFAA : 0xFFFF00AA;
        Uint8 a = static_cast<Uint8>(100 + 155 * std::sin(game->frame * 0.35f));
        SDL_SetRenderDrawColor(renderer, (aura_color >> 16) & 255, (aura_color >> 8) & 255, aura_color & 255, a);
        for (int rad = 0; rad < 18; rad += 3) {
            SDL_RenderDrawLine(renderer, static_cast<int>(position.x - rad*1.2f), static_cast<int>(position.y - rad*1.2f), 
                                     static_cast<int>(position.x + rad*1.2f), static_cast<int>(position.y - rad*1.2f));
        }
    }
}

void GasCloud::render(SDL_Renderer* renderer) const {
    float pulse = 0.8f + 0.2f * std::sin(phase + game->frame * 0.14f);
    int rad = static_cast<int>(size * pulse * density);
    
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 50);
    for (int dy = -static_cast<int>(rad*1.3f); dy <= static_cast<int>(rad*1.3f); dy += 7) {
        int w = static_cast<int>(std::sqrt(rad*rad*1.7f - dy*dy) * 0.35f);
        if (w > 0)
            SDL_RenderDrawLine(renderer, static_cast<int>(position.x - w), static_cast<int>(position.y + dy), static_cast<int>(position.x + w), static_cast<int>(position.y + dy));
    }
    
    SDL_SetRenderDrawColor(renderer, (color>>16)&255, (color>>8)&255, color&255, 255);
    for (int dy = -rad; dy <= rad; dy += 3) {
        int w = static_cast<int>(std::sqrt(rad*rad - dy*dy) * density * 0.9f);
        SDL_RenderDrawLine(renderer, static_cast<int>(position.x - w), static_cast<int>(position.y + dy), static_cast<int>(position.x + w), static_cast<int>(position.y + dy));
    }
    
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
    for (int r = 0; r < 8; r++) {
        SDL_RenderDrawLine(renderer, static_cast<int>(position.x - r), static_cast<int>(position.y), static_cast<int>(position.x + r), static_cast<int>(position.y));
        SDL_RenderDrawLine(renderer, static_cast<int>(position.x), static_cast<int>(position.y - r), static_cast<int>(position.x), static_cast<int>(position.y + r));
    }
}

void NebulaCreature::render(SDL_Renderer* renderer) const {
    float pulse = 0.85f + 0.15f * std::sin(game->frame * 0.18f + hunt_phase);
    int sz = static_cast<int>(size * pulse);
    
    SDL_SetRenderDrawColor(renderer, (color>>16)&255, (color>>8)&255, color&255, (color>>24)&255);
    for (int dy = -sz; dy <= sz; dy += 3) {
        int w = static_cast<int>(std::sqrt(sz*sz - dy*dy));
        SDL_RenderDrawLine(renderer, static_cast<int>(position.x - w), static_cast<int>(position.y + dy), static_cast<int>(position.x + w), static_cast<int>(position.y + dy));
    }
    
    if (type == 0) {
        SDL_SetRenderDrawColor(renderer, 200, 220, 255, 180);
        for (int i = 0; i < 5; i++) {
            float ang = angle + i * M_PI / 2.5f + std::sin(wiggle + i) * 0.3f;
            Vector2 end = position + Vector2(std::cos(ang), std::sin(ang)) * (sz + 10);
            thick_line(renderer, static_cast<int>(position.x), static_cast<int>(position.y), static_cast<int>(end.x), static_cast<int>(end.y), 2);
        }
    } else if (type == 1) {
        SDL_SetRenderDrawColor(renderer, 255, 120, 120, 220);
        for (int i = 0; i < 6; i++) {
            float ang = angle + i * M_PI / 3 + std::sin(wiggle + i) * 0.4f;
            Vector2 end = position + Vector2(std::cos(ang), std::sin(ang)) * (sz + 16);
            thick_line(renderer, static_cast<int>(position.x), static_cast<int>(position.y), static_cast<int>(end.x), static_cast<int>(end.y), 4);
        }
    } else {
        SDL_SetRenderDrawColor(renderer, 180, 100, 220, 200);
        for (int i = 0; i < 8; i++) {
            float ang = angle + i * M_PI / 4 + std::sin(wiggle * 0.8f + i) * 0.6f;
            Vector2 end = position + Vector2(std::cos(ang), std::sin(ang)) * (sz + 18);
            thick_line(renderer, static_cast<int>(position.x), static_cast<int>(position.y), static_cast<int>(end.x), static_cast<int>(end.y), 3);
        }
    }
    
    float dist_to_ship = distance(position, game->ship.position);
    if (dist_to_ship < CREATURE_DANGER_DIST) {
        Uint8 glow = static_cast<Uint8>(255 * (1.0f - dist_to_ship / CREATURE_DANGER_DIST));
        SDL_SetRenderDrawColor(renderer, 255, 80, 80, glow);
        for (int r = 0; r < 20; r += 4) {
            SDL_RenderDrawLine(renderer, static_cast<int>(position.x - r), static_cast<int>(position.y), static_cast<int>(position.x + r), static_cast<int>(position.y));
        }
    }
}

void Nebula::render(SDL_Renderer* renderer) const {
    float nx = position.x - game->scrollX * 0.08f;
    if (nx < -400 || nx > WINDOW_W + 400) return;
    
    float brightness_pulse = 0.9f + 0.1f * std::sin(pulse);
    Uint8 alpha_base = static_cast<Uint8>(0x88 * brightness_pulse);
    
    int r = static_cast<int>(radius);
    SDL_SetRenderDrawColor(renderer, (color>>16)&255, (color>>8)&255, color&255, alpha_base);
    for (int dy = -r; dy <= r; dy += 5) {
        float swirl_off = std::sin((dy * 0.025f + swirl * 3) * 1.7f) * density * 35;
        int hw = static_cast<int>(std::sqrt(r*r - dy*dy) + swirl_off);
        SDL_RenderDrawLine(renderer, static_cast<int>(nx - hw), static_cast<int>(position.y + dy), static_cast<int>(nx + hw), static_cast<int>(position.y + dy));
    }
    
    SDL_SetRenderDrawColor(renderer, 180, 190, 255, static_cast<Uint8>(70 * brightness_pulse));
    for (int dy = -r/4; dy <= r/4; dy += 10) {
        int hw = static_cast<int>(std::sqrt((r/4)*(r/4) - dy*dy) * 1.2f);
        SDL_RenderDrawLine(renderer, static_cast<int>(nx - hw), static_cast<int>(position.y + dy), static_cast<int>(nx + hw), static_cast<int>(position.y + dy));
    }
}

void Particle::render(SDL_Renderer* renderer) const {
    int alpha = static_cast<int>(255 * (life / 60.0f));
    if (alpha < 25) return;
    SDL_SetRenderDrawColor(renderer, (color>>16)&255, (color>>8)&255, color&255, alpha);
    int px = static_cast<int>(position.x), py = static_cast<int>(position.y);
    SDL_RenderDrawPoint(renderer, px, py);
    if (alpha > 100) {
        SDL_RenderDrawPoint(renderer, px+1, py);
        SDL_RenderDrawPoint(renderer, px, py+1);
    }
}

void PowerUp::render(SDL_Renderer* renderer) const {
    float pulse = 1.0f + 0.2f * std::sin(game->frame * 0.2f);
    int rad = static_cast<int>(size * pulse);
    
    SDL_SetRenderDrawColor(renderer, (color>>16)&255, (color>>8)&255, color&255, 255);
    for (int dy = -rad; dy <= rad; dy += 2) {
        int w = static_cast<int>(std::sqrt(rad*rad - dy*dy));
        SDL_RenderDrawLine(renderer, static_cast<int>(position.x - w), static_cast<int>(position.y + dy), static_cast<int>(position.x + w), static_cast<int>(position.y + dy));
    }
}

void Game::init() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    ship = Ship(this);
    clouds.clear();
    creatures.clear();
    nebulas.clear();
    particles.clear();
    projectiles.clear();
    powerups.clear();
    frame = 0;
    scrollX = 0.0f;
    danger_level = 0.0f;
    wave = 1;
    clouds_collected_this_wave = 0;
    clouds_needed_for_next_wave = CLOUDS_PER_WAVE_BASE;
    wave_flash_timer = 0;
    current_wave_display_timer = 0;
    
    for (int i = 0; i < 35; i++) {
        GasCloud c(this);
        c.spawn(this);
        clouds.push_back(c);
    }
    nebulas.resize(MAX_NEBULAE, Nebula(this));
    for (int i = 0; i < MAX_NEBULAE; i++) nebulas[i].spawn(i);
    
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].base_x = (rand() % 90000) - 45000;
        stars[i].base_y = rand() % WINDOW_H;
        stars[i].brightness = 110 + rand() % 145;
        stars[i].phase = rand() % 256;
        stars[i].size = 1 + (rand() % 3);
    }
    for (int i = 0; i < NUM_DEBRIS; i++) {
        debris[i].base_x = (rand() % 120000) - 60000;
        debris[i].base_y = rand() % WINDOW_H;
        debris[i].vx = 0.25f + (rand() % 80)/100.0f;
        debris[i].size = 1 + rand() % 3;
    }
    
    for (int i = 0; i < NUM_PLANETS; i++) {
        planets[i].base_x = 800 + (rand() % 1200);
        planets[i].base_y = 100 + rand() % 400;
        planets[i].radius = 28 + rand() % 28;
        planets[i].color = (rand() % 128 + 64) << 16 | (rand() % 128 + 64) << 8 | (rand() % 255);
        planets[i].spin = 0;
    }
    
    sun.base_x = WINDOW_W * 0.7f;
    sun.base_y = WINDOW_H * 0.25f;
    sun.radius = 110;
    sun.pulse_phase = 0;
    
    for (int i = 0; i < 8; i++) {
        NebulaCreature n(this);
        n.spawn(this, ship);
        creatures.push_back(n);
    }
}

void Game::update() {
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    frame++;
    scrollX += 0.9f + danger_level * 0.12f;
    sun.pulse_phase += 0.018f;
    danger_level = std::fmin(1.3f, danger_level + 0.00008f * clouds.size());
    
    for (auto& p : planets) {
        p.base_x -= 0.11f + danger_level * 0.007f;
        if (p.base_x < -400) {
            p.base_x = WINDOW_W + 600 + rand() % 400;
            p.base_y = 120 + rand() % 350;
        }
    }
    
    ship.update(keys, danger_level);
    if (ship.thrusting) thrust_flame();
    if (ship.is_critical_overheat()) critical_overheat_effect();
    if (ship.damage_this_frame > 0) danger_trail(ship.position);
    wrap(ship.position);
    trail_emit();
    
    for (auto it = clouds.begin(); it != clouds.end(); ) {
        if (!it->active) {
            it = clouds.erase(it);
            continue;
        }
        it->update(ship);
        wrap(it->position);
        if (distance(it->position, ship.position) < HARVEST_RANGE) {
            int points = static_cast<int>(it->value * (1 + ship.combo * 0.2f) * ship.score_multiplier);
            ship.score += points;
            harvest_effect(it->position, it->value);
            it = clouds.erase(it);
            ship.combo++;
            combo_timer = 300;
            clouds_collected_this_wave++;
            
            if (clouds_collected_this_wave >= clouds_needed_for_next_wave) {
                wave++;
                clouds_collected_this_wave = 0;
                clouds_needed_for_next_wave = CLOUDS_PER_WAVE_BASE + wave * 18;
                wave_flash_timer = 180;
                current_wave_display_timer = 180;
                
                for (int j = 0; j < WAVE_CREATURE_BONUS + wave / 2; j++) {
                    NebulaCreature n(this);
                    n.spawn(this, ship);
                    creatures.push_back(n);
                }
                
                danger_level += 0.2f;
            }
            continue;
        }
        ++it;
    }
    
    while (clouds.size() < static_cast<size_t>(40 + static_cast<int>(danger_level * 35))) {
        GasCloud c(this);
        c.spawn(this);
        clouds.push_back(c);
    }
    
    for (auto it = creatures.begin(); it != creatures.end(); ) {
        if (!it->active) {
            it = creatures.erase(it);
            continue;
        }
        it->update(ship);
        wrap(it->position);
        if (distance(it->position, ship.position) < it->size + 28) {
            ship.lives--;
            ship.fuel *= 0.4f;
            ship.heat = OVERHEAT_MAX * 0.92f;
            danger_trail(ship.position);
            it = creatures.erase(it);
            ship.combo = 0;
            if (ship.lives <= 0) {
                std::cout << "Game Over! Final Score: " << ship.score << std::endl;
                init();
            }
            continue;
        }
        ++it;
    }
    
    if (frame % 520 == 0 && creatures.size() < static_cast<size_t>(14 + static_cast<int>(danger_level * 12))) {
        NebulaCreature n(this);
        n.spawn(this, ship);
        creatures.push_back(n);
    }
    
    for (auto it = particles.begin(); it != particles.end(); ) {
        it->update();
        if (it->life <= 0) {
            it = particles.erase(it);
        } else {
            wrap(it->position);
            ++it;
        }
    }
    
    for (auto it = projectiles.begin(); it != projectiles.end(); ) {
        it->update();
        if (it->life <= 0) {
            it = projectiles.erase(it);
            continue;
        }
        bool hit = false;
        for (auto& cr : creatures) {
            if (cr.active && distance(it->position, cr.position) < cr.size) {
                cr.health -= PROJECTILE_DAMAGE;
                if (cr.health <= 0) {
                    ship.score += 50 * ship.score_multiplier;  // Points for destroying creature
                    creature_destroy_effect(cr.position);
                    cr.active = false;
                }
                hit = true;
                break;
            }
        }
        if (hit) {
            it = projectiles.erase(it);
        } else {
            ++it;
        }
    }
    
    if (combo_timer > 0) combo_timer--;
    else ship.combo = 0;
    
    // New for episode 6: Power-up spawning and update
    if (wave >= 2 && static_cast<float>(rand()) / RAND_MAX < POWERUP_SPAWN_CHANCE && powerups.size() < MAX_POWERUPS) {
        PowerUp pu(this);
        pu.spawn(this);
        powerups.push_back(pu);
    }
    
    for (auto it = powerups.begin(); it != powerups.end(); ) {
        it->update(ship);
        wrap(it->position);
        if (!it->active) {
            it = powerups.erase(it);
        } else {
            ++it;
        }
    }
    
    for (auto& n : nebulas) n.update();
    if (wave_flash_timer > 0) wave_flash_timer--;
    if (current_wave_display_timer > 0) current_wave_display_timer--;
}

void Game::render() {
    SDL_SetRenderDrawColor(renderer, 3, 3, 12, 255);
    SDL_RenderClear(renderer);
    
    for (const auto& s : stars) {
        float px = s.base_x - scrollX * 0.18f;
        px = std::fmod(px + 120000, 240000) - 120000;
        if (px < -60 || px > WINDOW_W + 60) continue;
        float twinkle = 0.65f + 0.35f * std::sin(frame * 0.09f + s.phase);
        int br = static_cast<int>(s.brightness * twinkle);
        SDL_SetRenderDrawColor(renderer, br, br, br + 40, 255);
        int sx = static_cast<int>(px), sy = static_cast<int>(s.base_y);
        for (int sz = -s.size; sz <= s.size; sz++) {
            SDL_RenderDrawPoint(renderer, sx + sz, sy);
            SDL_RenderDrawPoint(renderer, sx, sy + sz);
        }
    }
    
    for (size_t i = 0; i < debris.size(); i++) {
        const auto& d = debris[i];
        float px = d.base_x - scrollX * 0.45f;
        px = std::fmod(px + 180000, 360000) - 180000;
        if (px < -40 || px > WINDOW_W + 40) continue;
        int g = 100 + static_cast<int>(d.vx * 180 + std::sin(frame * 0.06f + i * 0.1f) * 35);
        SDL_SetRenderDrawColor(renderer, g, g + 20, 180, 200);
        for(int sz = 0; sz < d.size * 2 + 1; sz++) {
            SDL_RenderDrawPoint(renderer, static_cast<int>(px) + sz, static_cast<int>(d.base_y));
        }
    }
    
    float sun_pulse = 1.0f + 0.12f * std::sin(sun.pulse_phase);
    float sun_r = sun.radius * sun_pulse;
    SDL_SetRenderDrawColor(renderer, 255, 255, 180, 90);
    for (int r = static_cast<int>(sun_r) + 55; r > static_cast<int>(sun_r) + 18; r -= 12) {
        for (int dy = -r; dy <= r; dy += 9) {
            int hw = static_cast<int>(std::sqrt(r*r - dy*dy));
            SDL_RenderDrawLine(renderer, static_cast<int>(sun.base_x) - hw, static_cast<int>(sun.base_y + dy),
                                     static_cast<int>(sun.base_x) + hw, static_cast<int>(sun.base_y + dy));
        }
    }
    SDL_SetRenderDrawColor(renderer, 255, 240, 140, 255);
    for (int dy = -static_cast<int>(sun_r); dy <= static_cast<int>(sun_r); dy += 5) {
        int hw = static_cast<int>(std::sqrt(sun_r*sun_r - dy*dy));
        SDL_RenderDrawLine(renderer, static_cast<int>(sun.base_x) - hw, static_cast<int>(sun.base_y + dy),
                                 static_cast<int>(sun.base_x) + hw, static_cast<int>(sun.base_y + dy));
    }
    
    for (auto& n : nebulas) if (n.active) n.render(renderer);
    
    for (auto& p : planets) {
        float px = p.base_x - scrollX * 0.12f;
        if (px < -350 || px > WINDOW_W + 350) continue;
        p.spin += 0.0018f;
        int rad = static_cast<int>(p.radius);
        for (int dy = -rad; dy <= rad; dy += 4) {
            int hw = static_cast<int>(std::sqrt(rad*rad - dy*dy));
            float swirl = std::sin(dy * 0.035f + p.spin * 5);
            Uint8 alpha = 140 + static_cast<int>(95 * swirl);
            SDL_SetRenderDrawColor(renderer, (p.color>>16)&255, (p.color>>8)&255, p.color&255, alpha);
            SDL_RenderDrawLine(renderer, static_cast<int>(px - hw), static_cast<int>(p.base_y + dy), static_cast<int>(px + hw), static_cast<int>(p.base_y + dy));
        }
    }
    
    for (const auto& c : clouds) if (c.active) c.render(renderer);
    for (const auto& cr : creatures) if (cr.active) cr.render(renderer);
    for (const auto& pu : powerups) if (pu.active) pu.render(renderer);
    
    for (const auto& part : particles) part.render(renderer);
    for (const auto& proj : projectiles) proj.render(renderer);
    
    ship.render(renderer);
    
    // Score display
    int display_score = ship.score % 1000000;
    std::string score_str = std::to_string(display_score);
    while (score_str.length() < 6) score_str = "0" + score_str;
    
    int digit_w = 32;
    int digit_spacing = digit_w + 10;
    int start_x = WINDOW_W - 60;
    int start_y = 10;
    
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int i = 0; i < 6; i++) {
        int digit = score_str[5 - i] - '0';
        int bx = start_x - i * digit_spacing;
        draw_7segment_digit(renderer, bx, start_y, digit);
    }
    
    // Lives
    for (int i = 0; i < ship.lives; i++) {
        int lx = 40 + i * 45;
        SDL_SetRenderDrawColor(renderer, 180, 255, 180, 255);
        thick_line(renderer, lx, 20, lx + 30, 20, 5);
        thick_line(renderer, lx + 8, 30, lx + 22, 30, 5);
    }
    
    // Fuel bar
    int fuel_fill = static_cast<int>((ship.fuel / 1000.0f) * 220);
    SDL_SetRenderDrawColor(renderer, 40, 60, 80, 220);
    SDL_Rect fuel_bg_rect = {30, 70, 240, 18};
    SDL_RenderFillRect(renderer, &fuel_bg_rect);
    SDL_SetRenderDrawColor(renderer, 80, 200, 255, 255);
    SDL_Rect fuel_fill_rect = {33, 73, fuel_fill, 12};
    SDL_RenderFillRect(renderer, &fuel_fill_rect);
    
    // Heat bar
    int heat_fill = static_cast<int>((ship.heat / OVERHEAT_MAX) * 220);
    SDL_SetRenderDrawColor(renderer, 100, 40, 40, 220);
    SDL_Rect heat_bg_rect = {30, 95, 240, 14};
    SDL_RenderFillRect(renderer, &heat_bg_rect);
    if (ship.is_critical_overheat()) {
        SDL_SetRenderDrawColor(renderer, 255, 60, 40, 255);
    } else if (ship.is_overheat_warning()) {
        SDL_SetRenderDrawColor(renderer, 255, 140, 40, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 100, 80, 255);
    }
    SDL_Rect heat_fill_rect = {33, 98, heat_fill, 8};
    SDL_RenderFillRect(renderer, &heat_fill_rect);
    
    // Combo meter
    if (ship.combo > 0) {
        int combo_w = ship.combo * 10;
        int max_w = 200;
        if (combo_w > max_w) combo_w = max_w;
        
        float pulse = std::sin(frame * 0.35f) * 0.5f + 0.5f;
        Uint8 r = 255;
        Uint8 g = 220 + static_cast<Uint8>(35 * pulse);
        Uint8 b = 100 + static_cast<Uint8>(100 * pulse);
        Uint8 a = static_cast<Uint8>(200 + 55 * pulse);
        
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
        SDL_Rect combo_fill_rect = {WINDOW_W/2 - combo_w/2, 20, combo_w, 24};
        SDL_RenderFillRect(renderer, &combo_fill_rect);
        
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(100 + 155 * pulse));
        SDL_Rect combo_border_rect = {WINDOW_W/2 - combo_w/2 - 3, 17, combo_w + 6, 30};
        SDL_RenderDrawRect(renderer, &combo_border_rect);
        
        if (ship.combo_boost_active) {
            SDL_SetRenderDrawColor(renderer, 255, 220, 50, 255);
            for (int off = 0; off < 8; off += 2) {
                SDL_Rect glow_rect = {WINDOW_W/2 - combo_w/2 - 8 - off, 12 - off, combo_w + 16 + off*2, 40 + off*2};
                SDL_RenderDrawRect(renderer, &glow_rect);
            }
        }
    }
    
    // Wave indicator
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    int tx = WINDOW_W - 100;
    int ty = WINDOW_H - 50;
    int wave_digit_x = tx;
    draw_7segment_digit(renderer, wave_digit_x + 35, ty - 8, wave % 10);
    if (wave >= 10) draw_7segment_digit(renderer, wave_digit_x, ty - 8, (wave / 10) % 10);
    
    if (current_wave_display_timer > 0) {
        Uint8 flash_alpha = static_cast<Uint8>(180 + 75 * std::sin(frame * 0.5f));
        SDL_SetRenderDrawColor(renderer, 255, 255, 100, flash_alpha);
        draw_7segment_digit(renderer, wave_digit_x + 35, ty - 8, wave % 10);
        if (wave >= 10) draw_7segment_digit(renderer, wave_digit_x, ty - 8, (wave / 10) % 10);
    }
    
    SDL_RenderPresent(renderer);
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    Game game;
    game.window = SDL_CreateWindow("Nebula Harvester - Episode 6", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, 0);
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