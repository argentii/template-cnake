// Writes <dir>/case<I>.hpp from the serializer, one per case.
#include <cstdio>
#include <cstdlib>
#include "../../engine/serialize.hpp"
#include "cases.hpp"

template<int I> static void write(const char* dir) {
    char path[512];
    std::snprintf(path, sizeof path, "%s/case%d.hpp", dir, I);
    FILE* f = std::fopen(path, "wb");
    if (!f) { std::perror(path); std::exit(1); }
    constexpr auto& t = ts::HeaderText<typename rt::Case<I>::type>;
    std::fwrite(t.c, 1, t.size, f);
    std::fclose(f);
}
template<int... I> static void writeAll(const char* dir, ts::ISeq<int, I...>) { (write<I>(dir), ...); }

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    writeAll(argv[1], ts::MakeSeq<rt::NumCases>{});
}
