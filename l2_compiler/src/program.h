#pragma once

#include "utils.h"
#include <memory>
#include <vector>
#include <map>
#include <optional>
#include <string>
#include <iostream>
#include <string_view>
#include <set>
#include <type_traits>

namespace L2::program {
	struct Variable;
	struct Register;
	template<typename Item, typename ItemRef, bool DefineOnUse>
	class Scope;
	struct AggregateScope;

	struct RegisterRef;
	struct NumberLiteral;
	struct StackArg;
	struct MemoryLocation;
	struct LabelRef;
	struct VariableRef;
	struct L2FunctionRef;
	struct ExternalFunctionRef;

	class ExprVisitor {
		public:

		virtual void visit(RegisterRef &expr) = 0;
		virtual void visit(NumberLiteral &expr) = 0;
		virtual void visit(StackArg &expr) = 0;
		virtual void visit(MemoryLocation &expr) = 0;
		virtual void visit(LabelRef &expr) = 0;
		virtual void visit(VariableRef &expr) = 0;
		virtual void visit(L2FunctionRef &expr) = 0;
		virtual void visit(ExternalFunctionRef &expr) = 0;
	};

	class Expr {
		public:

		virtual std::string to_string() const = 0;

		// which sub-values are read when this Expr is read
		virtual utils::set<Variable *> get_vars_on_read() const {
			return {};
		}
		// which sub-values are read/written when this Expr is written
		virtual utils::set<Variable *> get_vars_on_write(bool get_read_vars) const {
			return {};
		}
		virtual void bind_all(AggregateScope &agg_scope) {}
		virtual void accept(ExprVisitor &v) = 0;
	};

	class RegisterRef : public Expr {
		private:
		std::string_view free_name;
		Register *referent;

		public:

		RegisterRef(Register* referent);
		RegisterRef(const std::string_view &free_name);

		std::string_view get_ref_name() const;
		virtual std::string to_string() const override;
		virtual utils::set<Variable *> get_vars_on_read() const override;
		virtual utils::set<Variable *> get_vars_on_write(bool get_read_vars) const override;
		virtual void bind_all(AggregateScope &agg_scope) override;
		void bind(Register *referent);
		Register *get_referent() const { return this->referent; }
		virtual void accept(ExprVisitor &v) override {v.visit(*this); }

	};

	struct NumberLiteral : Expr {
		int64_t value;

		NumberLiteral(int64_t value) : value {value} {}

		virtual std::string to_string() const override;
		virtual void accept(ExprVisitor &v) override {v.visit(*this); }
	};

	struct StackArg : Expr {
		std::unique_ptr<NumberLiteral> stack_num;

		StackArg(std::unique_ptr<NumberLiteral> &&stack_num) : stack_num {std::move(stack_num)} {}
		// StackArg(const StackArg &other) : stack_num {std::make_unique<NumberLiteral>(*other.stack_num)} {}

		virtual std::string to_string() const override;
		virtual void accept(ExprVisitor &v) override {v.visit(*this); }

	};

	struct MemoryLocation : Expr {
		std::unique_ptr<Expr> base;
		std::unique_ptr<NumberLiteral> offset;

		MemoryLocation(std::unique_ptr<Expr> &&base, std::unique_ptr<NumberLiteral> &&offset) :
			base {std::move(base)}, offset {std::move(offset)}
		{}
		// MemoryLocation(const MemoryLocation &other) :
		// 	base {std::make_unique<Expr>(*other.base)},
		// 	offset {std::make_unique<NumberLiteral>(*other.offset)}
		// {}

		virtual std::string to_string() const override;
		virtual utils::set<Variable *> get_vars_on_read() const override;
		virtual utils::set<Variable *> get_vars_on_write(bool get_read_vars) const override;
		virtual void bind_all(AggregateScope &agg_scope) override;
		virtual void accept(ExprVisitor &v) override {v.visit(*this); }
	};

	struct InstructionLabel;

	class LabelRef : public Expr {
		private:
		std::string free_name;
		InstructionLabel **referent;

		public:

