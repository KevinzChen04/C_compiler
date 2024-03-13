#pragma once

#include "std_alias.h"
#include "hir.h"
#include "utils.h"
#include <memory>
#include <fstream>
#include <optional>

namespace Lb::parser {
	using namespace std_alias;

	Uptr<Lb::hir::Program> parse_file(char *fileName, Opt<std::string> parse_tree_output);
}
