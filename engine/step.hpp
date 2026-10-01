// Movement, growth and collision. Pure types and specializations.
#pragma once
#include "core.hpp"

namespace ts {

// ---- Direction table (screen coordinates: y grows downward) ----------------
template<Dir D> struct DirInfo;
template<> struct DirInfo<Dir::Up> {
    static constexpr int dx = 0, dy = -1;
    static constexpr Dir left = Dir::Left, right = Dir::Right, opposite = Dir::Down;
};
template<> struct DirInfo<Dir::Down> {
    static constexpr int dx = 0, dy = 1;
    static constexpr Dir left = Dir::Right, right = Dir::Left, opposite = Dir::Up;
};
template<> struct DirInfo<Dir::Left> {
    static constexpr int dx = -1, dy = 0;
    static constexpr Dir left = Dir::Down, right = Dir::Up, opposite = Dir::Right;
};
template<> struct DirInfo<Dir::Right> {
    static constexpr int dx = 1, dy = 0;
    static constexpr Dir left = Dir::Up, right = Dir::Down, opposite = Dir::Left;
};
template<Dir D> inline constexpr Dir TurnLeft  = DirInfo<D>::left;
template<Dir D> inline constexpr Dir TurnRight = DirInfo<D>::right;
template<Dir D> inline constexpr Dir Opposite  = DirInfo<D>::opposite;

// ---- Move / bounds ----------------------------------------------------------
template<class Pt, Dir D> struct MoveT;
template<int X, int Y, Dir D>
struct MoveT<P<X, Y>, D> { using type = P<X + DirInfo<D>::dx, Y + DirInfo<D>::dy>; };
template<class Pt, Dir D> using Move = typename MoveT<Pt, D>::type;

template<class Pt> inline constexpr bool InBounds = false;
template<int X, int Y>
inline constexpr bool InBounds<P<X, Y>> = (X >= 0 && X < W && Y >= 0 && Y < H);

// ---- One step of the snake --------------------------------------------------
// Result of moving Body one cell in direction D with food at FoodPos.
//   Dead : hit a wall or itself
//   Ate  : the new head is on the food (the snake grows: tail is kept)
//   Body : the new body (meaningless when Dead)
template<bool IsDead, bool DidEat, class NewBody> struct StepResult {
    static constexpr bool Dead = IsDead;
    static constexpr bool Ate  = DidEat;
    using Body = NewBody;
};

template<class Body, Dir D, class FoodPos> struct StepT {
    using NewHead = Move<Head<Body>, D>;
    static constexpr bool ate = Same<NewHead, FoodPos>;
    // When not growing the tail vacates its cell this tick, so it is not an obstacle.
    using Rest = If<ate, Body, PopBack<Body>>;
    static constexpr bool dead = !InBounds<NewHead> || Contains<NewHead, Rest>;
    using type = StepResult<dead, ate, PushFront<NewHead, Rest>>;
};
template<class Body, Dir D, class FoodPos> using Step = typename StepT<Body, D, FoodPos>::type;

} // namespace ts
