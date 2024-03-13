#include "program.h"
#include "std_alias.h"
#include "utils.h"
#include <assert.h>
#include <map>
#include <optional>
#include <algorithm>

namespace L3::program {
	using namespace std_alias;

	template<> std::string ItemRef<Variable>::to_string() const {
		std::string result = "%" + this->get_ref_name();
		if (!this->referent_nullable) {
			result += "?";
		}
		return result;
	}
	template<> void ItemRef<Variable>::bind_to_scope(AggregateScope &agg_scope) {
		agg_scope.variable_scope.add_ref(*this);
	}
	template<> Uptr<ComputationNode> ItemRef<Variable>::to_computation_tree() const {
		if (!this->referent_nullable) {
			std::cerr << "Error: can't convert free variable name to computation tree.\n";
			exit(1);
		}
		return mkuptr<VariableCn>(this->referent_nullable);
	}
	template<> std::string ItemRef<BasicBlock>::to_string() const {
		std::string result = ":" + this->get_ref_name();
		if (!this->referent_nullable) {
			result += "?";
		}
		return result;
	}
	template<> void ItemRef<BasicBlock>::bind_to_scope(AggregateScope &agg_scope) {
		agg_scope.label_scope.add_ref(*this);
	}
	template<> Uptr<ComputationNode> ItemRef<BasicBlock>::to_computation_tree() const {
		if (!this->referent_nullable) {
			std::cerr << "Error: can't convert free label name to computation tree.\n";
			exit(1);
		}
		return mkuptr<LabelCn>(this->referent_nullable);
	}
	template<> std::string ItemRef<L3Function>::to_string() const {
		std::string result = "@" + this->get_ref_name();
		if (!this->referent_nullable) {
			result += "?";
		}
		return result;
	}
	template<> void ItemRef<L3Function>::bind_to_scope(AggregateScope &agg_scope) {
		agg_scope.l3_function_scope.add_ref(*this);
	}
	template<> Uptr<ComputationNode> ItemRef<L3Function>::to_computation_tree() const {
		if (!this->referent_nullable) {
			std::cerr << "Error: can't convert free L3 function name to computation tree.\n";
			exit(1);
		}
		return mkuptr<FunctionCn>(this->referent_nullable);
	}
	template<> std::string ItemRef<ExternalFunction>::to_string() const {
		std::string result = this->get_ref_name();
		if (!this->referent_nullable) {
			result += "?";
		}
		return result;
	}
	template<> void ItemRef<ExternalFunction>::bind_to_scope(AggregateScope &agg_scope) {
		agg_scope.external_function_scope.add_ref(*this);
	}
	template<> Uptr<ComputationNode> ItemRef<ExternalFunction>::to_computation_tree() const {
		if (!this->referent_nullable) {
			std::cerr << "Error: can't convert free external function name to computation tree.\n";
			exit(1);
		}
		return mkuptr<FunctionCn>(this->referent_nullable);
	}

	NumberLiteral::NumberLiteral(std::string_view value_str) :
		value { utils::string_view_to_int<int64_t>(value_str) }
	{}
	void NumberLiteral::bind_to_scope(AggregateScope &agg_scope) {
		// empty bc literals make no reference to names
	}
	Uptr<ComputationNode> NumberLiteral::to_computation_tree() const {
		return mkuptr<NumberCn>(this->value);
	}
	std::string NumberLiteral::to_string() const {
		return std::to_string(this->value);
	}

	void MemoryLocation::bind_to_scope(AggregateScope &agg_scope) {
		this->base->bind_to_scope(agg_scope);
	}
	Uptr<ComputationNode> MemoryLocation::to_computation_tree() const {
		return mkuptr<LoadCn>(
			Opt<Variable *>(),
			this->base->to_computation_tree()
		);
	}
	std::string MemoryLocation::to_string() const {
		return "load " + this->base->to_string();
	}

