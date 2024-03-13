#include "program.h"

namespace IR::program {
	using namespace std_alias;

	std::string encode_expr(const std::string &encode_to, const std::string &target, std::string &prefix){
		std::string sol = "\t" + encode_to + " <- " + target + " << 1\n";
		sol += "\t" + encode_to + " <- " + encode_to + " + 1\n";
		return sol;
	}

	std::string decode_expr(const std::string &decode_to, const std::string &target, std::string &prefix) {
		return "\t" + decode_to + " <- " + target + " >> 1\n";
	}
	std::string make_new_var_name(std::string prefix, int counter) {
		return "%" + prefix + std::to_string(counter);
	}

	std::pair<A_type, int64_t> str_to_a_type(const std::string& str) {
		static const std::map<std::string, A_type> stringToTypeMap = {
			{"int64", A_type::int64},
			{"code", A_type::code},
			{"tuple", A_type::tuple},
			{"void", A_type::void_type}
		};
		int transitionIndex = -1;
		for (int i = 0; i < str.size(); ++i) {
			if (str[i] == '[') {
				transitionIndex = i;
				break;
			}
		}
		A_type sol_type;
		if (transitionIndex == -1) {
			sol_type = stringToTypeMap.find(str)->second;
		} else {
			sol_type = stringToTypeMap.find(str.substr(0, transitionIndex))->second;
		}
		int bracketPairs = 0;
		for (int i = transitionIndex; i < str.size(); ++i) {
			if (str[i] == '[' && i + 1 < str.size() && str[i + 1] == ']') {
				++bracketPairs;
				++i; // Skip the next character as it is part of the counted pair
			}
		}
		return std::make_pair(sol_type, bracketPairs);
	}
	std::string to_string(A_type t) {
		switch (t) {
			case A_type::int64: return "int64";
			case A_type::code: return "code";
			case A_type::tuple: return "tuple";
			case A_type::void_type : return "void";
			default: return "unknown";
		}
	}
	
	Type::Type(const std::string& str){
		auto[a_type, num_dim] = str_to_a_type(str);
		this->a_type = a_type;
		this->num_dim = num_dim;
	}
	std::string Type::to_string() const {
		static const std::map< A_type, std::string> type_to_str = {
			{A_type::int64, "int64"},
			{A_type::code, "code"},
			{A_type::tuple, "tuple"},
			{A_type::void_type, "void"}
		};
		std::string sol = type_to_str.find(this->a_type)->second;;


		for(int i = 0; i < this->num_dim; i++) {
			sol += "[]";
		}
		return sol;
	}

