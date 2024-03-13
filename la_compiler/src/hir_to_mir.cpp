#include "hir_to_mir.h"
#include "std_alias.h"
#include "utils.h"
#include <assert.h>

namespace La::hir_to_mir {
	using namespace std_alias;

	class InstructionAdder : public hir::InstructionVisitor {
		mir::FunctionDef &mir_function;
		const Map<hir::ExternalFunction *, mir::ExternalFunction *> &ext_func_map;
		const Map<hir::LaFunction *, mir::FunctionDef *> &func_map;
		Map<hir::Variable *, mir::LocalVar *> &var_map;
		Map<std::string, mir::BasicBlock *> block_map;

		// local variables and blocks used for compiler purposes such as array checking
		// nullptr if we did not use them
		struct CompilerAdditions {
			mir::LocalVar *temp_condition; // used to store the value of a really short-lived boolean condition
			mir::LocalVar *line_number; // used to store the line number for tensor-error etc. purposes; ENCODED
			mir::LocalVar *error_dim; // used to store the dimension index for tensor-error etc.; ENCODED
			mir::LocalVar *error_length; // used to store the dimension length for tensor-error etc.; ENCODED
			mir::LocalVar *error_index; // used to store the index for tensor-error etc.; ENCODED
			mir::BasicBlock *unalloced_error; // used to report use of an unallocated tensor
			mir::BasicBlock *out_of_range_tuple_error; // used to report use of an out-of-range tuple
			mir::BasicBlock *out_of_range_one_dim_error; // used to report use of an out-of-range 1D tensor
			mir::BasicBlock *out_of_range_multi_dim_error; // used to report use of an out-of-range n-D tensor
		} compiler_additions;

		// null if the previous BasicBlock already has a terminator or there are no BasicBlocks yet
		mir::BasicBlock *active_basic_block_nullable;

