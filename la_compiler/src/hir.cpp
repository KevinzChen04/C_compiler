#include "hir.h"
#include "std_alias.h"
#include "utils.h"

namespace La::hir {
	using namespace std_alias;

	template<> void ItemRef<Nameable>::bind_to_scope(Scope<Nameable> &scope) {
		scope.add_ref(*this);
	}
	template<> std::string ItemRef<Nameable>::to_string() const {
		std::string result = this->get_ref_name();
		if (!this->referent_nullable) {
			result += "?";
		}
		return result;
	}

	void NumberLiteral::bind_to_scope(Scope<Nameable> &scope) {
		// empty bc literals make no reference to names
	}
	std::string NumberLiteral::to_string() const {
		return std::to_string(this->value);
	}

	mir::Operator str_to_op(std::string_view str) {
		static const Map<std::string, mir::Operator> map {
			{ "<", mir::Operator::lt },
			{ "<=", mir::Operator::le },
			{ "=", mir::Operator::eq },
			{ ">=", mir::Operator::ge },
			{ ">", mir::Operator::gt },
			{ "+", mir::Operator::plus },
			{ "-", mir::Operator::minus },
			{ "*", mir::Operator::times },
			{ "&", mir::Operator::bitwise_and },
			{ "<<", mir::Operator::lshift },
			{ ">>", mir::Operator::rshift }
		};
		return map.find(str)->second;
	}

	void BinaryOperation::bind_to_scope(Scope<Nameable> &scope) {
		this->lhs->bind_to_scope(scope);
		this->rhs->bind_to_scope(scope);
	}
	std::string BinaryOperation::to_string() const {
		return this->lhs->to_string()
			+ " " + mir::to_string(this->op)
			+ " " + this->rhs->to_string();
	}

	void IndexingExpr::bind_to_scope(Scope<Nameable> &scope) {
		this->target->bind_to_scope(scope);
		for (Uptr<Expr> &index : this->indices) {
			index->bind_to_scope(scope);
		}
	}
	std::string IndexingExpr::to_string() const {
		std::string result = this->target->to_string();
		for (const Uptr<Expr> &index : this->indices) {
			result += "[" + index->to_string() + "]";
		}
		return result;
	}

	void LengthGetter::bind_to_scope(Scope<Nameable> &scope) {
		this->target->bind_to_scope(scope);
		if (this->dimension.has_value()) {
			this->dimension.value()->bind_to_scope(scope);
		}
	}
	std::string LengthGetter::to_string() const {
		std::string result = "length " + this->target->to_string();
		if (this->dimension.has_value()) {
			result += " " + this->dimension.value()->to_string();
		}
		return result;
	}

	void FunctionCall::bind_to_scope(Scope<Nameable> &scope) {
		this->callee->bind_to_scope(scope);
		for (Uptr<Expr> &arg : this->arguments) {
			arg->bind_to_scope(scope);
		}
	}
	std::string FunctionCall::to_string() const {
		std::string result = this->callee->to_string() + "(";
		result += utils::format_comma_delineated_list(
			this->arguments,
			[](const Uptr<Expr> &argument){ return argument->to_string(); }
		);
		result += ")";
		return result;
	}

	void NewArray::bind_to_scope(Scope<Nameable> &scope) {
		for (Uptr<Expr> &dim_length : this->dimension_lengths) {
			dim_length->bind_to_scope(scope);
		}
	}
	std::string NewArray::to_string() const {
		std::string result = "new Array(";
		result += utils::format_comma_delineated_list(
			this->dimension_lengths,
			[](const Uptr<Expr> &dim_length){ return dim_length->to_string(); }
		);
		result += ")";
		return result;
	}

	void NewTuple::bind_to_scope(Scope<Nameable> &scope) {
		this->length->bind_to_scope(scope);
	}
	std::string NewTuple::to_string() const {
		return "new Tuple(" + this->length->to_string() + ")";
	}

	void InstructionDeclaration::bind_to_scope(Scope<Nameable> &scope) {
		this->variable->bind_to_scope(scope);
	}
	std::string InstructionDeclaration::to_string() const {
		return this->type.to_ir_syntax() + " " + this->variable_name;
	}

