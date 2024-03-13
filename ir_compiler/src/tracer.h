#pragma once
#include "std_alias.h"
#include "program.h"

#include <iostream>
#include <iomanip>
#include <queue>
#include <algorithm>
#include <assert.h>

namespace IR::tracer {
	using namespace std_alias;
    using namespace IR::program;


    Vec<Trace> trace_cfg(const Vec<Uptr<BasicBlock>> &blocks);
}