		void add_inst(Opt<Uptr<mir::Place>> destination, Uptr<mir::Rvalue> rvalue) {
			this->active_basic_block_nullable->instructions.push_back(
				mkuptr<mir::Instruction>(mv(destination), mv(rvalue))
			);
		}
		mir::LocalVar *get_compiler_addition_temp_condition() {
			if (!this->compiler_additions.temp_condition) {
				this->compiler_additions.temp_condition = this->make_local_var_int64("tempcond");
			}
			return this->compiler_additions.temp_condition;
		}
		mir::LocalVar *get_compiler_addition_line_number() {
			if (!this->compiler_additions.line_number) {
				this->compiler_additions.line_number = this->make_local_var_int64("linenum");
			}
			return this->compiler_additions.line_number;
		}
		mir::LocalVar *get_compiler_addition_error_dim() {
			if (!this->compiler_additions.error_dim) {
				this->compiler_additions.error_dim = this->make_local_var_int64("errordim");
			}
			return this->compiler_additions.error_dim;
		}
		mir::LocalVar *get_compiler_addition_error_length() {
			if (!this->compiler_additions.error_length) {
				this->compiler_additions.error_length = this->make_local_var_int64("errorlength");
			}
			return this->compiler_additions.error_length;
		}
		mir::LocalVar *get_compiler_addition_error_index() {
			if (!this->compiler_additions.error_index) {
				this->compiler_additions.error_index = this->make_local_var_int64("errorindex");
			}
			return this->compiler_additions.error_index;
		}
		mir::BasicBlock *get_compiler_addition_unalloced_error() {
			if (!this->compiler_additions.unalloced_error) {
				this->compiler_additions.unalloced_error = this->create_basic_block(false, "unallocederror");
				Vec<Uptr<mir::Operand>> args;
				args.push_back(mkuptr<mir::Place>(this->get_compiler_addition_line_number()));
				compiler_additions.unalloced_error->instructions.push_back(mkuptr<mir::Instruction>(
					Opt<Uptr<mir::Place>>(),
					mkuptr<mir::FunctionCall>(
						mkuptr<mir::ExtCodeConstant>(&mir::tensor_error),
						mv(args)
					)
				));
			}
			return this->compiler_additions.unalloced_error;
		}
		mir::BasicBlock *get_compiler_addition_out_of_range_tuple_error() {
			if (!this->compiler_additions.out_of_range_tuple_error) {
				this->compiler_additions.out_of_range_tuple_error = this->create_basic_block(false, "outofrangetuple");
				Vec<Uptr<mir::Operand>> args;
				args.push_back(mkuptr<mir::Place>(this->get_compiler_addition_line_number()));
				args.push_back(mkuptr<mir::Place>(this->get_compiler_addition_error_length()));
				args.push_back(mkuptr<mir::Place>(this->get_compiler_addition_error_index()));
				compiler_additions.out_of_range_tuple_error->instructions.push_back(mkuptr<mir::Instruction>(
					Opt<Uptr<mir::Place>>(),
					mkuptr<mir::FunctionCall>(
						mkuptr<mir::ExtCodeConstant>(&mir::tuple_error),
						mv(args)
					)
				));
			}
			return this->compiler_additions.out_of_range_tuple_error;
		}
		mir::BasicBlock *get_compiler_addition_out_of_range_one_dim_error() {
			if (!this->compiler_additions.out_of_range_one_dim_error) {
				this->compiler_additions.out_of_range_one_dim_error = this->create_basic_block(false, "outofrangeonedim");
				Vec<Uptr<mir::Operand>> args;
				args.push_back(mkuptr<mir::Place>(this->get_compiler_addition_line_number()));
				args.push_back(mkuptr<mir::Place>(this->get_compiler_addition_error_length()));
				args.push_back(mkuptr<mir::Place>(this->get_compiler_addition_error_index()));
				compiler_additions.out_of_range_one_dim_error->instructions.push_back(mkuptr<mir::Instruction>(
					Opt<Uptr<mir::Place>>(),
					mkuptr<mir::FunctionCall>(
						mkuptr<mir::ExtCodeConstant>(&mir::tensor_error),
						mv(args)
					)
				));
			}
			return this->compiler_additions.out_of_range_one_dim_error;
		}
		mir::BasicBlock *get_compiler_addition_out_of_range_multi_dim_error() {
			if (!this->compiler_additions.out_of_range_multi_dim_error) {
				this->compiler_additions.out_of_range_multi_dim_error = this->create_basic_block(false, "outofrangemultidim");
				Vec<Uptr<mir::Operand>> args;
				args.push_back(mkuptr<mir::Place>(this->get_compiler_addition_line_number()));
				args.push_back(mkuptr<mir::Place>(this->get_compiler_addition_error_dim()));
				args.push_back(mkuptr<mir::Place>(this->get_compiler_addition_error_length()));
				args.push_back(mkuptr<mir::Place>(this->get_compiler_addition_error_index()));
				compiler_additions.out_of_range_multi_dim_error->instructions.push_back(mkuptr<mir::Instruction>(
					Opt<Uptr<mir::Place>>(),
					mkuptr<mir::FunctionCall>(
						mkuptr<mir::ExtCodeConstant>(&mir::tensor_error),
						mv(args)
					)
				));
			}
			return this->compiler_additions.out_of_range_multi_dim_error;
		}

		public:

		InstructionAdder(
			mir::FunctionDef &mir_function,
			const Map<hir::ExternalFunction *, mir::ExternalFunction *> &ext_func_map,
			const Map<hir::LaFunction *, mir::FunctionDef *> &func_map,
			Map<hir::Variable *, mir::LocalVar *> &var_map
		) :
			mir_function { mir_function },
			ext_func_map { ext_func_map },
			func_map { func_map },
			var_map { var_map },
			block_map {},
			compiler_additions {
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr
			},
			active_basic_block_nullable { nullptr }
		{}

