#pragma once

// The Lua rules host (docs/formats/lua-api.md). The script never touches the
// engine directly: everything goes through ScriptWorld, so the host can be
// tested without a world and the world without Lua.

#include <cstddef>
#include <cstdint>

namespace tg {

// A handle that stays safe after the actor is removed or its slot reused.
struct ActorRef {
    int index = -1;
    std::uint32_t serial = 0;
};

enum class ActorField { X, Y, Vx, Vy, OnGround };

class ScriptWorld {
public:
    virtual ~ScriptWorld() = default;
    virtual bool actorAlive(ActorRef actor) = 0;
    virtual const char* actorKind(ActorRef actor) = 0;            // "" if not alive
    virtual float actorGet(ActorRef actor, ActorField field) = 0;
    virtual void actorSet(ActorRef actor, ActorField field, float value) = 0;
    virtual void actorRemove(ActorRef actor) = 0;
    virtual bool actorFind(const char* kind, ActorRef& out) = 0;
    virtual void scoreAdd(int amount) = 0;
    virtual int scoreGet() = 0;
    virtual void fxSparkle(float x, float y) = 0;
    virtual void log(const char* text) = 0;
    virtual std::uint32_t random() = 0;                           // seeded source
};

class Script {
public:
    static constexpr std::size_t MEMORY_LIMIT = 1024 * 1024;
    static constexpr int INSTRUCTION_BUDGET = 200000;  // per call into Lua

    Script();
    ~Script();
    Script(const Script&) = delete;
    Script& operator=(const Script&) = delete;

    // Runs the source once so it can register handlers. `world` must outlive
    // the Script. False on a syntax or runtime error.
    bool load(const char* source, std::size_t length, ScriptWorld& world);
    void start();   // on.start handlers
    void tick();    // on.tick handlers
    // Runs on.touch handlers registered for (kind_a, kind_b). The engine calls
    // it once per overlapping pair per tick, with the kinds in both orders.
    void touch(const char* kind_a, const char* kind_b, ActorRef a, ActorRef b);
    // True when any handler is registered for this ordered pair (lets the
    // engine skip pairs nobody listens to).
    bool wantsTouch(const char* kind_a, const char* kind_b) const;

    // After any error (syntax, runtime, memory limit, instruction budget) the
    // script is failed: every call above becomes a no-op and error() explains.
    bool failed() const;
    const char* error() const;

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace tg
