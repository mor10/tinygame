#include "engine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <string>

#include "game/game_data.h"
#include "gfx/font.h"
#include "lua/script.h"
#include "gfx/sprite.h"
#include "world/camera.h"
#include "world/stage.h"
#include "world/tilemap.h"
#include "world/world.h"

namespace tg {

namespace {
constexpr int BLOCK_PX = 16;  // screen pixels per block: art scale 2
constexpr int ART_SCALE = BLOCK_PX / ART_BLOCK;
constexpr int STATUS_SHADE = 70;

int roundInt(float v) { return static_cast<int>(std::floor(v + 0.5f)); }

// "GAME STOPPED" and the message, wrapped to the screen width. No allocation.
void drawErrorScreen(gfx::Canvas& canvas, const gfx::Palette& pal, const char* message) {
    canvas.fill(pal.background);
    const int margin = 6;
    const int scale = canvas.width() >= 240 ? 2 : 1;
    const int advance = gfx::GLYPH_ADVANCE * scale;
    const int line_h = (gfx::GLYPH_H + 2) * scale;
    const int per_line = std::max(1, (canvas.width() - 2 * margin) / advance);
    gfx::drawText(canvas, margin, margin, "GAME STOPPED", pal.ink, scale);
    int y = margin + 2 * line_h;
    char line[128];
    const int cap = std::min(per_line, static_cast<int>(sizeof(line)) - 1);
    const char* p = message;
    while (*p != '\0' && y + line_h <= canvas.height()) {
        while (*p == ' ') ++p;
        int len = 0;
        while (p[len] != '\0' && len < cap) ++len;
        if (p[len] != '\0' && p[len] != ' ') {  // break at the last space when there is one
            int back = len;
            while (back > 0 && p[back - 1] != ' ') --back;
            if (back > 0) len = back;
        }
        int out = len;
        while (out > 0 && p[out - 1] == ' ') --out;
        std::memcpy(line, p, static_cast<std::size_t>(out));
        line[out] = '\0';
        gfx::drawText(canvas, margin, y, line, pal.ink, scale);
        y += line_h;
        p += len;
    }
}
}  // namespace

// Lets Lua rules read and change the world (see lua/script.h).
class WorldAdapter : public ScriptWorld {
public:
    World* world = nullptr;
    int sparkles = 0;  // fx.sparkle calls so far (a counted no-op in this slice)

    bool actorAlive(ActorRef a) override { return world->alive(a.index, a.serial); }
    const char* actorKind(ActorRef a) override {
        const Actor* actor = world->actor(a.index, a.serial);
        return actor ? world->kindName(*actor) : "";
    }
    float actorGet(ActorRef a, ActorField field) override {
        const Actor* actor = world->actor(a.index, a.serial);
        if (!actor) return 0;
        switch (field) {
            case ActorField::X: return actor->x;
            case ActorField::Y: return actor->y;
            case ActorField::Vx: return actor->vx;
            case ActorField::Vy: return actor->vy;
            case ActorField::OnGround: return actor->onGround() ? 1.0f : 0.0f;
        }
        return 0;
    }
    void actorSet(ActorRef a, ActorField field, float value) override {
        Actor* actor = world->actor(a.index, a.serial);
        if (!actor) return;
        switch (field) {
            case ActorField::X: World::set(*actor, &Actor::x, value); break;
            case ActorField::Y: World::set(*actor, &Actor::y, value); break;
            case ActorField::Vx: World::set(*actor, &Actor::vx, value); break;
            case ActorField::Vy: World::set(*actor, &Actor::vy, value); break;
            case ActorField::OnGround: break;
        }
    }
    void actorRemove(ActorRef a) override { world->remove(a.index, a.serial); }
    bool actorFind(const char* kind, ActorRef& out) override {
        int index = 0;
        std::uint32_t serial = 0;
        if (!world->find(kind, index, serial)) return false;
        out.index = index;
        out.serial = serial;
        return true;
    }
    void scoreAdd(int amount) override { world->addScore(amount); }
    int scoreGet() override { return world->score(); }
    void fxSparkle(float, float) override { ++sparkles; }
    void log(const char*) override {}
    std::uint32_t random() override { return world->random(); }
};

struct Engine::Impl {
    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    ~Impl() { delete script; }

    std::string error;
    Script* script = nullptr;  // null when the game has no rules
    WorldAdapter adapter;
    bool loaded = false;
    GameData game;
    World world;
    StageLayout layout;
    TileCache tiles;
    Camera camera;
    std::uint32_t ticks = 0;

