#pragma once

namespace brickbreaker::physics {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct Ball {
    Vec2 position;
    Vec2 velocity;
    float radius = 0.0f;
};

// Reflects the ball off the left, right, and top edges of `field` and moves it
// back inside. The bottom edge is open. Returns true if a bounce occurred.
bool bounceWalls(Ball& ball, const Rect& field);

// Bounces a downward-moving ball off the paddle. The outgoing angle from
// vertical scales with the hit offset from the paddle centre, up to
// `max_angle_radians` at the edges. Speed is preserved.
bool bouncePaddle(Ball& ball, const Rect& paddle, float max_angle_radians);

// Circle-vs-rectangle collision. Reflects on the axis of least penetration
// and moves the ball out of the rectangle. Returns true on hit.
bool bounceRect(Ball& ball, const Rect& rect);

// Ensures |vy| >= min_vertical_speed while keeping the ball's speed.
void enforceMinVerticalSpeed(Ball& ball, float min_vertical_speed);

float speed(const Ball& ball);

}  // namespace brickbreaker::physics
