#pragma once

#include <cstdint>

#include "tg_types.h"

namespace maze {

inline constexpr int COLS = 13;
inline constexpr int ROWS = 6;
inline constexpr int CELLS = COLS * ROWS;
inline constexpr int CELL = 24;
inline constexpr int LEFT = 4;
inline constexpr int TOP = 24;
inline constexpr int HORIZONTAL_WALLS = COLS * (ROWS + 1);
inline constexpr int WALLS = HORIZONTAL_WALLS + (COLS + 1) * ROWS;
inline constexpr float RADIUS = 4.0f;
inline constexpr float STEP = 1.0f / 120.0f;
inline constexpr float MAX_FRAME = 0.1f;
inline constexpr float MAX_SPEED = 70.0f;
inline constexpr int MAX_HAZARDS = 4;
inline constexpr float LASER_OFF = 3.0f;
inline constexpr float LASER_WARNING = 0.75f;
inline constexpr float LASER_ACTIVE = 1.25f;
inline constexpr float PROTECTION = 1.0f;

struct Vec2 { float x = 0; float y = 0; };
struct Rect { float x; float y; float w; float h; };
enum class Mode : std::uint8_t { Chill, Thrill };
enum class Stage : std::uint8_t { Picker, Playing, Transition, Over };
enum class LaserPhase : std::uint8_t { Off, Warning, Active };
enum Event : unsigned {
    STARTED = 1U << 0,
    GENERATED = 1U << 1,
    COMPLETED = 1U << 2,
    SCORED = 1U << 3,
    RESET = 1U << 4,
    TIMED_OUT = 1U << 5,
};
struct Laser { int wall = 0; float offset = 0; };
struct Gate { int wall = 0; int from = 0; int to = 0; };

struct State {
    Mode mode = Mode::Chill;
    Stage stage = Stage::Picker;
    bool walls[WALLS] = {};
    bool protected_route[CELLS] = {};
    int start = 0;
    int goal = 0;
    int route_length = 0;
    Vec2 ball;
    Vec2 velocity;
    std::uint32_t completed = 0;
    std::uint32_t revision = 0;
    Laser lasers[MAX_HAZARDS];
    int traps[MAX_HAZARDS] = {};
    Gate gates[MAX_HAZARDS];
    int laser_count = 0;
    int trap_count = 0;
    int gate_count = 0;
    int requested_hazards = 0;
    int admitted_gate = -1;
    float remaining = 0;
    float budget = 0;
    float clock = 0;
    float protection = 0;
    float transition = 0;
};

class Model {
public:
    explicit Model(std::uint32_t seed = 1) : rng_(seed ? seed : 1) {}
    void begin(Mode mode, std::uint32_t seed, int start = 0);
    unsigned update(const tg::Input& input, float seconds);
    unsigned step(float tilt_x, float tilt_y);
    const State& state() const { return state_; }
    State& mutableState() { return state_; }
    bool connected(int from, int to) const;
    static int edge(int from, int to);
    static Vec2 center(int cell);
    static Rect wallRect(int wall);
    static void wallCells(int wall, int& from, int& to);
    static LaserPhase laserPhase(const Laser& laser, float clock);
    static Rect gatePost(int wall, bool end);
    bool safeRoute(int origin) const;

private:
    std::uint32_t random();
    void generate();
    void placeHazards();
    bool trapAt(int cell) const;
    bool hitHazard() const;
    bool passGate(int gate);
    void move(float tilt_x, float tilt_y);
    State state_;
    std::uint32_t rng_ = 1;
    float accumulator_ = 0;
    tg::Picker mode_picker_;
};

}  // namespace maze