    void updateCamera();
    void runRules();
    ActorProbe probeOf(int index) const;
};

void Engine::Impl::updateCamera() {
    const int p = world.playerIndex();
    if (p < 0) return;
    const Actor& a = world.pool().at(p);
    FollowRules rules;
    rules.view_w = layout.playfield.w;
    rules.view_h = layout.playfield.h;
    rules.world_w = world.grid().cols * BLOCK_PX;
    rules.world_h = world.grid().rows * BLOCK_PX;
    rules.dead_w = layout.stage.w / 4;
    rules.dead_h = layout.stage.h / 2;
    follow(camera, roundInt((a.x + a.w * 0.5f) * BLOCK_PX), roundInt((a.y + a.h * 0.5f) * BLOCK_PX),
           rules);
}

// ---- Lua rules -------------------------------------------------------------
// Step 3 of a tick (docs/formats/motion.md): for every overlapping pair of
// living actors, Script::touch(kind_a, kind_b, ...) in both orders (only when
// a handler listens), then Script::tick(). A script error stops the game.
void Engine::Impl::runRules() {
    if (script == nullptr) return;
    for (int i = 0; i < MAX_ACTORS && !script->failed(); ++i) {
        for (int j = i + 1; j < MAX_ACTORS && !script->failed(); ++j) {
            ActorPool& pool = world.pool();
            if (!pool.valid(i)) break;  // actor i was removed by a handler
            if (!pool.valid(j) || !actorsOverlap(pool.at(i), pool.at(j))) continue;
            const ActorRef a{i, pool.at(i).serial};
            const ActorRef b{j, pool.at(j).serial};
            const char* kind_a = world.kindName(pool.at(i));
            const char* kind_b = world.kindName(pool.at(j));
            if (script->wantsTouch(kind_a, kind_b) && world.alive(a.index, a.serial) &&
                world.alive(b.index, b.serial))
                script->touch(kind_a, kind_b, a, b);
            if (script->failed() || !world.alive(a.index, a.serial) ||
                !world.alive(b.index, b.serial))
                continue;
            if (script->wantsTouch(kind_b, kind_a)) script->touch(kind_b, kind_a, b, a);
        }
    }
    if (!script->failed()) script->tick();
    if (script->failed()) error = script->error();
}

ActorProbe Engine::Impl::probeOf(int index) const {
    const Actor& a = world.pool().at(index);
    ActorProbe probe;
    probe.index = index;
    probe.kind = world.kindName(a);
    probe.x = a.x;
    probe.y = a.y;
    probe.vx = a.vx;
    probe.vy = a.vy;
    probe.on_ground = a.onGround();
    return probe;
}

Engine::Engine() : impl_(new Impl) {}
Engine::~Engine() { delete impl_; }

bool Engine::load(const std::uint8_t* bundle, std::size_t size, int screen_w, int screen_h) {
    Impl fresh;
    const LoadError error = loadBundle(bundle, size, fresh.game);
    if (error != LoadError::None) {
        impl_->loaded = false;
        impl_->error = loadErrorText(error);
        return false;
    }
    if (screen_w <= 0 || screen_h <= 0) {
        impl_->loaded = false;
        impl_->error = "This screen size cannot be used.";
        return false;
    }
    StageRequest request;
    request.view = fresh.game.view;
    request.playfield_cols = fresh.game.playfield_cols;
    request.playfield_rows = fresh.game.playfield_rows;
    fresh.layout = layoutStage(screen_w, screen_h, BLOCK_PX, request);
    if (!fresh.tiles.build(fresh.game.sprites, fresh.game.tiles, fresh.game.palette, ART_SCALE, true) ||
        (fresh.tiles.count() > 0 && fresh.tiles.block() != BLOCK_PX)) {
        impl_->loaded = false;
        impl_->error = "This game's blocks could not be drawn.";
        return false;
    }
    // The world keeps pointers into the game, so it is started where the game will live.
    Impl& in = *impl_;
    in.game = std::move(fresh.game);
    in.layout = fresh.layout;
    in.tiles = std::move(fresh.tiles);
    in.camera = Camera{};
    in.ticks = 0;
    in.error.clear();
    delete in.script;
    in.script = nullptr;
    in.adapter = WorldAdapter{};
    in.loaded = in.world.start(in.game, 0);
    if (!in.loaded) {
        in.error = "This game has no level.";
        return false;
    }
    in.adapter.world = &in.world;
    if (!in.game.rules.empty()) {
        in.script = new Script;
        if (!in.script->load(in.game.rules.data(), in.game.rules.size(), in.adapter)) {
            in.error = in.script->error();
            in.loaded = false;
            return false;
        }
        in.script->start();
        if (in.script->failed()) in.error = in.script->error();
    }
    in.updateCamera();
    return true;
}

const char* Engine::error() const { return impl_->error.c_str(); }

void Engine::tick(const Input& input) {
    Impl& in = *impl_;
    if (!in.loaded || !in.error.empty()) return;
    in.world.tick(input);
    in.runRules();
    if (!in.error.empty()) return;
    in.updateCamera();
    ++in.ticks;
}

void Engine::draw(gfx::Canvas& canvas) const {
    const Impl& in = *impl_;
    canvas.resetClip();
    if (!in.loaded) {
        canvas.fill(0);
        return;
    }
    if (!in.error.empty()) {
        drawErrorScreen(canvas, in.game.palette, in.error.c_str());
        return;
    }
    const gfx::Palette& pal = in.game.palette;
    canvas.fill(pal.background);

    const gfx::Rect view = in.layout.playfield;
    const World& world = in.world;
    const Layer layer{world.grid().cols, world.grid().rows, world.grid().cells, 256};
    drawLayer(canvas, in.tiles, layer, view, in.camera.x, in.camera.y);

    canvas.setClip(view);
    for (int i = 0; i < MAX_ACTORS; ++i) {
        if (!world.pool().valid(i)) continue;
        const Actor& a = world.pool().at(i);
        const KindData& kind = world.kindOf(a);
        gfx::DrawOptions options;
        options.scale = ART_SCALE;
        // The player's art faces right as drawn, a walker's faces left.
        const bool art_faces_left = kind.behaviour == Behaviour::Walker && !a.isPlayer();
        options.flip_x = art_faces_left ? a.facing > 0 : a.facing < 0;
        gfx::drawSprite(canvas, in.game.sprites[static_cast<std::size_t>(kind.sprite)], pal,
                        view.x + roundInt(a.x * BLOCK_PX) - in.camera.x,
                        view.y + roundInt(a.y * BLOCK_PX) - in.camera.y, options);
    }
    canvas.resetClip();

    const gfx::Rect& status = in.layout.status;
    if (status.w > 0 && status.h > 0) {
        canvas.shade(status.x, status.y, status.w, status.h, STATUS_SHADE);
        char line[32];
        std::snprintf(line, sizeof(line), "SCORE %d", world.score());
        const int scale = status.h >= 40 ? 2 : 1;
        const int margin = status.h >= 40 ? 8 : 3;
        gfx::drawText(canvas, status.x + margin, status.y + margin, line, pal.ink, scale);
    }
}

std::uint32_t Engine::ticks() const { return impl_->ticks; }
int Engine::score() const { return impl_->loaded ? impl_->world.score() : 0; }
int Engine::hurts() const { return impl_->loaded ? impl_->world.hurts() : 0; }
int Engine::actorCount() const { return impl_->loaded ? impl_->world.pool().count() : 0; }

bool Engine::actor(int n, ActorProbe& out) const {
    if (!impl_->loaded || n < 0) return false;
    int seen = 0;
    for (int i = 0; i < MAX_ACTORS; ++i) {
        if (!impl_->world.pool().valid(i)) continue;
        if (seen++ == n) {
            out = impl_->probeOf(i);
            return true;
        }
    }
    return false;
}

bool Engine::player(ActorProbe& out) const {
    if (!impl_->loaded) return false;
    const int p = impl_->world.playerIndex();
    if (p < 0) return false;
    out = impl_->probeOf(p);
    return true;
}

std::size_t Engine::probeText(char* buffer, std::size_t capacity) const {
    if (buffer == nullptr || capacity == 0) return 0;
    buffer[0] = '\0';
    std::size_t used = 0;
    auto add = [&](const char* format, auto... args) {
        const int n = std::snprintf(buffer + used, capacity - used, format, args...);
        if (n < 0 || static_cast<std::size_t>(n) >= capacity - used) {
            buffer[used] = '\0';  // the line does not fit: leave it out whole
            return false;
        }
        used += static_cast<std::size_t>(n);
        return true;
    };
    auto num = [](float v) { return std::fabs(v) < 0.005f ? 0.0f : v; };  // never "-0.00"
    if (!add("tick %lu\n", static_cast<unsigned long>(impl_->ticks))) return used;
    if (!add("score %d\n", score())) return used;
    if (!add("hurts %d\n", hurts())) return used;
    if (!add("sparkles %d\n", impl_->adapter.sparkles)) return used;
    for (int i = 0; impl_->loaded && i < MAX_ACTORS; ++i) {
        if (!impl_->world.pool().valid(i)) continue;
        const ActorProbe p = impl_->probeOf(i);
        if (!add("actor %d %s x=%.2f y=%.2f vx=%.2f vy=%.2f ground=%d\n", p.index, p.kind,
                 num(p.x), num(p.y), num(p.vx), num(p.vy), p.on_ground ? 1 : 0))
            break;
    }
    return used;
}

}  // namespace tg
