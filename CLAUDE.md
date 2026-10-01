# Template Snake: instructions for Claude Code

## Goal

Build a **real-time** (not turn-based) terminal Snake game where **all game logic is computed by C++ templates at compile time**. The snake moves on a fixed tick whether or not the player presses anything, exactly like classic Snake.

Because templates only run at compile time, the game works by **recompiling once per tick**. The compiler is the CPU. A small runtime driver handles the keyboard, the clock, and launching the compiler, and it is forbidden from doing game logic.

## The purity rule (non-negotiable)

Game logic lives **only** in templates (types, specializations, alias templates, fold expressions over packs). The runtime code is limited to:

- reading raw keyboard input and keeping time
- spawning/killing compiler processes and running the binaries they produce
- **looking up** a value in a table the template engine produced (e.g. `slot = table[key]`)
- printing a precomputed frame string and writing a precomputed header string to disk

The runtime must never compute or decide any of the following:

- movement or growth
- collisions or game over
- food placement or randomness
- score
- which direction a key means relative to the snake
- what the board looks like

If you catch yourself writing `if` logic about the snake in runtime code, stop and move it into a metafunction that bakes the answer into a table.

`constexpr` functions are allowed **only** for turning finished type-level results into character arrays (rendering and serialization output). The game rules themselves must be types and specializations, not `constexpr` functions.

## Architecture

```
template-snake/
  CLAUDE.md
  engine/            # pure template metaprogramming, header-only
    core.hpp         # P<X,Y>, Dir, Snake<...>, type-list utilities
    step.hpp         # Move, PopBack, Step, Contains, collision
    rng.hpp          # compile-time LCG, food spawning
    render.hpp       # state -> constexpr char frame
    serialize.hpp    # state -> constexpr char source text ("using State = ...;")
    game.hpp         # Game<...> state type, GameOver<Score>, Successors<State>
  frame.cpp          # includes "state.hpp" (generated), exposes results
  driver/driver.cpp  # runtime shell: input, timing, compile pipeline
  tests/static_tests.cpp
  initial_state.hpp
  Makefile (or CMakeLists.txt)
```

### State as a type

```cpp
template<int X, int Y> struct P {};
template<class... Ps> struct Snake {};            // head first
enum class Dir { Up, Down, Left, Right };
template<int X, int Y> struct Food {};
template<unsigned N>   struct Seed {};
template<int N>        struct Score {};

template<Dir D, class F, class S, class Sc, class Body> struct Game {};
template<int FinalScore> struct GameOver {};
```

The grid size is a compile-time constant (default 16x12) defined in one place.

### Successors and the key table

For a live state `S`, the engine computes `Successors<S>`. This holds exactly **3** candidate next states, indexed as slots 0 = straight, 1 = turn left, 2 = turn right. Any of them may be a `GameOver<Score>`.

The engine also computes a **key table**: a `constexpr` array mapping each input to a slot. The inputs are none, Up, Down, Left, and Right. Pressing the reverse direction maps to straight, and no input also maps to straight. Because this mapping depends on the current direction, it is game logic, so it must come from templates.

### The frame binary `B(S)`

`frame.cpp` includes the generated `state.hpp`, which defines `using State = ...;`. Compiled against it, the resulting binary supports these modes:

| Command | Output |
|---|---|
| `B frame` | Prints the rendered board and score for `S` itself. |
| `B info` | Prints the 5-entry key table and whether `S` is terminal. Use a simple line format such as `keys 0 0 0 1 2` and `terminal 0`. |
| `B emit DIR` | Writes `DIR/succ0.hpp`, `succ1.hpp`, and `succ2.hpp`, each a complete `state.hpp` for that successor. |

All three outputs are string literals or arrays baked into the binary at compile time. The binary's `main` is just argument dispatch.

### The driver pipeline (speculative compilation)

The driver hides compile latency by always compiling one tick ahead:

```
tick n starts:  state S is current; B(S) already built
  1. run `B(S) frame`   -> draw screen
  2. run `B(S) info`    -> key table + terminal flag (if terminal: show game over, exit)
  3. run `B(S) emit tmp/n/` -> 3 successor headers
  4. spawn 3 compiles in parallel: B(succ0), B(succ1), B(succ2)
  5. during the tick, record the LAST key pressed (non-blocking input)
tick n ends:
  6. slot = keytable[last_key]
  7. wait for compile `slot` if it isn't finished yet; kill or ignore the other two
  8. B(succ_slot) becomes current -> next tick
```

A few practical details:

