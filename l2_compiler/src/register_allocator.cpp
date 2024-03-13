#include "register_allocator.h"

namespace L2::program::analyze {
	std::vector<const Register *> create_register_color_table(RegisterScope &register_scope) {
		static const std::vector<std::string> register_order = {
			"rax", "rdi", "rsi", "rdx", "rcx",
			"r8", "r9", "r10", "r11", "r12",
			"r13", "r14", "r15", "rbx", "rbp"
		};

		std::vector<const Register *> color_table;
		for (const std::string &reg_name : register_order) {
			if (std::optional<Register *> reg = register_scope.get_item_maybe(reg_name); reg) {
				color_table.push_back(*reg);
			}
		}
		return color_table;
	}

	RegAllocMap coloring_to_reg_alloc(
		const std::map<VariableGraph::Node, VariableGraph::Color> &coloring,
		const std::vector<const Register *> &register_color_table
	) {
		RegAllocMap result;
		for (const auto &[var_ptr, color] : coloring) {
			result.insert(std::make_pair(var_ptr, register_color_table[color]));
		}
		return result;
	}

	std::optional<RegAllocMap> allocate_and_spill(L2Function &l2_function, program::spiller::Spiller &spill_man) {
		std::vector<const Register *> register_color_table = create_register_color_table(l2_function.agg_scope.register_scope);
		while (true) {
			InstructionsAnalysisResult liveness_results = analyze_instructions(l2_function);
			VariableGraph graph = generate_interference_graph(l2_function, liveness_results, register_color_table);
			std::vector<const Variable *> spills = attempt_color_graph(graph, register_color_table);

			if (spills.empty()) {
				// It worked! Return this register allocation
				return std::make_optional(coloring_to_reg_alloc(graph.get_coloring(), register_color_table));
			}

			// this attempt did not work, spill a variable and try again
			bool spillable_found = false;
			for (auto it = spills.rbegin(); it != spills.rend(); ++it) {
				const Variable *next_var = *it;
				if (next_var->spillable) {
					// program::spiller::spill(l2_function, next_var, get_next_prefix(l2_function, "s"), spill_calls);
					spill_man.spill(next_var);
					spillable_found = true;
					break;
				}
			}
			if (!spillable_found) {
				// we got stuck :(
				return {};
			}
		}
	}

	RegAllocMap allocate_and_spill_all(L2Function &l2_function, program::spiller::Spiller &spill_man) {
		std::vector<const Register *> register_color_table = create_register_color_table(l2_function.agg_scope.register_scope);
		spill_man.spill_all();
		InstructionsAnalysisResult liveness_results = analyze_instructions(l2_function);
		VariableGraph graph = generate_interference_graph(l2_function, liveness_results, register_color_table);
		std::vector<const Variable *> spills = attempt_color_graph(graph, register_color_table);
		if (!spills.empty()) {
			std::cerr << "Oops! Spilling all did not work\n";
			exit(1);
		}
		return coloring_to_reg_alloc(graph.get_coloring(), register_color_table);
	}

	RegAllocMap allocate_and_spill_with_backup(L2Function &l2_function) {
		program::spiller::Spiller spill_man(l2_function, "S");
		std::optional<RegAllocMap> normal_attempt = allocate_and_spill(l2_function, spill_man);
		if (normal_attempt) {
			//std::cerr << "normal attempt was good enough\n";
			return *normal_attempt;
		}
		//std::cerr << "normal attempt was NOT good enough\n";

		// TODO just use a backup copy of the l2_function instead... once you
		// figure out how to do that
		for (Variable *var : l2_function.agg_scope.variable_scope.get_all_items()) {
			var->spillable = true;
		}
		return allocate_and_spill_all(l2_function, spill_man);
	}
}