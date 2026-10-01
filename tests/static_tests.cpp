// Compile-time tests. If this file compiles (-fsyntax-only), every test passed.
#include "../engine/core.hpp"

namespace core_tests {
using namespace ts;

using S3 = Snake<P<5,5>, P<4,5>, P<3,5>>;
using S1 = Snake<P<0,0>>;
using S0 = Snake<>;

// Same / If
static_assert(Same<P<1,2>, P<1,2>>);
static_assert(!Same<P<1,2>, P<2,1>>);
static_assert(!Same<P<1,1>, P<1,2>>);
static_assert(Same<If<true, int, char>, int>);
static_assert(Same<If<false, int, char>, char>);

// Negative coordinates are representable
static_assert(P<-1, 3>::x == -1);
static_assert(!Same<P<-1,0>, P<0,-1>>);

// Contains
static_assert(Contains<P<4,5>, S3>);
static_assert(Contains<P<5,5>, S3>);
static_assert(Contains<P<3,5>, S3>);
static_assert(!Contains<P<5,4>, S3>);
static_assert(!Contains<P<0,0>, S0>);
static_assert(Contains<P<0,0>, S1>);

// Length
static_assert(Length<S0> == 0);
static_assert(Length<S1> == 1);
static_assert(Length<S3> == 3);

// PushFront
static_assert(Same<PushFront<P<6,5>, S3>, Snake<P<6,5>, P<5,5>, P<4,5>, P<3,5>>>);
static_assert(Same<PushFront<P<0,0>, S0>, S1>);

// At / Head / Last
static_assert(Same<At<0, S3>, P<5,5>>);
static_assert(Same<At<1, S3>, P<4,5>>);
static_assert(Same<At<2, S3>, P<3,5>>);
static_assert(Same<Head<S3>, P<5,5>>);
static_assert(Same<Last<S3>, P<3,5>>);
static_assert(Same<Head<S1>, Last<S1>>);

// Take / PopBack
static_assert(Same<Take<0, S3>, S0>);
static_assert(Same<Take<2, S3>, Snake<P<5,5>, P<4,5>>>);
static_assert(Same<PopBack<S3>, Snake<P<5,5>, P<4,5>>>);
static_assert(Same<PopBack<S1>, S0>);
static_assert(Same<PopBack<PopBack<S3>>, Snake<P<5,5>>>);
} // namespace core_tests

int main() {}