		void visit(hir::InstructionDeclaration &inst) override {
			this->ensure_active_basic_block();
			Uptr<mir::Operand> dest_operand = this->evaluate_expr(inst.variable);
			Uptr<mir::Place> dest_place = utils::downcast_uptr<mir::Operand, mir::Place>(mv(dest_operand));
			this->add_inst(
				mv(dest_place),
				inst.type.get_default_value()
			);
		}
		void visit(hir::InstructionAssignment &inst) override {
			this->ensure_active_basic_block();
			if (inst.maybe_dest.has_value()) {
				Uptr<mir::Operand> dest_operand = this->evaluate_indexing_expr(*inst.maybe_dest.value());
				Uptr<mir::Place> dest_place = utils::downcast_uptr<mir::Operand, mir::Place>(mv(dest_operand));
				this->evaluate_expr_into_existing_place(
					inst.source,
					mv(dest_place)
				);
			} else {
				this->evaluate_expr_into_existing_place(
					inst.source,
					{}
				);
			}
		}
		void visit(hir::InstructionLabel &inst) override {
			// must start a new basic block
			mir::BasicBlock *old_block = this->active_basic_block_nullable;
			this->enter_basic_block(true, inst.label_name);

			if (old_block) {
				// the old block falls through
				// assert(std::holds_alternative<mir::BasicBlock::ReturnVoid>(old_block->terminator));
				old_block->terminator = mir::BasicBlock::Goto { this->active_basic_block_nullable };
			}
		}
		void visit(hir::InstructionReturn &inst) override {
			this->ensure_active_basic_block();
			mir::BasicBlock::Terminator terminator;
			if (inst.return_value.has_value()) {
				terminator = mir::BasicBlock::ReturnVal {
					this->evaluate_expr(*inst.return_value)
				};
			} else {
				terminator = mir::BasicBlock::ReturnVoid {};
			}
			this->active_basic_block_nullable->terminator = mv(terminator);
			this->active_basic_block_nullable = nullptr;
		}
		void visit(hir::InstructionBranchUnconditional &inst) override {
			this->ensure_active_basic_block();
			this->active_basic_block_nullable->terminator = mir::BasicBlock::Goto {
				this->get_basic_block_by_name(inst.label_name)
			};
			this->active_basic_block_nullable = nullptr;
		}
		void visit(hir::InstructionBranchConditional &inst) override {
			this->ensure_active_basic_block();
			this->active_basic_block_nullable->terminator = mir::BasicBlock::Branch {
				this->decode(this->evaluate_expr(inst.condition)),
				this->get_basic_block_by_name(inst.then_label_name),
				this->get_basic_block_by_name(inst.else_label_name)
			};
			this->active_basic_block_nullable = nullptr;
		}

		void finish() {
			if (this->mir_function.basic_blocks.empty()) {
				this->create_basic_block(false, "");
			}
		}

		private:

		mir::LocalVar *make_local_var_int64(std::string debug_name) {
			auto var_ptr = mkuptr<mir::LocalVar>(false, mv(debug_name), mir::Type { mir::Type::ArrayType { 0 } });
			mir::LocalVar *result = var_ptr.get();
			this->mir_function.local_vars.push_back(mv(var_ptr)); // TODO consider push_front?
			return result;
		}