	Operator str_to_op(std::string_view str) {
		static const Map<std::string, Operator> map {
			{ "<", Operator::lt },
			{ "<=", Operator::le },
			{ "=", Operator::eq },
			{ ">=", Operator::ge },
			{ ">", Operator::gt },
			{ "+", Operator::plus },
			{ "-", Operator::minus },
			{ "*", Operator::times },
			{ "&", Operator::bitwise_and },
			{ "<<", Operator::lshift },
			{ ">>", Operator::rshift }
		};
		return map.find(str)->second;
	}
	std::string to_string(Operator op) {
		static const std::string map[] = {
			"<", "<=", "=", ">=", ">", "+", "-", "*", "&", "<<", ">>"
		};
		return map[static_cast<int>(op)];
	}
	Opt<Operator> flip_operator(Operator op) {
		switch (op) {
			// operators that are commutative
			case Operator::eq:
			case Operator::plus:
			case Operator::times:
			case Operator::bitwise_and:
				return op;

			// operators that flip
			case Operator::lt: return Operator::gt;
			case Operator::le: return Operator::ge;
			case Operator::gt: return Operator::lt;
			case Operator::ge: return Operator::le;

			// operators that can't be flipped
			default: return {};
		}
	}

	void BinaryOperation::bind_to_scope(AggregateScope &agg_scope) {
		this->lhs->bind_to_scope(agg_scope);
		this->rhs->bind_to_scope(agg_scope);
	}
	Uptr<ComputationNode> BinaryOperation::to_computation_tree() const {
		return mkuptr<BinaryCn>(
			Opt<Variable *>(),
			this->op,
			this->lhs->to_computation_tree(),
			this->rhs->to_computation_tree()
		);
	}
	std::string BinaryOperation::to_string() const {
		return this->lhs->to_string()
			+ " " + program::to_string(this->op)
			+ " " + this->rhs->to_string();
	}

	void FunctionCall::bind_to_scope(AggregateScope &agg_scope) {
		this->callee->bind_to_scope(agg_scope);
		for (Uptr<Expr> &arg : this->arguments) {
			arg->bind_to_scope(agg_scope);
		}
	}
	Uptr<ComputationNode> FunctionCall::to_computation_tree() const {
		Vec<Uptr<ComputationNode>> arguments;
		for (const Uptr<Expr> &argument : this->arguments) {
			arguments.emplace_back(argument->to_computation_tree());
		}
		return mkuptr<CallCn>(
			Opt<Variable *>(),
			this->callee->to_computation_tree(),
			mv(arguments)
		);
	}
	std::string FunctionCall::to_string() const {
		std::string result = "call " + this->callee->to_string() + "(";
		for (const Uptr<Expr> &argument : this->arguments) {
			result += argument->to_string() + ", ";
		}
		result += ")";
		return result;
	}

	void InstructionReturn::bind_to_scope(AggregateScope &agg_scope) {
		if (this->return_value) {
			(*this->return_value)->bind_to_scope(agg_scope);
		}
	}
	Uptr<ComputationNode> InstructionReturn::to_computation_tree() const {
		if (this->return_value) {
			return mkuptr<ReturnCn>(
				(*this->return_value)->to_computation_tree()
			);
		} else {
			return mkuptr<ReturnCn>();
		}
	}
	std::string InstructionReturn::to_string() const {
		std::string result = "return";
		if (this->return_value) {
			result += " " + (*this->return_value)->to_string();
		}
		return result;
	}

	void InstructionAssignment::bind_to_scope(AggregateScope &agg_scope) {
		if (this->maybe_dest) {
			(*this->maybe_dest)->bind_to_scope(agg_scope);
		}
		this->source->bind_to_scope(agg_scope);
	}
	Uptr<ComputationNode> InstructionAssignment::to_computation_tree() const {
		Uptr<ComputationNode> tree = this->source->to_computation_tree();
		// put a destination on the top node; or make a MoveCn if
		// there already is a destination there
		if (!tree->destination.has_value()) {
			if (this->maybe_dest) {
				tree->destination = (*this->maybe_dest)->get_referent().value(); // .value() to assert that the destination variable must be bound if it exists
			} // if there is no destination, just leave it blank
			return mv(tree);
		} else {
			// make a MoveCn
			return mkuptr<MoveCn>(
				(*this->maybe_dest)->get_referent().value(), // .value() to assert that there must be a destination; otherwise what's the point (all possibility of side effects was handled in the branch checking if the tree was a ComputationNode)
				mv(tree)
			);
		}
	}
	Instruction::ControlFlowResult InstructionAssignment::get_control_flow() const {
		// FUTURE only the source value can have a call
		// this works for now but will fail if we have more complex
		// subexpressions such as calls inside other expressions.
		// thankfully I don't think we will run into that problem
		bool has_call = dynamic_cast<FunctionCall *>(this->source.get());
		return { true, has_call, Opt<ItemRef<BasicBlock> *>() };
	}
	std::string InstructionAssignment::to_string() const {
		std::string result;
		if (this->maybe_dest) {
			result += (*this->maybe_dest)->to_string() + " <- ";
		}
		result += this->source->to_string();
		return result;
	}

