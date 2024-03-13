#pragma once

#include "std_alias.h"
#include <string>
#include <string_view>
#include <iostream>
#include <typeinfo>
#include <variant>

namespace L3::program {
	using namespace std_alias;

	// TODO rename `Builder` classes to `Mother` and `get_result` to `birth`

	template<typename Item> class ItemRef;
	class Variable;
	class BasicBlock;
	class Function;
	class L3Function;
	class ExternalFunction;
	class NumberLiteral;
	class MemoryLocation;
	class BinaryOperation;
	class FunctionCall;

	// interface
	class ExprVisitor {
		public:
		virtual void visit(ItemRef<Variable> &expr) = 0;
		virtual void visit(ItemRef<BasicBlock> &expr) = 0;
		virtual void visit(ItemRef<L3Function> &expr) = 0;
		virtual void visit(ItemRef<ExternalFunction> &expr) = 0;
		virtual void visit(NumberLiteral &expr) = 0;
		virtual void visit(MemoryLocation &expr) = 0;
		virtual void visit(BinaryOperation &expr) = 0;
		virtual void visit(FunctionCall &expr) = 0;
	};

	struct AggregateScope;
	struct ComputationNode;

	// interface
	class Expr {
		public:
		// virtual Set<Variable *> get_vars_on_read() const { return {}; }
		// virtual Set<Variable *> get_vars_on_write(bool get_read_vars) const { return {}; }
		virtual void bind_to_scope(AggregateScope &agg_scope) = 0;
		virtual Uptr<ComputationNode> to_computation_tree() const = 0;
		virtual std::string to_string() const = 0;
		virtual void accept(ExprVisitor &v) = 0;
	};

	// instantiations must implement the virtual methods
	template<typename Item>
	class ItemRef : public Expr {
		std::string free_name; // the name originally given to the variable
		Item *referent_nullable;

		public:

		ItemRef(std::string free_name) :
			free_name { mv(free_name) },
			referent_nullable { nullptr }
		{}
		// ItemRef(Item *referent);

		// virtual Set<Item *> get_vars_on_read() const override;
		// virtual Set<Item *> get_vars_on_write(bool get_read_vars) const override;
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		void bind(Item *referent) {
			this->referent_nullable = referent;
		}
		Opt<Item *> get_referent() const {
			if (this->referent_nullable) {
				return this->referent_nullable;
			} else {
				return {};
			}
		}
		const std::string &get_ref_name() const {
			if (this->referent_nullable) {
				return this->referent_nullable->get_name();
			} else {
				return this->free_name;
			}
		}
		virtual Uptr<ComputationNode> to_computation_tree() const override;
		virtual std::string to_string() const override;
		virtual void accept(ExprVisitor &v) override { v.visit(*this); }
	};

	class NumberLiteral : public Expr {
		int64_t value;

		public:

		NumberLiteral(int64_t value) : value { value } {}
		NumberLiteral(std::string_view value_str);

		int64_t get_value() const { return this->value; }
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual Uptr<ComputationNode> to_computation_tree() const override;
		virtual std::string to_string() const override;
		virtual void accept(ExprVisitor &v) override { v.visit(*this); }
	};

	class Variable;

	class MemoryLocation : public Expr {
		Uptr<ItemRef<Variable>> base;

		public:

		MemoryLocation(Uptr<ItemRef<Variable>> &&base) : base { mv(base) } {}

		// virtual Set<Variable *> get_vars_on_read() const override;
		// virtual Set<Variable *> get_vars_on_write(bool get_read_vars) const override;
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual Uptr<ComputationNode> to_computation_tree() const override;
		virtual std::string to_string() const override;
		virtual void accept(ExprVisitor &v) override { v.visit(*this); }
	};

	enum struct Operator {
		lt,
		le,
		eq,
		ge,
		gt,
		plus,
		minus,
		times,
		bitwise_and,
		lshift,
		rshift
	};
	Operator str_to_op(std::string_view str);
	std::string to_string(Operator op);
	Opt<Operator> flip_operator(Operator op);

	class BinaryOperation : public Expr {
		Uptr<Expr> lhs;
		Uptr<Expr> rhs;
		Operator op;

