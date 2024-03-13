#include "hir.h"
#include "std_alias.h"
#include "utils.h"

namespace Lb::hir {
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

	std::string to_string(Operator op) {
		static const std::string map[] = {
			"<", "<=", "=", ">=", ">", "+", "-", "*", "&", "<<", ">>"
		};
		return map[static_cast<int>(op)];
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

	void BinaryOperation::bind_to_scope(Scope<Nameable> &scope) {
		this->lhs->bind_to_scope(scope);
		this->rhs->bind_to_scope(scope);
	}
	std::string BinaryOperation::to_string() const {
		return this->lhs->to_string()
			+ " " + hir::to_string(this->op)
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

	void StatementBlock::bind_to_scope(Scope<Nameable> &scope) {
		this->scope.set_parent(scope);
	}
	void StatementBlock::add_next_statement(Uptr<Statement> stmt) {
		if (StatementDeclaration *stmt_decl = dynamic_cast<StatementDeclaration *>(stmt.get())) {
			for (const auto &[var_name, _] : stmt_decl->variables) {
				Uptr<Variable> var_ptr = mkuptr<Variable>(var_name, stmt_decl->type_name);
				this->scope.resolve_item(var_name, var_ptr.get(), false);
				this->vars.push_back(mv(var_ptr));
			}
		}
		stmt->bind_to_scope(this->scope);
		this->statements.push_back(mv(stmt));
	}
	std::string StatementBlock::to_string() const {
		std::string result = "{\n";
		for (const Uptr<Statement> &stmt : this->statements) {
			result += "\t" + stmt->to_string() + "\n";
		}
		result += "}\n";
		return result;
	}

	void StatementDeclaration::bind_to_scope(Scope<Nameable> &scope) {
		for (auto &[_, item_ref] : this->variables) {
			item_ref->bind_to_scope(scope);
		}
	}
	std::string StatementDeclaration::to_string() const {
		return this->type_name + " " + utils::format_comma_delineated_list(
			this->variables,
			[](const auto &pair) { return pair.first; }
		);
	}

	void StatementAssignment::bind_to_scope(Scope<Nameable> &scope) {
		if (this->maybe_dest.has_value()) {
			this->maybe_dest.value()->bind_to_scope(scope);
		}
		this->source->bind_to_scope(scope);
	}
	std::string StatementAssignment::to_string() const {
		std::string result;
		if (this->maybe_dest.has_value()) {
			result += this->maybe_dest.value()->to_string() + " <- ";
		}
		result += this->source->to_string();
		return result;
	}

	void StatementLabel::bind_to_scope(Scope<Nameable> &scope) {}
	std::string StatementLabel::to_string() const {
		return ":" + this->label_name;
	}

	void StatementReturn::bind_to_scope(Scope<Nameable> &scope) {
		if (this->return_value.has_value()) {
			this->return_value.value()->bind_to_scope(scope);
		}
	}
	std::string StatementReturn::to_string() const {
		std::string result = "return";
		if (this->return_value.has_value()) {
			result += " " + this->return_value.value()->to_string();
		}
		return result;
	}

	void StatementContinue::bind_to_scope(Scope<Nameable> &scope) {}
	std::string StatementContinue::to_string() const {
		return "continue";
	}

	void StatementBreak::bind_to_scope(Scope<Nameable> &scope) {}
	std::string StatementBreak::to_string() const {
		return "break";
	}

	void StatementGoto::bind_to_scope(Scope<Nameable> &scope) {}
	std::string StatementGoto::to_string() const {
		return "goto :" + this->label_name;
	}

	void StatementIf::bind_to_scope(Scope<Nameable> &scope) {
		this->condition->bind_to_scope(scope);
	}
	std::string StatementIf::to_string() const {
		return "if (" + this->condition->to_string()
			+ ") :" + this->then_label_name
			+ " :" + this->else_label_name;
	}

	void StatementWhile::bind_to_scope(Scope<Nameable> &scope) {
		this->condition->bind_to_scope(scope);
	}
	std::string StatementWhile::to_string() const {
		return "while (" + this->condition->to_string()
			+ ") :" + this->body_label_name
			+ " :" + this->end_label_name;
	}

	LbFunction::LbFunction(std::string name, std::string return_type_name, Uptr<StatementBlock> body) :
		name { mv(name) }, return_type_name { mv(return_type_name) }, body { mv(body) }, parameter_vars {}, scope {}
	{
		// bind the body to the overall scope (which includes the parameter vars)
		this->body->bind_to_scope(this->scope);
	}
	std::string LbFunction::to_string() const {
		std::string result = this->return_type_name + " " + this->name + "(";
		result += utils::format_comma_delineated_list(
			this->parameter_vars,
			[](const Uptr<Variable> &parameter_var){ return parameter_var->type_name + " " + parameter_var->name; }
		);
		result += ")\n";
		result += this->body->to_string();
		return result;
	}
	void LbFunction::add_parameter_variable(std::string name, std::string type_name) {
		Uptr<Variable> var_ptr = mkuptr<Variable>(name, type_name);
		this->scope.resolve_item(mv(name), var_ptr.get(), true);
		this->parameter_vars.push_back(mv(var_ptr));
	}

	std::string Program::to_string() const {
		std::string result;
		for (const Uptr<LbFunction> &lb_function : this->lb_functions) {
			result += lb_function->to_string() + "\n";
		}
		return result;
	}
	void Program::add_lb_function(Uptr<LbFunction> lb_function) {
		lb_function->scope.set_parent(this->scope);
		this->scope.resolve_item(lb_function->get_name(), lb_function.get(), true);
		this->lb_functions.push_back(mv(lb_function));
	}
	void Program::add_external_function(Uptr<ExternalFunction> external_function) {
		this->scope.resolve_item(external_function->get_name(), external_function.get(), true);
		this->external_functions.push_back(mv(external_function));
	}

	void link_std(Program &program) {
		// includes all the information necessary to generate external-linkage
		// ExternalFunctions; namely the name of the function
		static const Vec<std::string> std_functions_info = {
			"input",
			"print"
		};
		for (const auto &name : std_functions_info) {
			program.add_external_function(mkuptr<ExternalFunction>(name));
		}
	}
}
