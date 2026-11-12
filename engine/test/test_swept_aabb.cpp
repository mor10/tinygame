#include "swept_aabb.h"

#include <cassert>
#include <cmath>
#include <limits>

using tg::engine::Aabb;
using tg::engine::SweepResult;

int main() {
    auto hit = tg::engine::sweepAabb({0, 0, 10, 10}, 200, 0, {100, 0, 10, 10});
    assert(hit.status == SweepResult::Status::Hit);
    assert(std::abs(hit.time - 0.45) < 1e-12);
    assert(hit.normal_x == -1 && hit.normal_y == 0);

    hit = tg::engine::sweepAabb({120, 0, 10, 10}, -40, 0, {100, 0, 10, 10});
    assert(hit.status == SweepResult::Status::Hit);
    assert(std::abs(hit.time - 0.25) < 1e-12);
    assert(hit.normal_x == 1 && hit.normal_y == 0);

    hit = tg::engine::sweepAabb({0, 0, 10, 10}, 20, 20, {20, 20, 10, 10});
    assert(hit.status == SweepResult::Status::Hit);
    assert(std::abs(hit.time - 0.5) < 1e-12);
    assert(hit.normal_x == -1 && hit.normal_y == -1);

    hit = tg::engine::sweepAabb({0, 0, 10, 10}, 0, 20, {0, 25, 10, 10});
    assert(hit.status == SweepResult::Status::Hit);
    assert(std::abs(hit.time - 0.75) < 1e-12);
    assert(hit.normal_x == 0 && hit.normal_y == -1);

    hit = tg::engine::sweepAabb({0, 0, 10, 10}, 20, 0, {0, 20, 10, 10});
    assert(hit.status == SweepResult::Status::Miss);
    hit = tg::engine::sweepAabb({0, 0, 10, 10}, 20, 0, {35, 0, 10, 10});
    assert(hit.status == SweepResult::Status::Miss);
    hit = tg::engine::sweepAabb({0, 0, 10, 10}, 20, 0, {30, 0, 10, 10});
    assert(hit.status == SweepResult::Status::Hit && hit.time == 1.0);

    hit = tg::engine::sweepAabb({0, 0, 10, 10}, 2, 0, {5, 5, 10, 10});
    assert(hit.status == SweepResult::Status::InitialContact);
    hit = tg::engine::sweepAabb({0, 0, 10, 10}, 0, 0, {10, 0, 10, 10});
    assert(hit.status == SweepResult::Status::InitialContact);

    hit = tg::engine::sweepAabb(
        {0, 0, 10, 10}, std::numeric_limits<double>::infinity(), 0, {20, 0, 10, 10});
    assert(hit.status == SweepResult::Status::InvalidInput);
    hit = tg::engine::sweepAabb(
        {0, 0, 10, 10}, std::numeric_limits<double>::quiet_NaN(), 0, {20, 0, 10, 10});
    assert(hit.status == SweepResult::Status::InvalidInput);
    hit = tg::engine::sweepAabb({0, 0, 0, 10}, 1, 0, {20, 0, 10, 10});
    assert(hit.status == SweepResult::Status::InvalidInput);
    hit = tg::engine::sweepAabb(
        {0, 0, 10, 10}, 1, 0,
        {std::numeric_limits<double>::max(), 0, std::numeric_limits<double>::max(), 10});
    assert(hit.status == SweepResult::Status::InvalidInput);
}
