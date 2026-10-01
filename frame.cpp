// The frame binary B(S). Everything printed or written is a constant baked in at
// compile time from the State type in the generated "state.hpp"; main is only
// argument dispatch and I/O.
//
//   B              exit 0 (used by the driver to warm a fresh binary)
//   B frame        print the rendered board and score
//   B info         print "keys k0 k1 k2 k3 k4\nterminal t\n"
//   B emit DIR     write DIR/succ0.hpp, succ1.hpp, succ2.hpp
#include <cstdio>
#include <cstring>
#include "engine/game.hpp"
#include "engine/render.hpp"
#include "engine/serialize.hpp"
#include "state.hpp"

namespace {
using namespace ts;

template<class S> struct Info {
    using K = KeyTable<S>;
    static constexpr auto text = Concat(
        Lit("keys "), IntStr<K::slots[0]>(), Lit(" "), IntStr<K::slots[1]>(), Lit(" "),
        IntStr<K::slots[2]>(), Lit(" "), IntStr<K::slots[3]>(), Lit(" "), IntStr<K::slots[4]>(),
        Lit("\nterminal "), IntStr<IsTerminal<S> ? 1 : 0>(), Lit("\n"));
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

int put(const char* data, int size) {
    return std::fwrite(data, 1, size, stdout) == (size_t)size ? 0 : 1;
}

int emit(const char* dir) {
    using E = Emit<State>;
    if (E::count == 0) { std::fputs("emit: state is terminal\n", stderr); return 1; }
    for (int i = 0; i < E::count; ++i) {
        char path[4096];
        std::snprintf(path, sizeof path, "%s/succ%d.hpp", dir, i);
        FILE* f = std::fopen(path, "wb");
        if (!f) { std::perror(path); return 1; }
        size_t n = std::fwrite(E::files[i].data, 1, E::files[i].size, f);
        if (std::fclose(f) != 0 || n != (size_t)E::files[i].size) { std::perror(path); return 1; }
    }
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) return 0;
    if (!std::strcmp(argv[1], "frame")) return put(Render<State>::frame.c, Render<State>::frame.size);
    if (!std::strcmp(argv[1], "info"))  return put(Info<State>::text.c, Info<State>::text.size);
    if (!std::strcmp(argv[1], "emit") && argc == 3) return emit(argv[2]);
    std::fputs("usage: B [frame | info | emit DIR]\n", stderr);
    return 2;
}
