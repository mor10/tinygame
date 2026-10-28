#include <cassert>
#include <cmath>
#include <cstdio>

#include "physics.h"

using namespace brickbreaker;

namespace {

bool near(float a, float b, float tolerance = 0.01f) {
    return std::fabs(a - b) <= tolerance;
}

const physics::Rect FIELD{0.0f, 18.0f, 320.0f, 154.0f};

physics::Ball makeBall(float x, float y, float vx, float vy) {
    physics::Ball ball;
    ball.position = {x, y};
    ball.velocity = {vx, vy};
    ball.radius = 3.0f;
    return ball;
}

void leftWallReflectsAndPushesInside() {
    physics::Ball ball = makeBall(1.0f, 80.0f, -50.0f, -50.0f);
    assert(physics::bounceWalls(ball, FIELD));
    assert(ball.velocity.x > 0.0f);
    assert(ball.position.x >= FIELD.x + ball.radius);
}

void rightWallReflects() {
    physics::Ball ball = makeBall(319.0f, 80.0f, 50.0f, -50.0f);
    assert(physics::bounceWalls(ball, FIELD));
    assert(ball.velocity.x < 0.0f);
    assert(ball.position.x <= FIELD.x + FIELD.w - ball.radius);
}

void topWallReflects() {
    physics::Ball ball = makeBall(100.0f, 19.0f, 10.0f, -50.0f);
    assert(physics::bounceWalls(ball, FIELD));
    assert(ball.velocity.y > 0.0f);
    assert(ball.position.y >= FIELD.y + ball.radius);
}

void bottomIsOpen() {
    physics::Ball ball = makeBall(100.0f, 171.0f, 10.0f, 50.0f);
    assert(!physics::bounceWalls(ball, FIELD));
    assert(ball.velocity.y > 0.0f);
}

void wallMovingAwayIsNotReflectedAgain() {
    physics::Ball ball = makeBall(1.0f, 80.0f, 50.0f, -50.0f);
    physics::bounceWalls(ball, FIELD);
    assert(ball.velocity.x > 0.0f);
}

const physics::Rect PADDLE{100.0f, 156.0f, 44.0f, 6.0f};
constexpr float MAX_ANGLE = 60.0f * 3.14159265f / 180.0f;

void paddleCentreHitGoesStraightUp() {
    physics::Ball ball = makeBall(122.0f, 154.0f, 30.0f, 100.0f);
    const float before = physics::speed(ball);
    assert(physics::bouncePaddle(ball, PADDLE, MAX_ANGLE));
    assert(near(ball.velocity.x, 0.0f));
    assert(ball.velocity.y < 0.0f);
    assert(near(physics::speed(ball), before));
}

void paddleEdgeHitAnglesOutward() {
    physics::Ball right = makeBall(144.0f, 154.0f, 0.0f, 100.0f);
    assert(physics::bouncePaddle(right, PADDLE, MAX_ANGLE));
    assert(right.velocity.x > 0.0f);
    assert(near(std::atan2(right.velocity.x, -right.velocity.y), MAX_ANGLE));

    physics::Ball left = makeBall(100.0f, 154.0f, 0.0f, 100.0f);
    assert(physics::bouncePaddle(left, PADDLE, MAX_ANGLE));
    assert(left.velocity.x < 0.0f);
}

void paddleHitDoesNotLeaveBallEmbedded() {
    physics::Ball ball = makeBall(122.0f, 158.0f, 0.0f, 100.0f);
    assert(physics::bouncePaddle(ball, PADDLE, MAX_ANGLE));
    assert(ball.position.y + ball.radius <= PADDLE.y);
}

void paddleIgnoresUpwardBall() {
    physics::Ball ball = makeBall(122.0f, 154.0f, 0.0f, -100.0f);
    assert(!physics::bouncePaddle(ball, PADDLE, MAX_ANGLE));
}

void paddleIgnoresMiss() {
    physics::Ball ball = makeBall(10.0f, 154.0f, 0.0f, 100.0f);
    assert(!physics::bouncePaddle(ball, PADDLE, MAX_ANGLE));
}

const physics::Rect BRICK{100.0f, 40.0f, 30.0f, 8.0f};

void brickHitFromBelowReflectsVertically() {
    physics::Ball ball = makeBall(115.0f, 50.0f, 20.0f, -80.0f);
    assert(physics::bounceRect(ball, BRICK));
    assert(ball.velocity.y > 0.0f);
    assert(near(ball.velocity.x, 20.0f));
    assert(ball.position.y - ball.radius >= BRICK.y + BRICK.h - 0.001f);
}

void brickHitFromSideReflectsHorizontally() {
    physics::Ball ball = makeBall(98.0f, 44.0f, 80.0f, 20.0f);
    assert(physics::bounceRect(ball, BRICK));
    assert(ball.velocity.x < 0.0f);
    assert(near(ball.velocity.y, 20.0f));
    assert(ball.position.x + ball.radius <= BRICK.x + 0.001f);
}

void brickMissReturnsFalse() {
    physics::Ball ball = makeBall(115.0f, 60.0f, 0.0f, -80.0f);
    assert(!physics::bounceRect(ball, BRICK));
}

void minVerticalSpeedIsEnforcedAndSpeedKept() {
    physics::Ball ball = makeBall(100.0f, 100.0f, 119.0f, -5.0f);
    const float before = physics::speed(ball);
    physics::enforceMinVerticalSpeed(ball, 35.0f);
    assert(near(ball.velocity.y, -35.0f));
    assert(ball.velocity.x > 0.0f);
    assert(near(physics::speed(ball), before));
}

}  // namespace

int main() {
    leftWallReflectsAndPushesInside();
    rightWallReflects();
    topWallReflects();
    bottomIsOpen();
    wallMovingAwayIsNotReflectedAgain();
    paddleCentreHitGoesStraightUp();
    paddleEdgeHitAnglesOutward();
    paddleHitDoesNotLeaveBallEmbedded();
    paddleIgnoresUpwardBall();
    paddleIgnoresMiss();
    brickHitFromBelowReflectsVertically();
    brickHitFromSideReflectsHorizontally();
    brickMissReturnsFalse();
    minVerticalSpeedIsEnforcedAndSpeedKept();
    std::puts("physics tests passed");
}