		// empty label name if anonymous block
		// sets the new basic block to be the current basic block
		void enter_basic_block(bool user_labeled, std::string_view label_name) {
			if (user_labeled) {
				this->active_basic_block_nullable = this->get_basic_block_by_name(label_name);
			} else {
				this->active_basic_block_nullable = this->create_basic_block(false, label_name);
			}
		}
		// makes sure that there is an active basic_block
		// should be called right before adding an instruction
		void ensure_active_basic_block() {
			if (!this->active_basic_block_nullable) {
				this->enter_basic_block(false, "");
			}
		}
		// inserts a branch instruction to the specified basic block if
		// the temp_condition variable is 1, or falls through if it is not
		void branch_to_block(mir::BasicBlock *jmp_dst) {
			mir::BasicBlock *old_block = this->active_basic_block_nullable;
			assert(old_block != nullptr);
			mir::BasicBlock *new_block = this->create_basic_block(false, "");
			old_block->terminator = mir::BasicBlock::Branch {
				mkuptr<mir::Place>(this->get_compiler_addition_temp_condition()),
				jmp_dst,
				new_block
			};
			this->active_basic_block_nullable = new_block;
		}
		// will create a basic block if it doesn't already exist
		// this must be the user-defined label name
		mir::BasicBlock *get_basic_block_by_name(std::string_view label_name) {
			assert(label_name.length() > 0);
			auto it = this->block_map.find(label_name);
			if (it == this->block_map.end()) {
				return this->create_basic_block(true, label_name);
			} else {
				return it->second;
			}
		}
		mir::BasicBlock *create_basic_block(bool user_labeled, std::string_view label_name) {
			Uptr<mir::BasicBlock> block = mkuptr<mir::BasicBlock>(user_labeled, std::string(label_name));
			if (std::holds_alternative<mir::Type::VoidType>(this->mir_function.return_type.type)) {
				// no return value
				block->terminator = mir::BasicBlock::ReturnVoid {};
			} else {
				// block->terminator = mir::BasicBlock::ReturnVal { this->mir_function.return_type.get_default_value() };

				// ignore the commented code above; do a random ass self-jump bc we have to terminate the block somehow
				block->terminator = mir::BasicBlock::Goto { block.get() };
			}
			mir::BasicBlock *block_ptr = block.get();
			this->mir_function.basic_blocks.push_back(mv(block));
			if (user_labeled) {
				// add the basic block to the mapping for label names
				auto [_, entry_is_new] = this->block_map.insert_or_assign(std::string(label_name), block_ptr);
				if (!entry_is_new) {
					std::cerr << "Logic error: creating basic block that already exists.\n";
					exit(1);
				}
			}
			return block_ptr;
		}

		// stores in the given place the result of the hir::Expr, adding
		// mir::Instructions (and possibly temporaries) to the active basic
		// block if necessary in order to evaluate the given expression
		// (including its side effects)
		// see also evaluate_expr
		void evaluate_expr_into_existing_place(const Uptr<hir::Expr> &expr, Opt<Uptr<mir::Place>> place) {
			if (const hir::BinaryOperation *bin_op = dynamic_cast<hir::BinaryOperation *>(expr.get())) {
				mir::LocalVar *decoded_result = this->make_local_var_int64("");
				this->add_inst(
					mkuptr<mir::Place>(decoded_result),
					mkuptr<mir::BinaryOperation>(
						this->decode(this->evaluate_expr(bin_op->lhs)),
						this->decode(this->evaluate_expr(bin_op->rhs)),
						bin_op->op
					)
				);
				this->add_inst(
					mv(place),
					this->encode(mkuptr<mir::Place>(decoded_result))
				);
			} else if (const hir::LengthGetter *length_getter = dynamic_cast<hir::LengthGetter *>(expr.get())) {
				Opt<Uptr<mir::Operand>> dimension;
				if (length_getter->dimension.has_value()) {
					dimension = this->decode(this->evaluate_expr(length_getter->dimension.value()));
				}
				this->add_inst(
					mv(place),
					mkuptr<mir::LengthGetter>(
						this->evaluate_expr(length_getter->target),
						mv(dimension)
					)
				);
			} else if (const hir::FunctionCall *call = dynamic_cast<hir::FunctionCall *>(expr.get())) {
				this->add_inst(
					mv(place),
					this->evaluate_function_call(*call)
				);
			} else if (const hir::NewArray *new_array = dynamic_cast<hir::NewArray *>(expr.get())) {
				Vec<Uptr<mir::Operand>> dimension_lengths;
				for (const Uptr<hir::Expr> &hir_dim_len : new_array->dimension_lengths) {
					dimension_lengths.push_back(this->evaluate_expr(hir_dim_len));
				}
				this->add_inst(
					mv(place),
					mkuptr<mir::NewArray>(mv(dimension_lengths))
				);
			} else if (const hir::NewTuple *new_tuple = dynamic_cast<hir::NewTuple *>(expr.get())) {
				this->add_inst(
					mv(place),
					mkuptr<mir::NewTuple>(this->evaluate_expr(new_tuple->length))
				);
			} else {
				this->add_inst(
					mv(place),
					this->evaluate_expr(expr)
				);
			}
		}

