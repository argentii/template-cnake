# Template Snake

A real-time terminal Snake game in which **every game rule is computed by C++
templates at compile time**. The snake moves on a fixed tick, like classic
Snake. Between ticks, the game recompiles itself: the compiler is the CPU.

```
+--------------------------------+
|. . . . . . . . . . . . . . . . |
|. . . . . . . . * . . . . . . . |
|. . . . o o o @ . . . . . . . . |
|. . . . o . . . . . . . . . . . |
|. . . . . . . . . . . . . . . . |
+--------------------------------+
 Score: 2  Best: 7
```

## Requirements

- macOS or Linux
- `clang++` with C++20 support (Apple clang 17 and LLVM clang 15+ work)
- `make`

The engine uses the clang builtins `__type_pack_element` and
`__make_integer_seq`, so GCC is not supported.

## Quick start

```sh
make             # builds build/driver
build/driver     # play
```

Run the driver from the repository root, because it compiles `frame.cpp` and
`initial_state.hpp` from the current directory. It needs a real terminal, so
start it from your shell, not from a tool that pipes stdin.

**Controls:** arrow keys or WASD to steer, `p` to pause and resume, `q` or
Ctrl-C to quit. On the game-over screen, `r` starts a new game with a fresh
food sequence.

While paused, the clock stops but steering keys still register, so you can
line up your next move. Resuming gives you a full tick to react.

Eat the `*` to grow and score. Each point makes the snake a little faster
(250 ms per tick at the start, 10 ms faster per point, down to 120 ms). Hitting
a wall or yourself ends the game. Fill the whole board to win. Pressing the
direction opposite to the one you're heading in is ignored.

Your best score is saved in `tmp/best/best.hpp` and shown during play and on
the game-over screen. `make clean` keeps it; delete that file to reset it.

## Options

| Option | Meaning |
|---|---|
| `--tick MS` | Slowest tick the compiler needs (the floor). Without it, the driver measures this machine at startup (see below). The game uses its own speed unless it's faster than the floor. |
| `--calibrate` | Measure, print the floor the driver would pick, and exit. |
| `--grid WxH` | Board size, 3x2 up to 63x63 (default 16x12). |
| `--seed N` | Food sequence seed (default 1). |
| `--naive` | Compile the next state only after the tick ends. Slow and stuttery; for debugging. |
| `--script KEYS` | Non-interactive replay: one input per tick, `U` `D` `L` `R` or `.` for none. Prints every frame. Replays ignore and never change the saved best score. |
| `--no-delay` | With `--script`: don't wait for the tick clock, run as fast as compiles allow. |
| `--root DIR` | Directory holding `frame.cpp` and `initial_state.hpp` (default `.`). |
| `--log FILE` | Timing and stall log (default `tmp/driver.log`). |

Example replay:

```sh
build/driver --script "RRDDDDDDDD" --no-delay --seed 1
```

## How it works

### The state is a type

```cpp
Game<Dir::Right, Food<8,8>, Seed<1015568748u>, Score<0>,
     Snake<P<6,6>, P<5,6>, P<4,6>>>      // head first
GameOver<Score, Won, Seed<N>>             // the final seed seeds the next game
```

The rules are metafunctions over these types: `Step` (movement, growth,
collision), `SpawnFood` (a compile-time LCG with bounded retries, then a
row-bitmask scan for a free cell), `Next`, and `Successors<S>`, which holds the
3 possible next states: straight, turn left and turn right. `TickMs<S>` is the
game's speed for a state, and `NewBest<S, Best<N>>` is the best score once `S`
is reached.

`KeyTable<S>` maps each input (none, Up, Down, Left, Right) to one of those 3
slots. The mapping depends on the snake's heading, so it's computed by the
templates too. Pressing the reverse direction maps to "straight".

### One binary per state

`frame.cpp` includes a generated `state.hpp` containing `using State = ...;`.
Compiled against it, it produces a binary `B(S)` whose outputs are all string
constants baked in at compile time:

| Command | Output |
|---|---|
| `B frame` | the rendered board and score |
| `B info` | the key table (`keys 0 1 2 0 0`), `terminal 0` or `1`, and the game's `tick` in ms |
| `B emit DIR` | `succ0.hpp`, `succ1.hpp`, `succ2.hpp`: the three successor states as source |
| `B best FILE` | the best-score header (`using BestScore = ts::Best<N>;`) once this state is reached |
| `B restart FILE` | game over only: `state.hpp` for the next game's first state, seeded from this game's final seed |

Each compile also includes `tmp/best/best.hpp` if it exists, so the board and
the game-over screen can show the best score.

### The tick loop (speculative compilation)