		public:

		BinaryOperation(Uptr<Expr> &&lhs, Uptr<Expr> &&rhs, Operator op) :
			lhs { mv(lhs) },
			rhs { mv(rhs) },
			op { op }
		{}

		// virtual Set<Variable *> get_vars_on_read() const override;
		// virtual Set<Variable *> get_vars_on_write(bool get_read_vars) const override;
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual Uptr<ComputationNode> to_computation_tree() const override;
		virtual std::string to_string() const override;
		virtual void accept(ExprVisitor &v) override { v.visit(*this); }
	};

	class FunctionCall : public Expr {
		Uptr<Expr> callee;
		Vec<Uptr<Expr>> arguments;

		public:

		FunctionCall(Uptr<Expr> &&callee, Vec<Uptr<Expr>> &&arguments) :
			callee { mv(callee) }, arguments { mv(arguments) }
		{}

		// virtual Set<Variable *> get_vars_on_read() const override;
		// virtual Set<Variable *> get_vars_on_write(bool get_read_vars) const override;
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual Uptr<ComputationNode> to_computation_tree() const override;
		virtual std::string to_string() const override;
		virtual void accept(ExprVisitor &v) override { v.visit(*this); }
	};

	class InstructionReturn;
	class InstructionAssignment;
	class InstructionStore;
	class InstructionLabel;
	class InstructionBranch;

	// interface
	class InstructionVisitor {
		public:
		virtual void visit(InstructionReturn &inst) = 0;
		virtual void visit(InstructionAssignment &inst) = 0;
		virtual void visit(InstructionStore &inst) = 0;
		virtual void visit(InstructionLabel &inst) = 0;
		virtual void visit(InstructionBranch &inst) = 0;
	};

	// interface
	class Instruction {
		public:
		virtual void bind_to_scope(AggregateScope &agg_scope) = 0;
		virtual Uptr<ComputationNode> to_computation_tree() const = 0;
		virtual std::string to_string() const = 0;
		virtual void accept(InstructionVisitor &v) = 0;

		// Returned by an instruction to describe the behavior of control flow
		// following that instruction.
		struct ControlFlowResult {
			bool falls_through; // whether the instruction might move to the next instruction
			bool yields_control; // whether the instruction yields control to another instruction on the promise that it will return (i.e. a function call)
			Opt<ItemRef<BasicBlock> *> jmp_dest; // a label this location might jump to
		};
		virtual ControlFlowResult get_control_flow() const = 0;
	};

	class InstructionReturn : public Instruction {
		Opt<Uptr<Expr>> return_value;

		public:

		InstructionReturn(Opt<Uptr<Expr>> &&return_value) : return_value { mv(return_value) } {}

		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual Uptr<ComputationNode> to_computation_tree() const override;
		virtual Instruction::ControlFlowResult get_control_flow() const { return { false, false, Opt<ItemRef<BasicBlock> *>() }; };
		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
	};

	class InstructionAssignment : public Instruction {
		// the destination is optional only for the pure call instruction
		Opt<Uptr<ItemRef<Variable>>> maybe_dest;
		Uptr<Expr> source;

		public:

		InstructionAssignment(Uptr<Expr> &&expr) : source { mv(expr) } {}
		InstructionAssignment(Uptr<Expr> &&source, Uptr<ItemRef<Variable>> &&destination) :
			maybe_dest { mv(destination) }, source { mv(source) }
		{}

		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual Uptr<ComputationNode> to_computation_tree() const override;
		virtual Instruction::ControlFlowResult get_control_flow() const override;
		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
	};

	class InstructionStore : public Instruction {
		Uptr<ItemRef<Variable>> base;
		Uptr<Expr> source;

		public:

		InstructionStore(Uptr<Expr> &&source, Uptr<ItemRef<Variable>> &&base) :
			base { mv(base) }, source { mv(source) }
		{}

		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual Uptr<ComputationNode> to_computation_tree() const override;
		virtual Instruction::ControlFlowResult get_control_flow() const override;
		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
	};

	class InstructionLabel : public Instruction {
		std::string label_name;

		public:

		InstructionLabel(std::string label_name) : label_name { mv(label_name) } {}