		// returns an mir::Operand which refers to the result of the hir::Expr,
		// adding mir::Instructions (and possibly temporaries) to the active
		// basic block if necessary in order to evaluate the given expression
		// (including its side effects)
		// see also evaluate_expr_into_existing_place
		Uptr<mir::Operand> evaluate_expr(const Uptr<hir::Expr> &expr) {
			if (const hir::ItemRef<hir::Nameable> *item_ref = dynamic_cast<hir::ItemRef<hir::Nameable> *>(expr.get())) {
				if (!item_ref->get_referent().has_value()) {
					std::cerr << "Compiler error: unbound name `" + item_ref->get_ref_name() + "`\n";
					exit(1);
				}
				hir::Nameable *referent = item_ref->get_referent().value();
				if (hir::Variable *hir_var = dynamic_cast<hir::Variable *>(referent)) {
					mir::LocalVar *mir_var = this->var_map.at(hir_var);
					return mkuptr<mir::Place>(
						mir_var,
						Vec<Uptr<mir::Operand>> {}
					);
				} else if (hir::LaFunction *hir_func = dynamic_cast<hir::LaFunction *>(referent)) {
					mir::FunctionDef *mir_func = this->func_map.at(hir_func);
					return mkuptr<mir::CodeConstant>(mir_func);
				} else if (hir::ExternalFunction *hir_func = dynamic_cast<hir::ExternalFunction *>(referent)) {
					mir::ExternalFunction *mir_func = this->ext_func_map.at(hir_func);
					return mkuptr<mir::ExtCodeConstant>(mir_func);
				} else {
					std::cerr << "Logic error: inexhaustive match on subclasses of Nameable\n";
					exit(1);
				}
			} else if (const hir::NumberLiteral *num_lit = dynamic_cast<hir::NumberLiteral *>(expr.get())) {
				return this->encode(mkuptr<mir::Int64Constant>(num_lit->value));
			} else if (const hir::IndexingExpr *indexing_expr = dynamic_cast<hir::IndexingExpr *>(expr.get())) {
				return evaluate_indexing_expr(*indexing_expr);
			} else {
				// FUTURE: LA doesn't allow expressions this complex, but if it
				// did then this is where we could add logic that:
				// - calls evaluate_expr_into_existing_local_var to evaluate the
				//   more complex expression
				// - creates a new LocalVar to act as a temporary to store the
				//   intermediate results
				std::cerr << "Logic Error: this expression is too complex to be converted to an mir::Operand\n";
				exit(1);
			}
		}

		Uptr<mir::Rvalue> evaluate_function_call(const hir::FunctionCall &call) {
			// FUTURE could add a check for if the callee is 0

			Uptr<mir::Operand> callee_operand = this->evaluate_expr(call.callee);
			// because an external function can only be used as a direct callee
			// (i.e. can't be stored in a variable and indirectly called), we
			// only need to check here to see if we're calling an std function
			// that needs encoding/decoding
			mir::ExternalFunction *std_func_nullable = nullptr;
			if (mir::ExtCodeConstant *ext_code = dynamic_cast<mir::ExtCodeConstant *>(callee_operand.get())) {
				std_func_nullable = ext_code->value;
			}

			Vec<Uptr<mir::Operand>> arguments;
			for (const Uptr<hir::Expr> &hir_arg : call.arguments) {
				arguments.push_back(this->evaluate_expr(hir_arg));
			}

			Uptr<mir::Rvalue> result = mkuptr<mir::FunctionCall>(
				mv(callee_operand),
				mv(arguments)
			);
			if (std_func_nullable && std_func_nullable->returns_val) {
				mir::LocalVar *temp_var = this->make_local_var_int64("");
				this->add_inst(
					mkuptr<mir::Place>(temp_var),
					mv(result)
				);
				result = mkuptr<mir::Place>(temp_var);
			}
			return result;
		}

