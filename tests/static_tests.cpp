// Compile-time tests. If this file compiles (-fsyntax-only), every test passed.
#include "../engine/core.hpp"
#include "../engine/step.hpp"
#include "../engine/rng.hpp"

namespace core_tests {
using namespace ts;

using S3 = Snake<P<5,5>, P<4,5>, P<3,5>>;
using S1 = Snake<P<0,0>>;
using S0 = Snake<>;

// Same / If
static_assert(Same<P<1,2>, P<1,2>>);
static_assert(!Same<P<1,2>, P<2,1>>);
static_assert(!Same<P<1,1>, P<1,2>>);
static_assert(Same<If<true, int, char>, int>);
static_assert(Same<If<false, int, char>, char>);

// Negative coordinates are representable
static_assert(P<-1, 3>::x == -1);
static_assert(!Same<P<-1,0>, P<0,-1>>);

// Contains
static_assert(Contains<P<4,5>, S3>);
static_assert(Contains<P<5,5>, S3>);
static_assert(Contains<P<3,5>, S3>);
static_assert(!Contains<P<5,4>, S3>);
static_assert(!Contains<P<0,0>, S0>);
static_assert(Contains<P<0,0>, S1>);

// Length
static_assert(Length<S0> == 0);
static_assert(Length<S1> == 1);
static_assert(Length<S3> == 3);

// PushFront
static_assert(Same<PushFront<P<6,5>, S3>, Snake<P<6,5>, P<5,5>, P<4,5>, P<3,5>>>);
static_assert(Same<PushFront<P<0,0>, S0>, S1>);

// At / Head / Last
static_assert(Same<At<0, S3>, P<5,5>>);
static_assert(Same<At<1, S3>, P<4,5>>);
static_assert(Same<At<2, S3>, P<3,5>>);
static_assert(Same<Head<S3>, P<5,5>>);
static_assert(Same<Last<S3>, P<3,5>>);
static_assert(Same<Head<S1>, Last<S1>>);

// Take / PopBack
static_assert(Same<Take<0, S3>, S0>);
static_assert(Same<Take<2, S3>, Snake<P<5,5>, P<4,5>>>);
static_assert(Same<PopBack<S3>, Snake<P<5,5>, P<4,5>>>);
static_assert(Same<PopBack<S1>, S0>);
static_assert(Same<PopBack<PopBack<S3>>, Snake<P<5,5>>>);
} // namespace core_tests

namespace step_tests {
using namespace ts;
using NoFood = P<-100, -100>;

// Direction table is consistent
static_assert(TurnLeft<TurnRight<Dir::Up>> == Dir::Up);
static_assert(TurnLeft<TurnLeft<Dir::Right>> == Opposite<Dir::Right>);
static_assert(TurnRight<Dir::Up> == Dir::Right && TurnLeft<Dir::Up> == Dir::Left);
static_assert(TurnRight<Dir::Right> == Dir::Down && TurnLeft<Dir::Right> == Dir::Up);
static_assert(Opposite<Opposite<Dir::Left>> == Dir::Left);

// Move
static_assert(Same<Move<P<3,3>, Dir::Up>,    P<3,2>>);
static_assert(Same<Move<P<3,3>, Dir::Down>,  P<3,4>>);
static_assert(Same<Move<P<3,3>, Dir::Left>,  P<2,3>>);
static_assert(Same<Move<P<3,3>, Dir::Right>, P<4,3>>);
static_assert(Same<Move<P<0,0>, Dir::Left>,  P<-1,0>>);

// Bounds
static_assert(InBounds<P<0,0>> && InBounds<P<W-1,H-1>>);
static_assert(!InBounds<P<-1,0>> && !InBounds<P<0,-1>>);
static_assert(!InBounds<P<W,0>> && !InBounds<P<0,H>>);

// Plain move: head advances, tail drops
using S = Snake<P<5,5>, P<4,5>, P<3,5>>;
using R1 = Step<S, Dir::Right, NoFood>;
static_assert(!R1::Dead && !R1::Ate);
static_assert(Same<R1::Body, Snake<P<6,5>, P<5,5>, P<4,5>>>);
using R2 = Step<S, Dir::Up, NoFood>;
static_assert(!R2::Dead && Same<R2::Body, Snake<P<5,4>, P<5,5>, P<4,5>>>);

// Growth: eating keeps the tail
using R3 = Step<S, Dir::Right, P<6,5>>;
static_assert(!R3::Dead && R3::Ate);
static_assert(Same<R3::Body, Snake<P<6,5>, P<5,5>, P<4,5>, P<3,5>>>);
static_assert(Length<R3::Body> == 4);

// Wall hits on all four sides
static_assert(Step<Snake<P<0,3>, P<1,3>>, Dir::Left, NoFood>::Dead);
static_assert(Step<Snake<P<W-1,3>, P<W-2,3>>, Dir::Right, NoFood>::Dead);
static_assert(Step<Snake<P<3,0>, P<3,1>>, Dir::Up, NoFood>::Dead);
static_assert(Step<Snake<P<3,H-1>, P<3,H-2>>, Dir::Down, NoFood>::Dead);
static_assert(!Step<Snake<P<1,3>, P<2,3>>, Dir::Left, NoFood>::Dead);

// Reversing into the neck is a self hit
static_assert(Step<S, Dir::Left, NoFood>::Dead);

// Self hit: 2x2 loop of length 5, head turning into its own body
//   (2,2)H (3,2)
//   (2,3)  (3,3)   body: H=(2,2) -> (2,3) -> (3,3) -> (3,2)... plus tail (4,2)
using Coil = Snake<P<2,2>, P<2,3>, P<3,3>, P<3,2>, P<4,2>>;
static_assert(Step<Coil, Dir::Right, NoFood>::Dead);          // into (3,2), not the tail
static_assert(!Step<Coil, Dir::Up, NoFood>::Dead);

// Moving into the cell the tail is vacating is legal when not growing...
//   square cycle of 4: head (1,1), tail (1,2); moving Down hits the old tail cell
using Ring = Snake<P<1,1>, P<2,1>, P<2,2>, P<1,2>>;
using R4 = Step<Ring, Dir::Down, NoFood>;
static_assert(!R4::Dead);
static_assert(Same<R4::Body, Snake<P<1,2>, P<1,1>, P<2,1>, P<2,2>>>);
// ...but deadly when the tail stays because the snake is growing.
// (Food can never be on the snake in a real game; this pins down the rule.)
static_assert(Step<Ring, Dir::Down, P<1,2>>::Dead);

// Length-1 snake can go any direction (no neck)
static_assert(!Step<Snake<P<4,4>>, Dir::Left, NoFood>::Dead);
static_assert(Same<Step<Snake<P<4,4>>, Dir::Left, NoFood>::Body, Snake<P<3,4>>>);
} // namespace step_tests