- If a compile is still running when the tick ends, the driver waits for it. That is a visible stall, so log it.
- Use a separate build directory per slot so parallel compiles don't clash.
- Use termios raw mode plus `poll()` for input. Restore the terminal on exit and on SIGINT.
- Target Linux and macOS (POSIX `fork`/`exec`/`waitpid`).
- Make the tick length configurable; the default is 200 ms.

## Build order

Work through these milestones in order. Do not start a milestone until the previous one passes its checks. Commit after each one.

0. **Toolchain + baseline.** Use `clang++ -std=c++20` (prefer clang because it compiles faster). Measure how long it takes to compile an empty `frame.cpp` (`time` it 10 times). This is the floor of your per-tick budget. Report it.
1. **Core types + list utilities** (`core.hpp`): `Contains`, `PushFront`, `PopBack`, `Length`, `Head`, `At<I>`. Every metafunction gets `static_assert` tests in `tests/static_tests.cpp`.
2. **Movement + growth + collision** (`step.hpp`). Covers wall hits, self hits, and moving into the cell the tail is vacating, which is legal when not growing. Test all of these with `static_assert`s.
3. **RNG + food** (`rng.hpp`). Use an LCG on `Seed<N>`. Food must never spawn on the snake. Use a bounded retry, then fall back to a deterministic scan for the first free cell, so the compiler never recurses without limit. Also handle the board-full case, which counts as a win or game over.
4. **Successors + key table** (`game.hpp`), with tests for reverse-key handling.
5. **Render** (`render.hpp`). Produce a `constexpr std::array<char, N>` of the board. Write a test that compares a known state to an expected string.
6. **Serialize** (`serialize.hpp`). This requires constexpr int-to-chars and concatenation of arrays. Add a round-trip test: serialize a state, compile it as a header, and check it with `std::is_same_v` against the original.
7. **Frame binary** (`frame.cpp`). Verify all three modes by hand on `initial_state.hpp`.
8. **Naive driver.** Compile synchronously after each tick ends. It will be slow and stuttery, but proves correctness end to end.
9. **Speculative driver.** Add the parallel pipeline described above.
10. **Performance pass.** See the performance section below.

## Testing

- `make test` compiles `tests/static_tests.cpp` with `-fsyntax-only`. If it compiles, the tests passed. Every new metafunction ships with tests.
- **Deterministic replay mode.** `driver --script "RRUUL..." --no-delay --seed N` feeds scripted inputs (one per tick, `.` meaning no input), runs as fast as compiles allow, and prints every frame. Use this for golden tests: save the output for a script, then diff against it later.
- Never claim a milestone is done without running its tests.

## Performance

Compile time per tick is the whole game. Measure it constantly.

- Use `clang++ -ftime-trace` to find expensive instantiations.
- Prefer fold expressions and `std::index_sequence` expansion over deep recursion. Keep recursion depth O(1) or O(log n) in snake length where possible.
- Avoid instantiating anything proportional to grid area more than once per compile. The renderer is the main risk here: generate cells in one pack expansion.
- Keep `frame.cpp`'s includes minimal: no `<iostream>`, use `<cstdio>`, and avoid heavy standard headers in the engine.
- Consider a precompiled header for the engine (`-include-pch`), since only `state.hpp` changes between ticks.
- Use `-O0`. The binary does almost nothing at runtime, so optimization only costs compile time.
- Track and report compile time at snake lengths 3, 20, 50, and 100. The goal is that a single compile stays under the tick length on the user's machine. If it doesn't, the levers in order are: PCH, a smaller grid, a longer tick, and then algorithmic fixes in the engine.

## Pitfalls to watch for

- **Template depth limits.** If you need `-ftemplate-depth=...`, the algorithm is probably too recursive. Fix the algorithm instead of raising the limit, unless it's a small bump.
- **Partial-specialization ambiguity.** Patterns like `P<X,Y>` vs `P<X,X>` can overlap and become ambiguous. Keep specializations simple and test edge cases.
- **Signed coordinates.** Moving off the left or top edge produces `-1`. Make sure `P<-1, Y>` is representable and caught by the bounds check.
- **Successor header naming.** Every generated header must define the same alias name (`State`) so `frame.cpp` stays unchanged.
- **Terminal restore.** Always restore cooked terminal mode, including on crash or Ctrl-C.
- **Stale binaries.** Clean up per-tick temp directories so the disk doesn't fill up during a long game.

## Definition of done

- The game runs in a terminal at a steady tick, with arrow keys or WASD, food, growth, score, wall and self collision, and a game-over screen.
- No visible stalls at normal snake lengths on the user's machine. Stalls are logged if they happen.
- `make test` passes, and replay golden tests pass.
- A short README explains the architecture and the purity rule, so it's clear the runtime does no game logic.