		Uptr<mir::Operand> evaluate_indexing_expr(const hir::IndexingExpr &indexing_expr) {
			// FUTURE even though the HIR allows it, the LA language allows us
			// to safely assume that the target of an indexing expression either
			// refers to a local variable or a function
			const hir::ItemRef<hir::Nameable> &item_ref = dynamic_cast<const hir::ItemRef<hir::Nameable> &>(*indexing_expr.target);
			if (!item_ref.get_referent().has_value()) {
				std::cerr << "Compiler error: unbound name `" + item_ref.get_ref_name() + "`\n";
				exit(1);
			}

			hir::Nameable *hir_name = item_ref.get_referent().value();
			if (hir::Variable *hir_var = dynamic_cast<hir::Variable *>(hir_name)) {
				mir::LocalVar *mir_var = this->var_map.at(hir_var);

				if (indexing_expr.indices.size() > 0) {
					// check that the array was allocated
					// %linenum <- LINE_NUM
					this->add_inst(
						mkuptr<mir::Place>(this->get_compiler_addition_line_number()),
						this->encode(mkuptr<mir::Int64Constant>(static_cast<int64_t>(indexing_expr.src_pos.value().line)))
					);
					// %booooool <- %TARGET = 0
					this->add_inst(
						mkuptr<mir::Place>(this->get_compiler_addition_temp_condition()),
						mkuptr<mir::BinaryOperation>(
							mkuptr<mir::Place>(mir_var),
							mkuptr<mir::Int64Constant>(0), // ideally would just be the default value of the array type but we don't have type checking
							mir::Operator::eq
						)
					);
					// br %booooool :unallocederror :CONTINUE
					this->branch_to_block(this->get_compiler_addition_unalloced_error());
				}


				Vec<Uptr<mir::Operand>> mir_indices;
				if (indexing_expr.indices.size() > 0) {
					bool is_tuple = std::holds_alternative<mir::Type::TupleType>(mir_var->type.type);
					mir::BasicBlock *error_reporter;
					bool is_multi_dim = false;
					if (is_tuple) {
						error_reporter = this->get_compiler_addition_out_of_range_tuple_error();
					} else if (indexing_expr.indices.size() == 1) {
						error_reporter = this->get_compiler_addition_out_of_range_one_dim_error();
					} else {
						// must be a multi-dimensional tensor
						error_reporter = this->get_compiler_addition_out_of_range_multi_dim_error();
						is_multi_dim = true;
					}

					for (int dim_num = 0; dim_num < indexing_expr.indices.size(); ++dim_num) {
						assert(!is_tuple || dim_num == 0);
						const Uptr<hir::Expr> &hir_index = indexing_expr.indices[dim_num];

						Uptr<mir::Operand> mir_index = this->evaluate_expr(hir_index);
						Uptr<mir::Operand> mir_index_clone0 = this->evaluate_expr(hir_index); // FUTURE rn we just use this for a quick and dirty clone, which works because all expressions in an index position in LA are simple

						// %errorindex <- %INDEX
						this->add_inst(
							mkuptr<mir::Place>(this->get_compiler_addition_error_index()),
							mv(mir_index_clone0)
						);
						// %errorlength <- length %TARGET DIM_NUM
						this->add_inst(
							mkuptr<mir::Place>(this->get_compiler_addition_error_length()),
							mkuptr<mir::LengthGetter>(
								mkuptr<mir::Place>(mir_var),
								is_tuple ? Opt<Uptr<mir::Operand>>() : mkuptr<mir::Int64Constant>(dim_num)
							)
						);
						if (is_multi_dim) {
							// %errordim <- encoded(DIM_NUM)
							this->add_inst(
								mkuptr<mir::Place>(this->get_compiler_addition_error_dim()),
								this->encode(mkuptr<mir::Int64Constant>(dim_num))
							);
						}
						// %booooool <- %errorindex < 1; compare with 1 instead of 0 because encoded(0) == 1
						this->add_inst(
							mkuptr<mir::Place>(this->get_compiler_addition_temp_condition()),
							mkuptr<mir::BinaryOperation>(
								mkuptr<mir::Place>(this->get_compiler_addition_error_index()),
								mkuptr<mir::Int64Constant>(1),
								mir::Operator::lt
							)
						);
						// br %booooool :ERROR_REPORTER :CONTINUE
						this->branch_to_block(error_reporter);
						// %booooool <- %errorindex >= %errorlength
						this->add_inst(
							mkuptr<mir::Place>(this->get_compiler_addition_temp_condition()),
							mkuptr<mir::BinaryOperation>(
								mkuptr<mir::Place>(this->get_compiler_addition_error_index()),
								mkuptr<mir::Place>(this->get_compiler_addition_error_length()),
								mir::Operator::ge
							)
						);
						// br %booooool :ERROR_REPORTER :CONTINUE
						this->branch_to_block(error_reporter);

						mir_indices.push_back(this->decode(mv(mir_index)));
					}
				}

				return mkuptr<mir::Place>(
					mir_var,
					mv(mir_indices)
				);
			} else if (hir::LaFunction *hir_func = dynamic_cast<hir::LaFunction *>(hir_name)) {
				mir::FunctionDef *mir_func = this->func_map.at(hir_func);
				return mkuptr<mir::CodeConstant>(mir_func);
			} else {
				std::cerr << "Logic error: can't convert this indexing expression to a place (probably because we don't yet support assigning std functions to variables)\n";
				exit(1);
			}
		}

