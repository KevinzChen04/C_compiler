#pragma once

#include "std_alias.h"
#include "program.h"
#include <memory>
#include <fstream>
#include <string_view>
#include <assert.h>
#include <typeinfo>
#include <optional>

namespace IR::parser {
	using namespace std_alias;

	Uptr<IR::program::Program> parse_input(char *fileName, Opt<std::string> parse_tree_output);
}