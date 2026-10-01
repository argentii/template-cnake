# Template Snake

Real-time terminal Snake in which **every game rule is computed by C++ templates
at compile time**. The game recompiles once per tick: the compiler is the CPU.

```
make            # builds build/driver
build/driver    # play: arrows or WASD, q quits
make test       # static tests, serialize round trip, golden replays
```

Options: `--tick MS` (default 200 ms, 450 ms on macOS; see below), `--seed N`,
`--naive` (compile only after the tick ends), `--script KEYS --no-delay`
(deterministic replay: one of `U D L R .` per tick, prints every frame).

## How it works

The game state is a type:

```cpp
Game<Dir::Right, Food<8,8>, Seed<1015568748u>, Score<0>,
     Snake<P<6,6>, P<5,6>, P<4,6>>>
```

`frame.cpp` includes a generated `state.hpp` (`using State = ...;`) and is
compiled into a binary `B(S)` whose outputs were all fixed at compile time:

| Command | Output |
|---|---|
| `B frame` | the rendered board and score |
| `B info` | the key table (`keys 0 1 2 0 0`) and `terminal 0/1` |
| `B emit DIR` | `succ0.hpp` (straight), `succ1.hpp` (left), `succ2.hpp` (right) |

Each tick, the driver shows the frame, starts compiling all three successors in
parallel, records the last key pressed, looks up `slot = keys[key]`, keeps that
compile and kills the other two.

```
engine/core.hpp       state types, O(1)-depth list utilities (pack expansion, clang builtins)
engine/step.hpp       movement, growth, wall/self collision
engine/rng.hpp        LCG seed, food spawn (8 random tries, then a row-bitmask scan)
engine/game.hpp       Next, Successors, KeyTable, Initial
engine/text.hpp       constexpr Str<N>, int-to-chars, concat
engine/render.hpp     state -> board text
engine/serialize.hpp  state -> state.hpp source text
frame.cpp             B(S): argument dispatch only
driver/driver.cpp     runtime shell
```

## The purity rule

Game logic lives only in types and specializations. The driver
(`driver/driver.cpp`) only:

- reads raw keyboard bytes and decodes them into one of five input indices
  (none, Up, Down, Left, Right), and keeps time;
- spawns, waits for and kills `clang++` and the binaries it produces;
- looks up `keys[input]` from the table `B info` printed, and stops when it
  says `terminal 1`;
- prints the frame text and copies the emitted headers into build directories.

It never decides movement, growth, collisions, food, randomness, score, what a
key means relative to the snake, or what the board looks like. Reversing into
yourself, for example, is turned into "straight" by `KeyTable` in
`engine/game.hpp`, not by the driver. `constexpr` functions are used only to
turn finished type-level results into characters (`text.hpp`, `render.hpp`,
`serialize.hpp`).

## macOS note

macOS scans every newly built executable on its first run, one at a time
(~107 ms each). With three binaries per tick that is ~330 ms, which is why the
macOS default tick is 450 ms. The driver runs each fresh binary once during the
tick so the scan is hidden behind it. Adding your terminal app under System
Settings → Privacy & Security → Developer Tools removes the scan; then
`--tick 200` works. Measurements are in `PERF.md`; stalls are logged to
`tmp/driver.log`.