		LabelRef(const std::string_view &free_name);

		void bind(InstructionLabel **referent);
		std::string_view get_ref_name() const;
		virtual std::string to_string() const override;
		virtual void bind_all(AggregateScope &agg_scope) override;
		InstructionLabel *get_referent() const { return *this->referent; }
		virtual void accept(ExprVisitor &v) override {v.visit(*this); }
	};

	class VariableRef : public Expr {
		private:
		// the null-ness of this->referent determines whether this is a free
		// or bound variable.
		// once bound, this->name has no meaning and should never be used
		std::string_view free_name;
		Variable *referent;

		public:

		// VariableRefs *must* be bound before the passed-in string_view becomes invalid
		VariableRef(const std::string_view &free_name);
		VariableRef(Variable *referent);

		void bind(Variable *referent);
		std::string_view get_ref_name() const;
		virtual std::string to_string() const override;
		virtual utils::set<Variable *> get_vars_on_read() const override;
		virtual utils::set<Variable *> get_vars_on_write(bool get_read_vars) const override;
		virtual void bind_all(AggregateScope &agg_scope) override;
		Variable *get_referent() const { return this->referent; }
		virtual void accept(ExprVisitor &v) override {v.visit(*this); }
	};

	struct L2Function;
	struct ExternalFunction;

	class L2FunctionRef : public Expr {
		private:
		std::string_view free_name;
		L2Function **referent;

		public:

		L2FunctionRef(const std::string_view &free_name);

		void bind(L2Function **referent);
		std::string_view get_ref_name() const;
		virtual std::string to_string() const override;
		virtual void bind_all(AggregateScope &agg_scope) override;
		L2Function *get_referent() const { return *this->referent; }
		virtual void accept(ExprVisitor &v) override { v.visit(*this); };
	};

	class ExternalFunctionRef : public Expr {
		private:
		std::string_view free_name;
		ExternalFunction **referent;

		public:

		ExternalFunctionRef(const std::string_view &free_name);

		void bind(ExternalFunction **referent);
		std::string_view get_ref_name() const;
		virtual std::string to_string() const override;
		virtual void bind_all(AggregateScope &agg_scope) override;
		ExternalFunction *get_referent() const { return *this->referent; }
		virtual void accept(ExprVisitor &v) override { v.visit(*this); };
	};

	struct InstructionReturn;
	struct InstructionAssignment;
	struct InstructionCompareAssignment;
	struct InstructionCompareJump;
	struct InstructionLabel;
	struct InstructionGoto;
	struct InstructionCall;
	struct InstructionLeaq;

	class InstructionVisitor {
		public:

		virtual void visit(InstructionReturn &inst) = 0;
		virtual void visit(InstructionAssignment &inst) = 0;
		virtual void visit(InstructionCompareAssignment &inst) = 0;
		virtual void visit(InstructionCompareJump &inst) = 0;
		virtual void visit(InstructionLabel &inst) = 0;
		virtual void visit(InstructionGoto &inst) = 0;
		virtual void visit(InstructionCall &inst) = 0;
		virtual void visit(InstructionLeaq &inst) = 0;
	};

	struct Instruction {
		virtual std::string to_string() const = 0;
		virtual void accept(InstructionVisitor &v) = 0;
		virtual void bind_all(AggregateScope &agg_scope) {}
	};

	struct InstructionReturn : Instruction {
		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
	};

	enum struct AssignOperator {
		pure,
		add,
		subtract,
		multiply,
		bitwise_and,
		lshift,
		rshift
	};

	AssignOperator str_to_ass_op(const std::string_view &str);

	std::string to_string(AssignOperator op);

	struct InstructionAssignment : Instruction {
		std::unique_ptr<Expr> source;
		AssignOperator op;
		std::unique_ptr<Expr> destination;

		InstructionAssignment(
			AssignOperator op,
			std::unique_ptr<Expr> &&source,
			std::unique_ptr<Expr> &&destination
		) :
			op { op }, source { std::move(source) }, destination { std::move(destination )}
		{}
		// InstructionAssignment(const InstructionAssignment &other) :
		// 	source {std::make_unique<Expr>(*other.source)},
		// 	op {other.op},
		// 	destination {std::make_unique<Expr>(*other.destination)}
		// {}

		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
		virtual void bind_all(AggregateScope &agg_scope) override;
	};

