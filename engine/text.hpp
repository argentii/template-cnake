// constexpr text building blocks for render and serialize. Output only: these
// turn finished type-level results into characters and decide nothing.
#pragma once

namespace ts {

template<int N> struct Str {
    char c[N + 1] = {};  // NUL-terminated
    static constexpr int size = N;
    constexpr char& operator[](int i) { return c[i]; }
    constexpr char operator[](int i) const { return c[i]; }
};

// String literal -> Str
template<int N> constexpr Str<N - 1> Lit(const char (&s)[N]) {
    Str<N - 1> r;
    for (int i = 0; i < N - 1; ++i) r[i] = s[i];
    return r;
}

// Integer -> decimal Str, length known at compile time
template<long long V> inline constexpr int DigitCount = [] {
    unsigned long long u = V < 0 ? 0ull - (unsigned long long)V : (unsigned long long)V;
    int n = V < 0 ? 2 : 1;
    while (u >= 10) { u /= 10; ++n; }
    return n;
}();

template<long long V> constexpr Str<DigitCount<V>> IntStr() {
    Str<DigitCount<V>> r;
    unsigned long long u = V < 0 ? 0ull - (unsigned long long)V : (unsigned long long)V;
    for (int i = DigitCount<V> - 1; i >= (V < 0 ? 1 : 0); --i) { r[i] = char('0' + u % 10); u /= 10; }
    if (V < 0) r[0] = '-';
    return r;
}

// Concatenate any number of Strs with a single copy pass.
template<int... Ns> constexpr Str<(0 + ... + Ns)> Concat(const Str<Ns>&... parts) {
    Str<(0 + ... + Ns)> r;
    int o = 0;
    ((([&] { for (int i = 0; i < Ns; ++i) r[o + i] = parts[i]; o += Ns; })()), ...);
    return r;
}

template<int A, int B> constexpr bool StrEq(const Str<A>& a, const char (&b)[B]) {
    if (A != B - 1) return false;
    for (int i = 0; i < A; ++i) if (a[i] != b[i]) return false;
    return true;
}

} // namespace ts
