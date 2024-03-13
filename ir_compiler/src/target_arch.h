#pragma once
#include "std_alias.h"
#include "program.h"
#include "tracer.h"
#include <string>

namespace IR::code_gen::target_arch {
	using namespace std_alias;
	using namespace IR::program;

	std::string encode_expr(Expr &exp, std::string &prefix);

	std::string decode_expr(Variable &decode_to, Expr &target, std::string &prefix);

	std::string new_variable_names(IRFunction &fun, BasicBlock& bb);

    // Modifies a program so that its label names are all globally unique
	// and always start with an underscore (so that non-underscore names can
	// be used by the generator)
	void mangle_label_names(Program &program);
}