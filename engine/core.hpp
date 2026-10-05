// Core state types and type-list utilities. Header-only, no standard headers.
// All list operations are O(1) instantiation depth: they use pack expansion
// and clang builtins (__type_pack_element, __make_integer_seq), never recursion.
#pragma once

namespace ts {

// ---- Grid size: the single place it is defined -----------------------------
#ifndef TS_GRID_W
#define TS_GRID_W 16
#endif
#ifndef TS_GRID_H
#define TS_GRID_H 12
#endif
inline constexpr int W = TS_GRID_W;
inline constexpr int H = TS_GRID_H;

// ---- State types ------------------------------------------------------------
template<int X, int Y> struct P { static constexpr int x = X, y = Y; };
enum class Dir { Up, Down, Left, Right };
template<class... Ps> struct Snake {};  // head first
template<int X, int Y> struct Food {};
template<unsigned N>   struct Seed {};
template<int N>        struct Score {};

template<Dir D, class F, class S, class Sc, class Body> struct Game {};
// Won is true only when the snake fills the whole board. S is the seed at the
// end of the game; the next game's food sequence is derived from it.
template<int FinalScore, bool Won = false, class S = Seed<0>> struct GameOver {};
// Best score from earlier games (generated best.hpp: using BestScore = ts::Best<N>;).
template<int N> struct Best {};

// ---- Basic predicates -------------------------------------------------------
template<class A, class B> inline constexpr bool Same = false;
template<class A>          inline constexpr bool Same<A, A> = true;

template<bool C, class T, class F> struct IfT            { using type = T; };
template<class T, class F>         struct IfT<false, T, F> { using type = F; };
template<bool C, class T, class F> using If = typename IfT<C, T, F>::type;

// ---- Index sequences without <utility> --------------------------------------
template<class T, T... I> struct ISeq {};
template<int N> using MakeSeq = __make_integer_seq<ISeq, int, N>;

// ---- List utilities (work on any L<Ps...>, normally Snake) ------------------
template<class X, class List> struct ContainsT;
template<class X, template<class...> class L, class... Ps>
struct ContainsT<X, L<Ps...>> { static constexpr bool value = (Same<X, Ps> || ... || false); };
template<class X, class List> inline constexpr bool Contains = ContainsT<X, List>::value;

template<class X, class List> struct PushFrontT;
template<class X, template<class...> class L, class... Ps>
struct PushFrontT<X, L<Ps...>> { using type = L<X, Ps...>; };
template<class X, class List> using PushFront = typename PushFrontT<X, List>::type;

template<class List> struct LengthT;
template<template<class...> class L, class... Ps>
struct LengthT<L<Ps...>> { static constexpr int value = sizeof...(Ps); };
template<class List> inline constexpr int Length = LengthT<List>::value;

template<int I, class List> struct AtT;
template<int I, template<class...> class L, class... Ps>
struct AtT<I, L<Ps...>> {
    static_assert(I >= 0 && I < (int)sizeof...(Ps), "At: index out of range");
    using type = __type_pack_element<I, Ps...>;
};
template<int I, class List> using At = typename AtT<I, List>::type;

template<class List> using Head = At<0, List>;
template<class List> using Last = At<Length<List> - 1, List>;

// Take<N, List>: the first N elements, as one pack expansion.
template<class List, class Seq> struct TakeImpl;
template<template<class...> class L, class... Ps, int... I>
struct TakeImpl<L<Ps...>, ISeq<int, I...>> { using type = L<__type_pack_element<I, Ps...>...>; };
template<int N, class List> using Take = typename TakeImpl<List, MakeSeq<N>>::type;

template<class List> struct PopBackT;
template<template<class...> class L, class P0, class... Ps>
struct PopBackT<L<P0, Ps...>> { using type = Take<sizeof...(Ps), L<P0, Ps...>>; };
template<class List> using PopBack = typename PopBackT<List>::type;

} // namespace ts
