// Generated-header round trip: each case<I>.hpp must name exactly Case<I>.
#include "../../engine/game.hpp"
#include "cases.hpp"
namespace c0 {
#include "case0.hpp"
}
static_assert(ts::Same<c0::State, rt::Case<0>::type>, "round trip 0");
namespace c1 {
#include "case1.hpp"
}
static_assert(ts::Same<c1::State, rt::Case<1>::type>, "round trip 1");
namespace c2 {
#include "case2.hpp"
}
static_assert(ts::Same<c2::State, rt::Case<2>::type>, "round trip 2");
namespace c3 {
#include "case3.hpp"
}
static_assert(ts::Same<c3::State, rt::Case<3>::type>, "round trip 3");
namespace c4 {
#include "case4.hpp"
}
static_assert(ts::Same<c4::State, rt::Case<4>::type>, "round trip 4");
namespace c5 {
#include "case5.hpp"
}
static_assert(ts::Same<c5::State, rt::Case<5>::type>, "round trip 5");
namespace c6 {
#include "case6.hpp"
}
static_assert(ts::Same<c6::State, rt::Case<6>::type>, "round trip 6");
int main() {}