	enum struct ComparisonOperator {
		lt,
		le,
		eq
	};

	ComparisonOperator str_to_cmp_op(const std::string_view &str);

	std::string to_string(ComparisonOperator op);

	struct InstructionCompareAssignment : Instruction {
		std::unique_ptr<Expr> destination;
		ComparisonOperator op;
		std::unique_ptr<Expr> lhs;
		std::unique_ptr<Expr> rhs;

		InstructionCompareAssignment(
			std::unique_ptr<Expr> &&destination,
			ComparisonOperator op,
			std::unique_ptr<Expr> &&lhs,
			std::unique_ptr<Expr> &&rhs
		) :
			destination {std::move(destination)},
			op {op},
			lhs {std::move(lhs)},
			rhs {std::move(rhs)}
		{}
		// InstructionCompareAssignment(const InstructionCompareAssignment &other) :
		// 	destination {std::make_unique<Expr>(*other.destination)},
		// 	op {other.op},
		// 	lhs {std::make_unique<Expr>(*other.lhs)},
		// 	rhs {std::make_unique<Expr>(*other.rhs)}
		// {}

		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
		virtual void bind_all(AggregateScope &agg_scope) override;
	};

	struct InstructionCompareJump : Instruction {
		ComparisonOperator op;
		std::unique_ptr<Expr> lhs;
		std::unique_ptr<Expr> rhs;
		std::unique_ptr<LabelRef> label;

		InstructionCompareJump(
			ComparisonOperator op,
			std::unique_ptr<Expr> &&lhs,
			std::unique_ptr<Expr> &&rhs,
			std::unique_ptr<LabelRef> label
		):
			op {op}, lhs{std::move(lhs)}, rhs{std::move(rhs)}, label{std::move(label)}
		{}
		// InstructionCompareJump(const InstructionCompareJump &other) :
		// 	op {other.op},
		// 	lhs {std::make_unique<Expr>(*other.lhs)},
		// 	rhs {std::make_unique<Expr>(*other.rhs)},
		// 	label {std::make_unique<LabelRef>(*other.label)}
		// {}

		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
		virtual void bind_all(AggregateScope &agg_scope) override;
	};

	struct InstructionLabel : Instruction {
		std::string label_name;

		InstructionLabel(const std::string_view &label_name) : label_name {label_name} {}

		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
		virtual void bind_all(AggregateScope &agg_scope) override;
	};

	struct InstructionGoto : Instruction {
		std::unique_ptr<LabelRef> label;

		InstructionGoto(std::unique_ptr<LabelRef> &&label) : label {std::move(label)} {}
		// InstructionGoto(const InstructionGoto &other) :
		// 	label {std::make_unique<LabelRef>(*other.label)}
		// {}

		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
		virtual void bind_all(AggregateScope &agg_scope) override;
	};

	struct InstructionCall : Instruction {
		std::unique_ptr<Expr> callee;
		int64_t num_arguments; // specified by user, doesn't necessarily match function's actual num args

		InstructionCall(std::unique_ptr<Expr> &&callee, int64_t num_arguments) :
			callee {std::move(callee)},
			num_arguments {num_arguments}
		{
			if (this->num_arguments < 0) {
				std::cerr << "Error: negative number of call arguments.\n";
				exit(-1);
			}
		}
		// InstructionCall(const InstructionCall &other) :
		// 	callee {std::make_unique<Expr>(*other.callee)},
		// 	num_arguments {other.num_arguments}
		// {}

		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
		virtual void bind_all(AggregateScope &agg_scope) override;
	};

	struct InstructionLeaq : Instruction {
		std::unique_ptr<Expr> destination;
		std::unique_ptr<Expr> base;
		std::unique_ptr<Expr> offset;
		int64_t scale;

