# Lua rules API (first slice)

`rules.lua` holds only what the stock parts cannot express. Positions are in
blocks; x grows to the right and y grows downward; an actor's x, y is its
top-left corner.

The script runs once at load (to register handlers), then the engine calls the
handlers. Standard libraries available: base (without `load`, `loadstring`,
`dofile`, `loadfile`, `require`, `collectgarbage`), `math`, `string`, `table`.
`math.random` and `math.randomseed` use the engine's seeded generator.

| Call | Meaning |
|---|---|
| `on.start(fn)` | `fn()` runs once when a level starts |
| `on.tick(fn)` | `fn()` runs every tick (60 per second) |
| `on.touch(a, b, fn)` | `fn(actor_a, actor_b)` runs on each tick in which an actor of kind `a` overlaps one of kind `b` |
| `actor.find(kind)` | the first living actor of that kind, or `nil` |
| `score.add(n)`, `score.get()` | change or read the score |
| `fx.sparkle(x, y)` | a sparkle effect (a counted no-op in this slice) |
| `log(text)` | write a line to the game log |

Actor objects: read `a.kind`, `a.x`, `a.y`, `a.vx`, `a.vy`, `a.on_ground`,
`a.alive`; write `a.x`, `a.y`, `a.vx`, `a.vy`; call `a:remove()`. Using an
actor after it was removed is harmless: reads give `nil` (`alive` gives
`false`) and writes do nothing.

Order within a tick: input and movement for every actor, then stock touch
handling (hurt, pickup), then Lua `on.touch` handlers, then `on.tick`.

Limits: 1 MB of memory per game and 200,000 VM instructions per engine call.
Going over either stops the game with an error screen; so does any Lua error.
