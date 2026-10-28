#include <cassert>
#include <cstdio>

#include "input_adapter.h"
#include "settings.h"

int main() {
    // Right handed (default): the right encoder is main; clicks merge.
    input_adapter::Adapter a;
    settings::Settings settings;
    input::State hw;
    devtools::Overrides dev;
    auto in = a.read(hw, {}, dev, 0);
    assert(in.main.delta == 0 && !in.click);
    hw.left.delta = 40;
    hw.right.delta = -9;
    hw.left.button.events = input::EVENT_SHORT_PRESS;
    in = a.read(hw, {}, dev, 10, settings.leftHanded());
    assert(in.main.delta == -9 && in.left.delta == 40 && in.click && !in.right.click);

    // Left handed: the left encoder is main.
    hw.left.button.events = 0;
    hw.right.button.events = input::EVENT_PRESSED;
    hw.right.button.down = true;
    settings::setValueIndex(settings, settings::HANDEDNESS, 1);
    in = a.read(hw, {}, dev, 50, settings.leftHanded());
    assert(in.main.delta == 40 && in.right.delta == -9);
    assert(in.down && in.pressed && !in.click && !in.left.pressed);
    settings::setValueIndex(settings, settings::HANDEDNESS, 0);
    in = a.read(hw, {}, dev, 60, settings.leftHanded());
    assert(in.main.delta == -9 && in.left.delta == 40 && in.right.delta == -9);
    assert(in.down && !in.pressed && !in.released);
    hw = {};

    dev.left.delta = 4;
    dev.right.events = input::EVENT_SHORT_PRESS;
    in = a.read(hw, {}, dev, 120);
    assert(in.left.delta == 4 && in.main.delta == 0 && in.right.click && in.click);
    dev.clearFrame();
    in = a.read(hw, {}, dev, 130);
    assert(in.left.delta == 0 && !in.right.click);
    dev.left.down = true;
    in = a.read(hw, {}, dev, 200);
    assert(in.down && in.pressed);
    in = a.read(hw, {}, dev, 300);
    assert(in.held_ms == 100 && !in.pressed);
    dev.left.down = false;
    in = a.read(hw, {}, dev, 400);
    assert(!in.down && in.released && in.held_ms == 200);
    dev.left.events = input::EVENT_LONG_PRESS;
    in = a.read(hw, {}, dev, 500);
    assert(!in.click);
    dev.clearFrame();
    dev.left.down = true;
    in = a.read(hw, {}, dev, 600);
    assert(in.down && in.pressed);
    hw.left.button.down = true;
    hw.left.button.events = input::EVENT_PRESSED;
    in = a.read(hw, {}, dev, 610);
    assert(in.down && !in.pressed);
    hw.left.button.down = false;
    hw.left.button.events = input::EVENT_RELEASED;
    in = a.read(hw, {}, dev, 620);
    assert(in.down && !in.released);
    hw.left.button.events = 0;
    dev.left.down = false;
    in = a.read(hw, {}, dev, 630);
    assert(!in.down && in.released);
    assert(a.rightEvidence().clicks == 1);
    assert(a.rightEvidence().total == -27);
    std::puts("input adapter tests passed");
}