		Uptr<mir::Operand> encode(Uptr<mir::Operand> operand) {
			if (mir::Int64Constant *num = dynamic_cast<mir::Int64Constant *>(operand.get())) {
				num->value = num->value * 2 + 1;
				return operand;
			} else if (mir::Place *place = dynamic_cast<mir::Place *>(operand.get())) {
				// LA does not allow complex indexing expressions here
				assert(place->indices.size() == 0);

				if (const mir::Type::ArrayType *arr_type = std::get_if<mir::Type::ArrayType>(&place->target->type.type)) {
					if (arr_type->num_dimensions > 0) {
						// encoding an array is a no-op
						return operand;
					} else {
						// encode an int64 by bit-shifting

						// %TEMP_VAR <- %OPERAND << 1
						mir::LocalVar *temp_var = this->make_local_var_int64("");
						this->add_inst(
							mkuptr<mir::Place>(temp_var),
							mkuptr<mir::BinaryOperation>(
								mkuptr<mir::Place>(place->target),
								mkuptr<mir::Int64Constant>(1),
								mir::Operator::lshift
							)
						);
						// %TEMP_VAR <- %TEMP_VAR + 1
						this->add_inst(
							mkuptr<mir::Place>(temp_var),
							mkuptr<mir::BinaryOperation>(
								mkuptr<mir::Place>(temp_var),
								mkuptr<mir::Int64Constant>(1),
								mir::Operator::plus
							)
						);

						// %TEMP_VAR holds our encoded value
						return mkuptr<mir::Place>(temp_var);
					}
				} else if (std::get_if<mir::Type::TupleType>(&place->target->type.type)) {
					// encoding a tuple is a no-op
					return operand;
				} else {
					std::cerr << "Logic error: can't encode this operand.\n";
					exit(1);
				}
			} else {
				std::cerr << "Logic error: can't encode this operand.\n";
				exit(1);
			}
		}
		// Uptr<mir::Operand> evaluate_to_decoded(const Uptr<hir::Expr> &expr) {
		// 	Uptr<mir::Operand> operand = this->evaluate_expr(expr);
		// 	if (dynamic_cast<const hir::NumberLiteral *>(expr.get())) {
		// 		// user-inputted constant, we can take a shortcut