		const std::string &get_name() const { return this->label_name; }
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual Uptr<ComputationNode> to_computation_tree() const override;
		virtual Instruction::ControlFlowResult get_control_flow() const { return { true, false, Opt<ItemRef<BasicBlock> *>()}; };
		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
	};

	class InstructionBranch : public Instruction {
		Opt<Uptr<Expr>> condition;
		Uptr<ItemRef<BasicBlock>> label;

		public:

		InstructionBranch(Uptr<ItemRef<BasicBlock>> &&label, Uptr<Expr> &&condition) :
			condition { mv(condition) }, label { mv(label) }
		{}
		InstructionBranch(Uptr<ItemRef<BasicBlock>> &&label) :
			condition {}, label { mv(label) }
		{}

		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual Instruction::ControlFlowResult get_control_flow() const override;
		virtual Uptr<ComputationNode> to_computation_tree() const override;
		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
	};

	// represents a computation from a series of instructions, starting with
	// the leaves as input and ultimately outputting the root.
	// subclasses are suffixed "Cn" meaning "computation node"
	struct ComputationNode {
		Opt<Variable *> destination;
		// none if this computation is only for its side effects, or if there is
		// no actual L3 variable through which a computuation flows (e.g. `%a <-
		// 1 + 2` doesn't have the values 1 and 2 flow through any variables)

		ComputationNode(Opt<Variable *> destination) :
			destination { destination }
		{}
		virtual std::string to_string() const;
		virtual Set<Variable*> get_vars_read() const = 0;
		virtual Opt<Variable*> get_var_written() const;

		// Returns every instance of the specific variable found in the leaves
		// of the tree
		virtual Vec<Uptr<ComputationNode> *> get_merge_targets(Variable *target) = 0;
	};

	struct NoOpCn : ComputationNode {
		// notice there is no destination
		NoOpCn() : ComputationNode({}) {}
		virtual std::string to_string() const override;
		virtual Set<Variable*> get_vars_read() const override;
		virtual Vec<Uptr<ComputationNode> *> get_merge_targets(Variable *target) override;
	};

	// represents an atomic "computation" that just returns the value of a variable
	struct NumberCn : ComputationNode {
		int64_t value;

		NumberCn(int64_t value) : ComputationNode({}), value { value } {}
		virtual std::string to_string() const override;
		virtual Set<Variable *> get_vars_read() const override;
		virtual Vec<Uptr<ComputationNode> *> get_merge_targets(Variable *target) override;
	};

	// represents an atomic "computation" that just returns the value of a variable
	struct VariableCn : ComputationNode {
		// re-use the parent's destination field as the field "read" by this node
		VariableCn(Variable *var) : ComputationNode(std::make_optional<Variable *>(var)) {}
		virtual std::string to_string() const override;
		virtual Set<Variable *> get_vars_read() const override;
		virtual Vec<Uptr<ComputationNode> *> get_merge_targets(Variable *target) override;
	};

	// represents an atomic "computation" that just returns a function pointer
	struct FunctionCn : ComputationNode {
		Function *function;

		FunctionCn(Function *function) : ComputationNode({}), function { function } {}
		virtual std::string to_string() const override;
		virtual Set<Variable *> get_vars_read() const override;
		virtual Vec<Uptr<ComputationNode> *> get_merge_targets(Variable *target) override;
	};

	// represents an atomic "computation" that just returns a labeled location
	struct LabelCn : ComputationNode {
		BasicBlock *jmp_dest;

		LabelCn(BasicBlock *jmp_dest) : ComputationNode({}), jmp_dest { jmp_dest } {}
		virtual std::string to_string() const override;
		virtual Set<Variable *> get_vars_read() const override;
		virtual Vec<Uptr<ComputationNode> *> get_merge_targets(Variable *target) override;
	};

	struct MoveCn : ComputationNode {
		Uptr<ComputationNode> source;

		MoveCn(Opt<Variable *> destination, Uptr<ComputationNode> source) :
			ComputationNode(destination), source { mv(source) }
		{}
		virtual std::string to_string() const override;
		virtual Set<Variable *> get_vars_read() const override;
		virtual Vec<Uptr<ComputationNode> *> get_merge_targets(Variable *target) override;
	};

