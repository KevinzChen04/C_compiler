#pragma once

#include "std_alias.h"
#include "hir.h"
#include <memory>
#include <optional>

namespace La::parser {
	using namespace std_alias;

	Uptr<La::hir::Program> parse_file(char *fileName, Opt<std::string> parse_tree_output);
}