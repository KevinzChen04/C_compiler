#include "liveness.h"
#include <string>
#include <iostream>
#include <assert.h>
#include <algorithm>

namespace L2::program::analyze {
	// TODO if only I could get this to work
	// template<typename D, typename S>
	// utils::set<D> &operator+=(utils::set<D> &dest, const utils::set<S> &&source) {
	// 	std::cerr << "REMOVE ME\n";
	// 	dest.merge(source);
	// 	return dest;
	// }

	template<typename D, typename S>
	utils::set<D> &operator+=(utils::set<D> &dest, const utils::set<S> &source) {
		dest.insert(source.begin(), source.end());
		return dest;
	}

	// Accumulates a map<Instruction *, InstructionAnalysisResult> with only the
	// successors, gen_set, and kill_set fields filled out.
	// ASSUMES THAT YOU ITERATE THROUGH THE INSTRUCTIONS IN ORDER STARTING WITH
	// THE FIRST ONE
	class InstructionPreAnalyzer : public InstructionVisitor {
		private:

		const L2Function &target; // the function being analyzed
		int index; // the index of the current instruction being analyzed
		InstructionsAnalysisResult accum;

		utils::set<const Register *> caller_saved_registers;
		std::vector<const Register *> argument_registers;
		utils::set<const Register *> callee_saved_registers;
		const Register * return_value_register;

		public:

		InstructionPreAnalyzer(const L2Function &target) :
			target {target},
			index {0},
			caller_saved_registers {},
			argument_registers {},
			callee_saved_registers {},
			return_value_register {nullptr}
		{
			std::vector<const Register *> all_registers = this->target.agg_scope.register_scope.get_all_items();
			this->argument_registers.reserve(6); // guess at the number of argument registers in scope
			for (const Register *reg : all_registers) {
				if (int order = reg->argument_order; order >= 0) {
					if (order >= this->argument_registers.size()) {
						this->argument_registers.resize(order + 1);
					}
					assert(this->argument_registers[order] == nullptr);
					this->argument_registers[order] = reg;
				}
				if (!reg->ignores_liveness) {
					if (reg->is_callee_saved) {
						this->callee_saved_registers.insert(reg);
					} else {
						this->caller_saved_registers.insert(reg);
					}
					if (reg->is_return_value) {
						assert(this->return_value_register == nullptr);
						this->return_value_register = reg;
					}
				}
			}
			// TODO add assert that there are no nullptrs in this->argument_registers
		}

		std::map<Instruction *, InstructionAnalysisResult> get_accumulator() {
			return std::move(this->accum);
		}

		virtual void visit(InstructionReturn &inst) override {
			InstructionAnalysisResult &entry = accum[&inst];
			entry.gen_set += this->callee_saved_registers;
			entry.gen_set.insert(this->return_value_register);
			index += 1;
		}
		virtual void visit(InstructionAssignment &inst) override {
			InstructionAnalysisResult &entry = accum[&inst];
			entry.successors.push_back(this->get_next_instruction());
			entry.kill_set += inst.destination->get_vars_on_write(false);
			entry.gen_set += inst.source->get_vars_on_read();
			entry.gen_set += inst.destination->get_vars_on_write(true);
			if (inst.op != AssignOperator::pure) {
				// also reads from the destination
				entry.gen_set += inst.destination->get_vars_on_read();
			}
			index += 1;
		}
		virtual void visit(InstructionCompareAssignment &inst) override {
			InstructionAnalysisResult &entry = accum[&inst];
			entry.successors.push_back(this->get_next_instruction());
			entry.kill_set += inst.destination->get_vars_on_write(false);
			entry.gen_set += inst.lhs->get_vars_on_read();
			entry.gen_set += inst.rhs->get_vars_on_read();
			index += 1;
		}
		virtual void visit(InstructionCompareJump &inst) override {
			InstructionAnalysisResult &entry = accum[&inst];
			entry.successors.push_back(this->get_next_instruction());
			entry.successors.push_back(inst.label->get_referent());
			entry.gen_set += inst.lhs->get_vars_on_read();
			entry.gen_set += inst.rhs->get_vars_on_read();
			index += 1;
		}
		virtual void visit(InstructionLabel &inst) override {
			InstructionAnalysisResult &entry = accum[&inst];
			entry.successors.push_back(this->get_next_instruction());
			index += 1;
		}
		virtual void visit(InstructionGoto &inst) override {
			InstructionAnalysisResult &entry = accum[&inst];
			entry.successors.push_back(inst.label->get_referent());
			index += 1;
		}
		virtual void visit(InstructionCall &inst) override {
			InstructionAnalysisResult &entry = accum[&inst];
			entry.gen_set += inst.callee->get_vars_on_read();
			entry.gen_set.insert(
				this->argument_registers.begin(),
				this->argument_registers.begin() + std::min(
					static_cast<std::size_t>(inst.num_arguments),
					this->argument_registers.size()
				)
			);
			entry.kill_set += caller_saved_registers;
			if (
				ExternalFunctionRef *fn = dynamic_cast<ExternalFunctionRef *>(inst.callee.get()); // TODO best way to avoid dynamic casting?
				!fn || !fn->get_referent()->get_never_returns()
			) {
				entry.successors.push_back(this->get_next_instruction());
			}
			index += 1;
		}
		virtual void visit(InstructionLeaq &inst) override {
			InstructionAnalysisResult &entry = accum[&inst];
			entry.successors.push_back(this->get_next_instruction());
			entry.kill_set += inst.destination->get_vars_on_write(false);
			entry.gen_set += inst.base->get_vars_on_read();
			entry.gen_set += inst.offset->get_vars_on_read();
			entry.gen_set += inst.destination->get_vars_on_write(true);
			index += 1;
		}

