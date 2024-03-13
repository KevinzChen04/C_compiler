#pragma once

#include "program.h"
#include <memory>
#include <optional>

namespace L2::parser {
	std::unique_ptr<L2::program::Program> parse_file(char *fileName, std::optional<std::string> parse_tree_output);
	std::unique_ptr<L2::program::Program> parse_function_file(char *fileName); // returns a program with exactly one function
	std::unique_ptr<L2::program::SpillProgram> parse_spill_file(char *fileName);
}