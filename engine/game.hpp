// Game state transitions, successors and the key table.
#pragma once
#include "core.hpp"
#include "step.hpp"
#include "rng.hpp"

namespace ts {

// ---- One tick of the game, heading in direction ND --------------------------
template<class Game_, Dir ND> struct NextT;
template<bool Dead, bool Ate, class R, Dir ND, int Fx, int Fy, class S, int Sc> struct NextSel;

template<Dir D, int Fx, int Fy, class S, int Sc, class Body, Dir ND>
struct NextT<Game<D, Food<Fx, Fy>, S, Score<Sc>, Body>, ND> {
    using R = Step<Body, ND, P<Fx, Fy>>;
    using type = typename NextSel<R::Dead, R::Ate, R, ND, Fx, Fy, S, Sc>::type;
};

// Died: the score stays as it was.
template<bool Ate, class R, Dir ND, int Fx, int Fy, class S, int Sc>
struct NextSel<true, Ate, R, ND, Fx, Fy, S, Sc> { using type = GameOver<Sc>; };

// Plain move: food, seed and score unchanged.
template<class R, Dir ND, int Fx, int Fy, class S, int Sc>
struct NextSel<false, false, R, ND, Fx, Fy, S, Sc> {
    using type = Game<ND, Food<Fx, Fy>, S, Score<Sc>, typename R::Body>;
};

// Ate: score +1, respawn food; a full board is a win.
template<bool Full, class Sp, class Body, Dir ND, int Sc> struct AteSel;
template<class Sp, class Body, Dir ND, int Sc>
struct AteSel<true, Sp, Body, ND, Sc> { using type = GameOver<Sc, true>; };
template<class Sp, class Body, Dir ND, int Sc>
struct AteSel<false, Sp, Body, ND, Sc> {
    using type = Game<ND, Food<Sp::Pos::x, Sp::Pos::y>, typename Sp::Seed, Score<Sc>, Body>;
};

template<class R, Dir ND, int Fx, int Fy, class S, int Sc>
struct NextSel<false, true, R, ND, Fx, Fy, S, Sc> {
    using Sp = SpawnFood<typename R::Body, S>;
    using type = typename AteSel<Sp::Full, Sp, typename R::Body, ND, Sc + 1>::type;
};

template<class Game_, Dir ND> using Next = typename NextT<Game_, ND>::type;

// ---- Successors: slot 0 = straight, 1 = turn left, 2 = turn right ----------
template<class S0, class S1, class S2> struct Succ3 {
    using Straight = S0;
    using Left = S1;
    using Right = S2;
};

template<class State> struct SuccessorsT;
template<Dir D, class F, class S, class Sc, class Body>
struct SuccessorsT<Game<D, F, S, Sc, Body>> {
    using G = Game<D, F, S, Sc, Body>;
    using type = Succ3<Next<G, D>, Next<G, TurnLeft<D>>, Next<G, TurnRight<D>>>;
};
template<class State> using Successors = typename SuccessorsT<State>::type;

template<int I, class Succ> struct SlotT;
template<class A, class B, class C> struct SlotT<0, Succ3<A, B, C>> { using type = A; };
template<class A, class B, class C> struct SlotT<1, Succ3<A, B, C>> { using type = B; };
template<class A, class B, class C> struct SlotT<2, Succ3<A, B, C>> { using type = C; };
template<int I, class Succ> using Slot = typename SlotT<I, Succ>::type;

// ---- Terminal states --------------------------------------------------------
template<class State> inline constexpr bool IsTerminal = false;
template<int Sc, bool Won> inline constexpr bool IsTerminal<GameOver<Sc, Won>> = true;

// ---- Key table --------------------------------------------------------------
// Inputs, in table order. The driver only maps raw bytes to these indices.
enum class Key { None = 0, Up = 1, Down = 2, Left = 3, Right = 4 };
inline constexpr int NumKeys = 5;

// Which slot pressing absolute direction K selects while heading D.
// Same direction and reverse both mean "straight".
template<Dir D, Dir K> inline constexpr int SlotForDir =
    K == TurnLeft<D> ? 1 : K == TurnRight<D> ? 2 : 0;

template<class State> struct KeyTable {
    // Terminal states have no successors; every input maps to slot 0.
    static constexpr int slots[NumKeys] = {0, 0, 0, 0, 0};
};
template<Dir D, class F, class S, class Sc, class Body>
struct KeyTable<Game<D, F, S, Sc, Body>> {
    static constexpr int slots[NumKeys] = {
        0,
        SlotForDir<D, Dir::Up>, SlotForDir<D, Dir::Down>,
        SlotForDir<D, Dir::Left>, SlotForDir<D, Dir::Right>,
    };
};

// ---- Initial state ----------------------------------------------------------
// Snake of length 3 heading right in the middle-left of the board; food from Seed<N>.
template<unsigned N> struct InitialT {
    using Body = Snake<P<W / 4 + 2, H / 2>, P<W / 4 + 1, H / 2>, P<W / 4, H / 2>>;
    // Advance once first: small seeds have all-zero high bits.
    using Sp = SpawnFood<Body, NextSeed<Seed<N>>>;
    using type = Game<Dir::Right, Food<Sp::Pos::x, Sp::Pos::y>, typename Sp::Seed, Score<0>, Body>;
};
template<unsigned N> using Initial = typename InitialT<N>::type;

} // namespace ts
