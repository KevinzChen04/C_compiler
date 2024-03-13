#include "code_gen.h"
#include "std_alias.h"
#include "tiles.h"
#include "target_arch.h"

namespace L3::code_gen {
	using namespace std_alias;
	using namespace L3::program;

	void generate_l3_function_code(const L3Function &l3_function, std::ostream &o) {
		// function header
		o << "\t(@" << l3_function.get_name()
			<< " " << l3_function.get_parameter_vars().size() << "\n";

		// assign parameter registers to variables
		const Vec<Variable *> &parameter_vars = l3_function.get_parameter_vars();
		for (int i = 0; i < parameter_vars.size(); ++i) {
			o << "\t\t" << target_arch::get_argument_loading_instruction(
				target_arch::to_l2_expr(parameter_vars[i]),
				i,
				parameter_vars.size()
			) << "\n";
		}

		// print each block
		for (const Uptr<BasicBlock> &block : l3_function.get_blocks()) {
			if (block->get_name().size() > 0) {
				o << "\t\t:" << block->get_name() << "\n";
			}
			Vec<Uptr<tiles::Tile>> tiles = tiles::tile_trees(block->get_tree_boxes());
			for (const Uptr<tiles::Tile> &tile : tiles) {
				for (const std::string &inst : tile->to_l2_instructions()) {
					o << "\t\t" << inst << "\n";
				}
			}
		}

		// close
		o << "\t)\n";
	}

	void generate_program_code(Program &program, std::ostream &o) {
		target_arch::mangle_label_names(program);

		o << "(@" << (*program.get_main_function_ref().get_referent())->get_name() << "\n";
		for (const Uptr<L3Function> &function : program.get_l3_functions()) {
			generate_l3_function_code(*function, o);
		}
		o << ")\n";
	}
}