	struct BinaryCn : ComputationNode {
		Operator op;
		Uptr<ComputationNode> lhs;
		Uptr<ComputationNode> rhs;

		BinaryCn(Opt<Variable *> destination, Operator op, Uptr<ComputationNode> lhs, Uptr<ComputationNode> rhs) :
			ComputationNode(destination), op {op}, lhs { mv(lhs) }, rhs { mv(rhs) }
		{}
		virtual std::string to_string() const override;
		virtual Set<Variable *> get_vars_read() const override;
		virtual Vec<Uptr<ComputationNode> *> get_merge_targets(Variable *target) override;
	};

	struct CallCn : ComputationNode {
		Uptr<ComputationNode> callee;
		Vec<Uptr<ComputationNode>> arguments;

		CallCn(Opt<Variable *> destination, Uptr<ComputationNode> callee, Vec<Uptr<ComputationNode>> arguments) :
			ComputationNode(destination), callee { mv(callee) }, arguments { mv(arguments) }
		{}
		virtual std::string to_string() const override;
		virtual Set<Variable *> get_vars_read() const override;
		virtual Vec<Uptr<ComputationNode> *> get_merge_targets(Variable *target) override;

	};

	struct LoadCn : ComputationNode {
		Uptr<ComputationNode> address;

		LoadCn(Opt<Variable *> destination, Uptr<ComputationNode> address) :
			ComputationNode(destination), address { mv(address) }
		{}
		virtual std::string to_string() const override;
		virtual Set<Variable *> get_vars_read() const override;
		virtual Vec<Uptr<ComputationNode> *> get_merge_targets(Variable *target) override;
	};

	struct StoreCn : ComputationNode {
		Uptr<ComputationNode> address;
		Uptr<ComputationNode> value;

		// note that there is no destination argument
		StoreCn(Uptr<ComputationNode> address, Uptr<ComputationNode> value) :
			ComputationNode({}), address { mv(address) }, value { mv(value) }
		{}
		virtual std::string to_string() const override;
		virtual Set<Variable *> get_vars_read() const override;
		virtual Vec<Uptr<ComputationNode> *> get_merge_targets(Variable* target) override;
	};

	struct BranchCn : ComputationNode {
		BasicBlock *jmp_dest;
		Opt<Uptr<ComputationNode>> condition;

		// note that there is no destination argument
		BranchCn(BasicBlock *jmp_dest, Opt<Uptr<ComputationNode>> condition) :
			ComputationNode({}), jmp_dest { jmp_dest }, condition { mv(condition) }
		{}
		virtual std::string to_string() const override;
		virtual Set<Variable *> get_vars_read() const override;
		virtual Vec<Uptr<ComputationNode> *> get_merge_targets(Variable *target) override;
	};

	struct ReturnCn : ComputationNode {
		Opt<Uptr<ComputationNode>> value;

		// note that there is no destination argument
		ReturnCn() : ComputationNode({}), value {} {}
		ReturnCn(Uptr<ComputationNode> value) : ComputationNode({}), value { mv(value) } {}
		virtual std::string to_string() const override;
		virtual Set<Variable *> get_vars_read() const override;
		virtual Vec<Uptr<ComputationNode> *> get_merge_targets(Variable *target) override;
	};

	std::string to_string(const Uptr<ComputationNode> &node);
	std::string to_string(const ComputationNode &node);

	template<typename... CnSubclasses>
	bool is_dynamic_type(const ComputationNode &s) {
		return (dynamic_cast<const CnSubclasses *>(&s) || ...);
	}

	// meant to hold a computation tree as well as all the information that comes
	// along with it: variables read and variables written, as well as all
	// possible merge candidates
	class ComputationTreeBox {
		Uptr<ComputationNode> root_nullable; // null means this box has been stolen from in a merge
		bool has_load;
		bool has_store;

		public:

