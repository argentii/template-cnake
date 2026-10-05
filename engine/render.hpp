// State -> constexpr frame text. The game state is already decided; this only
// paints it. Cost: one constexpr pass over the grid plus one over the body,
// no per-cell template instantiations.
#pragma once
#include "core.hpp"
#include "text.hpp"
#include "game.hpp"

namespace ts {

// Each cell is 2 columns wide so the board looks roughly square.
inline constexpr int RowLen   = 2 * W + 3;            // '|' cells '|' '\n'
inline constexpr int BoardLen = RowLen * (H + 2);     // plus top and bottom borders

inline constexpr char CellEmpty = '.';
inline constexpr char CellBody  = 'o';
inline constexpr char CellHead  = '@';
inline constexpr char CellFood  = '*';

// Paint a board from already-computed coordinates.
template<int N>
constexpr Str<BoardLen> PaintBoard(const int (&xs)[N], const int (&ys)[N], int fx, int fy) {
    Str<BoardLen> b;
    for (int r = 0; r < H + 2; ++r) {
        char* row = b.c + r * RowLen;
        bool border = (r == 0 || r == H + 1);
        row[0] = border ? '+' : '|';
        for (int i = 1; i <= 2 * W; ++i) row[i] = border ? '-' : (i % 2 ? CellEmpty : ' ');
        row[2 * W + 1] = border ? '+' : '|';
        row[2 * W + 2] = '\n';
    }
    auto at = [&](int x, int y) -> char& { return b.c[(y + 1) * RowLen + 1 + 2 * x]; };
    at(fx, fy) = CellFood;
    for (int i = N - 1; i >= 0; --i) at(xs[i], ys[i]) = i == 0 ? CellHead : CellBody;
    return b;
}

// Render<State, Best<B>>: B is the best score from earlier games.
template<class State, class B = Best<0>> struct Render;

template<Dir D, int Fx, int Fy, class S, int Sc, class... Ps, int B>
struct Render<Game<D, Food<Fx, Fy>, S, Score<Sc>, Snake<Ps...>>, Best<B>> {
    static constexpr int xs[] = {Ps::x...};
    static constexpr int ys[] = {Ps::y...};
    static constexpr auto frame =
        Concat(PaintBoard(xs, ys, Fx, Fy), Lit(" Score: "), IntStr<Sc>(),
               Lit("  Best: "), IntStr<B>(), Lit("\n"));
};

template<bool Won> inline constexpr auto OverBanner = Lit("\n  *** GAME OVER ***\n");
template<>          inline constexpr auto OverBanner<true> = Lit("\n  *** YOU WIN! ***\n");

template<bool IsNew, int B> inline constexpr auto BestLine = Concat(Lit("  Best: "), IntStr<B>(), Lit("\n"));
template<int B>             inline constexpr auto BestLine<true, B> = Lit("  New best!\n");

template<int Sc, bool Won, class S, int B>
struct Render<GameOver<Sc, Won, S>, Best<B>> {
    static constexpr auto frame =
        Concat(OverBanner<Won>, Lit("  Final score: "), IntStr<Sc>(), Lit("\n"),
               BestLine<IsNewBest<GameOver<Sc, Won, S>, Best<B>>, B>, Lit("\n"));
};

} // namespace ts