```
tick n:  B(S) is already built and warmed up
  1. B frame         -> draw the screen
  2. B info          -> key table, terminal flag, tick length
                        (terminal: show game over, B best, exit)
  3. B emit tmp/...  -> 3 successor headers
  4. compile all 3 successors in parallel, each in its own directory
  5. record the last key pressed during the tick
end of tick:
  6. slot = keys[last_key]
  7. wait for that compile if needed (logged as a stall); kill the other two
  8. B(successor) becomes the current binary
```

Compiles run one tick ahead, so the player never waits for the compiler
unless a compile is slower than a tick.

### The purity rule

All game logic lives in types and template specializations. The driver
(`driver/driver.cpp`) only:

- reads raw keyboard bytes and decodes them into one of 5 input indices, and
  keeps time;
- spawns, waits for and kills `clang++` and the binaries it produces;
- looks up `keys[input]` in the table `B info` printed, and stops when it says
  `terminal 1`;
- waits for the tick length `B info` printed, or longer if compiles need it
  (choosing the slower of two durations is timing, not game logic);
- at game over, runs `B best` and moves the file it wrote into place. The
  binary decided whether the score is a new best;
- on `r` at the game-over screen, runs `B restart` and compiles the state it
  wrote. The binary picked the next game's seed;
- prints the frame text and copies emitted headers into build directories.

It never decides movement, growth, collisions, food placement, randomness,
score, the game's speed, the best score, what a key means relative to the
snake, or what the board looks like.
`constexpr` functions appear only where finished type-level results are turned
into characters (`text.hpp`, `render.hpp`, `serialize.hpp`).

### Layout

```
engine/core.hpp       state types, list utilities (O(1) template depth)
engine/step.hpp       movement, growth, wall and self collision
engine/rng.hpp        LCG seed, food spawning
engine/game.hpp       Next, Successors, KeyTable, TickMs, NewBest, Initial, Restart
engine/text.hpp       constexpr strings, int-to-chars, concat
engine/render.hpp     state -> board text
engine/serialize.hpp  state -> state.hpp source text
frame.cpp             B(S): argument dispatch only
initial_state.hpp     tick 0 state (seeded by -DTS_SEED)
driver/driver.cpp     runtime shell: input, timing, compile pipeline
tests/                static tests, serialize round trip, golden replays
PERF.md               measurements
```

The grid size (default 16x12) is set in `engine/core.hpp` and can be
overridden with `-DTS_GRID_W` / `-DTS_GRID_H`.

## Tests

```sh
make test
```

CI (`.github/workflows/ci.yml`) runs the same suite on Ubuntu and macOS on
every push, runs the golden replays against the naive driver too, and prints
each runner's measured tick floor.

- **Static tests** (`tests/static_tests.cpp`): `static_assert`s for every
  metafunction. If the file compiles, they pass.
- **Serialize round trip**: states are written out as headers, compiled back
  in and checked with `Same<...>` against the original types.
- **Golden replays** (`tests/golden/*.case`): scripted games diffed against
  recorded output. Run them against the naive driver with
  `make golden DRIVER_ARGS=--naive`, and re-record with `make golden-update`.

## Performance

A tick's compile and link takes about 43 ms at snake length 3 and 64 ms at
length 100 on an Apple M4.

**Tick floor.** The game asks for 250 ms ticks, speeding up as you score, but a
tick can't be shorter than three compiles. Unless you pass `--tick`, the driver
measures that floor at startup: three compiles of the initial state in
parallel, each followed by its first run. It does this twice, takes the slower
round, adds 10% plus 25 ms for the per-tick bookkeeping, and rounds up to 10 ms
(capped at 1000 ms). Each tick then lasts the game's requested time or the
floor, whichever is longer. Measuring adds about a second to startup, and one
of the compiles becomes the first frame. Run `build/driver --calibrate` to see
the floor.

**macOS:** macOS scans every newly built executable on its first run, one at a
time (about 107 ms each, more under load). Three new binaries per tick costs
about 400 ms, so the floor on an M4 comes out around 480–500 ms. That's slower
than the game's own speed, so on macOS the speed-up isn't visible by default.
The driver runs each fresh binary once during the tick to hide that cost. If
you add your terminal app under System Settings → Privacy & Security →
Developer Tools, the scan goes away, the floor should drop below the game's
speed, and the speed-up becomes visible.

Stalls (ticks where the chosen compile wasn't ready) are logged to
`tmp/driver.log`. Full numbers are in `PERF.md`.

## Troubleshooting

- **"stdin is not a terminal"**: run `build/driver` directly in a terminal,
  or use `--script` for non-interactive runs.
- **Compile errors about `frame.cpp` not found**: run from the repository root
  or pass `--root`.
- **The snake pauses now and then**: check `tmp/driver.log` for `STALL` lines
  and try a longer `--tick`. The measured tick reflects the machine at
  startup; heavy load later in the game can still cause stalls.
- **Leftover `tmp/run-*` directories** after a crash or Ctrl-C are removed the
  next time the driver starts.