		ComputationTreeBox(const Instruction &inst);
		// it is the reponsibility of the caller to make sure this box has a
		// value before doing any other operation
		const bool has_value() const { return static_cast<bool>(this->root_nullable); }
		const Uptr<ComputationNode> &get_tree() const { return this->root_nullable; }
		Set<Variable *> get_variables_read() const { return this->root_nullable->get_vars_read(); }
		const bool get_has_load() const { return this->has_load; }
		const bool get_has_store() const { return this->has_store; }
		Opt<Variable *> get_var_written() const { return this->root_nullable->get_var_written(); }

		// steals from the other ComputationTreeBox and merges.
		// fails and returns false if there are too many or not enough merge
		// targets.
		bool merge(ComputationTreeBox &other);
	};

	class BasicBlock {
		std::string name; // the empty string is treated as a lack of name; we can't just have an optional because ItemRef<BasicBlock> demans that the get_name method always returns a string
		Vec<Uptr<Instruction>> raw_instructions;
		Vec<ComputationTreeBox> tree_boxes;
		struct VarLiveness {
			Set<Variable *> gen_set;
			Set<Variable *> kill_set;
			Set<Variable *> in_set;
			Set<Variable *> out_set;
		} var_liveness;
		Vec<BasicBlock *> succ_blocks;

		explicit BasicBlock();

		/* explicit BasicBlock(
			Opt<std::string> name,
			Vec<Uptr<Instruction>> raw_instructions,
			Vec<BasicBlock *> succ_blocks
		) :
			name { mv(name) },
			raw_instructions { mv(raw_instructions) },
			succ_blocks { mv(succ_blocks) }
		{} */

		public:

		const std::string &get_name() const { return this->name; }
		void mangle_name(std::string new_name) { this->name = mv(new_name); }
		Vec<Uptr<Instruction>> &get_raw_instructions() { return this->raw_instructions; }
		const Vec<Uptr<Instruction>> &get_raw_instructions() const { return this->raw_instructions; }
		const Vec<ComputationTreeBox> &get_tree_boxes() const { return this->tree_boxes; }
		const Vec<BasicBlock *> &get_succ_blocks() const { return this->succ_blocks; }
		void generate_computation_trees(); // also generates the gen and kill sets
		bool update_in_out_sets();
		void merge_trees();
		std::string to_string() const;

		class Builder {
			Uptr<BasicBlock> fetus;
			Opt<ItemRef<BasicBlock> *> succ_block_refs;
			bool must_end;
			// whether the BasicBlock being built can still be added to;
			// false if there is an instruction that yields control flow
			bool falls_through;

			public:

			Builder();

			// Dies if there are unresolved label names at the time this
			// function is called, since that means that the instructions of
			// this block can't see its successors.
			Uptr<BasicBlock> get_result(BasicBlock *successor_nullable);

			// Returns a pointer to the undeveloped BasicBlock currently
			// being built. This only exists because we need a memory location
			// so that BasicBlocks can be linked to each other.
			// THIS POINTER SHOULD NOT BE READ!
			Pair<BasicBlock *, Opt<std::string>> get_fetus_and_name();

			// Takes an Instruction that has all its free names either bound
			// or pending binding to a scope.
			// Returns whether the instruction was added. (Fails without moving
			// if the basic block must end at the given instruction.)
			bool add_next_instruction(Uptr<Instruction> &&inst);
		};
	};

	std::string to_string(BasicBlock *const &block);

	// A Scope represents a namespace of Items that the ItemRefs care about.
	// A Scope does not own any of the Items it maps to.
	// `(name, item)` pairs in this->dict represent Items defined in this scope
	// under `name`.
	// `(name, ItemRef *)` in free_referrers represents that that ItemRef has
	// refers to `name`, but that it is a free name (unbound to anything in this
	// scope)
	template<typename Item>
	class Scope {
		// If a Scope has a parent, then it cannot have any
		// free_refs; they must have been transferred to the parent.
		Opt<Scope *> parent;
		Map<std::string, Item *> dict;
		Map<std::string, Vec<ItemRef<Item> *>> free_refs;

		public:

		Scope() : parent {}, dict {}, free_refs {} {}

		Vec<Item *> get_all_items() const {
			Vec<Item *> result;
			if (this->parent) {
				result = mv(static_cast<const Scope *>(*this->parent)->get_all_items());
			}
			for (const auto &[name, item] : this->dict) {
				result.push_back(item);
			}
			return result;
		}