namespace rng_tests {
using namespace ts;

// Cell index -> point, row-major
template<int I> using CellAt = P<I % W, I / W>;
// A body covering cells [0, N) except index Skip (Skip = -1: skip nothing).
template<int Skip, class Seq> struct CoverImpl;
template<int Skip, int... I>
struct CoverImpl<Skip, ISeq<int, I...>> {
    using type = Snake<CellAt<(Skip >= 0 && I >= Skip) ? I + 1 : I>...>;
};
template<int N, int Skip = -1> using Cover = typename CoverImpl<Skip, MakeSeq<N>>::type;

// LCG
static_assert(Same<NextSeed<Seed<0>>, Seed<1013904223u>>);
static_assert(Same<NextSeed<Seed<1>>, Seed<1664525u + 1013904223u>>);
static_assert(!Same<NextSeed<Seed<42>>, Seed<42>>);

// Seed cells are always on the board
static_assert(InBounds<SeedCell<Seed<0>>>);
static_assert(InBounds<SeedCell<Seed<0xFFFFFFFFu>>>);
static_assert(InBounds<SeedCell<NextSeed<NextSeed<Seed<7>>>>>);

// On an empty-ish board the first try wins and the seed advances once
using Small = Snake<P<0,0>>;
using Sp1 = SpawnFood<Snake<P<-5,-5>>, Seed<7>>;
static_assert(!Sp1::Full);
static_assert(Same<Sp1::Pos, SeedCell<Seed<7>>>);
static_assert(Same<Sp1::Seed, NextSeed<Seed<7>>>);

// Food never spawns on the snake: put the snake exactly where the seed points
using Blocker = Snake<SeedCell<Seed<7>>>;
using Sp2 = SpawnFood<Blocker, Seed<7>>;
static_assert(!Sp2::Full && !Contains<Sp2::Pos, Blocker>);
static_assert(InBounds<Sp2::Pos>);

// Deterministic scan: row 0 full plus 3 cells of row 1 -> first free is (3,1)
using Partial = Cover<W + 3>;
static_assert(Length<Partial> == W + 3);
using Sc1 = ScanFree<Partial, Seed<9>>::type;
static_assert(!Sc1::Full && Same<Sc1::Pos, P<3,1>>);
static_assert(Same<Sc1::Seed, Seed<9>>);
static_assert(Same<ScanFree<Small, Seed<1>>::type::Pos, P<1,0>>);
static_assert(Same<ScanFree<Snake<P<5,5>>, Seed<1>>::type::Pos, P<0,0>>);

// Out of retries -> falls back to the scan
static_assert(Same<TrySpawn<Partial, Seed<9>, 0>::type, Sc1>);

// Board with exactly one free cell: spawn must find it (random tries ~always miss)
constexpr int Hole = W * H / 2 + 3;
using OneFree = Cover<W * H - 1, Hole>;
static_assert(!Contains<CellAt<Hole>, OneFree>);
using Sp3 = SpawnFood<OneFree, Seed<12345>>;
static_assert(!Sp3::Full && Same<Sp3::Pos, CellAt<Hole>>);
// Last cell of the board
using Sp4 = SpawnFood<Cover<W * H - 1>, Seed<3>>;
static_assert(!Sp4::Full && Same<Sp4::Pos, P<W-1, H-1>>);

// Full board: Full is reported, no infinite recursion
using Sp5 = SpawnFood<Cover<W * H>, Seed<3>>;
static_assert(Sp5::Full);
} // namespace rng_tests

int main() {}
