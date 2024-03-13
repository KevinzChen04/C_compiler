	#include "code_gen.h"
	#include "std_alias.h"
	#include "utils.h"
	#include <iostream>
	#include <string>

	namespace code_gen {
		using namespace Lb::hir;

		std::string get_unique_user_var_name(const Variable &var) {
			return "uservar_" + std::to_string(reinterpret_cast<uintptr_t>(&var)) + "_" + var.name;
		}

		std::string get_user_var_declaration(const Variable &var) {
			return var.type_name + " " + get_unique_user_var_name(var);
		}

		std::string get_prefixed_user_label_name(const std::string &label_name) {
			return "userlabel_" + label_name;
		}

		std::string get_unique_statement_label_name(const Statement &stmt) {
			return "stmtlabel_" + std::to_string(reinterpret_cast<uintptr_t>(&stmt));
		}

		std::string translate_expression(const Expr &expr) {
			if (auto item_ref = dynamic_cast<const ItemRef<Nameable> *>(&expr)) {
				const Nameable *referent = item_ref->get_referent().value();
				if (auto var = dynamic_cast<const Variable *>(referent)) {
					return get_unique_user_var_name(*var);
				} else {
					return referent->get_name();
				}
			} else if (auto num_lit = dynamic_cast<const NumberLiteral *>(&expr)) {
				return std::to_string(num_lit->value);
			} else if (auto bin_op = dynamic_cast<const BinaryOperation *>(&expr)) {
				return translate_expression(*bin_op->lhs)
					+ " " + to_string(bin_op->op)
					+ " " + translate_expression(*bin_op->rhs);
			} else if (auto indexing_expr = dynamic_cast<const IndexingExpr *>(&expr)) {
				std::string result = translate_expression(*indexing_expr->target);
				for (const Uptr<Expr> &index : indexing_expr->indices) {
					result += "[" + translate_expression(*index) + "]";
				}
				return result;
			} else if (auto length_getter = dynamic_cast<const LengthGetter *>(&expr)) {
				std::string result = "length " + translate_expression(*length_getter->target);
				if (length_getter->dimension.has_value()) {
					result += " " + translate_expression(*length_getter->dimension.value());
				}
				return result;
			} else if (auto function_call = dynamic_cast<const FunctionCall *>(&expr)) {
				return translate_expression(*function_call->callee) + "("
					+ utils::format_comma_delineated_list(
						function_call->arguments,
						[](const Uptr<Expr> &arg) { return translate_expression(*arg); }
					) + ")";
			} else if (auto new_array = dynamic_cast<const NewArray *>(&expr)) {
				return "new Array("
					+ utils::format_comma_delineated_list(
						new_array->dimension_lengths,
						[](const Uptr<Expr> &dim_len) { return translate_expression(*dim_len); }
					) + ")";
			} else if (auto new_tuple = dynamic_cast<const NewTuple *>(&expr)) {
				return "new Tuple("
					+ translate_expression(*new_tuple->length)
					+ ")";
			} else {
				std::cerr << "Logic error: inexhaustive match over Expr subclasses.\n";
				exit(1);
			}
		}

		class LabelMapper : public StatementVisitor {
			public:

			Map<std::string, const StatementWhile *> body_map;
			Map<std::string, const StatementWhile *> end_map;

			LabelMapper() : body_map {}, end_map {} {}

			void visit(StatementDeclaration &stmt) override {}
			void visit(StatementAssignment &stmt) override {}
			void visit(StatementReturn &stmt) override {}
			void visit(StatementContinue &stmt) override {}
			void visit(StatementBreak &stmt) override {}
			void visit(StatementGoto &stmt) override {}
			void visit(StatementIf &stmt) override {}
			void visit(StatementLabel &stmt) override {}
			void visit(StatementBlock &block) override {
				for (const Uptr<Statement> &stmt : block.statements) {
					stmt->accept(*this);
				}
			}
			void visit(StatementWhile &stmt) override {
				this->body_map.insert_or_assign(stmt.body_label_name, &stmt);
				this->end_map.insert_or_assign(stmt.end_label_name, &stmt);
			}
		};

		// Each statement that is visited gets converted to one or multiple strings
		// representing LA instructions
		class StatementTranslator : public StatementVisitor {
			const Map<std::string, const StatementWhile *> &body_map;
			const Map<std::string, const StatementWhile *> &end_map;
			Vec<const StatementWhile *> loop_stack;
			bool has_temp_cond_var;

			public:

			Vec<std::string> la_instructions;

			StatementTranslator(
				const Map<std::string, const StatementWhile *> &body_map,
				const Map<std::string, const StatementWhile *> &end_map
			) :
				body_map { body_map },
				end_map { end_map },
				loop_stack {},
				has_temp_cond_var { false },
				la_instructions {}
			{}

			void visit(StatementBlock &block) override {
				for (const Uptr<Statement> &stmt : block.statements) {
					stmt->accept(*this);
				}
			}
			void visit(StatementDeclaration &stmt) override {
				for (const auto &[_, item_ref] : stmt.variables) {
					const Nameable *referent = item_ref->get_referent().value();
					const Variable *referent_var = dynamic_cast<const Variable *>(referent);
					assert(referent_var != nullptr);
					this->la_instructions.push_back(get_user_var_declaration(*referent_var));
				}
			}
			void visit(StatementAssignment &stmt) override {
				std::string translated;
				if (stmt.maybe_dest.has_value()) {
					translated += translate_expression(*stmt.maybe_dest.value()) + " <- ";
				}
				translated += translate_expression(*stmt.source);
				this->la_instructions.push_back(mv(translated));
			}
			void visit(StatementReturn &stmt) override {
				std::string translated = "return";
				if (stmt.return_value.has_value()) {
					translated += " " + translate_expression(*stmt.return_value.value());
				}
				this->la_instructions.push_back(mv(translated));
			}
			void visit(StatementContinue &stmt) override {
				const StatementWhile *innermost_loop = this->loop_stack.back();
				std::string translated = "br :" + get_unique_statement_label_name(*innermost_loop);
				this->la_instructions.push_back(mv(translated));
			}
			void visit(StatementBreak &stmt) override {
				const StatementWhile *innermost_loop = this->loop_stack.back();
				std::string translated = "br :" + get_prefixed_user_label_name(innermost_loop->end_label_name);
				this->la_instructions.push_back(mv(translated));
			}
			void visit(StatementGoto &stmt) override {
				std::string translated = "br :" + get_prefixed_user_label_name(stmt.label_name);
				this->la_instructions.push_back(mv(translated));
			}
			void visit(StatementIf &stmt) override {
				if (!this->has_temp_cond_var) {
					this->la_instructions.push_back("int64 tempcond");
					this->has_temp_cond_var = true;
				}

				this->la_instructions.push_back("tempcond <- " + translate_expression(*stmt.condition));
				this->la_instructions.push_back("br tempcond :" + get_prefixed_user_label_name(stmt.then_label_name) + " :" + get_prefixed_user_label_name(stmt.else_label_name));
			}
			void visit(StatementLabel &stmt) override {
				this->la_instructions.push_back(":" + get_prefixed_user_label_name(stmt.label_name));

				auto body_iter = this->body_map.find(stmt.label_name);
				if (body_iter != this->body_map.end()) {
					this->loop_stack.push_back(body_iter->second);
					return;
				}

				auto end_iter = this->end_map.find(stmt.label_name);
				if (end_iter != this->end_map.end()) {
					// because LB disgustingly allows you to interleave loops, this
					// the popped loop is not guaranteed to be the same as the one
					// that this label actually terminates, e.g.
					/*
					while (cond) :body0 :end0
					while (cond) :body1 :end1
					:body0
					:body1
					:end0
					:end1
					*/
					this->loop_stack.pop_back();
					return;
				}
			}
			void visit(StatementWhile &stmt) override {
				if (!this->has_temp_cond_var) {
					this->la_instructions.push_back("int64 tempcond");
					this->has_temp_cond_var = true;
				}

				this->la_instructions.push_back(":" + get_unique_statement_label_name(stmt));
				this->la_instructions.push_back("tempcond <- " + translate_expression(*stmt.condition));
				this->la_instructions.push_back("br tempcond :" + get_prefixed_user_label_name(stmt.body_label_name) + " :" + get_prefixed_user_label_name(stmt.end_label_name));
			}
		};

		std::string generate_function_code(const LbFunction &lb_function) {
			std::string result;

			// generate function header
			result += lb_function.return_type_name + " " + lb_function.name + "(";
			result += utils::format_comma_delineated_list(
				lb_function.parameter_vars,
				[](const Uptr<Variable> &var) { return get_user_var_declaration(*var); }
			);
			result += ") ";

			// map each label to an optional while instruction for which it is the body or the end label
			LabelMapper label_mapper;
			lb_function.body->accept(label_mapper);

			StatementTranslator stmt_translator(label_mapper.body_map, label_mapper.end_map);
			lb_function.body->accept(stmt_translator);

			result += "{\n";
			for (const std::string &la_inst : stmt_translator.la_instructions) {
				result += "\t" + la_inst + "\n";
			}
			result += "}\n";
			return result;
		}

		std::string generate_program_code(const Program &program) {
			std::string result;
			for (const Uptr<LbFunction> &lb_function : program.lb_functions) {
				result += generate_function_code(*lb_function) + "\n";
			}
			return result;
		}
	}