	Operator str_to_op(std::string str) {
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
	std::string op_to_string(Operator op) {
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
	
	template<> std::string ItemRef<Variable>::to_string() const {
		std::string result = "%" + this->get_ref_name();
		if (!this->referent_nullable) {
			result += "?";
		}
		return result;
	}
	template<> void ItemRef<Variable>::bind_to_scope(AggregateScope &agg_scope){
		agg_scope.variable_scope.add_ref(*this);
	}
	template<> std::string ItemRef<Variable>::to_l3_expr(std::string prefix) {
		return "%" + this->get_ref_name();
	}
	template<> std::string ItemRef<BasicBlock>::to_string() const {
		std::string result = ":" + this->get_ref_name();
		if (!this->referent_nullable) {
			result += "?";
		}
		return result;
	}
	template<> void ItemRef<BasicBlock>::bind_to_scope(AggregateScope &agg_scope){
		agg_scope.basic_block_scope.add_ref(*this);
	}
	template<>std::string ItemRef<BasicBlock>::to_l3_expr(std::string prefix) {
		return ":" + this->get_ref_name();
	}
	template<> std::string ItemRef<IRFunction>::to_string() const {
		std::string result = "@" + this->get_ref_name();
		if (!this->referent_nullable) {
			result += "?";
		}
		return result;
	}
	template<> void ItemRef<IRFunction>::bind_to_scope(AggregateScope &agg_scope){
		agg_scope.ir_function_scope.add_ref(*this);
	}
	template<> std::string ItemRef<IRFunction>::to_l3_expr(std::string prefix) {
		return "@" + this->get_ref_name();
	}
	template<> std::string ItemRef<ExternalFunction>::to_string() const {
		std::string result = this->get_ref_name();
		if (!this->referent_nullable) {
			result += "?";
		}
		return result;
	}
	template<> void ItemRef<ExternalFunction>::bind_to_scope(AggregateScope &agg_scope){
		agg_scope.external_function_scope.add_ref(*this);
	}
	template<> std::string ItemRef<ExternalFunction>::to_l3_expr(std::string prefix) {
		return this->get_ref_name();
	}

	std::string Variable::to_string() const {
		return "%" + this->get_name();
	}

	std::string BinaryOperation::to_string() const {
		return this->lhs->to_string()
			+ " " + program::op_to_string(this->op)
			+ " " + this->rhs->to_string();
	}
	void BinaryOperation::bind_to_scope(AggregateScope &agg_scope) {
		this->lhs->bind_to_scope(agg_scope);
		this->rhs->bind_to_scope(agg_scope);
	}
	std::string BinaryOperation::to_l3_expr(std::string prefix) {
		std::string sol = this->lhs->to_l3_expr(prefix) + " ";
		sol += op_to_string(this->op) + " ";
		sol += this->rhs->to_l3_expr(prefix);
		return sol;
	}
	std::string FunctionCall::to_string() const {
		std::string result = "call " + this->callee->to_string() + "(";
		for (const Uptr<Expr> &argument : this->arguments) {
			result += argument->to_string() + ", ";
		}
		result += ")";
		return result;
	}
	void FunctionCall::bind_to_scope(AggregateScope &agg_scope) {
		this->callee->bind_to_scope(agg_scope);
		for (Uptr<Expr> &arg : this->arguments) {
			arg->bind_to_scope(agg_scope);
		}
	}
	std::string FunctionCall::to_l3_expr(std::string prefix) {
		std::string sol = "call ";
		sol += this->callee->to_l3_expr(prefix) + "(";
		bool first = true;
		for (Uptr<Expr> &arg: this->arguments){
			if (first){
				first = false;
				sol += arg->to_l3_expr(prefix);
			} else {
				sol += ", " + arg->to_l3_expr(prefix);
			}
		}
		sol += ")";
		return sol;
	}
	
	std::string MemoryLocation::to_string() const {
		std::string sol = "" + this->base->to_string();
		for (const auto &expr : this->dimensions) {
			sol += "[" + expr->to_string() + "]";
		}
		return sol;
	}
	void MemoryLocation::bind_to_scope(AggregateScope &agg_scope) {
		this->base->bind_to_scope(agg_scope);
		for (const auto &expr : this->dimensions) {
			expr->bind_to_scope(agg_scope);
		}
	}
	std::string MemoryLocation::to_l3(std::string prefix) {
		int n = this->dimensions.size();
		if (this->base->get_referent().value()->get_type().get_a_type() == A_type::tuple) {
			std::string dimension = this->dimensions[0]->to_l3_expr(prefix);
			std::string sol = "\t%" + prefix + "sol <- 1 + " + dimension + "\n"; 
			sol += "\t%" + prefix + "sol <- 8 * %" + prefix + "sol\n";
			sol += "\t%" + prefix + "sol <- %" + prefix + "sol + " + this->base->to_l3_expr(prefix) + "\n";
			return sol;
		}
		std::string base = this->base->to_l3_expr(prefix);
		std::string sol = "";
		int counter = 0;
		for (int i = 0; i < n; i ++) {
			std::string new_var = make_new_var_name(prefix, counter);
			sol += "\t" + new_var + " <- " + std::to_string((i + 1) * 8) + " + " + base + "\n";
			sol += "\t" +  new_var + " <- load " + new_var + "\n";
			sol += decode_expr(new_var, new_var, prefix);
			counter++;
		}
		std::string accum = make_new_var_name(prefix, counter);
		sol += "\t" + accum + " <- 0" + "\n";
		counter++;
		for (int i = 0; i < n; i++) {
			std::string curr_row = make_new_var_name(prefix, counter);
			counter++;
			sol += "\t" + curr_row + " <- 1\n";
			for (int j = i + 1; j < n; j++) {
				std::string multiply = make_new_var_name(prefix, j);
				sol += "\t" + curr_row + " <- " + curr_row + " * " + multiply + "\n";
			}
			sol += "\t" + curr_row + " <- " + curr_row + " * " + this->dimensions[i]->to_l3_expr(prefix) + "\n";
			sol += "\t" + accum + " <- " + accum + " + " + curr_row + "\n";  
		}
		sol += "\t" + accum + " <- " + accum + " + " + std::to_string(n + 1) + "\n";
		sol += "\t" + accum + " <- " + accum + " * 8\n";
		sol += "\t" + accum + " <- " + accum + " + " + base + "\n"; 
		sol += "\t%" + prefix + "sol <- " + accum + "\n";
		return sol;
	}
	std::string ArrayDeclaration::to_string() const {
		std::string sol = "new Array (";
		for (const auto &arg : this->args) {
			sol += arg->to_string() + ", ";
		}
		sol += ")";
		return sol;
	}
	void ArrayDeclaration::bind_to_scope(AggregateScope &agg_scope) {
		for (const auto &arg : this->args) {
			arg->bind_to_scope(agg_scope);
		}
	}
	std::string Length::to_string() const {
		std::string sol = "Length " + this->var->to_string();
		if (this->dimension.has_value()){
			sol += " " + this->var->to_string();
		}
		return sol;
	}
	void Length::bind_to_scope(AggregateScope &agg_scope) {
		this->var->bind_to_scope(agg_scope);
	}

	std::string InstructionAssignment::to_string() const {
		std::string sol = "";
		if (this->maybe_dest.has_value()) {
			sol += this->maybe_dest.value()->to_string();
			sol += " <- ";
		}
		sol += this->source->to_string();
		return sol;
	}
	void InstructionAssignment::bind_to_scope(AggregateScope &agg_scope){
		if (this->maybe_dest.has_value()) {
			this->maybe_dest.value()->bind_to_scope(agg_scope);
		}
		this->source->bind_to_scope(agg_scope);
	}
	std::string InstructionAssignment::to_l3_inst(std::string prefix) {
		std::string sol = "\t";
		if (this->maybe_dest.has_value()) {
			sol += this->maybe_dest.value()->to_l3_expr(prefix);
			sol += " <- ";
		}
		sol += this->source->to_l3_expr(prefix);
		return sol + "\n";
	}
	std::string InstructionDeclaration::to_string() const {
		std::string sol =  this->var->get_type().to_string() + " ";
		sol += this->var->to_string();
		return sol;
	}
	void InstructionDeclaration::bind_to_scope(AggregateScope &agg_scope){
	}
	void InstructionDeclaration::resolver(AggregateScope &agg_scope) {
		agg_scope.variable_scope.resolve_item(this->var->get_name(), this->var.get());
	}
	std::string InstructionDeclaration::to_l3_inst(std::string prefix) {
		return "";
	}
	std::string InstructionStore::to_string() const {
		return this->dest->to_string() + " <- " + this->source->to_string();
	}
	void InstructionStore::bind_to_scope(AggregateScope &agg_scope) {
		this->dest->bind_to_scope(agg_scope);
		this->source->bind_to_scope(agg_scope);
	}
	std::string InstructionStore::to_l3_inst(std::string prefix) {
		std::string sol = "";
		sol += this->dest->to_l3(prefix);
		sol += "\tstore %" + prefix + "sol <- " + this->source->to_l3_expr(prefix) + "\n";
		return sol;
	}
	std::string InstructionLoad::to_string() const {
		return this->dest->to_string() + " <- " + this-> source->to_string();
	}
	void InstructionLoad::bind_to_scope(AggregateScope &agg_scope) {
		this->dest->bind_to_scope(agg_scope);
		this->source->bind_to_scope(agg_scope);
	}
	std::string InstructionLoad::to_l3_inst(std::string prefix) {
		std::string sol = "";
		sol += this->source->to_l3(prefix);
		sol += "\t" + this->dest->to_l3_expr(prefix) + " <- load %" + prefix + "sol\n";
		return sol;
	}
	std::string InstructionInitializeArray::to_string() const {
		std::string sol = this->dest->to_string();
		sol += " <- ";
		sol += this->newArray->to_string();
		return sol;
	}
	void InstructionInitializeArray::bind_to_scope(AggregateScope &agg_scope) {
		this->dest->bind_to_scope(agg_scope);
		this->dest->get_referent().value()->set_args(this->newArray->get_args());
	}
	std::string InstructionInitializeArray::to_l3_inst(std::string prefix) {
		Vec<Uptr<Expr>> &args = this->newArray->get_args();
		if (this->dest->get_referent().value()->get_type().get_a_type() == A_type::tuple) {
			std::string sol = "";
			Uptr<Expr> &arg = args[0];
			sol += "\t" + this->dest->to_l3_expr(prefix) + " <- call allocate(" + arg->to_l3_expr(prefix) + ", 1)\n";
			return sol;
		}
		int counter = 1;
		std::string base = "%" + prefix + std::to_string(0);
		std::string sol = "\t" + base + " <- 1\n";
		for(Uptr<Expr> &arg: args){
			std::string new_var = "%" + prefix + std::to_string(counter);
			sol += decode_expr(new_var, arg->to_l3_expr(prefix), prefix);
			sol += "\t" + base + " <- " + base + " * " + new_var + "\n";
			counter++;
		}
		sol += "\t" + base + " <- " + base + " + " + std::to_string(args.size()) + "\n";
		sol += encode_expr(base, base, prefix);
		sol += "\t" + this->dest->to_l3_expr(prefix) + " <- call allocate(" + base + ", 1)\n";
		int index = 1;
		for(Uptr<Expr> &arg: args){
			std::string new_var = "%" + prefix + std::to_string(counter);
			sol += "\t" + new_var + " <- " + this->dest->to_l3_expr(prefix) + " + " + std::to_string(index * 8) + "\n";
			sol += "\tstore " + new_var + " <- " + arg->to_l3_expr(prefix) + "\n";
			counter++;
			index++;
		}
		return sol;
	}
	std::string InstructionLength::to_string() const {
		return this->dest->to_string() + " <- " + this->source->to_string();
	}
	void InstructionLength::bind_to_scope(AggregateScope &agg_scope) {
		this->dest->bind_to_scope(agg_scope);
		this->source->bind_to_scope(agg_scope);
	}
	std::string InstructionLength::to_l3_inst(std::string prefix) {
		int64_t dim = 0;
		if (this->source->get_dim().has_value()) {
			dim = this->source->get_dim().value();
		} else {
			std::string sol = "\t" + this->dest->to_l3_expr(prefix) + " <- load " + this->source->get_var().to_l3_expr(prefix) + "\n";
			sol += encode_expr(this->dest->to_l3_expr(prefix), this->dest->to_l3_expr(prefix), prefix);
			return sol;
		}
		dim += 1;
		std::string new_var = "%" + prefix + "0";
		std::string sol = "\t" + new_var + " <- " + std::to_string(dim) + " * 8\n";
		sol += "\t" + new_var + " <- " + this->source->get_var().to_l3_expr(prefix) + " + " + new_var + "\n";
		sol += "\t" + this->dest->to_l3_expr(prefix) + " <- load " + new_var + "\n";
		return sol;
	}

	void TerminatorBranchOne::bind_to_scope(AggregateScope &agg_scope) {
		this->bb_ref->bind_to_scope(agg_scope);
	}
	std::string TerminatorBranchOne::to_string() const {
		return "br" + this->bb_ref->to_string();
	}
	Vec<Pair<BasicBlock *, double>> TerminatorBranchOne::get_successor() {
		Vec<Pair<BasicBlock *, double>> sol;
		sol.push_back(std::make_pair(this->bb_ref->get_referent().value(), 1.0));
		return sol;
	}
	std::string TerminatorBranchOne::to_l3_terminator(std::string prefix, Trace &my_trace, BasicBlock *my_bb) {
		bool printMe = true;
		bool isNext = false;
		for (BasicBlock *bb: my_trace.block_sequence) {
			if (isNext && this->bb_ref->get_referent().value() == bb){
				printMe = false;
			}
			if (bb == my_bb) {
				isNext = true;
			} else {
				isNext = false;
			}
		}
		if (printMe) {
			return "\tbr " + this->bb_ref->to_l3_expr(prefix) + "\n";
		}
		return "";
	}
	void TerminatorBranchTwo::bind_to_scope(AggregateScope &agg_scope) {
		this->condition->bind_to_scope(agg_scope);
		this->branchTrue->bind_to_scope(agg_scope);
		this->branchFalse->bind_to_scope(agg_scope);
	}
	std::string TerminatorBranchTwo::to_string() const {
		std::string sol = "br " + this->condition->to_string();
		sol += " " + this->branchTrue->to_string();
		sol += " " + this->branchFalse->to_string()  + "\n";
		return sol;
	}
	Vec<Pair<BasicBlock *, double>> TerminatorBranchTwo::get_successor() {
		Vec<Pair<BasicBlock *, double>> sol;
		sol.emplace_back(std::make_pair(this->branchTrue->get_referent().value(), 0.7));
		sol.emplace_back(std::make_pair(this->branchFalse->get_referent().value(), 0.3));
		return sol;
	}
	std::string TerminatorBranchTwo::to_l3_terminator(std::string prefix, Trace &my_trace, BasicBlock *my_bb) {
		bool printTrue = true;
		bool isNext = false;
		bool printFalse = true;
		for (BasicBlock *bb: my_trace.block_sequence) {
			if (isNext && this->branchTrue->get_referent().value() == bb){
				printTrue = false;
			}
			if (isNext && this->branchFalse->get_referent().value() == bb){
				printFalse = false;
			}
			if (my_bb == bb) {
				isNext = true;
			} else {
				isNext = false;
			}
		}
		std::string sol = "";
		if (printTrue && printFalse) {
			sol += "\tbr " + this->condition->to_l3_expr(prefix) + " " + this->branchTrue->to_l3_expr(prefix) + "\n";
			sol += "\tbr " + this->branchFalse->to_l3_expr(prefix) + "\n";
			return sol;
		}
		if (printTrue) {
			sol += "\tbr " + this->condition->to_l3_expr(prefix) + " " + this->branchTrue->to_l3_expr(prefix) + "\n";
			return sol;
		} else {
			sol += "\t%" + prefix + "t <- " + this->condition->to_l3_expr(prefix) + "\n";
			sol += "\t%" + prefix + "t <- %" + prefix + "t = 1\n"; 
			sol += "\t%" + prefix + "t <- %" + prefix + "t = 0\n"; 
			sol += "\tbr %" + prefix + "t " + this->branchFalse->to_l3_expr(prefix) + "\n";
			return sol;
		}
	}
	void TerminatorReturnVar::bind_to_scope(AggregateScope &agg_scope) {
		this->ret_expr->bind_to_scope(agg_scope);
	}
	std::string TerminatorReturnVar::to_string() const {
		return "return" + this->ret_expr->to_string();
	} 
	std::string TerminatorReturnVar::to_l3_terminator(std::string prefix, Trace &my_trace, BasicBlock *my_bb) {
		return "\treturn " + this->ret_expr->to_l3_expr(prefix) + "\n";
	}
	

	Uptr<BasicBlock> BasicBlock::Builder::get_result() {
		return Uptr<BasicBlock>(new BasicBlock(
			mv(this->name),
			mv(this->inst),
			mv(this->te)
		));
	}
	void BasicBlock::Builder::add_name(std::string name){
		this->name = mv(name);
	}
	void BasicBlock::Builder::add_instruction(Uptr<Instruction> &&inst, AggregateScope &agg_scope) {
		inst->resolver(agg_scope);
		inst->bind_to_scope(agg_scope);
		this->inst.push_back(mv(inst));
	}
	void BasicBlock::Builder::add_terminator(Uptr<Terminator> &&te, AggregateScope &agg_scope) {
		te->bind_to_scope(agg_scope);
		this->te = mv(te);
	}
	std::string BasicBlock::to_string() const {
		std::string sol = "\t:" + this->name + "\n";
		for (const Uptr<Instruction> &inst : this->inst) {
			sol += "\t" + inst->to_string() + "\n";
		}
		sol += "\t"+ this->te->to_string() + "\n";
		return sol;
	}
	void BasicBlock::bind_to_scope(AggregateScope &scope){
		for (auto const& inst: this->inst) {
			inst->bind_to_scope(scope);
		}
		this->te->bind_to_scope(scope);
	}

	void AggregateScope::set_parent(AggregateScope &parent) {
		this->variable_scope.set_parent(parent.variable_scope);
		this->basic_block_scope.set_parent(parent.basic_block_scope);
		this->ir_function_scope.set_parent(parent.ir_function_scope);
		this->external_function_scope.set_parent(parent.external_function_scope);
	}

	Uptr<IRFunction> IRFunction::Builder::get_result() {
		for (auto &bb: this->basic_blocks){
			bb->set_successors(bb->get_terminator()->get_successor());
		}
		return Uptr<IRFunction>(new IRFunction(
			mv(this->name),
			mv(this->ret_type),
			mv(this->basic_blocks),
			mv(this->vars),
			mv(this->parameter_vars),
			mv(this->agg_scope)
		));
	}
	void IRFunction::Builder::add_name(std::string name) {
		this->name = mv(name);
	}
	void IRFunction::Builder::add_ret_type(Type t){
		this->ret_type = mv(t);
	}
	void IRFunction::Builder::add_block(Uptr<BasicBlock> &&bb){
		bb->bind_to_scope(this->agg_scope);
		this->agg_scope.basic_block_scope.resolve_item(bb->get_name(), bb.get());
		this->basic_blocks.push_back(mv(bb));
	}
	void IRFunction::Builder::add_parameter(Type type, std::string var_name){
		Uptr<Variable> var_ptr = mkuptr<Variable>(var_name, type);
		this->agg_scope.variable_scope.resolve_item(mv(var_name), var_ptr.get());
		this->parameter_vars.push_back(var_ptr.get());
		this->vars.emplace_back(mv(var_ptr));
	}
	std::string IRFunction::to_string() const {
		std::string result = "define @" + this->name + "(";
		for (const Variable *var : this->parameter_vars) {
			result += "%" + var->get_name() + ", ";
		}
		result += ") {\n";
		for (const Uptr<BasicBlock> &block : this->blocks) {
			result += block->to_string() + "\n";
		}
		result += "}";
		return result;
	}

	std::string ExternalFunction::to_string() const {
		return "[[function std::" + this->name + "]]";
	}

	Program::Builder::Builder(){
		for (Uptr<ExternalFunction> &function_ptr : generate_std_functions()) {
			this->agg_scope.external_function_scope.resolve_item(
				function_ptr->get_name(),
				function_ptr.get()
			);
			this->external_functions.emplace_back(mv(function_ptr));
		}
	}
	void Program::Builder::add_ir_function(Uptr<IRFunction> &&function){
		function->get_scope().set_parent(this->agg_scope);
		this->agg_scope.ir_function_scope.resolve_item(function->get_name(), function.get());
		this->ir_functions.push_back(mv(function));
	}
	Uptr<Program> Program::Builder::get_result(){
		return Uptr<Program>(new Program(
			mv(this->ir_functions),
			mv(this->external_functions)
		));
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
	std::string Program::to_string() const {
		std::string result = "";
		for (const Uptr<IRFunction> &function : this->ir_functions) {
			result += function->to_string() + "\n";
		}
		return result;
	}
}