#include "actors.h"

namespace tg {

int ActorPool::spawn(int kind, float x, float y, float w, float h) {
    for (int i = 0; i < MAX_ACTORS; ++i) {
        if (actors_[i].alive) continue;
        Actor actor;
        actor.alive = true;
        actor.serial = next_serial_++;
        if (next_serial_ == 0) next_serial_ = 1;
        actor.kind = kind;
        actor.x = x;
        actor.y = y;
        actor.w = w;
        actor.h = h;
        actors_[i] = actor;
        ++count_;
        return i;
    }
    return -1;
}

void ActorPool::remove(int index) {
    if (!valid(index)) return;
    actors_[index].alive = false;
    --count_;
}

void ActorPool::clear() {
    for (Actor& actor : actors_) actor.alive = false;
    count_ = 0;
}

}  // namespace tg
