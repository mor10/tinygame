#include "world.h"

#include <cmath>
#include <cstring>

namespace tg {

namespace {

constexpr int BUFFER_TICKS = 6;
constexpr int GRACE_TICKS = 6;
constexpr int KNOB_HOLD_TICKS = 12;
constexpr float TILT_DEAD = 0.15f;
constexpr float TILT_FULL = 0.6f;

float clampf(float v, float lo, float hi) {
    if (v != v) return lo;
    return v < lo ? lo : (v > hi ? hi : v);
}

// Run direction from tilt: nothing inside the dead zone, then a straight ramp
// up to full speed at TILT_FULL.
float tiltRun(float tilt) {
    const float a = std::fabs(tilt);
    if (!(a >= TILT_DEAD)) return 0.0f;
    const float scale = a >= TILT_FULL ? 1.0f : (a - TILT_DEAD) / (TILT_FULL - TILT_DEAD);
    return tilt < 0 ? -scale : scale;
}

}  // namespace

bool World::start(const GameData& game, int level) {
    if (level < 0 || level >= static_cast<int>(game.levels.size())) return false;
    game_ = &game;
    level_ = &game.levels[static_cast<std::size_t>(level)];
    tile_flags_.clear();
    for (const TileData& tile : game.tiles) tile_flags_.push_back(tile.flags);
    grid_.cols = level_->cols;
    grid_.rows = level_->rows;
    grid_.cells = level_->cells.data();
    grid_.tile_flags = tile_flags_.data();
    grid_.tile_count = static_cast<int>(tile_flags_.size());

    gravity_ = clampf(game.tuning.gravity, 0.0f, 1000.0f);
    run_speed_ = clampf(game.tuning.run_speed, 0.0f, MAX_SPEED);
    const float height = clampf(game.tuning.jump_height, 0.0f, 100.0f);
    jump_speed_ = clampf(std::sqrt(2.0f * gravity_ * height), 0.0f, MAX_SPEED);

    pool_.clear();
    score_ = 0;
    hurts_ = 0;
    player_ = -1;
    state_ = PlayerState{};
    rng_ = Random(1);
    for (const SpawnData& spawn : level_->spawns) {
        const KindData& kind = game.kinds[static_cast<std::size_t>(spawn.kind)];
        const SpriteData& sprite = game.sprites[static_cast<std::size_t>(kind.sprite)];
        const float w = static_cast<float>(sprite.width) / ART_BLOCK;
        const float h = static_cast<float>(sprite.height) / ART_BLOCK;
        const float x = static_cast<float>(spawn.col);
        float y = static_cast<float>(spawn.row + 1) - h;  // bottom-left block on the cell
        if (y < 0) y = 0;  // a tall actor on the top row would start inside the ceiling
        const int index = pool_.spawn(spawn.kind, x, y, w, h);
        if (index < 0) break;  // the pool is full: later spawns are dropped
        Actor& actor = pool_.at(index);
        actor.facing = kind.behaviour == Behaviour::Walker ? -1 : 1;
        // Only the first player-controlled spawn is the player.
        if (kind.player && player_ < 0) {
            player_ = index;
            actor.flags |= ACTOR_PLAYER;
            spawn_x_ = x;
            spawn_y_ = y;
        }
        if (kind.movement == Movement::Platformer && isSupported(grid_, actor))
            actor.flags |= ACTOR_ON_GROUND;
    }
    if (player_ >= 0 && pool_.at(player_).onGround()) state_.air_ticks = 0;
    else state_.air_ticks = 255;
    return true;
}

const char* World::kindName(const Actor& actor) const {
    if (game_ == nullptr || actor.kind < 0 || actor.kind >= static_cast<int>(game_->kinds.size()))
        return "";
    return game_->kinds[static_cast<std::size_t>(actor.kind)].name.c_str();
}

bool World::find(const char* kind, int& index, std::uint32_t& serial) const {
    if (kind == nullptr) return false;
    for (int i = 0; i < MAX_ACTORS; ++i) {
        if (!pool_.valid(i)) continue;
        if (std::strcmp(kindName(pool_.at(i)), kind) == 0) {
            index = i;
            serial = pool_.at(i).serial;
            return true;
        }
    }
    return false;
}

void World::set(Actor& actor, float Actor::*field, float value) {
    if (value == value && std::fabs(value) < 1e9f) actor.*field = value;
}

void World::respawnPlayer() {
    Actor& p = pool_.at(player_);
    p.x = spawn_x_;
    p.y = spawn_y_;
    p.vx = p.vy = 0;
    p.flags &= ~static_cast<std::uint32_t>(ACTOR_ON_GROUND | ACTOR_HIT_WALL);
    if (isSupported(grid_, p)) p.flags |= ACTOR_ON_GROUND;
    state_ = PlayerState{};
    state_.air_ticks = p.onGround() ? 0 : 255;
}

void World::hurtPlayer() {
    if (!pool_.valid(player_)) return;
    ++hurts_;
    respawnPlayer();
}

void World::playerInput(Actor& p, const Input& in) {
    // Run direction: a turn of the main knob is held for 12 more ticks;
    // otherwise tilt decides.
    float dir;
    if (in.main.delta != 0) {
        state_.knob_dir = in.main.delta > 0 ? 1 : -1;
        state_.knob_hold = KNOB_HOLD_TICKS;
        dir = static_cast<float>(state_.knob_dir);
    } else if (state_.knob_hold > 0) {
        --state_.knob_hold;
        dir = static_cast<float>(state_.knob_dir);
    } else {
        dir = tiltRun(in.tilt_x);
    }
    p.vx = dir * run_speed_;
    if (dir > 0) p.facing = 1;
    else if (dir < 0) p.facing = -1;

    // Jump: a press is remembered for 6 ticks; the player may still jump for
    // 6 ticks after leaving the ground.
    if (state_.press_age < 255) ++state_.press_age;
    if (in.pressed) state_.press_age = 0;
    if (state_.jumping && p.vy >= 0) state_.jumping = false;
    if (state_.press_age <= BUFFER_TICKS && state_.air_ticks <= GRACE_TICKS) {
        p.vy = -jump_speed_;
        p.flags &= ~static_cast<std::uint32_t>(ACTOR_ON_GROUND);
        state_.press_age = 255;
        state_.air_ticks = 255;
        state_.jumping = true;
    }
    // Letting go while still rising halves the climb, once per jump.
    if (in.released && state_.jumping && p.vy < 0) {
        p.vy *= 0.5f;
        state_.jumping = false;
    }
}

void World::walkerThink(Actor& a, const KindData& kind) const {
    bool turn = a.hitWall();
    if (!turn && a.onGround()) {
        // The cell just below the leading bottom corner must be solid or one-way.
        const int col = a.facing > 0 ? static_cast<int>(std::ceil(a.x + a.w)) - 1
                                     : static_cast<int>(std::floor(a.x));
        const int row = static_cast<int>(std::floor(a.y + a.h));
        if (!(grid_.flagsAt(col, row) & (TILE_SOLID | TILE_ONE_WAY))) turn = true;
    }
    if (turn) a.facing = -a.facing;
    a.vx = static_cast<float>(a.facing) * kind.speed;
}

void World::tick(const Input& input) {
    if (game_ == nullptr) return;
    // 1. Player input, then movement and behaviours in pool order.
    if (pool_.valid(player_)) playerInput(pool_.at(player_), input);
    for (int i = 0; i < MAX_ACTORS; ++i) {
        if (!pool_.valid(i)) continue;
        Actor& a = pool_.at(i);
        const KindData& kind = kindOf(a);
        const bool is_player = i == player_;
        if (!is_player && kind.behaviour == Behaviour::Walker) walkerThink(a, kind);
        if (kind.movement == Movement::Platformer) stepPlatformer(a, grid_, gravity_);
        if (is_player) {
            state_.air_ticks = a.onGround() ? 0 : (state_.air_ticks < 255 ? state_.air_ticks + 1 : 255);
        }
        if (hasFallenOut(grid_, a)) {
            if (is_player) hurtPlayer();
            else pool_.remove(i);
        }
    }
    // 2. Stock touches: pickups, then hurt.
    touches();
}

void World::touches() {
    if (!pool_.valid(player_)) return;
    for (int i = 0; i < MAX_ACTORS; ++i) {
        if (i == player_ || !pool_.valid(i)) continue;
        const Actor& a = pool_.at(i);
        const KindData& kind = kindOf(a);
        if (kind.behaviour == Behaviour::Pickup && actorsOverlap(pool_.at(player_), a)) {
            score_ += kind.points;
            pool_.remove(i);
        }
    }
    bool hurt = overlapsTileFlag(grid_, pool_.at(player_), TILE_HAZARD);
    for (int i = 0; i < MAX_ACTORS && !hurt; ++i) {
        if (i == player_ || !pool_.valid(i)) continue;
        const Actor& a = pool_.at(i);
        hurt = kindOf(a).on_touch_player == TouchPlayer::Hurt && actorsOverlap(pool_.at(player_), a);
    }
    if (hurt) hurtPlayer();
}

}  // namespace tg