		InstructionLeaq(
			std::unique_ptr<Expr> &&destination,
			std::unique_ptr<Expr> &&base,
			std::unique_ptr<Expr> &&offset,
			int64_t scale
		) :
			destination {std::move(destination)},
			base {std::move(base)},
			offset {std::move(offset)},
			scale {scale}
		{
		}
		// InstructionLeaq(const InstructionLeaq &other) :
		// 	destination {std::make_unique<Expr>(*other.destination)},
		// 	base {std::make_unique<Expr>(*other.base)},
		// 	offset {std::make_unique<Expr>(*other.offset)},
		// 	scale {other.scale}
		// {}

		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
		virtual void bind_all(AggregateScope &agg_scope) override;
	};

	struct Variable {
		std::string name;
		bool spillable;

		Variable(const std::string_view &name) :
			name {name},
			spillable {true}
		{}
		Variable(const std::string &name, bool spillable) :
			name{name},
			spillable{spillable}
		{}

		virtual std::string to_string() const;
		void update_spillable(bool spill);
	};

	struct Register : Variable {
		bool is_callee_saved;
		bool is_return_value;
		bool ignores_liveness; // only true for rsp
		int argument_order; // the ordinal number of the argument; 0 for first
		// -1 if not used as an argument

		Register(
			const std::string_view &name,
			bool is_callee_saved,
			bool is_return_value,
			bool ignores_liveness,
			int argument_order
		) :
			Variable(name),
			is_callee_saved {is_callee_saved},
			is_return_value {is_return_value},
			ignores_liveness {ignores_liveness},
			argument_order {argument_order}
		{}

		virtual std::string to_string() const override;
	};

	class Function {
		private:

		std::string name;
		int64_t num_arguments; // -1 for unknown (this is okay for now because there is only one special case)

		public:

		Function(const std::string_view &name, int64_t num_arguments);

		const std::string &get_name() const { return this->name; }
		const int64_t get_num_arguments() const { return this->num_arguments; }
		virtual bool get_never_returns() const = 0;
		virtual std::string to_string() const;
	};

	// so far, only used for std functions
	class ExternalFunction : public Function {
		private:
		bool never_returns;

		public:

		ExternalFunction(const std::string_view &name, int64_t num_arguments, bool never_returns);
		virtual bool get_never_returns() const override;
	};

	// A ScopeComponent represents a namespace of Items that the ItemRefs care
	// about.
	// `(name, item)` pairs in this->dict represent Items defined in this scope
	// under `name`.
	// `(name, ItemRef *)` in free_referrers represents that that ItemRef has
	// refers to `name`, but that it is a free name (unbound to anything in this
	// scope)
	// An ItemRef must have a
	// - void ItemRef::bind(Item *referent) method that can be used to bind a free
	// name to an Item once the item is encountered.
	// - std::string_view ItemRef::get_ref_name() method that can be used to get
	// the name that the ItemRef refers to.
	//
	// If DefineOnUse is true, Item must have a constructor that takes a name so
	// that one can be created if it did not already exist.
	template<typename Item, typename ItemRef, bool DefineOnUse>
	class Scope {
		private:
		// If a Scope has a parent, then it cannot have any
		// free_refs; they must have been transferred to the parent.
		std::optional<Scope *> parent;
		std::map<std::string, Item, std::less<void>> dict;
		std::map<std::string, std::vector<ItemRef *>, std::less<void>> free_refs;

		public:

		Scope() : parent {}, dict {}, free_refs {} {}
		Scope(const Scope &other) = delete;

		std::vector<const Item *> get_all_items() const {
			std::vector<const Item *> result;
			if (this->parent) {
				result = std::move(static_cast<const Scope *>(*this->parent)->get_all_items());
			}
			for (const auto &[name, item] : this->dict) {
				result.push_back(&item);
			}
			return result;
		}

		std::vector<Item *> get_all_items() {
			std::vector<Item *> result;
			if (this->parent) {
				result = std::move((*this->parent)->get_all_items());
			}
			for (auto &[name, item] : this->dict) {
				result.push_back(&item);
			}
			return result;
		}