		/* std::vector<Item> get_all_items() {
			std::vector<Item> result;
			if (this->parent) {
				result = std::move((*this->parent)->get_all_items());
			}
			for (auto &[name, item] : this->dict) {
				result.push_back(item);
			}
			return result;
		} */

		// returns whether the ref was immediately bound or was left as free
		bool add_ref(ItemRef<Item> &item_ref) {
			std::string_view ref_name = item_ref.get_ref_name();

			Opt<Item *> maybe_item = this->get_item_maybe(ref_name);
			if (maybe_item) {
				// bind the ref to the item
				item_ref.bind(*maybe_item);
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
		void resolve_item(std::string name, Item *item) {
			auto existing_item_it = this->dict.find(name);
			if (existing_item_it != this->dict.end()) {
				std::cerr << "name conflict: " << name << std::endl;
				exit(-1);
			}

			const auto [item_it, _] = this->dict.insert(std::make_pair(name, item));
			auto free_refs_vec_it = this->free_refs.find(name);
			if (free_refs_vec_it != this->free_refs.end()) {
				for (ItemRef<Item> *item_ref_ptr : free_refs_vec_it->second) {
					item_ref_ptr->bind(item_it->second);
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
		/* Item get_item_or_create(const std::string_view &name) {
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
		} */

		std::optional<Item *> get_item_maybe(std::string_view name) {
			auto item_it = this->dict.find(name);
			if (item_it != this->dict.end()) {
				return std::make_optional<Item *>(item_it->second);
			} else {
				if (this->parent) {
					return (*this->parent)->get_item_maybe(name);
				} else {
					return {};
				}
			}
		}

		/* void remove_item(Item *item) {
			for (auto it = this->dict.begin(); it != this->dict.end(); ++it) {
				if (&it->second == item) {
					dict.erase(it);
					break;
				}
			}
		} */

		// Sets the given Scope as the parent of this Scope, transferring all
		// current and future free names to the parent. If this scope already
		// has a parent, dies.
		void set_parent(Scope &parent) {
			if (this->parent) {
				std::cerr << "this scope already has a parent oops\n";
				exit(1);
			}

			this->parent = std::make_optional<Scope *>(&parent);

			for (auto &[name, our_free_refs_vec] : this->free_refs) {
				for (ItemRef<Item> *our_free_ref : our_free_refs_vec) {
					// TODO optimization here is possible; instead of using the
					// public API of the parent we can just query the dictionary
					// directly
					(*this->parent)->add_ref(*our_free_ref);
				}
			}
			this->free_refs.clear();
		}

		// returns whether free refs exist in this scope for the given name
		Vec<ItemRef<Item> *> get_free_refs() const {
			std::vector<ItemRef<Item> *> result;
			for (auto &[name, free_refs_vec] : this->free_refs) {
				result.insert(result.end(), free_refs_vec.begin(), free_refs_vec.end());
			}
			return result;
		}

		// returns the free names exist in this scope
		Vec<std::string> get_free_names() const {
			Vec<std::string> result;
			for (auto &[name, free_refs_vec] : this->free_refs) {
				result.push_back(name);
			}
			return result;
		}

		// // binds all free names to the given item
		/* void fake_bind_frees(Item *item_ptr) {
			for (auto &[name, free_refs_vec] : this->free_refs) {
				for (ItemRef *item_ref_ptr : free_refs_vec) {
					// TODO we should be allowed to print this
					// std::cerr << "fake-bound free name: " << item_ref_ptr->get_ref_name() << "\n";
					item_ref_ptr->bind(item_ptr);
				}
			}
			this->free_refs.clear();
		} */

		private:

		// Given an item_ref, exposes it as a ref with a free name. This may
		// be caught by the parent Scope and resolved, or the parent might
		// also expose it as a free ref recursively.
		void push_free_ref(ItemRef<Item> &item_ref) {
			std::string_view ref_name = item_ref.get_ref_name();
			if (this->parent) {
				(*this->parent)->add_ref(item_ref);
			} else {
				this->free_refs[std::string(ref_name)].push_back(&item_ref);
			}
		}
	};

	class L3Function;
	class ExternalFunction;

	struct AggregateScope {
		Scope<Variable> variable_scope;
		Scope<BasicBlock> label_scope;
		Scope<L3Function> l3_function_scope;
		Scope<ExternalFunction> external_function_scope;

		void set_parent(AggregateScope &parent);
	};

	class Variable {
		std::string name;

		public:

		Variable(std::string name) : name { mv(name) } {}

		const std::string &get_name() const { return this->name; }
		std::string to_string() const;
	};

	std::string to_string(Variable *const &variable);

	// interface
	class Function {
		public:
		virtual const std::string &get_name() const = 0;
		virtual bool verify_argument_num(int num) const = 0;
		// virtual bool get_never_returns() const = 0;
		virtual std::string to_string() const = 0;
	};

	class L3Function : public Function {
		std::string name;
		Vec<Uptr<BasicBlock>> blocks;
		Vec<Uptr<Variable>> vars;
		Vec<Variable *> parameter_vars;

		explicit L3Function(
			std::string name,
			Vec<Uptr<BasicBlock>> &&blocks,
			Vec<Uptr<Variable>> &&vars,
			Vec<Variable *> parameter_vars
		) :
			name { mv(name) },
			blocks { mv(blocks) },
			vars { mv(vars) },
			parameter_vars { mv(parameter_vars) }
		{}

		public:

		virtual const std::string &get_name() const override { return this->name; }
		Vec<Uptr<BasicBlock>> &get_blocks() { return this->blocks; }
		const Vec<Uptr<BasicBlock>> &get_blocks() const { return this->blocks; }
		const Vec<Variable *> &get_parameter_vars() const { return this->parameter_vars; }
		virtual bool verify_argument_num(int num) const override;
		// virtual bool get_never_returns() const override;
		virtual std::string to_string() const override;

		class Builder {
			std::string name;
			Vec<BasicBlock::Builder> block_builders;
			Vec<Uptr<Variable>> vars;
			Vec<Variable *> parameter_vars;

			AggregateScope agg_scope;

			public:
			Builder();
			Pair<Uptr<L3Function>, AggregateScope> get_result();
			void add_name(std::string name);
			void add_next_instruction(Uptr<Instruction> &&inst);
			void add_parameter(std::string var_name);
		};
	};

	class ExternalFunction : public Function {
		std::string name;
		Vec<int> valid_num_arguments;

		public:

		ExternalFunction(std::string name, Vec<int> valid_num_arguments) :
			name { mv(name) }, valid_num_arguments { mv(valid_num_arguments) }
		{}

		virtual const std::string &get_name() const override { return this->name; }
		virtual bool verify_argument_num(int num) const override;
		// virtual bool get_never_returns() const override;
		virtual std::string to_string() const override;
	};

	std::string to_string(Function *const &function);

	class Program {
		Vec<Uptr<L3Function>> l3_functions;
		Vec<Uptr<ExternalFunction>> external_functions;
		Uptr<ItemRef<L3Function>> main_function_ref;

		explicit Program(
			Vec<Uptr<L3Function>> &&l3_functions,
			Vec<Uptr<ExternalFunction>> &&external_functions,
			Uptr<ItemRef<L3Function>> &&main_function_ref
		) :
			l3_functions { mv(l3_functions) },
			external_functions { mv(external_functions) },
			main_function_ref { mv(main_function_ref) }
		{}

		public:

		std::string to_string() const;
		Vec<Uptr<L3Function>> &get_l3_functions() { return this->l3_functions; }
		const Vec<Uptr<L3Function>> &get_l3_functions() const { return this->l3_functions; }
		const ItemRef<L3Function> &get_main_function_ref() const { return *this->main_function_ref; }

		class Builder {
			Vec<Uptr<L3Function>> l3_functions;
			Uptr<ItemRef<L3Function>> main_function_ref;
			Vec<Uptr<ExternalFunction>> external_functions;
			AggregateScope agg_scope;

			public:
			Builder();
			Uptr<Program> get_result();
			void add_l3_function(Uptr<L3Function> &&function, AggregateScope &fun_scope);
		};
	};

	Vec<Uptr<ExternalFunction>> generate_std_functions();
}