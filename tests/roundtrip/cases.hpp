// States exercised by the serialize round-trip test. Case<I> for I in [0, NumCases).
#pragma once
#include "../../engine/game.hpp"

namespace rt {
using namespace ts;
inline constexpr int NumCases = 9;
template<int I> struct Case;
template<> struct Case<0> { using type = Initial<1>; };
template<> struct Case<1> { using type = Game<Dir::Up, Food<0, 0>, Seed<0xFFFFFFFFu>, Score<0>, Snake<P<5,5>>>; };
template<> struct Case<2> { using type = Game<Dir::Left, Food<W-1, H-1>, Seed<3000000000u>, Score<123>,
                                              Snake<P<3,1>, P<2,1>, P<1,1>, P<1,2>, P<0,2>>>; };
template<> struct Case<3> { using type = Game<Dir::Down, Food<7, 0>, Seed<0u>, Score<1>,
                                              Snake<P<10,10>, P<10,9>, P<10,8>>>; };
template<> struct Case<4> { using type = GameOver<0>; };
template<> struct Case<5> { using type = GameOver<190, true>; };
// A few ticks into a real game, including a turn
template<> struct Case<6> { using type = Next<Next<Next<Initial<9>, Dir::Right>, Dir::Down>, Dir::Down>; };
template<> struct Case<7> { using type = GameOver<12, false, Seed<4000000000u>>; };
template<> struct Case<8> { using type = Restart<GameOver<12, false, Seed<4000000000u>>>; };
} // namespace rt