	void InstructionStore::bind_to_scope(AggregateScope &agg_scope) {
		this->base->bind_to_scope(agg_scope);
		this->source->bind_to_scope(agg_scope);
	}
	Uptr<ComputationNode> InstructionStore::to_computation_tree() const {
		return mkuptr<StoreCn>(
			this->base->to_computation_tree(),
			this->source->to_computation_tree()
		);
	}
	Instruction::ControlFlowResult InstructionStore::get_control_flow() const {
		// FUTURE the grammar prohibits a store instruction from having any kind
		// of source expression other than a variable, so we know for sure that
		// there cannot be a call or anything. For now, simply returning false
		// will work
		return { true, false, Opt<ItemRef<BasicBlock> *>() };
	}
	std::string InstructionStore::to_string() const {
		return "store " + this->base->to_string() + " <- " + this->source->to_string();
	}

	void InstructionLabel::bind_to_scope(AggregateScope &agg_scope) {}
	Uptr<ComputationNode> InstructionLabel::to_computation_tree() const {
		// InstructionLabels don't do anything, so output a no-op tree
		return mkuptr<NoOpCn>();
	}
	std::string InstructionLabel::to_string() const {
		return ":" + this->label_name;
	}

	void InstructionBranch::bind_to_scope(AggregateScope &agg_scope) {
		if (this->condition) {
			(*this->condition)->bind_to_scope(agg_scope);
		}
		this->label->bind_to_scope(agg_scope);
	}
	Instruction::ControlFlowResult InstructionBranch::get_control_flow() const {
		return {
			this->condition.has_value(), // a conditional branch might fall through
			false, // a branch instruction gets no promise of return
			this->label.get()
		};
	}
	Uptr<ComputationNode> InstructionBranch::to_computation_tree() const {
		Opt<Uptr<ComputationNode>> condition_tree;
		if (this->condition) {
			condition_tree = (*this->condition)->to_computation_tree();
		}
		return mkuptr<BranchCn>(
			this->label->get_referent().value(), // .value() to assert that the value exists
			mv(condition_tree)
		);
	}
	std::string InstructionBranch::to_string() const {
		std::string result = "br ";
		if (this->condition) {
			result += (*this->condition)->to_string() + " ";
		}
		result += this->label->to_string();
		return result;
	}

	Vec<Uptr<ComputationNode> *> get_merge_targets(Uptr<ComputationNode> &tree, Variable *target) {
		// we must replace a variable node with the potential merge child
		if (VariableCn *var_node = dynamic_cast<VariableCn *>(tree.get())) {
			if (var_node->destination == target) {
				return { &tree };
			} else {
				return {};
			}
		}

		return tree->get_merge_targets(target);
	}

