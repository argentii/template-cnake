// Compile-time tests. If this file compiles (-fsyntax-only), every test passed.
#include "../engine/core.hpp"
#include "../engine/step.hpp"
#include "../engine/rng.hpp"
#include "../engine/game.hpp"
#include "../engine/render.hpp"
#include "../engine/serialize.hpp"

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

namespace game_tests {
using namespace ts;

using Body = Snake<P<5,5>, P<4,5>, P<3,5>>;
template<Dir D, int Fx, int Fy, int Sc = 0, class B = Body>
using G = Game<D, Food<Fx, Fy>, Seed<77>, Score<Sc>, B>;

// Plain move keeps food/seed/score
static_assert(Same<Next<G<Dir::Right, 0, 0>, Dir::Right>,
                   Game<Dir::Right, Food<0,0>, Seed<77>, Score<0>, Snake<P<6,5>, P<5,5>, P<4,5>>>>);

// Eating: score +1, body grows, food moves off the snake, seed advances
using Ate = Next<G<Dir::Right, 6, 5, 4>, Dir::Right>;
template<class T> struct Parts;
template<Dir D, int Fx, int Fy, class S, int Sc, class B>
struct Parts<Game<D, Food<Fx, Fy>, S, Score<Sc>, B>> {
    static constexpr Dir dir = D;
    using FoodP = P<Fx, Fy>;
    using Seed_ = S;
    static constexpr int score = Sc;
    using Body_ = B;
};
static_assert(Parts<Ate>::score == 5);
static_assert(Same<Parts<Ate>::Body_, Snake<P<6,5>, P<5,5>, P<4,5>, P<3,5>>>);
static_assert(!Contains<Parts<Ate>::FoodP, Parts<Ate>::Body_>);
static_assert(InBounds<Parts<Ate>::FoodP>);
static_assert(!Same<Parts<Ate>::Seed_, Seed<77>>);

// Death keeps the score
static_assert(Same<Next<G<Dir::Right, 0, 0, 3, Snake<P<W-1,2>, P<W-2,2>>>, Dir::Right>, GameOver<3>>);
static_assert(IsTerminal<GameOver<3>> && IsTerminal<GameOver<9, true>>);
static_assert(!IsTerminal<G<Dir::Up, 0, 0>>);

// Successors: straight, left, right relative to heading
using Su = Successors<G<Dir::Right, 0, 0>>;
static_assert(Same<Slot<0, Su>, Next<G<Dir::Right, 0, 0>, Dir::Right>>);
static_assert(Same<Slot<1, Su>, Next<G<Dir::Right, 0, 0>, Dir::Up>>);
static_assert(Same<Slot<2, Su>, Next<G<Dir::Right, 0, 0>, Dir::Down>>);
static_assert(Parts<Slot<1, Su>>::dir == Dir::Up);
static_assert(Same<Parts<Slot<2, Su>>::Body_, Snake<P<5,6>, P<5,5>, P<4,5>>>);
// A successor can be terminal: heading up at the top wall, straight dies
using Top = Successors<G<Dir::Up, 9, 9, 2, Snake<P<4,0>, P<4,1>>>>;
static_assert(Same<Slot<0, Top>, GameOver<2>>);
static_assert(!IsTerminal<Slot<1, Top>> && !IsTerminal<Slot<2, Top>>);

// Key table: order is None, Up, Down, Left, Right
template<Dir D> constexpr const int* K = KeyTable<G<D, 0, 0>>::slots;
// heading Right: Up = left turn, Down = right turn, Left (reverse) = straight
static_assert(K<Dir::Right>[0] == 0 && K<Dir::Right>[1] == 1 && K<Dir::Right>[2] == 2
           && K<Dir::Right>[3] == 0 && K<Dir::Right>[4] == 0);
// heading Left: Up = right, Down = left, Right (reverse) = straight
static_assert(K<Dir::Left>[0] == 0 && K<Dir::Left>[1] == 2 && K<Dir::Left>[2] == 1
           && K<Dir::Left>[3] == 0 && K<Dir::Left>[4] == 0);
// heading Up: Left = left, Right = right, Down (reverse) = straight
static_assert(K<Dir::Up>[0] == 0 && K<Dir::Up>[1] == 0 && K<Dir::Up>[2] == 0
           && K<Dir::Up>[3] == 1 && K<Dir::Up>[4] == 2);
// heading Down: Left = right, Right = left, Up (reverse) = straight
static_assert(K<Dir::Down>[0] == 0 && K<Dir::Down>[1] == 0 && K<Dir::Down>[2] == 0
           && K<Dir::Down>[3] == 2 && K<Dir::Down>[4] == 1);
// Every key's chosen slot heads where the key says (unless it is the reverse)
using VBody = Snake<P<5,5>, P<5,4>, P<5,3>>;
using GD = G<Dir::Down, 0, 0, 0, VBody>;
static_assert(Parts<Slot<KeyTable<GD>::slots[3], Successors<GD>>>::dir == Dir::Left);
static_assert(Parts<Slot<KeyTable<GD>::slots[4], Successors<GD>>>::dir == Dir::Right);
static_assert(Parts<Slot<KeyTable<GD>::slots[1], Successors<GD>>>::dir == Dir::Down);
// Terminal key table is all zeros
static_assert(KeyTable<GameOver<1>>::slots[1] == 0 && KeyTable<GameOver<1>>::slots[4] == 0);

// Board-full win: every cell but (W-1, H-1) is snake, head at (W-2, H-1)
// heading right, food on the last free cell. Eating fills the board.
template<int I> using C = P<I % W, I / W>;
template<class Seq> struct RevCover;
template<int... I> struct RevCover<ISeq<int, I...>> { using type = Snake<C<W * H - 2 - I>...>; };
using AlmostFull = RevCover<MakeSeq<W * H - 1>>::type;
static_assert(Same<Head<AlmostFull>, P<W-2, H-1>> && Length<AlmostFull> == W * H - 1);
using Win = Next<Game<Dir::Right, Food<W-1, H-1>, Seed<5>, Score<40>, AlmostFull>, Dir::Right>;
static_assert(Same<Win, GameOver<41, true>>);

// Initial state: valid, length 3, food off the snake
using I0 = Initial<1>;
static_assert(Length<Parts<I0>::Body_> == 3 && Parts<I0>::score == 0);
static_assert(!Contains<Parts<I0>::FoodP, Parts<I0>::Body_> && InBounds<Parts<I0>::FoodP>);
static_assert(!Same<Initial<1>, Initial<2>>);
} // namespace game_tests

