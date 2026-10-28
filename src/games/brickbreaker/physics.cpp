#include "physics.h"

#include <cmath>

namespace brickbreaker::physics {

namespace {

float clamp(float value, float low, float high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

}  // namespace

float speed(const Ball& ball) {
    return std::sqrt(ball.velocity.x * ball.velocity.x +
                     ball.velocity.y * ball.velocity.y);
}

bool bounceWalls(Ball& ball, const Rect& field) {
    bool bounced = false;
    const float left = field.x + ball.radius;
    const float right = field.x + field.w - ball.radius;
    const float top = field.y + ball.radius;

    if (ball.position.x < left) {
        ball.position.x = left;
        if (ball.velocity.x < 0.0f) {
            ball.velocity.x = -ball.velocity.x;
            bounced = true;
        }
    } else if (ball.position.x > right) {
        ball.position.x = right;
        if (ball.velocity.x > 0.0f) {
            ball.velocity.x = -ball.velocity.x;
            bounced = true;
        }
    }

    if (ball.position.y < top) {
        ball.position.y = top;
        if (ball.velocity.y < 0.0f) {
            ball.velocity.y = -ball.velocity.y;
            bounced = true;
        }
    }
    return bounced;
}

bool bouncePaddle(Ball& ball, const Rect& paddle, float max_angle_radians) {
    if (ball.velocity.y <= 0.0f) {
        return false;
    }
    const float closest_x = clamp(ball.position.x, paddle.x, paddle.x + paddle.w);
    const float closest_y = clamp(ball.position.y, paddle.y, paddle.y + paddle.h);
    const float dx = ball.position.x - closest_x;
    const float dy = ball.position.y - closest_y;
    if (dx * dx + dy * dy > ball.radius * ball.radius) {
        return false;
    }
    // A ball whose centre is already below the paddle is lost, not bounced.
    if (ball.position.y > paddle.y + paddle.h) {
        return false;
    }

    const float half_width = paddle.w / 2.0f;
    const float offset =
        clamp((ball.position.x - (paddle.x + half_width)) / half_width, -1.0f, 1.0f);
    const float angle = offset * max_angle_radians;
    const float current_speed = speed(ball);
    ball.velocity.x = current_speed * std::sin(angle);
    ball.velocity.y = -current_speed * std::cos(angle);
    ball.position.y = paddle.y - ball.radius;
    return true;
}

bool bounceRect(Ball& ball, const Rect& rect) {
    const float closest_x = clamp(ball.position.x, rect.x, rect.x + rect.w);
    const float closest_y = clamp(ball.position.y, rect.y, rect.y + rect.h);
    const float dx = ball.position.x - closest_x;
    const float dy = ball.position.y - closest_y;
    if (dx * dx + dy * dy >= ball.radius * ball.radius) {
        return false;
    }

    // Distance the ball must move to exit through each side.
    const float push_left = (ball.position.x + ball.radius) - rect.x;
    const float push_right = (rect.x + rect.w) - (ball.position.x - ball.radius);
    const float push_up = (ball.position.y + ball.radius) - rect.y;
    const float push_down = (rect.y + rect.h) - (ball.position.y - ball.radius);
    const float overlap_x = push_left < push_right ? push_left : push_right;
    const float overlap_y = push_up < push_down ? push_up : push_down;

    if (overlap_x < overlap_y) {
        if (push_left < push_right) {
            ball.position.x -= push_left;
            ball.velocity.x = -std::fabs(ball.velocity.x);
        } else {
            ball.position.x += push_right;
            ball.velocity.x = std::fabs(ball.velocity.x);
        }
    } else {
        if (push_up < push_down) {
            ball.position.y -= push_up;
            ball.velocity.y = -std::fabs(ball.velocity.y);
        } else {
            ball.position.y += push_down;
            ball.velocity.y = std::fabs(ball.velocity.y);
        }
    }
    return true;
}

void enforceMinVerticalSpeed(Ball& ball, float min_vertical_speed) {
    if (std::fabs(ball.velocity.y) >= min_vertical_speed) {
        return;
    }
    const float current_speed = speed(ball);
    const float vy_sign = ball.velocity.y < 0.0f ? -1.0f : 1.0f;
    if (current_speed <= min_vertical_speed) {
        ball.velocity.y = vy_sign * current_speed;
        ball.velocity.x = 0.0f;
        return;
    }
    const float vx_sign = ball.velocity.x < 0.0f ? -1.0f : 1.0f;
    ball.velocity.y = vy_sign * min_vertical_speed;
    ball.velocity.x = vx_sign * std::sqrt(current_speed * current_speed -
                                          min_vertical_speed * min_vertical_speed);
}

}  // namespace brickbreaker::physics
