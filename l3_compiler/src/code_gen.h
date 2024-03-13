#pragma once
#include "program.h"
#include <iostream>

namespace L3::code_gen {
	// TODO for the code generation, it'd be really nice if we just mapped to
	// a memory representation of the L2 code, and then did a to_string-ish
	// operation on that. That would let us avoid most of the problems with
	// conflicting names and whatnot because the memory representation doesn't
	// use names, so we can just assign them at the very end.

	void generate_l3_function_code(const L3::program::L3Function &l3_function, std::ostream &o);

	void generate_program_code(L3::program::Program &program, std::ostream &o);
}
