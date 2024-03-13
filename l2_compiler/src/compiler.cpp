#include "parser.h"
#include "liveness.h"
#include "interference_graph.h"
#include "register_allocator.h"
#include "spiller.h"
#include "code_gen.h"
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
#include <iostream>
#include <assert.h>
#include <optional>

void print_help(char *progName) {
	std::cerr << "Usage: " << progName << " [-v] [-g 0|1] [-O 0|1|2] [-s] [-l] [-i] [-p] SOURCE" << std::endl;
	return;
}

int main(
	int argc,
	char **argv
) {
	bool enable_code_generator = true;
	bool spill_only = false;
	bool interference_only = false;
	bool liveness_only = false;
	std::optional<std::string> parse_tree_output;
	int32_t optLevel = 3;

	/*
	 * Check the compiler arguments.
	 */
	bool verbose = false;
	if (argc < 2) {
		print_help(argv[0]);
		return 1;
	}
	int32_t opt;
	int64_t functionNumber = -1;
	while ((opt = getopt(argc, argv, "vg:O:slip:")) != -1) {
		switch (opt) {
			case 'l':
				liveness_only = true;
				break;
			case 'i':
				interference_only = true;
				break;
			case 's':
				spill_only = true;
				break;
			case 'O':
				optLevel = strtoul(optarg, NULL, 0);
				break;
			case 'g':
				enable_code_generator = (strtoul(optarg, NULL, 0) == 0) ? false : true;
				break;
			case 'v':
				verbose = true;
				break;
			case 'p':
				parse_tree_output = std::string(optarg);
				break;
			default:
				print_help(argv[0]);
				return 1;
		}
	}

	/*
	 * Parse the input file.
	 */
	std::unique_ptr<L2::program::Program> p;
	if (spill_only) {
		// Parse an L2 function and the spill arguments.
		std::unique_ptr<L2::program::SpillProgram> spillprogram = L2::parser::parse_spill_file(argv[optind]);
		L2::program::L2Function *f = spillprogram->program->get_l2_function(0);
		std::string prefix = spillprogram->prefix;
		L2::program::Variable* var = spillprogram->var;
		L2::program::spiller::Spiller spill_man(*f, prefix);
		spill_man.spill(var);
		std::cout << spill_man.printDaSpiller() << std::endl;
		return 0;
	} else if (liveness_only) {
		// Parse an L2 function.
		p = L2::parser::parse_function_file(argv[optind]);

		// Analyze results
		L2::program::L2Function *f = p->get_l2_function(0);
		L2::program::analyze::InstructionsAnalysisResult liveness_results
			= L2::program::analyze::analyze_instructions(*f);

		// print the results
		L2::program::analyze::print_liveness(*f, liveness_results);
		return 0;
	} else if (interference_only){
		// Parse an L2 function.
		p = L2::parser::parse_function_file(argv[optind]);
		L2::program::L2Function *f = p->get_l2_function(0);
		L2::program::analyze::InstructionsAnalysisResult liveness_results
			= L2::program::analyze::analyze_instructions(*f);

		// Analyze results
		L2::program::analyze::VariableGraph graph = generate_interference_graph(
			*f,
			liveness_results,
			L2::program::analyze::create_register_color_table(f->agg_scope.register_scope)
		);

		// Print the results
		std::cout << graph.to_string() << std::endl;
		return 0;
	} else {
		// Parse the L2 program.
		p = L2::parser::parse_file(argv[optind], parse_tree_output);
		auto mp = L2::program::analyze::allocate_and_spill_with_backup(*p->get_l2_function(0));
		// std::cout << p->to_string();
		// for (const auto [key, value] : mp) {
		// 	std::cout << key->to_string() << ": " << value->to_string() << "\n";
		// }
	}

	// /*
	//  * Special cases.
	//  */
	// if (spill_only) {

	// 	/*
	// 	 * Spill.
	// 	 */

	// 	/*
	// 	 * Dump the L2 code.
	// 	 */
	// 	//TODO

	// 	return 0;
	// }

	// /*
	//  * Interference graph test.
	//  */
	// if (interference_only) {
	// 	//TODO
	// 	return 0;
	// }

	// /*
	//  * Generate the target code.
	//  */

	if (enable_code_generator) {
		L2::code_gen::generate_code(*p);
	}

	return 0;
}
