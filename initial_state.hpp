// State for tick 0. The driver compiles this as state.hpp; TS_SEED picks the food sequence.
#pragma once
#ifndef TS_SEED
#define TS_SEED 1
#endif
using State = ts::Initial<TS_SEED>;
