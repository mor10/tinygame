#include <cassert>
#include <cmath>
#include <cstdio>

#include "config.h"
#include "logic.h"

using namespace rockrain;

namespace {

// A game with nothing falling, so tests control every brick and capsule.
logic::Game quietGame(float robot_target = 0.5f) {
    logic::Game g;
    g.newGame(1234, robot_target);
    g.mutableState().spawn_timer = 1000.0f;
    g.mutableState().capsule_timer = 1000.0f;
    g.mutableState().boss_timer = 1000.0f;
    g.mutableState().wave_rows_left = 0;
    g.mutableState().ammo = 0;
    return g;
}

// Runs the game for `seconds` in small frames (long frames are clamped).
std::uint8_t run(logic::Game& g, float seconds, float robot_target = 0.5f) {
    std::uint8_t events = 0;
    for (; seconds > 0.0f; seconds -= 0.05f) {
        events |= g.update(seconds < 0.05f ? seconds : 0.05f, robot_target);
    }
    return events;
}

// Puts a brick right on top of the robot's head.
logic::Brick& dropBrickOnRobot(logic::Game& g, bool big = false) {
    logic::Brick& b = g.mutableState().bricks[0];
    b.active = true;
    b.big = big;
    b.hits_left = big ? 2 : 1;
    b.rect = {g.state().robot_x, config::ROBOT_Y - 4.0f, 16.0f, 8.0f};
    b.speed = 60.0f;
    return b;
}

void stepMany(logic::Game& g, float target) {
    for (int i = 0; i < 60; ++i) g.step(target);
}

void robotGlidesToTargetAndStaysOnScreen() {
    logic::Game g = quietGame(0.0f);
    assert(g.state().robot_x == 0.0f);  // a new game snaps
    g.step(1.0f);
    const float first = g.state().robot_x;
    assert(first > 0.0f && first < (tg::WIDTH - config::ROBOT_WIDTH) / 2.0f);
    stepMany(g, 1.0f);
    assert(std::fabs(g.state().robot_x - (tg::WIDTH - config::ROBOT_WIDTH)) < 0.01f);
    stepMany(g, 2.0f);
    assert(g.state().robot_x <= tg::WIDTH - config::ROBOT_WIDTH);
    stepMany(g, -1.0f);
    assert(g.state().robot_x == 0.0f);
}

void brickHitCostsLifeAndGivesSafety() {
    logic::Game g = quietGame();
    dropBrickOnRobot(g);
    const std::uint8_t events = run(g, 0.2f, 0.5f);
    assert(events & logic::EVENT_ROBOT_HIT);
    assert(g.state().lives == config::STARTING_LIVES - 1);
    assert(g.state().invulnerable_seconds_left > 0.0f);
    assert(!g.state().bricks[0].active);

    // While blinking, another brick passes right through.
    dropBrickOnRobot(g);
    assert((run(g, 0.1f, 0.5f) & logic::EVENT_ROBOT_HIT) == 0);
    assert(g.state().lives == config::STARTING_LIVES - 1);
}

void threeHitsIsGameOver() {
    logic::Game g = quietGame();
    std::uint8_t events = 0;
    for (int i = 0; i < config::STARTING_LIVES; ++i) {
        g.mutableState().invulnerable_seconds_left = 0.0f;
        dropBrickOnRobot(g);
        events = run(g, 0.1f, 0.5f);
    }
    assert(events & logic::EVENT_GAME_OVER);
    assert(g.state().game_over);
    assert(g.state().lives == 0);
    assert(run(g, 1.0f, 0.5f) == 0);  // nothing moves after game over
}

void bricksThatMissDisappear() {
    logic::Game g = quietGame(0.0f);
    logic::Brick& b = g.mutableState().bricks[0];
    b.active = true;
    b.hits_left = 1;
    b.rect = {200.0f, tg::HEIGHT - 2.0f, 16.0f, 8.0f};
    b.speed = 60.0f;
    run(g, 0.1f, 0.0f);
    assert(!b.active);
    assert(g.state().lives == config::STARTING_LIVES);
}

void cannotShootWithoutPowerUp() {
    logic::Game g = quietGame();
    assert(!g.canShoot());
    assert(!g.fire());
    for (const logic::Bolt& b : g.state().bolts) {
        assert(!b.active);
    }
}

void ammoGoesDownPerShot() {
    logic::Game g = quietGame();
    g.mutableState().ammo = 2;
    assert(g.fire());
    assert(g.state().ammo == 1);
    assert(!g.fire());  // cooldown
    run(g, config::AMMO_COOLDOWN_SECONDS + 0.02f, 0.5f);
    assert(g.fire());
    assert(g.state().ammo == 0);
    run(g, 1.0f, 0.5f);
    assert(!g.fire());
}

void rapidFireCostsNoAmmo() {
    logic::Game g = quietGame();
    g.mutableState().rapid_seconds_left = 1.0f;
    for (int i = 0; i < 5; ++i) {
        assert(g.fire());
        run(g, config::RAPID_COOLDOWN_SECONDS + 0.02f, 0.5f);
    }
    assert(g.state().ammo == 0);
    run(g, 1.0f, 0.5f);
    assert(!g.canShoot());
}

// Fires one bolt at a brick parked above the robot and lets it fly.
std::uint8_t shootAt(logic::Game& g, logic::Brick& b) {
    b.rect = {g.state().robot_x, 60.0f, b.rect.w, b.rect.h};
    b.speed = 0.0f;
    g.mutableState().fire_cooldown = 0.0f;
    assert(g.fire());
    return run(g, 0.5f, 0.5f);
}

void smallBrickBreaksInOneShotBigInTwo() {
    logic::Game g = quietGame();
    g.mutableState().ammo = 5;
    logic::Brick& small = dropBrickOnRobot(g, false);
    const std::uint32_t before = g.state().score;
    assert(shootAt(g, small) & logic::EVENT_BRICK_BROKEN);
    assert(!small.active);
    assert(g.state().score >= before + config::SCORE_SMALL_BRICK);

    logic::Brick& big = dropBrickOnRobot(g, true);
    std::uint8_t events = shootAt(g, big);
    assert(events & logic::EVENT_BRICK_HIT);
    assert(!(events & logic::EVENT_BRICK_BROKEN));
    assert(big.active && big.hits_left == 1);
    events = shootAt(g, big);
    assert(events & logic::EVENT_BRICK_BROKEN);
    assert(!big.active);
    assert(g.state().last_broken_big);
}

void catchingCapsulesGivesPowerUps() {
    logic::Game g = quietGame();
    logic::Capsule& c = g.mutableState().capsules[0];
    c.active = true;
    c.type = logic::PowerUp::Ammo;
    c.rect = {g.state().robot_x, config::ROBOT_Y, config::CAPSULE_SIZE, config::CAPSULE_SIZE};
    assert(run(g, 0.05f, 0.5f) & logic::EVENT_POWERUP);
    assert(g.state().ammo == config::AMMO_PER_CAPSULE);

    c.active = true;
    c.type = logic::PowerUp::Rapid;
    c.rect = {g.state().robot_x, config::ROBOT_Y, config::CAPSULE_SIZE, config::CAPSULE_SIZE};
    assert(run(g, 0.05f, 0.5f) & logic::EVENT_POWERUP);
    assert(g.state().rapid_seconds_left > config::RAPID_SECONDS - 0.1f);
    assert(g.state().last_powerup == logic::PowerUp::Rapid);

    g.mutableState().ammo = config::MAX_AMMO;
    c.active = true;
    c.type = logic::PowerUp::Ammo;
    c.rect = {g.state().robot_x, config::ROBOT_Y, config::CAPSULE_SIZE, config::CAPSULE_SIZE};
    run(g, 0.05f, 0.5f);
    assert(g.state().ammo == config::MAX_AMMO);
}

void lifeCapsuleAddsHeartUpToMax() {
    logic::Game g = quietGame();
    logic::Capsule& c = g.mutableState().capsules[0];
    for (int i = 0; i < config::MAX_LIVES; ++i) {
        c.active = true;
        c.type = logic::PowerUp::Life;
        c.rect = {g.state().robot_x, config::ROBOT_Y, config::CAPSULE_SIZE, config::CAPSULE_SIZE};
        assert(run(g, 0.05f, 0.5f) & logic::EVENT_POWERUP);
        const int expected = config::STARTING_LIVES + i + 1;
        assert(g.state().lives == (expected > config::MAX_LIVES ? config::MAX_LIVES : expected));
    }
    assert(g.state().lives == config::MAX_LIVES);
}

// Parks a boss somewhere (far from the robot unless asked).
logic::Boss& placeBoss(logic::Game& g, float x, float y) {
    logic::Boss& boss = g.mutableState().boss;
    boss.active = true;
    boss.hits_left = config::BOSS_HITS;
    boss.rect = {x, y, config::BOSS_WIDTH, config::BOSS_HEIGHT};
    boss.drop_timer = config::BOSS_DROP_INTERVAL;
    return boss;
}

int activeBricks(const logic::Game& g) {
    int n = 0;
    for (const logic::Brick& b : g.state().bricks) {
        n += b.active ? 1 : 0;
    }
    return n;
}

void bossArrivesOnItsOwnOneAtATime() {
    logic::Game g = quietGame();
    g.mutableState().boss_timer = 0.05f;
    run(g, 0.1f, 0.5f);
    const logic::Boss& boss = g.state().boss;
    assert(boss.active);
    assert(boss.hits_left == config::BOSS_HITS);
    assert(boss.rect.x >= 0.0f && boss.rect.x + boss.rect.w <= tg::WIDTH);
    // The next-boss timer waits while this one is out.
    const float timer = g.state().boss_timer;
    assert(timer >= config::BOSS_INTERVAL_MIN - 0.2f);
    run(g, 1.0f, 0.5f);
    assert(g.state().boss_timer == timer);
}

void bossDropsSmallBricks() {
    logic::Game g = quietGame(0.0f);
    placeBoss(g, 200.0f, 20.0f);
    run(g, config::BOSS_DROP_INTERVAL * 3.0f + 0.05f, 0.0f);
    assert(activeBricks(g) >= 3);
    for (const logic::Brick& b : g.state().bricks) {
        if (b.active) {
            assert(!b.big);
        }
    }
}

void bossTakesTenShots() {
    logic::Game g = quietGame();
    g.mutableState().rapid_seconds_left = 100.0f;
    logic::Boss& boss = placeBoss(g, g.state().robot_x - 9.0f, 60.0f);
    const std::uint32_t before = g.state().score;
    std::uint8_t events = 0;
    for (int i = 0; i < config::BOSS_HITS; ++i) {
        boss.rect.x = g.state().robot_x - 9.0f;  // keep it parked
        boss.rect.y = 60.0f;
        boss.drop_timer = 100.0f;
        g.mutableState().fire_cooldown = 0.0f;
        assert(g.fire());
        events = run(g, 0.5f, 0.5f);
        if (i < config::BOSS_HITS - 1) {
            assert(events & logic::EVENT_BOSS_HIT);
            assert(boss.active);
        }
    }
    assert(events & logic::EVENT_BRICK_BROKEN);
    assert(!boss.active);
    assert(g.state().last_broken_boss);
    assert(g.state().score >= before + config::SCORE_BOSS);
}

void bossHoversInTheMiddleUntilBroken() {
    logic::Game g = quietGame();
    g.mutableState().boss_timer = 0.01f;
    run(g, 0.05f, 0.5f);
    logic::Boss& boss = g.mutableState().boss;
    assert(boss.active);
    run(g, 5.0f, 0.5f);  // long enough to sink to the middle
    assert(boss.rect.y == config::BOSS_HOVER_Y);

    // It goes back and forth, stays on screen and never leaves by itself.
    bool went_left = false;
    bool went_right = false;
    for (int i = 0; i < 300; ++i) {
        g.mutableState().lives = config::STARTING_LIVES;  // ignore its bricks
        run(g, 0.1f, 0.5f);
        assert(boss.active);
        assert(boss.rect.y == config::BOSS_HOVER_Y);
        assert(boss.rect.x >= 0.0f && boss.rect.x + boss.rect.w <= tg::WIDTH);
        went_left = went_left || boss.vx < 0.0f;
        went_right = went_right || boss.vx > 0.0f;
    }
    assert(went_left && went_right);
}

void bossFightBringsAmmoAndRapid() {
    logic::Game g = quietGame();
    g.mutableState().capsule_timer = 50.0f;
    g.mutableState().capsules_since_life = config::LIFE_GUARANTEE_AFTER;  // a LIFE is "due"
    g.mutableState().boss_timer = 0.01f;
    run(g, 0.05f, 0.5f);
    assert(g.state().boss.active);
    // Help arrives quickly, and keeps coming.
    assert(g.state().capsule_timer <= config::BOSS_FIRST_CAPSULE_DELAY);
    for (int i = 0; i < 20; ++i) {
        logic::Capsule& c = g.mutableState().capsules[0];
        c.active = false;
        g.mutableState().capsule_timer = 0.0f;
        g.step(0.5f);
        assert(c.active);
        assert(c.type != logic::PowerUp::Life);
        assert(g.state().capsule_timer <= config::BOSS_CAPSULE_INTERVAL_MAX);
        c.active = false;
    }
}

void lifeCapsulesTurnUpOften() {
    // Over many games, there is never a long run of capsules without a LIFE.
    for (std::uint32_t seed = 1; seed <= 50; ++seed) {
        logic::Game g;
        g.newGame(seed, 0.0f);
        g.mutableState().spawn_timer = 1000.0f;
        g.mutableState().boss_timer = 1000.0f;
        g.mutableState().wave_rows_left = 0;
        int since_life = 0;
        for (int i = 0; i < 40; ++i) {
            g.mutableState().capsule_timer = 0.0f;
            logic::Capsule& c = g.mutableState().capsules[0];
            c.active = false;
            g.step(0.0f);
            assert(c.active);
            since_life = c.type == logic::PowerUp::Life ? 0 : since_life + 1;
            assert(since_life <= config::LIFE_GUARANTEE_AFTER);
            c.active = false;
        }
    }
}

// Checks one freshly spawned wave row (all active bricks at `top`): it fills
// the screen except for 1 or 2 openings the robot fits through.
void checkRow(const logic::Game& g, float top) {
    bool filled[config::ROW_SLOTS] = {};
    for (const logic::Brick& b : g.state().bricks) {
        if (b.active && b.rect.y == top) {
            assert(!b.big);
            const int first = static_cast<int>(b.rect.x / config::SMALL_BRICK_WIDTH);
            const int count = static_cast<int>(b.rect.w / config::SMALL_BRICK_WIDTH);
            assert(first * config::SMALL_BRICK_WIDTH == b.rect.x);
            assert(count * config::SMALL_BRICK_WIDTH == b.rect.w);
            for (int s = first; s < first + count; ++s) {
                assert(!filled[s]);
                filled[s] = true;
            }
        }
    }
    int openings = 0;
    int run_len = 0;
    for (int slot = 0; slot <= config::ROW_SLOTS; ++slot) {
        if (slot < config::ROW_SLOTS && !filled[slot]) {
            ++run_len;
            continue;
        }
        if (run_len > 0) {
            ++openings;
            assert(run_len * config::SMALL_BRICK_WIDTH > config::ROBOT_WIDTH + 8.0f);
        }
        run_len = 0;
    }
    assert(openings == 1 || openings == 2);
}

void gameStartsWithAWaveOfRows() {
    for (std::uint32_t seed = 1; seed <= 30; ++seed) {
        logic::Game g;
        g.newGame(seed, 0.0f);
        assert(g.state().wave_rows_left == config::WAVE_ROWS);
        int rows = 0;
        float last_boss_timer = g.state().boss_timer;
        for (int i = 0; i < 2000 && g.state().wave_rows_left > 0; ++i) {
            const std::uint8_t before = g.state().wave_rows_left;
            g.mutableState().invulnerable_seconds_left = 10.0f;  // don't die while checking
            g.step(0.0f);
            // The boss waits while a wave falls.
            if (g.state().wave_rows_left > 0) {
                assert(g.state().boss_timer == last_boss_timer);
            }
            last_boss_timer = g.state().boss_timer;
            if (g.state().wave_rows_left != before) {
                ++rows;
                // The new row is the highest set of bricks.
                float top = 1000.0f;
                for (const logic::Brick& b : g.state().bricks) {
                    if (b.active && b.rect.y < top) {
                        top = b.rect.y;
                    }
                }
                checkRow(g, top);
            }
        }
        assert(rows == config::WAVE_ROWS);
        assert(g.state().waves_done == 1);
    }
}

void noRandomBricksDuringAWave() {
    logic::Game g;
    g.newGame(7, 0.0f);
    g.mutableState().spawn_timer = 0.0f;  // would spawn right away if allowed
    for (int i = 0; i < 60; ++i) {
        g.mutableState().invulnerable_seconds_left = 10.0f;
        g.step(0.0f);
    }
    for (const logic::Brick& b : g.state().bricks) {
        if (b.active) {
            assert(!b.big);
            assert(b.speed == config::WAVE_FALL_SPEED);
        }
    }
}

void startWithAmmo() {
    logic::Game g;
    g.newGame(3, 0.5f);
    assert(g.state().ammo == config::STARTING_AMMO);
    assert(g.canShoot());
}

void shootingARowKnocksOutOnePiece() {
    logic::Game g = quietGame();
    g.mutableState().ammo = 5;
    // A 6-piece stretch whose 3rd piece sits over the robot's cannon.
    const float cannon = g.state().robot_x + config::ROBOT_WIDTH / 2.0f;
    const float piece = config::SMALL_BRICK_WIDTH;
    logic::Brick& stretch = g.mutableState().bricks[0];
    stretch.active = true;
    stretch.hits_left = 1;
    stretch.rect = {cannon - 2.5f * piece, 60.0f, 6.0f * piece, 8.0f};
    stretch.speed = 0.0f;
    const std::uint32_t before = g.state().score;
    assert(g.fire());
    assert(run(g, 0.5f, 0.5f) & logic::EVENT_BRICK_BROKEN);
    assert(g.state().score >= before + config::SCORE_SMALL_BRICK);
    // Left part: 2 pieces. Right part: 3 pieces. A one-piece hole between.
    assert(stretch.active && stretch.rect.w == 2.0f * piece);
    float right_w = 0.0f;
    float right_x = 0.0f;
    for (const logic::Brick& b : g.state().bricks) {
        if (b.active && &b != &stretch) {
            right_w = b.rect.w;
            right_x = b.rect.x;
        }
    }
    assert(right_w == 3.0f * piece);
    assert(right_x == stretch.rect.x + 3.0f * piece);
}

void beatingTheBossStartsAFasterWave() {
    logic::Game g = quietGame();
    g.mutableState().waves_done = 1;
    g.mutableState().rapid_seconds_left = 100.0f;
    logic::Boss& boss = placeBoss(g, g.state().robot_x - 9.0f, 60.0f);
    boss.hits_left = 1;
    boss.drop_timer = 100.0f;
    assert(g.fire());
    assert(run(g, 0.5f, 0.5f) & logic::EVENT_BRICK_BROKEN);
    assert(!boss.active);
    assert(g.state().wave_rows_left == config::WAVE_ROWS);
    assert(g.waveSpeed() > config::WAVE_FALL_SPEED);
}

void survivingScoresPoints() {
    logic::Game g = quietGame();
    for (int i = 0; i < 31; ++i) {
        run(g, 0.1f, 0.5f);
    }
    assert(g.state().score == 3 * config::SCORE_PER_SECOND);
}

void getsHarderAndCaps() {
    logic::Game g = quietGame();
    const float speed0 = g.fallSpeed();
    const float interval0 = g.spawnInterval();
    g.mutableState().elapsed = 30.0f;
    assert(g.fallSpeed() > speed0);
    assert(g.spawnInterval() < interval0);
    g.mutableState().elapsed = 10000.0f;
    assert(g.fallSpeed() == config::FALL_SPEED_MAX);
    assert(g.spawnInterval() == config::SPAWN_INTERVAL_MIN);
}

void bricksAndCapsulesSpawnOnTheirOwn() {
    logic::Game g;
    g.newGame(99, 0.0f);
    bool brick = false;
    bool capsule = false;
    for (int i = 0; i < 100 && !(brick && capsule); ++i) {
        run(g, 0.1f, 0.0f);
        for (const logic::Brick& b : g.state().bricks) {
            if (b.active) {
                brick = true;
                assert(b.rect.x >= 0.0f && b.rect.x + b.rect.w <= tg::WIDTH);
            }
        }
        for (const logic::Capsule& c : g.state().capsules) {
            capsule = capsule || c.active;
        }
    }
    assert(brick && capsule);
}

}  // namespace

int main() {
    robotGlidesToTargetAndStaysOnScreen();
    brickHitCostsLifeAndGivesSafety();
    threeHitsIsGameOver();
    bricksThatMissDisappear();
    cannotShootWithoutPowerUp();
    ammoGoesDownPerShot();
    rapidFireCostsNoAmmo();
    smallBrickBreaksInOneShotBigInTwo();
    catchingCapsulesGivesPowerUps();
    lifeCapsuleAddsHeartUpToMax();
    bossArrivesOnItsOwnOneAtATime();
    bossDropsSmallBricks();
    bossTakesTenShots();
    bossHoversInTheMiddleUntilBroken();
    bossFightBringsAmmoAndRapid();
    lifeCapsulesTurnUpOften();
    gameStartsWithAWaveOfRows();
    noRandomBricksDuringAWave();
    beatingTheBossStartsAFasterWave();
    startWithAmmo();
    shootingARowKnocksOutOnePiece();
    survivingScoresPoints();
    getsHarderAndCaps();
    bricksAndCapsulesSpawnOnTheirOwn();
    std::puts("rockrain logic tests passed");
    return 0;
}