		// returns whether the ref was immediately bound or was left as free
		bool add_ref(ItemRef &item_ref) {
			std::string_view ref_name = item_ref.get_ref_name();

			std::optional<Item *> maybe_item_ptr = this->get_item_maybe(ref_name);
			if (maybe_item_ptr) {
				// bind the ref to the item
				item_ref.bind(*maybe_item_ptr);
				return true;
			} else {
				// there is no definition of this name in the current scope
				this->push_free_ref(item_ref);
				return false;
			}
		}

		// Adds the specified item to this scope under the specified name,
		// resolving all free refs who were depending on that name. Dies if
		// there already exists an item under that name.
		void resolve_item(std::string name, Item item) {
			auto existing_item_it = this->dict.find(name);
			if (existing_item_it != this->dict.end()) {
				std::cerr << "name conflict: " << name << std::endl;
				exit(-1);
			}

			const auto [item_it, _] = this->dict.insert(std::make_pair(
				name,
				std::move(item)
			));
			auto free_refs_vec_it = this->free_refs.find(name);
			if (free_refs_vec_it != this->free_refs.end()) {
				for (ItemRef *item_ref_ptr : free_refs_vec_it->second) {
					item_ref_ptr->bind(&item_it->second);
				}
				this->free_refs.erase(free_refs_vec_it);
			}
		}

		// In addition to using free names like normal, clients may also use
		// this method to define an Item at the same time that it is used.
		// (kinda like python variable declaration).
		// The below conditional inclusion trick doesn't work because
		// gcc-toolset-11 doesn't seem to respect SFINAE, so just allow all
		// instantiation sto use it and hope for the best.
		// template<typename T = std::enable_if_t<DefineOnUse>>
		Item *get_item_or_create(const std::string_view &name) {
			std::optional<Item *> maybe_item_ptr = get_item_maybe(name);
			if (maybe_item_ptr) {
				return *maybe_item_ptr;
			} else {
				const auto [item_it, _] = this->dict.insert(std::make_pair(
					std::string(name),
					Item(name)
				));
				return &item_it->second;
			}
		}

		std::optional<Item *> get_item_maybe(const std::string_view &name) {
			auto item_it = this->dict.find(name);
			if (item_it == this->dict.end()) {
				if (this->parent) {
					return (*this->parent)->get_item_maybe(name);
				} else {
					return {};
				}
			} else {
				return std::make_optional<Item *>(&item_it->second);
			}
		}

		// Sets the given Scope as the parent of this Scope, transferring all
		// current and future free names to the parent. If this scope already
		// has a parent, dies.
		void set_parent(Scope &parent) {
			if (this->parent) {
				std::cerr << "this scope already has a parent oops\n";
				exit(-1);
			}

			this->parent = std::make_optional<Scope *>(&parent);

			for (auto &[name, our_free_refs_vec] : this->free_refs) {
				for (ItemRef *our_free_ref : our_free_refs_vec) {
					// TODO optimization here is possible; instead of using the
					// public API of the parent we can just query the dictionary
					// directly
					(*this->parent)->add_ref(*our_free_ref);
				}
			}
			this->free_refs.clear();
		}

		// returns whether free refs exist in this scope for the given name
		std::vector<ItemRef *> get_free_refs() const {
			std::vector<ItemRef *> result;
			for (auto &[name, free_refs_vec] : this->free_refs) {
				result.insert(result.end(), free_refs_vec.begin(), free_refs_vec.end());
			}
			return result;
		}

		// returns whether free refs exist in this scope for the given name
		std::vector<std::string> get_free_names() const {
			std::vector<std::string> result;
			for (auto &[name, free_refs_vec] : this->free_refs) {
				result.push_back(name);
			}
			return result;
		}

		// // binds all free names to the given item
		// void fake_bind_frees(Item *item_ptr) {
		// 	for (auto &[name, free_refs_vec] : this->free_refs) {
		// 		for (ItemRef *item_ref_ptr : free_refs_vec) {
		// 			// TODO we should be allowed to print this
		// 			// std::cerr << "fake-bound free name: " << item_ref_ptr->get_ref_name() << "\n";
		// 			item_ref_ptr->bind(item_ptr);
		// 		}
		// 	}
		// 	this->free_refs.clear();
		// }

