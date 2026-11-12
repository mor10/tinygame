#include <cassert>
#include <cstdint>

#include "actor_pool.h"

int main() {
    using tg::engine::ActorHandle;
    using tg::engine::GenerationalPool;
    using tg::engine::PoolError;

    GenerationalPool<std::uint32_t, 2> actors;
    assert(actors.capacity() == 2);
    assert(actors.size() == 0);

    const auto first = actors.create(11);
    const auto second = actors.create(22);
    assert(first && second);
    assert(first.handle.slot == 0 && second.handle.slot == 1);
    assert(actors.size() == 2);

    const auto full = actors.create(33);
    assert(full.error == PoolError::Full);
    assert(!full.handle.valid());

    auto first_actor = actors.get(first.handle);
    assert(first_actor && *first_actor.value == 11);
    *first_actor.value = 12;
    const auto const_view = static_cast<const decltype(actors)&>(actors).get(first.handle);
    assert(const_view && *const_view.value == 12);

    assert(actors.erase(first.handle) == PoolError::None);
    assert(actors.erase(first.handle) == PoolError::InvalidHandle);
    assert(actors.get(first.handle).error == PoolError::InvalidHandle);
    assert(actors.get(second.handle) && *actors.get(second.handle).value == 22);
    assert(!actors.handleAt(0).valid());
    assert(actors.size() == 1);

    const auto replacement = actors.create(44);
    assert(replacement && replacement.handle.slot == first.handle.slot);
    assert(replacement.handle.generation != first.handle.generation);
    assert(actors.get(first.handle).error == PoolError::InvalidHandle);
    assert(actors.get(replacement.handle) && *actors.get(replacement.handle).value == 44);
    assert(actors.handleAt(0) == replacement.handle);

    assert(actors.erase(ActorHandle{}) == PoolError::InvalidHandle);
    assert(actors.erase({2, 1}) == PoolError::InvalidHandle);
    assert(actors.get({0, 0}).error == PoolError::InvalidHandle);

    GenerationalPool<std::uint32_t, 4> ordered;
    const auto at_zero = ordered.create(10);
    const auto at_one = ordered.create(20);
    const auto at_two = ordered.create(30);
    assert(ordered.erase(at_one.handle) == PoolError::None);
    const auto reused_one = ordered.create(40);
    assert(reused_one.handle.slot == 1);

    std::uint32_t visited[4] = {};
    std::size_t visited_count = 0;
    ordered.forEach([&](ActorHandle handle) {
        const auto actor = ordered.get(handle);
        assert(actor);
        visited[visited_count++] = *actor.value;
    });
    assert(visited_count == 3);
    assert(visited[0] == 10 && visited[1] == 40 && visited[2] == 30);
    assert(at_zero.handle.slot < reused_one.handle.slot);
    assert(reused_one.handle.slot < at_two.handle.slot);

    bool inserted_during_visit = false;
    bool visited_inserted_entry = false;
    ordered.forEach([&](ActorHandle handle) {
        if (handle.slot == 1) {
            const auto actor = ordered.get(handle);
            assert(actor);
            visited_inserted_entry = *actor.value == 50;
        }
        if (handle.slot != 0 || inserted_during_visit) return;
        assert(ordered.erase(reused_one.handle) == PoolError::None);
        const auto inserted = ordered.create(50);
        assert(inserted && inserted.handle.slot == 1);
        inserted_during_visit = true;
    });
    assert(inserted_during_visit && visited_inserted_entry);
}
