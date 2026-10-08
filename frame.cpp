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
//
// Built with -DTS_LOOKAHEAD (the driver's lookahead pipeline), the binary also
// carries the outputs of each successor of S, selected with -cN (N = slot 0..2):
//   B -cN frame | info | state FILE | best FILE | restart FILE
// where "state FILE" writes that successor's state.hpp. One binary per tick then
// serves whichever slot the player picks.
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
    static constexpr Text text = {nullptr, 0};
};
template<class S> struct RestartFile<S, true> {
    static constexpr Text text = {HeaderText<Restart<S>>.c, HeaderText<Restart<S>>.size};
};

// Everything the driver can ask about one state T.
struct Target { Text frame, info, state, best, restart; };

template<class T, bool WithState> struct StateText {
    static constexpr Text text = {HeaderText<T>.c, HeaderText<T>.size};
};
template<class T> struct StateText<T, false> { static constexpr Text text = {nullptr, 0}; };

template<class T, bool WithState> struct TargetOf {
    using R = Render<T, BestScore>;
    using NB = BestHeader<NewBest<T, BestScore>>;
    static constexpr Target value = {
        {R::frame.c, R::frame.size},
        {Info<T>::text.c, Info<T>::text.size},
        StateText<T, WithState>::text,
        {NB::text.c, NB::text.size},
        RestartFile<T>::text,
    };
};

#ifdef TS_LOOKAHEAD
inline constexpr bool Lookahead = true;
#else
inline constexpr bool Lookahead = false;
#endif

// targets[0] is S itself; with lookahead, targets[1 + slot] are its successors.
template<class S, bool WithChildren = Lookahead && !IsTerminal<S>> struct Targets {
    static constexpr Target list[1] = {TargetOf<S, false>::value};
    static constexpr int count = 1;
};
template<class S> struct Targets<S, true> {
    using Su = Successors<S>;
    static constexpr Target list[4] = {
        TargetOf<S, false>::value,
        TargetOf<Slot<0, Su>, true>::value,
        TargetOf<Slot<1, Su>, true>::value,
        TargetOf<Slot<2, Su>, true>::value,
    };
    static constexpr int count = 4;
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

int usage() {
    std::fputs("usage: B [-cN] [frame | info | emit DIR | state FILE | best FILE | restart FILE]\n", stderr);
    return 2;
}

int writeText(const char* path, Text t, const char* what) {
    if (!t.data) { std::fprintf(stderr, "%s: not available for this state\n", what); return 1; }
    return writeFile(path, t.data, t.size);
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) return 0;
    using T = Targets<State>;
    int target = 0;
    if (argv[1][0] == '-' && argv[1][1] == 'c') {
        int slot = argv[1][2] - '0';
        if (argv[1][3] != '\0' || slot < 0 || slot > 2 || T::count != 4) {
            std::fputs("B: -cN needs a lookahead build of a live state, N in 0..2\n", stderr);
            return 2;
        }
        target = 1 + slot;
        ++argv;
        --argc;
        if (argc < 2) return usage();
    }
    const Target& t = T::list[target];
    const char* cmd = argv[1];
    if (!std::strcmp(cmd, "frame") && argc == 2) return put(t.frame.data, t.frame.size);
    if (!std::strcmp(cmd, "info") && argc == 2)  return put(t.info.data, t.info.size);
    if (!std::strcmp(cmd, "emit") && argc == 3 && target == 0) return emit(argv[2]);
    if (!std::strcmp(cmd, "state") && argc == 3)   return writeText(argv[2], t.state, "state");
    if (!std::strcmp(cmd, "best") && argc == 3)    return writeText(argv[2], t.best, "best");
    if (!std::strcmp(cmd, "restart") && argc == 3) return writeText(argv[2], t.restart, "restart");
    return usage();
}