		private:
		Instruction *get_next_instruction() {
			return this->target.instructions[this->index + 1].get();
		}
	};

	InstructionsAnalysisResult analyze_instructions(const L2Function &function) {
		auto num_instructions = function.instructions.size();
		InstructionPreAnalyzer pre_analyzer(function);

		for (const std::unique_ptr<Instruction> &instruction : function.instructions) {
			instruction->accept(pre_analyzer);
		}
		// "resol" is a compromise between the authors' preferred accumulator variables "result" and "sol"
		InstructionsAnalysisResult resol = pre_analyzer.get_accumulator();

		// Each instruction starts with only its gen set as its in set.
		// This initially satisfies the in set's constraints.
		for (const std::unique_ptr<Instruction> &inst : function.instructions) {
			InstructionAnalysisResult &entry = resol[inst.get()];
			entry.in_set = entry.gen_set;
		}
		bool sets_changed;
		do {

			sets_changed = false;
			for (int i = num_instructions - 1; i >= 0; --i) {
				InstructionAnalysisResult &entry = resol[function.instructions[i].get()];

				// out[i] = UNION (s in successors(i)) {in[s]}
				utils::set<const Variable *> new_out_set;
				for (Instruction *succ : entry.successors) {
					for (const Variable *var : resol[succ].in_set) {
						new_out_set.insert(var);
					}
				}
				if (entry.out_set != new_out_set) {
					sets_changed = true;
					entry.out_set = std::move(new_out_set);
				}

				// in[i] = gen[i] UNION (out[i] MINUS kill[i])
				// Everything currently in in[i] is either there because it was
				// in gen[i] or because it's in out[i]. out[i] is the only thing
				// that might have changed. Assuming that this equation is
				// upheld in the previous iteration, these operations will
				// satisfy the formula again.
				// - remove all existing elements that are not in out[i] nor gen[i]
				// - add all elements that are in out[i] but not kill[i]
				utils::set<const Variable *> new_in_set;
				for (const Variable *var : entry.in_set) {
					if (entry.out_set.find(var) != entry.out_set.end()
						|| entry.gen_set.find(var) != entry.gen_set.end())
					{
						new_in_set.insert(var);
					}
				}
				for (const Variable *var : entry.out_set) {
					if (entry.kill_set.find(var) == entry.kill_set.end()) {
						new_in_set.insert(var);
					}
				}
				if (entry.in_set != new_in_set) {
					sets_changed = true;
					entry.in_set = std::move(new_in_set);
				}
			}
		} while (sets_changed);

		return resol;
	}

	void print_liveness(const L2Function &function, InstructionsAnalysisResult &liveness_results){
		std::cout << "(\n(in\n";
		for (const std::unique_ptr<Instruction> &instruction : function.instructions) {
			const InstructionAnalysisResult &entry = liveness_results[instruction.get()];
			std::cout << "(";
			for (const Variable *element : entry.in_set) {
        		std::cout << element->to_string() << " ";
    		}
			std::cout << ")\n";
		}

		std::cout << ")\n\n(out\n";
		// print out sets
		for (const auto &instruction : function.instructions) {
			const InstructionAnalysisResult &entry = liveness_results[instruction.get()];
			std::cout << "(";
			for (const Variable *element : entry.out_set) {
        		std::cout << element->to_string() << " ";
    		}
			std::cout << ")\n";
		}
		std::cout << ")\n\n)\n";
	}
}
