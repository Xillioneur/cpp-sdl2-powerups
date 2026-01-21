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

#define MAX_CLOUDS 120
#define MAX_CREATURES 38
#define MAX_PROJECTILES 50

#define SHIP_ROT_SPEED 0.10f
#define SHIP_THRUST 0.12f
#define HARVEST_RANGE 65.0f
#define CREATURE_DANGER_DIST 140.0f
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

#define TRACTOR_RANGE 220.0f

#define CLOUDS_PER_WAVE_BASE 45

#define PROJECTILE_SPEED 8.0f
#define PROJECTILE_LIFE 120
#define SHOOT_COOLDOWN 10
#define PROJECTILE_DAMAGE 1

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
    int lives = 3;
    bool tractor_active = false;
    float tractor_charge = 0.0f;
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

class GasCloud : public Entity {
public:
    float size;
    float density;
    float phase;
    float pull_strength;
    int value;
    Uint32 color;

    GasCloud (Game* g) : Entity(g) {}
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
    std::vector<GasCloud> clouds;
    std::vector<NebulaCreature> creatures;
    std::vector<Projectile> projectiles;

    int frame = 0;
    float danger_level = 0.0f;
    
    int wave = 1;
    int clouds_collected_this_wave = 0;
    int clouds_needed_for_next_wave = CLOUDS_PER_WAVE_BASE;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;


    Game() : ship(this) {
        clouds.reserve(MAX_CLOUDS);
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
    health = type + 1;

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
    static bool prev_tractor = false;
    bool tractor = keys[SDL_SCANCODE_SPACE];

    if (tractor && !prev_tractor) tractor_active = true;
    if (tractor) {
        tractor_charge += 0.25f;
        // TODO: Combo boost
    } else {
        tractor_active = false;
        tractor_charge = std::fmax(0.0f, tractor_charge - 0.4f);
    }
    prev_tractor = tractor;


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

void GasCloud::update(const Ship& ship) {
    // TODO: Combo boost
    if (ship.tractor_active && distance(position, ship.position) < TRACTOR_RANGE) {
       Vector2 delta = ship.position - position;
       float dist = delta.magnitude();
        if (dist > 0) {
            float pull = pull_strength * std::fmin(ship.tractor_charge * 0.02f, 1.0f);
            velocity = velocity + delta.normalize();
            // TODO: Tractor beam
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
            SDL_RenderDrawLine(renderer, static_cast<int>(position.x - r), static_cast<int>(position.y), static_cast<int>(position.x + r), static_cast<int>(position.y));        }
    }
}

void Game::init() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    ship = Ship(this);
    clouds.clear();
    creatures.clear();
    projectiles.clear();
    frame = 0;
    wave = 1;
    clouds_collected_this_wave = 0;
    clouds_needed_for_next_wave = CLOUDS_PER_WAVE_BASE;

    for (int i = 0; i < 35; i++) {
        GasCloud c(this);
        c.spawn(this);
        clouds.push_back(c);
    }

    for (int i = 0; i < 8; i++) {
        NebulaCreature n(this);
        n.spawn(this, ship);
        creatures.push_back(n);
    }
}

void Game::render() {
    SDL_SetRenderDrawColor(renderer, 3, 3, 12, 255);
    SDL_RenderClear(renderer);

    for (const auto& c : clouds) if (c.active) c.render(renderer);
    for (const auto& cr : creatures) if (cr.active) cr.render(renderer);   

    for (const auto& proj : projectiles) proj.render(renderer);
    ship.render(renderer);

    SDL_RenderPresent(renderer);
}

void Game::update() {
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    frame++;
    danger_level = std::fmin(1.3f, danger_level + 0.00008f * clouds.size());

    ship.update(keys);
    wrap(ship.position);
    // TODO: Code thrust_fame()

    for (auto it = clouds.begin(); it != clouds.end();) { 
        if (!it->active) {
            it = clouds.erase(it);
            continue;
        }
        it->update(ship);
        wrap(it->position);
        if (distance(it->position, ship.position) < HARVEST_RANGE) {
            // TODO: Points
            // TODO: Harvest effect
            it = clouds.erase(it);
            clouds_collected_this_wave++;

            if (clouds_collected_this_wave >= clouds_needed_for_next_wave) {
                wave++;
                clouds_collected_this_wave = 0;
                clouds_needed_for_next_wave = CLOUDS_PER_WAVE_BASE + wave * 18;

                // TODO: Spawn more creatures

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
            // TODO: Danger trail
            it = creatures.erase(it);
            // TODO: Combo
            if (ship.lives <= 0) {
                std::cout << "Game Over!" << std::endl; // TOOD: Score
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