#include <cassert>
#include <cstddef>

#include "deferred_queue.h"

int main() {
    using tg::engine::DeferredQueue;
    using tg::engine::DeferredQueueError;

    DeferredQueue<int, 3> queue;
    assert(queue.capacity() == 3);
    assert(queue.empty());
    assert(queue.enqueue(1) == DeferredQueueError::None);
    assert(queue.enqueue(2) == DeferredQueueError::None);
    assert(queue.enqueue(3) == DeferredQueueError::None);
    assert(queue.enqueue(4) == DeferredQueueError::Full);
    assert(queue.size() == 3);

    int observed[4] = {};
    std::size_t count = 0;
    const std::size_t drained = queue.drainBatch([&](int value) {
        observed[count++] = value;
        if (value == 1) {
            assert(queue.enqueue(4) == DeferredQueueError::None);
        }
    });
    assert(drained == 3);
    assert(count == 3);
    assert(observed[0] == 1 && observed[1] == 2 && observed[2] == 3);
    assert(queue.size() == 1);

    const std::size_t second_drain = queue.drainBatch([&](int value) {
        observed[count++] = value;
    });
    assert(second_drain == 1);
    assert(count == 4 && observed[3] == 4);
    assert(queue.empty());

    assert(queue.enqueue(5) == DeferredQueueError::None);
    assert(queue.enqueue(6) == DeferredQueueError::None);
    assert(queue.enqueue(7) == DeferredQueueError::None);
    int wrapped[3] = {};
    std::size_t wrapped_count = 0;
    assert(queue.drainBatch([&](int value) { wrapped[wrapped_count++] = value; }) == 3);
    assert(wrapped_count == 3);
    assert(wrapped[0] == 5 && wrapped[1] == 6 && wrapped[2] == 7);
    assert(queue.empty());
}
