// State -> constexpr source text of a complete state.hpp:
//   #pragma once
//   using State = ts::Game<...>;
// Output only: the state was decided by the engine.
#pragma once
#include "core.hpp"
#include "text.hpp"

namespace ts {

template<Dir D> inline constexpr auto DirName = Lit("ts::Dir::Up");
template<> inline constexpr auto DirName<Dir::Down>  = Lit("ts::Dir::Down");
template<> inline constexpr auto DirName<Dir::Left>  = Lit("ts::Dir::Left");
template<> inline constexpr auto DirName<Dir::Right> = Lit("ts::Dir::Right");

template<class Pt> struct PText;
template<int X, int Y> struct PText<P<X, Y>> {
    static constexpr auto text = Concat(Lit("ts::P<"), IntStr<X>(), Lit(","), IntStr<Y>(), Lit(">"));
    static constexpr auto withComma = Concat(Lit(","), text);
};

template<class State> struct Serialize;

template<Dir D, int Fx, int Fy, unsigned N, int Sc, class P0, class... Ps>
struct Serialize<Game<D, Food<Fx, Fy>, Seed<N>, Score<Sc>, Snake<P0, Ps...>>> {
    static constexpr auto type_text = Concat(
        Lit("ts::Game<"), DirName<D>,
        Lit(",ts::Food<"), IntStr<Fx>(), Lit(","), IntStr<Fy>(), Lit(">"),
        Lit(",ts::Seed<"), IntStr<N>(), Lit("u>"),
        Lit(",ts::Score<"), IntStr<Sc>(), Lit(">"),
        Lit(",ts::Snake<"), PText<P0>::text, PText<Ps>::withComma..., Lit(">>"));
};

template<bool B> inline constexpr auto BoolName = Lit("false");
template<>       inline constexpr auto BoolName<true> = Lit("true");

template<int Sc, bool Won> struct Serialize<GameOver<Sc, Won>> {
    static constexpr auto type_text =
        Concat(Lit("ts::GameOver<"), IntStr<Sc>(), Lit(","), BoolName<Won>, Lit(">"));
};

template<class State> inline constexpr auto HeaderText =
    Concat(Lit("#pragma once\nusing State = "), Serialize<State>::type_text, Lit(";\n"));

} // namespace ts