	std::string ComputationNode::to_string() const {
		return "("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") {}";
	}
	Opt<Variable *> ComputationNode::get_var_written() const {
		return this->destination;
	}
	Vec<Uptr<ComputationNode> *> get_merge_targets(Variable *target) {
		return {};
	}
	std::string NoOpCn::to_string() const {
		return "("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") NoOp {}";
	}
	Set<Variable *> NoOpCn::get_vars_read() const {
		return {};
	}
	Vec<Uptr<ComputationNode> *> NoOpCn::get_merge_targets(Variable *target) {
		return {};
	}
	std::string NumberCn::to_string() const {
		if (this->destination) {
			return "("
				+ utils::to_string<Variable *, program::to_string>(this->destination)
				+ ") NumberCn { "
				+ std::to_string(this->value)
				+ " }";
		} else {
			return std::to_string(this->value);
		}
	}
	Set<Variable *> NumberCn::get_vars_read() const {
		return {};
	}
	Vec<Uptr<ComputationNode> *> NumberCn::get_merge_targets(Variable *target) {
		return {};
	}
	std::string VariableCn::to_string() const {
		return program::to_string(*this->destination);
	}
	Set<Variable *> VariableCn::get_vars_read() const {
		return { *this->destination };
	}
	Vec<Uptr<ComputationNode> *> VariableCn::get_merge_targets(Variable *target) {
		// we should never get here because the parent should've already return
		// the Uptr to this VariableCn as the merge target, not recursing into
		// VariableCn
		std::cerr << "Weird: should not have asked VariableCn for its merge targets.\n";
		exit(1);
	}
	std::string FunctionCn::to_string() const {
		if (this->destination) {
			return "("
				+ utils::to_string<Variable *, program::to_string>(this->destination)
				+ ") FunctionCn { "
				+ program::to_string(this->function)
				+ " }";
		} else {
			return program::to_string(this->function);
		}
	}
	Set<Variable *> FunctionCn::get_vars_read() const {
		return {};
	}
	Vec<Uptr<ComputationNode> *> FunctionCn::get_merge_targets(Variable *target) {
		return {};
	}
	std::string LabelCn::to_string() const {
		if (this->destination) {
			return "("
				+ utils::to_string<Variable *, program::to_string>(this->destination)
				+ ") LabelCn { "
				+ program::to_string(this->jmp_dest)
				+ " }";
		} else {
			return program::to_string(this->jmp_dest);
		}
	}
	Set<Variable *> LabelCn::get_vars_read() const {
		return {};
	}
	Vec<Uptr<ComputationNode> *> LabelCn::get_merge_targets(Variable *target) {
		return {};
	}
	std::string MoveCn::to_string() const {
		return "("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") MoveCn { "
			+ this->source->to_string()
			+ " }";
	}
	Set<Variable*> MoveCn::get_vars_read() const {
		return this->source->get_vars_read();
	}
	Vec<Uptr<ComputationNode> *> MoveCn::get_merge_targets(Variable *target) {
		return program::get_merge_targets(this->source, target);
	}
	std::string BinaryCn::to_string() const {
		return "("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") "
			+ program::to_string(this->op)
			+ " { "
			+ this->lhs->to_string()
			+ ", "
			+ this->rhs->to_string()
			+ " }";
	}
	Set<Variable *> BinaryCn::get_vars_read() const {
		Set<Variable *> result;
		result.merge(this->lhs->get_vars_read());
		result.merge(this->rhs->get_vars_read());
		return result;
	}
	Vec<Uptr<ComputationNode> *> BinaryCn::get_merge_targets(Variable *target){
		Vec<Uptr<ComputationNode> *> sol = program::get_merge_targets(this->lhs, target);
		Vec<Uptr<ComputationNode> *> rhs_sol = program::get_merge_targets(this->rhs, target);
		sol += rhs_sol;
		return sol;
	}
	std::string CallCn::to_string() const {
		std::string result = "("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") CallCn { "
			+ this->callee->to_string()
			+ ", [";
		for (const Uptr<ComputationNode> &tree : this->arguments) {
			result += tree->to_string() + ", ";
		}
		result += "] }";
		return result;
	}
	Set<Variable *> CallCn::get_vars_read() const {
		Set<Variable *> sol;
		for (const Uptr<ComputationNode> &computation_tree: arguments) {
			sol.merge(computation_tree->get_vars_read());
		}
		return sol;
	}
	Vec<Uptr<ComputationNode> *> CallCn::get_merge_targets(Variable *target) {
		Vec<Uptr<ComputationNode> *> sol;
		for (Uptr<ComputationNode> &computation_tree : this->arguments) {
			sol += program::get_merge_targets(computation_tree, target);
		}
		return sol;
	}
	std::string LoadCn::to_string() const {
		return "("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") LoadCn { "
			+ this->address->to_string()
			+ " }";
	}
	Set<Variable *> LoadCn::get_vars_read() const {
		return this->address->get_vars_read();
	}
	Vec<Uptr<ComputationNode> *> LoadCn::get_merge_targets(Variable *target) {
		return program::get_merge_targets(this->address, target);
	}
	std::string StoreCn::to_string() const {
		return "("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") StoreCn { "
			+ this->address->to_string()
			+ ", "
			+ this->value->to_string()
			+ " }";
	}
	Set<Variable *> StoreCn::get_vars_read() const {
		Set<Variable *> sol;
		sol.merge(this->address->get_vars_read());
		sol.merge(this->value->get_vars_read());
		return sol;
	}
	Vec<Uptr<ComputationNode> *> StoreCn::get_merge_targets(Variable *target) {
		Vec<Uptr<ComputationNode> *> sol = program::get_merge_targets(this->address, target);
		Vec<Uptr<ComputationNode> *> value_sol = program::get_merge_targets(this->value, target);
		sol += value_sol;
		return sol;
	}
	std::string BranchCn::to_string() const {
		return "("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") BranchCn { "
			+ program::to_string(this->jmp_dest)
			+ ", "
			+ utils::to_string<Uptr<ComputationNode>, program::to_string>(this->condition)
			+ " }";
	}
	Set<Variable *> BranchCn::get_vars_read() const {
		if (this->condition.has_value()) {
			return (*this->condition)->get_vars_read();
		}
		return Set<Variable *>();
	}
	Vec<Uptr<ComputationNode> *> BranchCn::get_merge_targets(Variable *target) {
		if (this->condition.has_value()){
			return program::get_merge_targets(*this->condition, target);
		}
		return {};
	}
	std::string ReturnCn::to_string() const {
		return "("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") ReturnCn { "
			+ utils::to_string<Uptr<ComputationNode>, program::to_string>(this->value)
			+ " }";
	}
	Set<Variable *> ReturnCn::get_vars_read() const {
		if (this->value.has_value()) {
			return (*this->value)->get_vars_read();
		}
		return Set<Variable *>();
	}
	Vec<Uptr<ComputationNode> *> ReturnCn::get_merge_targets(Variable *target) {
		if (this->value.has_value()) {
			return program::get_merge_targets(*this->value, target);
		}
		return {};
	}
	std::string to_string(const Uptr<ComputationNode> &node) {
		return node->to_string();
	}
	std::string to_string(const ComputationNode &node) {
		return node.to_string();
	}

