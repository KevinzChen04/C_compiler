#pragma once

#include "std_alias.h"
#include "program.h"
#include <memory>
#include <optional>

namespace L3::parser {
	using namespace std_alias;

	Uptr<L3::program::Program> parse_file(char *fileName, Opt<std::string> parse_tree_output);
}