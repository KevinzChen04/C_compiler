#include "std_alias.h"
#include "parser.h"
#include "hir_to_mir.h"
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <set>
#include <iterator>
#include <iostream>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <stdint.h>
#include <unistd.h>
#include <fstream>
#include <assert.h>
#include <optional>

using namespace std_alias;

void print_help(char *progName) {
	std::cerr << "Usage: " << progName << " [-v] [-g 0|1] [-O 0|1|2] [-p] SOURCE" << std::endl;
	return;
}

int main(
	int argc,
	char **argv
) {
	bool enable_code_generator = true;
	bool output_parse_tree = false;
	bool verbose = false;
	int32_t optimizationLevel = 3;

	// Check the compiler arguments.
	if (argc < 2) {
		print_help(argv[0]);
		return 1;
	}

	int32_t option;
	int64_t functionNumber = -1;
	while ((option = getopt(argc, argv, "vg:O:p")) != -1) {
		switch (option) {
			case 'O':
				optimizationLevel = strtoul(optarg, NULL, 0);
				break;
			case 'g':
				enable_code_generator = (strtoul(optarg, NULL, 0) == 0) ? false : true;
				break;
			case 'v':
				verbose = true;
				break;
			case 'p':
				output_parse_tree = true;
				break;
			default:
				print_help(argv[0]);
				return 1;
		}
	}

	// Parse the input file.
	Uptr<La::hir::Program> hir_program = La::parser::parse_file(
		argv[optind],
		output_parse_tree ? std::make_optional("parse_tree.dot") : Opt<std::string>()
	);

	if (enable_code_generator) {
		auto mir_program = La::hir_to_mir::make_mir_program(*hir_program);
		std::ofstream o;
		o.open("prog.IR");
		o << mir_program->to_ir_syntax();
		o.close();
	}

	return 0;
}