	ComputationTreeBox::ComputationTreeBox(const Instruction &inst) :
		root_nullable { inst.to_computation_tree() },
		has_load { static_cast<bool>(dynamic_cast<LoadCn *>(this->root_nullable.get())) },
		has_store { static_cast<bool>(dynamic_cast<StoreCn *>(this->root_nullable.get())) }
	{}
	bool ComputationTreeBox::merge(ComputationTreeBox &other) {
		if (!other.get_var_written()) {
			std::cerr << "can't merge these two trees because the child tree has no destination.\n";
			exit(1);
		}
		Variable *var = *other.get_var_written();

		Vec<Uptr<ComputationNode> *> merge_targets = this->root_nullable->get_merge_targets(var);
		if (merge_targets.size() != 1) {
			// FUTURE I don't know how to merge if there is more than one merge target
			return false;
		}

		// at this point, we will go through with the merge

		if (other.get_has_load()) {
			this->has_load = true;
		}
		if (other.get_has_store()) {
			this->has_store = true;
		}

		Uptr<ComputationNode> *merge_target = merge_targets[0];
		Uptr<ComputationNode> *merge_child = &other.root_nullable;
		if (MoveCn *move_node = dynamic_cast<MoveCn *>(merge_child->get())) {
			// optimize away a child move node; e.g. a <- b <- c becomes a <- c
			merge_child = &move_node->source;
		} else if (is_dynamic_type<NumberCn, FunctionCn, LabelCn>(*merge_child->get())) {
			// remove an intermediate variable if the node is a constant
			(*merge_child)->destination.reset();
		}
		*merge_target = mv(*merge_child);
		other.root_nullable = nullptr;
		return true;
	}

