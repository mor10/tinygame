#pragma once

#include <cstdint>

#include "tg_types.h"

namespace tiltblock {

inline constexpr int SIDE = 8;
inline constexpr int CELLS = SIDE * SIDE;
inline constexpr int LAYERS_PER_LEVEL = 8;
inline constexpr int START_LIVES = 5;
inline constexpr int MAX_LIVES = 9;
inline constexpr float STEP = 1.0f / 120.0f;
inline constexpr float MAX_FRAME = 0.1f;
inline constexpr float BALL_RADIUS = 0.18f;
inline constexpr float MAX_SPEED = 4.5f;
inline constexpr float FALL_SECONDS = 0.8f;
inline constexpr float BEAM_SECONDS = 1.0f;

enum class Mode : std::uint8_t { Tilt, Twist };
enum class Stage : std::uint8_t {
    ModeSelect,
    Calibration,
    Playing,
    Falling,
    BeamUp,
    LayerDrop,
    LevelClear,
    TractorSave,
    Over
};
enum class Bonus : std::uint8_t { None, Life, Zone, Boing, Quake, Tractor, Laser };

enum Event : unsigned {
    NO_EVENT = 0,
    LIFE_LOST = 1U << 0,
    CUBE_TOUCHED = 1U << 1,
    LAYER_CLEARED = 1U << 2,
    LEVEL_CLEARED = 1U << 3,
    MONSTER_KILLED = 1U << 4,
    GAME_OVER = 1U << 5,
    BONUS_USED = 1U << 6
};

struct State {
    Mode mode = Mode::Tilt;
    Stage stage = Stage::ModeSelect;
    bool touched[CELLS] = {};
    bool wall[CELLS] = {};
    bool hole[CELLS] = {};
    Bonus bonus[CELLS] = {};
    float x = 3.5f;
    float y = 3.5f;
    float vx = 0.0f;
    float vy = 0.0f;
    float left_angle = 0.0f;
    float right_angle = 0.0f;
    float calibration_x = 0.0f;
    float calibration_y = 0.0f;
    float timer = 0.0f;
    float safety = 0.0f;
    float airborne = 0.0f;
    float earthquake = 0.0f;
    float quake_shove = 0.0f;
    float monster_spawn = 0.0f;
    float monster_x = 0.0f;
    float monster_y = 0.0f;
    float monster_pause = 0.0f;
    float laser_cooldown = 0.0f;
    float laser_flash = 0.0f;
    float laser_x1 = 0.0f;
    float laser_y1 = 0.0f;
    float laser_x2 = 0.0f;
    float laser_y2 = 0.0f;
    std::uint32_t score = 0;
    std::uint32_t best = 0;
    int lives = START_LIVES;
    int level = 1;
    int layer = LAYERS_PER_LEVEL;
    int monster_corner = -1;
    int laser_cell = -1;
    bool tractor = false;
    bool laser_armed = false;
    bool monster_pending = false;
    bool monster_alive = false;
    bool fall_edge = false;
    bool fall_hole = false;
};

class Model {
public:
    explicit Model(std::uint32_t seed = 1);
    unsigned update(const tg::Input& input, float seconds);
    const State& state() const { return state_; }
    State& mutableState() { return state_; }
    static int cell(int col, int row);
    static bool solvable(const State& state);
    static float stackDepthLayersForView(const State& state);
    static void tiltAcceleration(float tilt_x, float tilt_y, float& x, float& y);
    static void twistAcceleration(float left_angle, float right_angle, float& x, float& y);

private:
    std::uint32_t random();
    void startRun();
    void generateLayer();
    void addBonus(Bonus bonus);
    bool connected() const;
    bool complete() const;
    void touch(int index);
    void collect(int index, Bonus bonus);
    void markTouched(int index);
    void step(float ax, float ay, unsigned& events);
    void move(float ax, float ay, unsigned& events);
    void loseLife(bool edge_fall, unsigned& events);
    void beginFall(bool edge, bool hole);
    void respawn();
    void finishLayer(unsigned& events);
    void moveMonster(float dt, unsigned& events);
    void fireLaser(unsigned& events);
    int safeCell(int preferred) const;
    int cornerCell(int corner) const;

    State state_;
    std::uint32_t seed_ = 1;
    float accumulator_ = 0.0f;
    int playtest_level_ = 1;
    int playtest_layer_ = LAYERS_PER_LEVEL;
    bool playtest_mode_ = false;
    tg::Picker mode_picker_;
};

}  // namespace tiltblock
