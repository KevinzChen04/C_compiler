#pragma once

#include "hir.h"
#include "std_alias.h"
#include <iostream>
#include <string>

namespace code_gen {
	std::string generate_program_code(const Lb::hir::Program &program);
}