	BasicBlock::BasicBlock() {} // default-initialize everything
	// implementations for BasicBlock::generate_computation_trees and
	// update_in_out_sets are in analyze_trees.cpp
	std::string BasicBlock::to_string() const {
		std::string result = "-----\n";
		result += "in: ";
		for (Variable *var : this->var_liveness.in_set) {
			result += var->get_name() + ", ";
		}
		result += "\nout: ";
		for (Variable *var : this->var_liveness.out_set) {
			result += var->get_name() + ", ";
		}
		result += "\ntrees:\n";
		for (const ComputationTreeBox &tree_box : this->tree_boxes) {
			result += tree_box.get_tree()->to_string() + "\n";
		}
		return result;
	}
	BasicBlock::Builder::Builder() :
		fetus { Uptr<BasicBlock>(new BasicBlock()) },
		succ_block_refs {},
		must_end { false },
		falls_through { true }
	{}
	Uptr<BasicBlock> BasicBlock::Builder::get_result(BasicBlock *successor_nullable) {
		if (this->succ_block_refs.has_value()) {
			Opt<BasicBlock *> another_successor = (*this->succ_block_refs)->get_referent();
			if (another_successor) {
				this->fetus->succ_blocks.push_back(*another_successor);
			} else {
				std::cerr << "Error: control flow goes to unknown label: " << (*this->succ_block_refs)->to_string() << "\n";
				exit(1);
			}
		}
		if (this->falls_through && successor_nullable) {
			this->fetus->succ_blocks.push_back(successor_nullable);
		} // TODO shouldn't it be an error to fall through without a successor? bc we must end with a return?

		return mv(this->fetus);
	}
	Pair<BasicBlock *, Opt<std::string>> BasicBlock::Builder::get_fetus_and_name() {
		return {
			this->fetus.get(),
			this->fetus->get_name().size() > 0 ? this->fetus->get_name() : Opt<std::string>()
		};
	}
	bool BasicBlock::Builder::add_next_instruction(Uptr<Instruction> &&inst) {
		if (this->must_end) {
			return false;
		}

		if (InstructionLabel *inst_label = dynamic_cast<InstructionLabel *>(inst.get())) {
			if (this->fetus->raw_instructions.empty()) {
				this->fetus->name = inst_label->get_name();
			} else {
				return false;
			}
		}
		auto [falls_through, yields_control, jmp_dest] = inst->get_control_flow();
		this->falls_through = falls_through;
		if (!falls_through || yields_control) {
			this->must_end = true;
		}
		if (jmp_dest) {
			this->must_end = true;
			this->succ_block_refs = *jmp_dest;
		}
		this->fetus->raw_instructions.push_back(mv(inst));
		return true;
	}
	std::string to_string(BasicBlock *const &block) {
		return block->get_name();
	}

	void AggregateScope::set_parent(AggregateScope &parent) {
		this->variable_scope.set_parent(parent.variable_scope);
		this->label_scope.set_parent(parent.label_scope);
		this->l3_function_scope.set_parent(parent.l3_function_scope);
		this->external_function_scope.set_parent(parent.external_function_scope);
	}

	std::string Variable::to_string() const {
		return "%" + this->name;
	}

	std::string to_string(Variable *const &variable) {
		return variable->to_string();
	}

