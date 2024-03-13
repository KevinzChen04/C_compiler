#pragma once
#include "hir.h"
#include "mir.h"
#include "std_alias.h"

namespace La::hir_to_mir {
	using namespace std_alias;

	Uptr<mir::Program> make_mir_program(const hir::Program &hir_program);
}