namespace text_tests {
using namespace ts;
static_assert(StrEq(IntStr<0>(), "0"));
static_assert(StrEq(IntStr<7>(), "7"));
static_assert(StrEq(IntStr<10>(), "10"));
static_assert(StrEq(IntStr<4294967295>(), "4294967295"));
static_assert(StrEq(IntStr<-1>(), "-1"));
static_assert(StrEq(IntStr<-305>(), "-305"));
static_assert(StrEq(Concat(Lit("ab"), Lit(""), IntStr<12>(), Lit("c")), "ab12c"));
static_assert(StrEq(Lit("xyz"), "xyz") && !StrEq(Lit("xyz"), "xy") && !StrEq(Lit("xyz"), "xya"));
static_assert(Concat(Lit("hi"), Lit("!")).c[3] == '\0');
} // namespace text_tests

namespace render_tests {
using namespace ts;
static_assert(W == 16 && H == 12, "golden frame below assumes the default grid");

using St = Game<Dir::Right, Food<10, 2>, Seed<1>, Score<12>,
                Snake<P<3,1>, P<2,1>, P<1,1>, P<1,2>, P<0,2>>>;
static_assert(StrEq(Render<St>::frame,
    "+--------------------------------+\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. o o @ . . . . . . . . . . . . |\n"
    "|o o . . . . . . . . * . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . @ |\n"
    "+--------------------------------+\n"
    " Score: 12\n") == false);  // sanity: a wrong frame must not match

static_assert(StrEq(Render<St>::frame,
    "+--------------------------------+\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. o o @ . . . . . . . . . . . . |\n"
    "|o o . . . . . . . . * . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "|. . . . . . . . . . . . . . . . |\n"
    "+--------------------------------+\n"
    " Score: 12  Best: 0\n"));

// Corner cells render (bounds of the paint loop)
using Corners = Game<Dir::Down, Food<0, 0>, Seed<1>, Score<0>, Snake<P<W-1, H-1>, P<W-1, H-2>>>;
static_assert(Render<Corners>::frame[RowLen + 1] == CellFood);
static_assert(Render<Corners>::frame[H * RowLen + 2 * W - 1] == CellHead);
static_assert(Render<Corners>::frame[(H - 1) * RowLen + 2 * W - 1] == CellBody);

// Game over: shows the previous best, or "New best!" when beaten
static_assert(StrEq(Render<GameOver<5>, Best<9>>::frame,
                    "\n  *** GAME OVER ***\n  Final score: 5\n  Best: 9\n\n"));
static_assert(StrEq(Render<GameOver<9>, Best<9>>::frame,
                    "\n  *** GAME OVER ***\n  Final score: 9\n  Best: 9\n\n"));
static_assert(StrEq(Render<GameOver<12>, Best<9>>::frame,
                    "\n  *** GAME OVER ***\n  Final score: 12\n  New best!\n\n"));
static_assert(StrEq(Render<GameOver<191, true>, Best<0>>::frame,
                    "\n  *** YOU WIN! ***\n  Final score: 191\n  New best!\n\n"));
static_assert(StrEq(Render<GameOver<0>>::frame,
                    "\n  *** GAME OVER ***\n  Final score: 0\n  Best: 0\n\n"));
// The live score line carries the best score
using St7 = Game<Dir::Up, Food<0, 0>, Seed<1>, Score<3>, Snake<P<5,5>>>;
static_assert(Render<St7, Best<42>>::frame[BoardLen + 17] == '4'
           && Render<St7, Best<42>>::frame[BoardLen + 18] == '2');
} // namespace render_tests