	void InstructionAssignment::bind_to_scope(Scope<Nameable> &scope) {
		if (this->maybe_dest.has_value()) {
			this->maybe_dest.value()->bind_to_scope(scope);
		}
		this->source->bind_to_scope(scope);
	}
	std::string InstructionAssignment::to_string() const {
		std::string result;
		if (this->maybe_dest.has_value()) {
			result += this->maybe_dest.value()->to_string() + " <- ";
		}
		result += this->source->to_string();
		return result;
	}

	void InstructionLabel::bind_to_scope(Scope<Nameable> &scope) {}
	std::string InstructionLabel::to_string() const {
		return ":" + this->label_name;
	}

	void InstructionReturn::bind_to_scope(Scope<Nameable> &scope) {
		if (this->return_value.has_value()) {
			this->return_value.value()->bind_to_scope(scope);
		}
	}
	std::string InstructionReturn::to_string() const {
		std::string result = "return";
		if (this->return_value.has_value()) {
			result += " " + this->return_value.value()->to_string();
		}
		return result;
	}

	void InstructionBranchUnconditional::bind_to_scope(Scope<Nameable> &scope) {}
	std::string InstructionBranchUnconditional::to_string() const {
		return "br :" + this->label_name;
	}

	void InstructionBranchConditional::bind_to_scope(Scope<Nameable> &scope) {
		this->condition->bind_to_scope(scope);
	}
	std::string InstructionBranchConditional::to_string() const {
		return "br " + this->condition->to_string()
			+ " :" + this->then_label_name
			+ " :" + this->else_label_name;
	}

	LaFunction::LaFunction(std::string name, mir::Type return_type) :
		name { mv(name) }, return_type { return_type }
	{}
	std::string LaFunction::to_string() const {
		std::string result = this->return_type.to_ir_syntax() + " " + this->name + "(";
		result += utils::format_comma_delineated_list(
			this->parameter_vars,
			[](Variable *const &parameter_var){ return parameter_var->type.to_ir_syntax() + " " + parameter_var->name; }
		);
		result += ") {\n";
		for (const Uptr<Instruction> &inst : this->instructions) {
			result += "\t" + inst->to_string() + "\n";
		}
		result += "}\n";
		return result;
	}
	void LaFunction::add_variable(std::string name, mir::Type type, bool is_parameter) {
		Uptr<Variable> var_ptr = mkuptr<Variable>(name, type);
		this->scope.resolve_item(mv(name), var_ptr.get());
		if (is_parameter) {
			this->parameter_vars.push_back(var_ptr.get());
		}
		this->vars.emplace_back(mv(var_ptr));
	}
	void LaFunction::add_next_instruction(Uptr<Instruction> inst) {
		if (InstructionDeclaration *inst_decl = dynamic_cast<InstructionDeclaration *>(inst.get())) {
			this->add_variable(inst_decl->variable_name, inst_decl->type, false);
		}
		inst->bind_to_scope(this->scope);
		this->instructions.push_back(mv(inst));
	}

	std::string Program::to_string() const {
		std::string result;
		for (const Uptr<LaFunction> &la_function : this->la_functions) {
			result += la_function->to_string() + "\n";
		}
		return result;
	}
	void Program::add_la_function(Uptr<LaFunction> la_function) {
		la_function->scope.set_parent(this->scope);
		this->scope.resolve_item(la_function->get_name(), la_function.get());
		this->la_functions.push_back(mv(la_function));
	}
	void Program::add_external_function(Uptr<ExternalFunction> external_function) {
		this->scope.resolve_item(external_function->get_name(), external_function.get());
		this->external_functions.push_back(mv(external_function));
	}

	void link_std(Program &program) {
		// includes all the information necessary to generate external-linkage
		// ExternalFunctions; namely the name of the function, the number
		// of parameters, and whether the function returns a value
		static const Vec<std::tuple<std::string, int, bool>> std_functions_info = {
			{ "input", 0, true },
			{ "print", 1, false }
		};
		for (const auto &[name, num_parameters, returns_val] : std_functions_info) {
			program.add_external_function(mkuptr<ExternalFunction>(name, num_parameters, returns_val));
		}
	}
}
