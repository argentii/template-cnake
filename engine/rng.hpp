// Compile-time LCG and food spawning.
#pragma once
#include "core.hpp"

namespace ts {

// ---- LCG (Numerical Recipes constants, mod 2^32 via unsigned wraparound) ---
template<class S> struct NextSeedT;
template<unsigned N> struct NextSeedT<Seed<N>> { using type = Seed<N * 1664525u + 1013904223u>; };
template<class S> using NextSeed = typename NextSeedT<S>::type;

// The cell a seed points at. High bits only: the low bits of an LCG are weak.
template<class S> struct SeedCellT;
template<unsigned N> struct SeedCellT<Seed<N>> {
    static constexpr int idx = (int)((N >> 16) % (unsigned)(W * H));
    using type = P<idx % W, idx / W>;
};
template<class S> using SeedCell = typename SeedCellT<S>::type;

// ---- Spawn result -----------------------------------------------------------
// Full: no free cell exists (the board is full; the player has won).
template<bool IsFull, class FoodPos, class NextS> struct Spawned {
    static constexpr bool Full = IsFull;
    using Pos = FoodPos;
    using Seed = NextS;
};

// ---- Deterministic fallback: first free cell in row-major order -------------
// One occupancy bitmask per row, folded over the snake body; the first free
// cell is found with ctz on the inverted masks. No grid-sized instantiation.
static_assert(W <= 63 && H <= 63, "row/column masks are 64-bit");
inline constexpr unsigned long long FullRow = (1ull << W) - 1;

template<class Body, int Y> struct RowMaskT;
template<template<class...> class L, class... Ps, int Y>
struct RowMaskT<L<Ps...>, Y> {
    static constexpr unsigned long long value =
        (0ull | ... | (Ps::y == Y ? (1ull << Ps::x) : 0ull));
};

template<class Body, class Rows> struct ScanFreeImpl;
template<class Body, int... Y>
struct ScanFreeImpl<Body, ISeq<int, Y...>> {
    // Bit y set <=> row y has at least one free cell.
    static constexpr unsigned long long rowsWithSpace =
        (0ull | ... | ((RowMaskT<Body, Y>::value != FullRow) ? (1ull << Y) : 0ull));
    static constexpr bool full = rowsWithSpace == 0;
    static constexpr int y = full ? 0 : __builtin_ctzll(rowsWithSpace);
    // Re-select the chosen row's mask with a fold (no recursion, no array).
    static constexpr unsigned long long mask =
        (0ull | ... | (Y == y ? RowMaskT<Body, Y>::value : 0ull));
    static constexpr int x = full ? 0 : __builtin_ctzll(~mask & FullRow);
};

template<class Body, class S> struct ScanFree {
    using Impl = ScanFreeImpl<Body, MakeSeq<H>>;
    using type = Spawned<Impl::full, P<Impl::x, Impl::y>, S>;
};

// ---- Bounded random retries, then the scan ----------------------------------
// Depth is bounded by Tries (a small constant), not by snake length.
inline constexpr int FoodTries = 8;

template<class Body, class S, int Tries> struct TrySpawn;
template<bool Free, class Body, class S, int Tries> struct TrySpawnSel;

template<class Body, class S, int Tries>
struct TrySpawn {
    using type = typename TrySpawnSel<!Contains<SeedCell<S>, Body>, Body, S, Tries>::type;
};
template<class Body, class S>
struct TrySpawn<Body, S, 0> { using type = typename ScanFree<Body, S>::type; };

// Only the selected branch is instantiated, so the scan never runs unless needed.
template<class Body, class S, int Tries>
struct TrySpawnSel<true, Body, S, Tries> { using type = Spawned<false, SeedCell<S>, NextSeed<S>>; };
template<class Body, class S, int Tries>
struct TrySpawnSel<false, Body, S, Tries> { using type = typename TrySpawn<Body, NextSeed<S>, Tries - 1>::type; };

// SpawnFood<Body, Seed<N>>: a Spawned<Full, P<x,y>, Seed<next>>.
template<class Body, class S> using SpawnFood = typename TrySpawn<Body, S, FoodTries>::type;

} // namespace ts
