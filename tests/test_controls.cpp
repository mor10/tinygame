#include <cassert>
#include <cmath>
#include <cstdio>

#include "tg_types.h"

namespace {

constexpr float TRAVEL = 276.0f;
constexpr float FRAME = 1.0f / 60.0f;
const tg::SteerTuning TUNING;

void slowTurnsUseSlowGain() {
    tg::Steer s(TRAVEL);
    s.reset(TRAVEL / 2);
    float before = s.position();
    // One transition every 12 frames = 5 transitions/s.
    for (int i = 0; i < 120; ++i) {
        const float p = s.update(i % 12 == 0 ? 1 : 0, FRAME);
        if (i % 12 == 0) assert(std::fabs(p - before - TUNING.slow_px) < 0.01f);
        before = p;
    }
}

void fastSpinsMoveFurther() {
    tg::Steer s(TRAVEL);
    s.reset(0);
    float gain = 0.0f;
    for (int i = 0; i < 6; ++i) {  // 6 transitions/frame = 360 transitions/s
        const float before = s.position();
        gain = (s.update(6, FRAME) - before) / 6.0f;
    }
    assert(std::fabs(gain - TUNING.fast_px) < 0.01f);
    s.reset(0);
    for (int i = 0; i < 3; ++i) s.update(6, FRAME);
    assert(s.position() > 6 * 3 * TUNING.slow_px * 2.0f);
}

void clampsReversesAndReportsFraction() {
    tg::Steer s(TRAVEL);
    s.reset(TRAVEL / 2);
    assert(s.fraction() == 0.5f);
    for (int i = 0; i < 200; ++i) s.update(10, FRAME);
    assert(s.position() == TRAVEL && s.fraction() == 1.0f);
    assert(s.update(-1, FRAME) < TRAVEL);  // no dead zone after hitting the edge
    for (int i = 0; i < 200; ++i) s.update(-10, FRAME);
    assert(s.position() == 0.0f);
    assert(s.update(0, 0.0f) == 0.0f);
    s.reset(500);
    assert(s.position() == TRAVEL);
    s.setTravel(100);
    assert(s.position() == 100);
}

void glideEasesAndSettles() {
    float x = 0;
    x = tg::glide(x, 100, 1.0f / 120.0f);
    assert(x > 0 && x < 50);
    for (int i = 0; i < 120; ++i) x = tg::glide(x, 100, 1.0f / 120.0f);
    assert(x == 100);
    assert(tg::glide(3, 9, 0.1f, 0.0f) == 9);
    assert(tg::glide(3, 9, 0.0f) == 3);
}

void pickerStepsEveryFourTransitions() {
    tg::Picker p;
    std::uint8_t v = 0;
    v = p.update(v, 3, 3);
    assert(v == 0);
    v = p.update(v, 3, 1);
    assert(v == 1);
    v = p.update(v, 3, 100);
    assert(v == 2);
    v = p.update(v, 3, -4);  // no wind-up past the end
    assert(v == 1);
    v = p.update(v, 3, -100);
    assert(v == 0);
    v = p.update(v, 3, 4);
    assert(v == 1);
    assert(p.update(7, 3, 0) == 2);
    assert(p.update(0, 0, 4) == 0);
}

}  // namespace

int main() {
    slowTurnsUseSlowGain();
    fastSpinsMoveFurther();
    clampsReversesAndReportsFraction();
    glideEasesAndSettles();
    pickerStepsEveryFourTransitions();
    std::puts("controls tests passed");
}
