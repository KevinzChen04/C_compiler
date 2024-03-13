#pragma once
#include "program.h"
#include "tracer.h"
#include "tracer.h"
#include "std_alias.h"
#include "target_arch.h"
#include <iostream>

namespace IR::code_gen {

	void generate_ir_function_code(IR::program::IRFunction &ir_function, std::ostream &o);

	void generate_program_code(IR::program::Program &program, std::ostream &o);
}