		private:

		// Given an item_ref, exposes it as a ref with a free name. This may
		// be caught by the parent Scope and resolved, or the parent might
		// also expose it as a free ref recursively.
		void push_free_ref(ItemRef &item_ref) {
			std::string_view ref_name = item_ref.get_ref_name();
			if (this->parent) {
				(*this->parent)->add_ref(item_ref);
			} else {
				this->free_refs[std::string(ref_name)].push_back(&item_ref);
			}
		}
	};

	// TODO: because we have raw pointers to some of these items, it's very
	// important that they never move around or else we get memory errors.
	// We're risking a lot by letting this happen; ideally they would be hidden
	// behind another layer of indirection; that is to say Item = ptr<Register>
	// It's fine the way it is for InstructionLabels, L2Functions, and ExternalFunctions
	// because those are stored behind unique_ptrs so they never move; this only
	// applies to the Variables and Registers
	using VariableScope = Scope<Variable, VariableRef, true>;
	using RegisterScope = Scope<Register, RegisterRef, false>;
	using LabelScope = Scope<InstructionLabel *, LabelRef, false>;
	using L2FunctionScope = Scope<L2Function *, L2FunctionRef, false>;
	using ExternalFunctionScope = Scope<ExternalFunction *, ExternalFunctionRef, false>;

	struct AggregateScope {
		VariableScope variable_scope;
		RegisterScope register_scope;
		LabelScope label_scope;
		L2FunctionScope l2_function_scope;
		ExternalFunctionScope external_function_scope;

		void set_parent(AggregateScope &parent);

		void ensure_no_frees() const; // fails if there are free names
		void fake_bind_frees(); // adds fake bindings to free names; leaks memory
	};

	class L2Function : public Function {
		public: // TODO make actual specifiers

		std::vector<std::unique_ptr<Instruction>> instructions;
		AggregateScope agg_scope;

		L2Function(const std::string_view &name, int64_t num_arguments);
		// L2Function(const L2Function &other) :
		// 	instructions {},
		// 	agg_scope {}
		// {
		// 	for (const std::unique_ptr<Instruction> &inst : other.instructions) {
		// TODO how in the world is this supposed to work
		// 	}
		// }

		void add_instruction(std::unique_ptr<Instruction> &&inst);
		void insert_instruction(int index, std::unique_ptr<Instruction> &&inst);
		void bind_all(AggregateScope &agg_scope);
		virtual std::string to_string() const override;
		bool get_never_returns() const override;
	};

	class Program {
		private:

		std::unique_ptr<L2FunctionRef> entry_function_ref;
		std::vector<std::unique_ptr<L2Function>> l2_functions;
		std::vector<std::unique_ptr<ExternalFunction>> external_functions;
		AggregateScope agg_scope;

		public:

		Program(std::unique_ptr<L2FunctionRef> &&entry_function_ref);

		std::string to_string() const;
		void add_l2_function(std::unique_ptr<L2Function> &&func);
		void add_external_function(std::unique_ptr<ExternalFunction> &&func);
		AggregateScope &get_scope();
		L2Function *get_l2_function(int index);
		const std::vector<std::unique_ptr<L2Function>> &get_l2_functions() const { return this->l2_functions; }
		const L2FunctionRef &get_entry_function_ref() const { return *this->entry_function_ref; }
	};

	struct SpillProgram {
		std::unique_ptr<Program> program;
		Variable *var;
		std::string prefix;

		SpillProgram(std::unique_ptr<Program> program, Variable *var, std::string prefix):
			program {std::move(program)}, var {var}, prefix {prefix}
		{}
	};

	// TODO we'd probably want something like a register table object that
	// allows us to look stuff up easier but whatever
	std::vector<Register> generate_registers();

	std::vector<std::unique_ptr<ExternalFunction>> generate_std_functions();

	void add_predefined_registers_and_std(Program &program);
}