namespace speed_and_best_tests {
using namespace ts;
template<int Sc> using G = Game<Dir::Up, Food<0, 0>, Seed<1>, Score<Sc>, Snake<P<5,5>>>;

// Speed: starts at StartTickMs, faster per point, never below FastestTickMs
static_assert(TickMs<G<0>> == StartTickMs);
static_assert(TickMs<G<1>> == StartTickMs - TickStepMs);
static_assert(TickMs<G<5>> < TickMs<G<4>>);
static_assert(TickMs<G<1000>> == FastestTickMs);
static_assert(TickMs<G<(StartTickMs - FastestTickMs) / TickStepMs>> == FastestTickMs);
static_assert(TickMs<G<(StartTickMs - FastestTickMs) / TickStepMs - 1>> == FastestTickMs + TickStepMs);
static_assert(TickMs<G<(StartTickMs - FastestTickMs) / TickStepMs - 1>> > FastestTickMs);
static_assert(TickMs<GameOver<7>> == StartTickMs);  // unused, but defined

// Best score
static_assert(Same<NewBest<GameOver<5>, Best<9>>, Best<9>>);
static_assert(Same<NewBest<GameOver<9>, Best<9>>, Best<9>>);
static_assert(Same<NewBest<GameOver<12>, Best<9>>, Best<12>>);
static_assert(Same<NewBest<GameOver<4, true>, Best<0>>, Best<4>>);
static_assert(Same<NewBest<G<50>, Best<9>>, Best<9>>);  // only a finished game counts
static_assert(IsNewBest<GameOver<10>, Best<9>> && !IsNewBest<GameOver<9>, Best<9>>);
static_assert(!IsNewBest<G<50>, Best<9>>);

// Best header text
static_assert(StrEq(BestHeader<Best<17>>::text, "#pragma once\nusing BestScore = ts::Best<17>;\n"));
} // namespace speed_and_best_tests

int main() {}