		// 	} else {
		// 		// not a user-inputted constant, do decode
		// 		mir::LocalVar *decoded_var = this->make_local_var_int64("");
		// 		this->add_inst(
		// 			mkuptr<mir::Place>(decoded_var),
		// 			this->decode(mv(operand))
		// 		);
		// 		operand = mkuptr<mir::Place>(decoded_var);
		// 	}
		// 	return operand;
		// }
		Uptr<mir::Operand> decode(Uptr<mir::Operand> operand) {
			if (mir::Place *place = dynamic_cast<mir::Place *>(operand.get())) {
				// assume that it is an int64
				assert(place->indices.size() == 0);
				const mir::Type::ArrayType &arr_type = std::get<mir::Type::ArrayType>(place->target->type.type);
				// assert(arr_type.num_dimensions == 0); TODO why is this assertion sometimes failing?

				mir::LocalVar *decoded_var = this->make_local_var_int64("");
				this->add_inst(
					mkuptr<mir::Place>(decoded_var),
					mkuptr<mir::BinaryOperation>(
						mkuptr<mir::Place>(place->target),
						mkuptr<mir::Int64Constant>(1),
						mir::Operator::rshift
					)
				);
				return mkuptr<mir::Place>(decoded_var);

			} else if (mir::Int64Constant *num = dynamic_cast<mir::Int64Constant *>(operand.get())) {
				return mkuptr<mir::Int64Constant>(num->value >> 1);
			} else {
				std::cerr << "Logic error: can't decode this operand.\n";
				exit(1);
			}
		}
	};

	// fills in the given mir::FunctionDef with the information in the given
	// hir::FunctionDef
	void fill_mir_function(
		mir::FunctionDef &mir_function,
		const hir::LaFunction &hir_function,
		const Map<hir::LaFunction *, mir::FunctionDef *> &func_map,
		const Map<hir::ExternalFunction *, mir::ExternalFunction *> &ext_func_map
	) {
		Map<hir::Variable *, mir::LocalVar *> var_map;

		// transfer the user-declared local variables and parameters
		for (const Uptr<hir::Variable> &hir_var : hir_function.vars) {
			Uptr<mir::LocalVar> mir_var = mkuptr<mir::LocalVar>(
				true,
				hir_var->name,
				hir_var->type
			);
			var_map.insert_or_assign(hir_var.get(), mir_var.get());
			mir_function.local_vars.push_back(mv(mir_var));
		}
		for (hir::Variable *parameter_var : hir_function.parameter_vars) {
			mir_function.parameter_vars.push_back(var_map.at(parameter_var));
		}

		// transfer over each instruction into the basic blocks
		InstructionAdder inst_adder(mir_function, ext_func_map, func_map, var_map);
		for (const Uptr<hir::Instruction> &hir_inst : hir_function.instructions) {
			hir_inst->accept(inst_adder);
		}
		inst_adder.finish();
	}

	Uptr<mir::Program> make_mir_program(const hir::Program &hir_program) {
		auto mir_program = mkuptr<mir::Program>();

		Map<hir::ExternalFunction *, mir::ExternalFunction *> ext_func_map;
		for (const Uptr<hir::ExternalFunction> &hir_ext_func : hir_program.external_functions) {
			auto mir_ext_func = mkuptr<mir::ExternalFunction>(hir_ext_func->value); // copy initialization
			ext_func_map.insert_or_assign(hir_ext_func.get(), mir_ext_func.get());
			mir_program->external_functions.push_back(mv(mir_ext_func));
		}

		// make two passes through the HIR: first, create all the function
		// definitions and track how the hir functions are being mapped to
		// mir::FunctionDefs. second, fill in the function definition using
		// the HIR.
		Map<hir::LaFunction *, mir::FunctionDef *> func_map;
		for (const Uptr<hir::LaFunction> &hir_function : hir_program.la_functions) {
			auto mir_function = mkuptr<mir::FunctionDef>(hir_function->name, hir_function->return_type);
			func_map.insert_or_assign(hir_function.get(), mir_function.get());
			mir_program->function_defs.push_back(mv(mir_function));
		}

		for (const auto [hir_function, mir_function] : func_map) {
			fill_mir_function(*mir_function, *hir_function, func_map, ext_func_map);
		}

		return mir_program;
	}
}