	bool L3Function::verify_argument_num(int num) const {
		return num == this->parameter_vars.size();
	}
	std::string L3Function::to_string() const {
		std::string result = "define @" + this->name + "(";
		for (const Variable *var : this->parameter_vars) {
			result += "%" + var->get_name() + ", ";
		}
		result += ") {\n";
		for (const Uptr<BasicBlock> &block : this->blocks) {
			result += block->to_string();
		}
		result += "}";
		return result;
	}
	L3Function::Builder::Builder()
		// default-construct everything
	{
		this->block_builders.emplace_back(); // start with at least one block
	}
	Pair<Uptr<L3Function>, AggregateScope> L3Function::Builder::get_result() {
		// bind all the blocks to the scope
		for (BasicBlock::Builder &builder : this->block_builders) {
			auto [block_ptr, maybe_name] = builder.get_fetus_and_name();
			if (maybe_name) {
				this->agg_scope.label_scope.resolve_item(mv(*maybe_name), block_ptr);
			}
		}

		// at this point, all the BasicBlock::Builders should be completed
		// get everything out of the block builders
		Vec<Uptr<BasicBlock>> blocks;
		BasicBlock *next_block_nullable = nullptr;
		for (auto it = this->block_builders.rbegin(); it != this->block_builders.rend(); ++it) {
			Uptr<BasicBlock> current_block = it->get_result(next_block_nullable);
			BasicBlock *temp = current_block.get();
			blocks.push_back(mv(current_block));
			next_block_nullable = temp;
		}
		std::reverse(blocks.begin(), blocks.end());

		// bind all unbound variables to new variable items
		for (std::string name : this->agg_scope.variable_scope.get_free_names()) {
			Uptr<Variable> var_ptr = mkuptr<Variable>(name);
			this->agg_scope.variable_scope.resolve_item(mv(name), var_ptr.get());
			this->vars.emplace_back(mv(var_ptr));
		}

		// return the result
		return std::make_pair(
			Uptr<L3Function>(new L3Function( // using constructor instead of make_unique because L3Function's private constructor
				mv(this->name),
				mv(blocks),
				mv(this->vars),
				mv(this->parameter_vars)
			)),
			mv(this->agg_scope)
		);
	}
	void L3Function::Builder::add_name(std::string name) {
		this->name = mv(name);
	}
	void L3Function::Builder::add_next_instruction(Uptr<Instruction> &&inst) {
		inst->bind_to_scope(this->agg_scope);
		bool success = this->block_builders.back().add_next_instruction(mv(inst));
		if (!success) {
			// The contract of BasicBlock::Builder stipulates that a failure
			// to add an instruction won't move out of the passed-in pointer,
			// so `inst` is still valid :D
			this->block_builders.emplace_back();
			success = this->block_builders.back().add_next_instruction(mv(inst));
			assert(success);
		}
	}
	void L3Function::Builder::add_parameter(std::string var_name) {
		Uptr<Variable> var_ptr = mkuptr<Variable>(var_name);
		this->agg_scope.variable_scope.resolve_item(mv(var_name), var_ptr.get());
		this->parameter_vars.push_back(var_ptr.get());
		this->vars.emplace_back(mv(var_ptr));
	}

	bool ExternalFunction::verify_argument_num(int num) const {
		for (int valid_num : this->valid_num_arguments) {
			if (num == valid_num) {
				return true;
			}
		}
		return false;
	}
	std::string ExternalFunction::to_string() const {
		return "[[function std::" + this->name + "]]";
	}

	std::string to_string(Function *const &function) {
		return "[[function " + function->get_name() + "]]";
	}

	std::string Program::to_string() const {
		std::string result;
		for (const Uptr<L3Function> &function : this->l3_functions) {
			result += function->to_string() + "\n";
		}
		return result;

	}
	Program::Builder::Builder() :
		// default-construct everything else
		main_function_ref { mkuptr<ItemRef<L3Function>>("main") }
	{
		for (Uptr<ExternalFunction> &function_ptr : generate_std_functions()) {
			this->agg_scope.external_function_scope.resolve_item(
				function_ptr->get_name(),
				function_ptr.get()
			);
			this->external_functions.emplace_back(mv(function_ptr));
		}
		this->agg_scope.l3_function_scope.add_ref(*this->main_function_ref);
	}
	Uptr<Program> Program::Builder::get_result() {
		// TODO verify no free names

		// return the result
		return Uptr<Program>(new Program(
			mv(this->l3_functions),
			mv(this->external_functions),
			mv(this->main_function_ref)
		));
	}
	void Program::Builder::add_l3_function(Uptr<L3Function> &&function, AggregateScope &fun_scope) {
		fun_scope.set_parent(this->agg_scope);
		this->agg_scope.l3_function_scope.resolve_item(function->get_name(), function.get());
		this->l3_functions.push_back(mv(function));
	}

	Vec<Uptr<ExternalFunction>> generate_std_functions() {
		Vec<Uptr<ExternalFunction>> result;
		result.push_back(mkuptr<ExternalFunction>("input", Vec<int> { 0 }));
		result.push_back(mkuptr<ExternalFunction>("print", Vec<int> { 1 }));
		result.push_back(mkuptr<ExternalFunction>("allocate", Vec<int> { 2 }));
		result.push_back(mkuptr<ExternalFunction>("tuple-error", Vec<int> { 3 }));
		result.push_back(mkuptr<ExternalFunction>("tensor-error", Vec<int> { 1, 3, 4 }));
		return result;
	}
}
