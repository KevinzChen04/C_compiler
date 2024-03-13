#include "interference_graph.h"
#include "program.h"
#include <stack>

namespace L2::program::analyze {
	template<typename D, typename S>
	std::vector<D> &operator+=(std::vector<D> &dest, const std::vector<S> &source) {
		dest.insert(dest.end(), source.begin(), source.end());
		return dest;
	}

	// SIRR: shift instruction register restrictions
	class SirrInstVisitor : public InstructionVisitor {
		private:

		VariableGraph &target; // the graph to which the restrictions will be added
		utils::set<const Register *> non_rcx_registers;

		public:

		SirrInstVisitor(VariableGraph &target, utils::set<const Register *> non_rsp_registers) :
			target {target}, non_rcx_registers(non_rsp_registers.begin(), non_rsp_registers.end())
		{
			for (auto it = this->non_rcx_registers.begin(); it != this->non_rcx_registers.end(); ++it) {
				if ((*it)->name == "rcx") {
					this->non_rcx_registers.erase(it);
					break;
				}
			}
		}

		virtual void visit(InstructionReturn &inst) {}
		virtual void visit(InstructionCompareAssignment &inst) {}
		virtual void visit(InstructionCompareJump &inst) {}
		virtual void visit(InstructionLabel &inst) {}
		virtual void visit(InstructionGoto &inst) {}
		virtual void visit(InstructionCall &inst) {}
		virtual void visit(InstructionLeaq &inst) {}
		virtual void visit(InstructionAssignment &inst) {
			if (inst.op == AssignOperator::lshift || inst.op == AssignOperator::rshift) {
				for (VariableGraph::Node read_var : inst.source->get_vars_on_read()) {
					for (VariableGraph::Node reg : this->non_rcx_registers) {
						this->target.add_edge(read_var, reg);
					}
				}

				// If only C++ let you pass a set<subtype> as a set<supertype>...
				// this->target.add_total_bipartite(
				// 	inst.source->get_vars_on_read();,
				// 	this->non_rcx_registers
				// );
			}
		}
	};

	void pre_color_registers(VariableGraph &graph, const std::vector<const Register *> &register_color_table) {
		for (VariableGraph::Color color = 0; color < register_color_table.size(); ++color) {
			graph.attempt_enable_with_color(register_color_table[color], color);
		}
	}

	VariableGraph generate_interference_graph(
		L2Function &l2_function,
		const InstructionsAnalysisResult &inst_analysis,
		const std::vector<const Register *> &register_color_table
	) {
		// TODO this will probably be more than necessary until we delete
		// spilled variables from the scope
		std::vector<VariableGraph::Node> total_vars = static_cast<const L2Function &>(l2_function).agg_scope.variable_scope.get_all_items();
		total_vars.insert(total_vars.end(), register_color_table.begin(), register_color_table.end());
		utils::set<const Register *> non_rsp_registers(register_color_table.begin(), register_color_table.end());

		VariableGraph result(total_vars);
		result.add_total_bipartite(
			(utils::set<VariableGraph::Node> &)non_rsp_registers, // I'm so sure these casts are safe... it's a set<derived> being passed into a CONST REF to set<base> ffs
			(utils::set<VariableGraph::Node> &)non_rsp_registers
		);
		pre_color_registers(result, register_color_table);

		SirrInstVisitor sirr_inst_visitor(result, non_rsp_registers);

		for (const auto &[inst_ptr, inst_analysis_result] : inst_analysis) {
			// add the in_set of this instruction to the graph
			result.add_clique(inst_analysis_result.in_set);

			// if this instruction has multiple successors, then also add the
			// out_set of this instruction, since the in_sets of the
			// succeeding instructions would not be enough to capture
			// all the conflicts
			if (inst_analysis_result.successors.size() > 1) {
				result.add_clique(inst_analysis_result.out_set);
			}

			// add edges between the kill and out sets
			result.add_total_bipartite(inst_analysis_result.out_set, inst_analysis_result.kill_set);

			// account for the special case where only rcx can be used as a shift argument
			inst_ptr->accept(sirr_inst_visitor);
		}
		return result;
	}

	std::optional<VariableGraph::Node> determine_variable_to_remove(VariableGraph &graph, int num_colors) {
		// contains the Variable * with the highest degree strictly less than num_colors
		std::pair<VariableGraph::Node, int> most_under_max = std::make_pair(nullptr, 0);
		// contains the Variable * with the highest degree overall
		std::pair<VariableGraph::Node, int> most_overall = std::make_pair(nullptr, 0);

		for (const auto &[node, i] : graph.get_node_map()) {
			const VariableGraph::NodeInfo &node_info = graph.get_node_info(i);
			if (!node_info.is_enabled || node_info.color) {
				continue;
			}
			// for the degree comparison, use >= just in case 0 is the max
			int degree = node_info.degree;
			if (degree < num_colors && degree >= most_under_max.second) {
				most_under_max = std::make_pair(node, degree);
			}
			if (degree >= most_overall.second) {
				most_overall = std::make_pair(node, degree);
			}
		}

		if (most_under_max.first != nullptr) {
			return std::make_optional(most_under_max.first);
		} else {
			if (most_overall.first != nullptr) {
				return std::make_optional(most_overall.first);
			} else {
				return {};
			}
		}
	}

	std::optional<VariableGraph::Color> determine_replacement_color(VariableGraph &graph, int num_colors, VariableGraph::Node var) {
		// std::cerr << "finding replacement color for " << var->to_string() << "\n";
		std::vector<bool> color_allowed(num_colors, true);
		for (std::size_t neighbor_idx : graph.get_node_info(var).adj_vec) {
			const VariableGraph::NodeInfo &neighbor_info = graph.get_node_info(neighbor_idx);
			// std::cerr << "neighbor " << neighbor_info.node->to_string();
			if (auto color = neighbor_info.color; neighbor_info.is_enabled && color) {
				// std::cerr << " with color " << *color;
				color_allowed[*color] = false;
			} else {
				// std::cerr << "!";
			}
			// std::cerr << "\n";
		}
		for (VariableGraph::Color color_cand = 0; color_cand < num_colors; ++color_cand) {
			if (color_allowed[color_cand]) {
				return std::make_optional(color_cand);
			}
		}
		return {};
	}

	std::vector<VariableGraph::Node> attempt_color_graph(
		VariableGraph &graph,
		const std::vector<const Register *> &register_color_table
	) {
		std::vector<VariableGraph::Node> spilled;
		std::stack<VariableGraph::Node> removed_vars;

		std::optional<VariableGraph::Node> to_remove;
		while (to_remove = determine_variable_to_remove(graph, register_color_table.size())) {
			removed_vars.push(*to_remove);
			graph.disable_node(*to_remove);
		}

		while (!removed_vars.empty()) {
			VariableGraph::Node top_var = removed_vars.top();
			removed_vars.pop();
			// std::cerr << "replacing node " << top_var->to_string() << "\n";

			std::optional<VariableGraph::Color> color = determine_replacement_color(graph, register_color_table.size(), top_var);
			if (color) {
				// add the node back with a color
				// std::cerr << "adding back with color " << *color << "\n";
				graph.attempt_enable_with_color(top_var, color);
			} else {
				// gotta spill it
				graph.attempt_enable_with_color(top_var, {});
				spilled.push_back(top_var);
			}
		}
		graph.verify_no_conflicts();

		return spilled;
	}
}
