// The frame binary B(S). Everything printed or written is a constant baked in at
// compile time from the State type in the generated "state.hpp"; main is only
// argument dispatch and I/O.
//
//   B              exit 0 (used by the driver to warm a fresh binary)
//   B frame        print the rendered board and score
//   B info         print "keys k0 k1 k2 k3 k4\nterminal t\ntick ms\n"
//   B emit DIR     write DIR/succ0.hpp, succ1.hpp, succ2.hpp
//   B best FILE    write FILE: best.hpp holding the best score once this state is reached
//   B restart FILE write FILE: state.hpp for the first state of the next game (terminal only)
#include <cstdio>
#include <cstring>
#include "engine/game.hpp"
#include "engine/render.hpp"
#include "engine/serialize.hpp"
#include "state.hpp"
// Best score of earlier games, if the driver supplied one.
#if __has_include("best.hpp")
#include "best.hpp"
#else
using BestScore = ts::Best<0>;
#endif

namespace {
using namespace ts;

template<class S> struct Info {
    using K = KeyTable<S>;
    static constexpr auto text = Concat(
        Lit("keys "), IntStr<K::slots[0]>(), Lit(" "), IntStr<K::slots[1]>(), Lit(" "),
        IntStr<K::slots[2]>(), Lit(" "), IntStr<K::slots[3]>(), Lit(" "), IntStr<K::slots[4]>(),
        Lit("\nterminal "), IntStr<IsTerminal<S> ? 1 : 0>(),
        Lit("\ntick "), IntStr<TickMs<S>>(), Lit("\n"));
};

struct Text { const char* data; int size; };

// Successor headers; a terminal state has none (Successors is never instantiated).
template<class S, bool Terminal = IsTerminal<S>> struct Emit {
    using Su = Successors<S>;
    static constexpr Text files[3] = {
        {HeaderText<Slot<0, Su>>.c, HeaderText<Slot<0, Su>>.size},
        {HeaderText<Slot<1, Su>>.c, HeaderText<Slot<1, Su>>.size},
        {HeaderText<Slot<2, Su>>.c, HeaderText<Slot<2, Su>>.size},
    };
    static constexpr int count = 3;
};
template<class S> struct Emit<S, true> {
    static constexpr const Text* files = nullptr;
    static constexpr int count = 0;
};

// The next game's first state; only a finished game has one.
template<class S, bool Terminal = IsTerminal<S>> struct RestartFile {
    static constexpr const char* data = nullptr;
    static constexpr int size = 0;
};
template<class S> struct RestartFile<S, true> {
    static constexpr const char* data = HeaderText<Restart<S>>.c;
    static constexpr int size = HeaderText<Restart<S>>.size;
};

int put(const char* data, int size) {
    return std::fwrite(data, 1, size, stdout) == (size_t)size ? 0 : 1;
}

int writeFile(const char* path, const char* data, int size) {
    FILE* f = std::fopen(path, "wb");
    if (!f) { std::perror(path); return 1; }
    size_t n = std::fwrite(data, 1, size, f);
    if (std::fclose(f) != 0 || n != (size_t)size) { std::perror(path); return 1; }
    return 0;
}

int emit(const char* dir) {
    using E = Emit<State>;
    if (E::count == 0) { std::fputs("emit: state is terminal\n", stderr); return 1; }
    for (int i = 0; i < E::count; ++i) {
        char path[4096];
        std::snprintf(path, sizeof path, "%s/succ%d.hpp", dir, i);
        if (writeFile(path, E::files[i].data, E::files[i].size)) return 1;
    }
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) return 0;
    using R = Render<State, BestScore>;
    using NB = BestHeader<NewBest<State, BestScore>>;
    if (!std::strcmp(argv[1], "frame")) return put(R::frame.c, R::frame.size);
    if (!std::strcmp(argv[1], "info"))  return put(Info<State>::text.c, Info<State>::text.size);
    if (!std::strcmp(argv[1], "emit") && argc == 3) return emit(argv[2]);
    if (!std::strcmp(argv[1], "best") && argc == 3) return writeFile(argv[2], NB::text.c, NB::text.size);
    if (!std::strcmp(argv[1], "restart") && argc == 3) {
        using RF = RestartFile<State>;
        if (!RF::data) { std::fputs("restart: state is not terminal\n", stderr); return 1; }
        return writeFile(argv[2], RF::data, RF::size);
    }
    std::fputs("usage: B [frame | info | emit DIR | best FILE | restart FILE]\n", stderr);
    return 2;
}
