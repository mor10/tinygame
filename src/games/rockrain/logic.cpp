#include "logic.h"

#include "tg_random.h"

namespace rockrain::logic {

namespace {

float clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void countDown(float& timer, float dt) {
    timer = timer > dt ? timer - dt : 0.0f;
}

}  // namespace

bool overlaps(const Rect& a, const Rect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

void Game::newGame(std::uint32_t seed, float robot_target) {
    state_ = State{};
    state_.rng = seed == 0 ? 1 : seed;
    state_.lives = config::STARTING_LIVES;
    state_.ammo = config::STARTING_AMMO;
    state_.spawn_timer = config::FIRST_BRICK_DELAY;
    state_.capsule_timer = config::FIRST_CAPSULE_DELAY;
    state_.boss_timer = randomRange(config::FIRST_BOSS_DELAY_MIN, config::FIRST_BOSS_DELAY_MAX);
    startWave(config::WAVE_FIRST_ROW_DELAY);
    accumulator_ = 0.0f;
    setRobot(robot_target, true);
}

bool Game::canShoot() const {
    return !state_.game_over && (state_.rapid_seconds_left > 0.0f || state_.ammo > 0);
}

bool Game::fire() {
    if (!canShoot() || state_.fire_cooldown > 0.0f) {
        return false;
    }
    for (Bolt& b : state_.bolts) {
        if (b.active) {
            continue;
        }
        b.active = true;
        b.rect = {state_.robot_x + (config::ROBOT_WIDTH - config::BOLT_WIDTH) / 2.0f,
                  config::ROBOT_Y - config::BOLT_HEIGHT, config::BOLT_WIDTH, config::BOLT_HEIGHT};
        if (state_.rapid_seconds_left > 0.0f) {
            state_.fire_cooldown = config::RAPID_COOLDOWN_SECONDS;
        } else {
            state_.ammo -= 1;
            state_.fire_cooldown = config::AMMO_COOLDOWN_SECONDS;
        }
        return true;
    }
    return false;  // every bolt is still flying
}

std::uint8_t Game::update(float seconds, float robot_target) {
    accumulator_ += clamp(seconds, 0.0f, config::MAX_FRAME_SECONDS);
    std::uint8_t events = 0;
    while (accumulator_ >= config::STEP_SECONDS) {
        accumulator_ -= config::STEP_SECONDS;
        events |= step(robot_target);
        if (state_.game_over) {
            accumulator_ = 0.0f;
            break;
        }
    }
    return events;
}

std::uint8_t Game::step(float robot_target) {
    if (state_.game_over) {
        return 0;
    }
    const float dt = config::STEP_SECONDS;
    setRobot(robot_target);
    state_.elapsed += dt;
    countDown(state_.fire_cooldown, dt);
    countDown(state_.rapid_seconds_left, dt);
    countDown(state_.invulnerable_seconds_left, dt);

    state_.survive_timer += dt;
    while (state_.survive_timer >= 1.0f) {
        state_.survive_timer -= 1.0f;
        state_.score += config::SCORE_PER_SECOND;
    }

    if (state_.wave_rows_left > 0) {
        state_.wave_timer -= dt;
        if (state_.wave_timer <= 0.0f) {
            spawnWaveRow();
            state_.wave_rows_left -= 1;
            state_.wave_timer += config::WAVE_ROW_SPACING / waveSpeed();
            if (state_.wave_rows_left == 0) {
                if (state_.waves_done < UINT32_MAX) ++state_.waves_done;
                state_.spawn_timer = config::WAVE_END_DELAY;
            }
        }
    } else {
        state_.spawn_timer -= dt;
        if (state_.spawn_timer <= 0.0f) {
            spawnBrick();
            state_.spawn_timer += spawnInterval();
        }
    }
    state_.capsule_timer -= dt;
    if (state_.capsule_timer <= 0.0f) {
        spawnCapsule();
        state_.capsule_timer =
            state_.boss.active
                ? randomRange(config::BOSS_CAPSULE_INTERVAL_MIN, config::BOSS_CAPSULE_INTERVAL_MAX)
                : randomRange(config::CAPSULE_INTERVAL_MIN, config::CAPSULE_INTERVAL_MAX);
    }
    if (!state_.boss.active && state_.wave_rows_left == 0) {
        state_.boss_timer -= dt;
        if (state_.boss_timer <= 0.0f) {
            spawnBoss();
            state_.boss_timer = randomRange(config::BOSS_INTERVAL_MIN, config::BOSS_INTERVAL_MAX);
        }
    }

    std::uint8_t events = stepBolts(dt);
    events |= stepBricks(dt);
    if (state_.game_over) return events;
    events |= stepCapsules(dt);
    stepBoss(dt);
    return events;
}

float Game::fallSpeed() const {
    return clamp(config::FALL_SPEED_START + config::FALL_SPEED_PER_SECOND * state_.elapsed, 0.0f,
                 config::FALL_SPEED_MAX);
}

float Game::waveSpeed() const {
    return clamp(config::WAVE_FALL_SPEED + config::WAVE_SPEED_STEP * state_.waves_done, 0.0f,
                 config::WAVE_FALL_SPEED_MAX);
}

float Game::spawnInterval() const {
    return clamp(config::SPAWN_INTERVAL_START - config::SPAWN_INTERVAL_PER_SECOND * state_.elapsed,
                 config::SPAWN_INTERVAL_MIN, config::SPAWN_INTERVAL_START);
}

Rect Game::robotHitbox() const {
    const float inset = config::ROBOT_HITBOX_INSET;
    return {state_.robot_x + inset, config::ROBOT_Y + inset, config::ROBOT_WIDTH - 2.0f * inset,
            config::ROBOT_HEIGHT - inset};
}

void Game::setRobot(float robot_target, bool snap) {
    const float target = clamp(robot_target, 0.0f, 1.0f) * (tg::WIDTH - config::ROBOT_WIDTH);
    state_.robot_x = snap ? target : tg::glide(state_.robot_x, target, config::STEP_SECONDS);
}

void Game::spawnBrick() {
    const bool big = nextRandom() % 100 < config::BIG_BRICK_PERCENT;
    const float w = big ? config::BIG_BRICK_WIDTH : config::SMALL_BRICK_WIDTH;
    const float h = big ? config::BIG_BRICK_HEIGHT : config::SMALL_BRICK_HEIGHT;
    spawnBrickAt(w / 2.0f + random01() * (tg::WIDTH - w), config::HUD_HEIGHT - h, big);
}

void Game::spawnBrickAt(float centre_x, float top, bool big) {
    const float w = big ? config::BIG_BRICK_WIDTH : config::SMALL_BRICK_WIDTH;
    const float h = big ? config::BIG_BRICK_HEIGHT : config::SMALL_BRICK_HEIGHT;
    const float speed = fallSpeed() * (1.0f - config::FALL_SPEED_SPREAD +
                                        2.0f * config::FALL_SPEED_SPREAD * random01());
    addBrick({clamp(centre_x - w / 2.0f, 0.0f, tg::WIDTH - w), top, w, h}, big, speed);
}

bool Game::addBrick(const Rect& r, bool big, float speed) {
    for (Brick& b : state_.bricks) {
        if (b.active) {
            continue;
        }
        b.active = true;
        b.big = big;
        b.hits_left = big ? 2 : 1;
        b.rect = r;
        b.speed = speed;
        return true;
    }
    return false;
}

void Game::startWave(float first_row_delay) {
    state_.wave_rows_left = config::WAVE_ROWS;
    state_.wave_timer = first_row_delay;
}

void Game::spawnWaveRow() {
    // Pick the openings as slot ranges [start, start + width).
    std::uint8_t starts[2] = {0, 0};
    std::uint8_t widths[2] = {0, 0};
    if (nextRandom() % 100 < config::TWO_GAP_PERCENT) {
        // One opening in each half, so they never join up.
        constexpr std::uint8_t half = config::ROW_SLOTS / 2;
        constexpr std::uint8_t w = config::TWO_GAP_BRICKS;
        starts[0] = static_cast<std::uint8_t>(nextRandom() % (half - w));
        starts[1] = static_cast<std::uint8_t>(half + 1 + nextRandom() % (half - w));
        widths[0] = w;
        widths[1] = w;
    } else {
        constexpr std::uint8_t w = config::ONE_GAP_BRICKS;
        starts[0] = static_cast<std::uint8_t>(nextRandom() % (config::ROW_SLOTS - w + 1));
        widths[0] = w;
    }
    // Each run of filled slots between the openings becomes one long brick.
    const float h = config::SMALL_BRICK_HEIGHT;
    int run_start = -1;
    for (int slot = 0; slot <= config::ROW_SLOTS; ++slot) {
        bool open = slot == config::ROW_SLOTS;
        for (int g = 0; g < 2; ++g) {
            open = open || (slot >= starts[g] && slot < starts[g] + widths[g]);
        }
        if (!open && run_start < 0) {
            run_start = slot;
        } else if (open && run_start >= 0) {
            addBrick({run_start * config::SMALL_BRICK_WIDTH, config::HUD_HEIGHT - h,
                      (slot - run_start) * config::SMALL_BRICK_WIDTH, h},
                     false, waveSpeed());
            run_start = -1;
        }
    }
}

bool Game::knockOutPiece(Brick& stretch, float x) {
    const float piece = config::SMALL_BRICK_WIDTH;
    const int pieces = static_cast<int>(stretch.rect.w / piece + 0.5f);
    int index = static_cast<int>((x - stretch.rect.x) / piece);
    index = index < 0 ? 0 : (index >= pieces ? pieces - 1 : index);
    const Rect hit = {stretch.rect.x + index * piece, stretch.rect.y, piece, stretch.rect.h};
    const float right_w = (pieces - index - 1) * piece;
    if (index > 0 && right_w > 0.0f &&
        !addBrick({hit.x + piece, hit.y, right_w, hit.h}, false, stretch.speed)) {
        return false;  // A full pool must not erase the unhit right section.
    }
    breakThing(hit, false, false, config::SCORE_SMALL_BRICK);
    if (index > 0) {
        stretch.rect.w = index * piece;
    } else if (right_w > 0.0f) {
        stretch.rect = {hit.x + piece, hit.y, right_w, hit.h};
    } else {
        stretch.active = false;
    }
    return true;
}

void Game::spawnCapsule() {
    for (Capsule& c : state_.capsules) {
        if (c.active) {
            continue;
        }
        c.active = true;
        const std::uint32_t roll = nextRandom() % 100;
        c.rect = {random01() * (tg::WIDTH - config::CAPSULE_SIZE),
                  config::HUD_HEIGHT - config::CAPSULE_SIZE, config::CAPSULE_SIZE,
                  config::CAPSULE_SIZE};
        if (state_.boss.active) {
            // Boss fight: only things that help you shoot.
            c.type = roll < 50 ? PowerUp::Rapid : PowerUp::Ammo;
            return;
        }
        const bool life_due = state_.capsules_since_life >= config::LIFE_GUARANTEE_AFTER;
        c.type = life_due || roll < config::LIFE_PERCENT              ? PowerUp::Life
                 : roll < config::LIFE_PERCENT + config::RAPID_PERCENT ? PowerUp::Rapid
                                                                        : PowerUp::Ammo;
        state_.capsules_since_life =
            c.type == PowerUp::Life ? 0 : static_cast<std::uint8_t>(state_.capsules_since_life + 1);
        return;
    }
}

void Game::spawnBoss() {
    Boss& boss = state_.boss;
    boss.active = true;
    boss.hits_left = config::BOSS_HITS;
    boss.rect = {random01() * (tg::WIDTH - config::BOSS_WIDTH),
                 config::HUD_HEIGHT - config::BOSS_HEIGHT, config::BOSS_WIDTH, config::BOSS_HEIGHT};
    boss.vx = nextRandom() % 2 == 0 ? config::BOSS_HOVER_SPEED : -config::BOSS_HOVER_SPEED;
    boss.drop_timer = config::BOSS_DROP_INTERVAL;
    // Send help soon.
    if (state_.capsule_timer > config::BOSS_FIRST_CAPSULE_DELAY) {
        state_.capsule_timer = config::BOSS_FIRST_CAPSULE_DELAY;
    }
}

std::uint8_t Game::hurtRobot() {
    state_.lives -= 1;
    state_.invulnerable_seconds_left = config::INVULNERABLE_SECONDS;
    if (state_.lives == 0) {
        state_.game_over = true;
        return EVENT_ROBOT_HIT | EVENT_GAME_OVER;
    }
    return EVENT_ROBOT_HIT;
}

void Game::breakThing(const Rect& r, bool big, bool boss, std::uint16_t points) {
    state_.score += points;
    state_.last_broken_x = r.x + r.w / 2.0f;
    state_.last_broken_y = r.y + r.h / 2.0f;
    state_.last_broken_big = big;
    state_.last_broken_boss = boss;
}

std::uint8_t Game::stepBolts(float dt) {
    std::uint8_t events = 0;
    for (Bolt& bolt : state_.bolts) {
        if (!bolt.active) {
            continue;
        }
        bolt.rect.y -= config::BOLT_SPEED * dt;
        if (bolt.rect.y + bolt.rect.h < config::HUD_HEIGHT) {
            bolt.active = false;
            continue;
        }
        for (Brick& brick : state_.bricks) {
            if (!brick.active || !overlaps(bolt.rect, brick.rect)) {
                continue;
            }
            bolt.active = false;
            if (!brick.big && brick.rect.w > config::SMALL_BRICK_WIDTH) {
                events |= knockOutPiece(brick, bolt.rect.x + bolt.rect.w / 2.0f)
                              ? EVENT_BRICK_BROKEN : EVENT_BRICK_HIT;
                break;
            }
            brick.hits_left -= 1;
            if (brick.hits_left > 0) {
                events |= EVENT_BRICK_HIT;
            } else {
                brick.active = false;
                breakThing(brick.rect, brick.big, false,
                           brick.big ? config::SCORE_BIG_BRICK : config::SCORE_SMALL_BRICK);
                events |= EVENT_BRICK_BROKEN;
            }
            break;
        }
        Boss& boss = state_.boss;
        if (!bolt.active || !boss.active || !overlaps(bolt.rect, boss.rect)) {
            continue;
        }
        bolt.active = false;
        boss.hits_left -= 1;
        if (boss.hits_left > 0) {
            events |= EVENT_BOSS_HIT;
        } else {
            boss.active = false;
            breakThing(boss.rect, true, true, config::SCORE_BOSS);
            startWave(config::WAVE_END_DELAY);
            events |= EVENT_BRICK_BROKEN;
        }
    }
    return events;
}

std::uint8_t Game::stepBricks(float dt) {
    std::uint8_t events = 0;
    const Rect robot = robotHitbox();
    for (Brick& brick : state_.bricks) {
        if (!brick.active) {
            continue;
        }
        brick.rect.y += brick.speed * dt;
        if (brick.rect.y > tg::HEIGHT) {
            brick.active = false;  // missed the robot: no harm done
            continue;
        }
        if (state_.invulnerable_seconds_left > 0.0f || state_.game_over ||
            !overlaps(brick.rect, robot)) {
            continue;
        }
        brick.active = false;
        events |= hurtRobot();
    }
    return events;
}

std::uint8_t Game::stepCapsules(float dt) {
    std::uint8_t events = 0;
    const Rect robot = robotHitbox();
    for (Capsule& c : state_.capsules) {
        if (!c.active) {
            continue;
        }
        c.rect.y += config::CAPSULE_FALL_SPEED * dt;
        if (c.rect.y > tg::HEIGHT) {
            c.active = false;
            continue;
        }
        if (!overlaps(c.rect, robot)) {
            continue;
        }
        c.active = false;
        if (c.type == PowerUp::Life) {
            if (state_.lives < config::MAX_LIVES) {
                state_.lives += 1;
            }
        } else if (c.type == PowerUp::Rapid) {
            state_.rapid_seconds_left = config::RAPID_SECONDS;
        } else {
            const int ammo = state_.ammo + config::AMMO_PER_CAPSULE;
            state_.ammo = static_cast<std::uint8_t>(ammo > config::MAX_AMMO ? config::MAX_AMMO : ammo);
        }
        state_.last_powerup = c.type;
        events |= EVENT_POWERUP;
    }
    return events;
}

void Game::stepBoss(float dt) {
    Boss& boss = state_.boss;
    if (!boss.active) {
        return;
    }
    if (boss.rect.y < config::BOSS_HOVER_Y) {
        // Sink in from the top until it reaches the middle.
        boss.rect.y += config::BOSS_ENTER_SPEED * dt;
        if (boss.rect.y > config::BOSS_HOVER_Y) {
            boss.rect.y = config::BOSS_HOVER_Y;
        }
    } else {
        // Slide back and forth, turning at the screen edges.
        boss.rect.x += boss.vx * dt;
        const float right = tg::WIDTH - boss.rect.w;
        if (boss.rect.x < 0.0f) {
            boss.rect.x = 0.0f;
            boss.vx = config::BOSS_HOVER_SPEED;
        } else if (boss.rect.x > right) {
            boss.rect.x = right;
            boss.vx = -config::BOSS_HOVER_SPEED;
        }
    }
    // Small bricks fall out of the boss from random spots along its bottom.
    boss.drop_timer -= dt;
    if (boss.drop_timer <= 0.0f) {
        boss.drop_timer += config::BOSS_DROP_INTERVAL;
        spawnBrickAt(boss.rect.x + random01() * boss.rect.w, boss.rect.y + boss.rect.h, false);
    }
}

float Game::randomRange(float lo, float hi) {
    return lo + random01() * (hi - lo);
}

std::uint32_t Game::nextRandom() {
    return tg::randomNext(state_.rng);
}

float Game::random01() {
    return static_cast<float>(nextRandom() % 10000U) / 10000.0f;
}

}  // namespace rockrain::logic
