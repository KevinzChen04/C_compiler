#pragma once

#include "std_alias.h"
#include <string>
#include <string_view>
#include <iostream>
#include <typeinfo>
#include <map>
#include <optional>
#include <algorithm>

namespace IR::program {
	using namespace std_alias;
	enum class A_type {
		int64,
		code,
		tuple,
		void_type 
	};

	class AggregateScope;
	class Variable;
	class BasicBlock;
	class IRFunction;
	class ExternalFunction;
	
	std::pair<A_type, int64_t> str_to_type(const std::string& str);
	std::string to_string(A_type t);
	
	class Type{
		A_type a_type;
		int64_t num_dim;

		public:

		Type(A_type a_type, int64_t num_dim): a_type {a_type}, num_dim {num_dim} {}
		Type(const std::string& str);
		Type(){};
		std::string to_string() const;
		int64_t get_num_dimensions(){return this->num_dim;}
		A_type get_a_type() {return this->a_type;}
	};
	class Expr {
		public: 

		virtual std::string to_string() const = 0;
		virtual void bind_to_scope(AggregateScope &agg_scope) = 0;
		virtual std::string to_l3_expr(std::string prefix) = 0;
	};
	struct Trace {
	    Vec<BasicBlock *> block_sequence; 
    };
	template<typename Item>
	class ItemRef : public Expr {
		std::string free_name;
		Item *referent_nullable;

		public:

		ItemRef(std::string free_name) :
			free_name { mv(free_name) },
			referent_nullable { nullptr }
		{}
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual std::string to_string() const override;
		virtual std::string to_l3_expr(std::string prefix) override;
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
		void bind(Item *referent) {
			this->referent_nullable = referent;
		}
	};
	class NumberLiteral : public Expr {
		int64_t value;

		public: 
		
		NumberLiteral(int64_t value) : value { value } {}
		int64_t get_value() const { return this->value; }
		virtual std::string to_string() const override {return std::to_string(this->value);};
		virtual void bind_to_scope(AggregateScope &agg_scope) {return;}
		virtual std::string to_l3_expr(std::string prefix) {return std::to_string(this->value); }
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
	Operator str_to_op(std::string str);
	std::string op_to_string(Operator op);
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
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual std::string to_string() const override;
		virtual std::string to_l3_expr(std::string prefix) override;
	};
	class FunctionCall : public Expr {
		Uptr<Expr> callee;
		Vec<Uptr<Expr>> arguments;

		public:

		FunctionCall(Uptr<Expr> &&callee, Vec<Uptr<Expr>> &&arguments) :
			callee { mv(callee) }, arguments { mv(arguments) }
		{}
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual std::string to_string() const override;
		virtual std::string to_l3_expr(std::string prefix) override;
	};
	class MemoryLocation{
		Uptr<ItemRef<Variable>> base;
		Vec<Uptr<Expr>> dimensions;

		public:

		MemoryLocation(Uptr<ItemRef<Variable>> &&base, Vec<Uptr<Expr>> dimensions) : 
			base {mv(base)}, 
			dimensions {mv(dimensions)} 
		{}
		void bind_to_scope(AggregateScope &agg_scope);
		std::string to_string() const;
		std::string to_l3(std::string prefix);
		Vec<Uptr<Expr>> &get_dimensions() {return this->dimensions; }
	};
	class ArrayDeclaration {
		Vec<Uptr<Expr>> args;

		public:

		ArrayDeclaration(Vec<Uptr<Expr>> args): args {mv(args)}{}
		void bind_to_scope(AggregateScope &agg_scope);
		std::string to_string() const;
		std::string to_l3(std::string prefix);
		Vec<Uptr<Expr>> &get_args(){return this->args;}

	};
	class Length {
		Uptr<ItemRef<Variable>> var;
		Opt<int64_t> dimension;

		public:

		Length(Uptr<ItemRef<Variable>> var): var{mv(var)}{}
		Length(Uptr<ItemRef<Variable>> var, int64_t dimension): var{mv(var)}, dimension{dimension} {}
		void bind_to_scope(AggregateScope &agg_scope);
		ItemRef<Variable> &get_var() const {return *this->var; }
		Opt<int64_t> get_dim() const {return this->dimension; }
		std::string to_string() const;
		std::string to_l3(std::string prefix);
	};

	class Variable {
		std::string name;
		Type t;
		Vec<Uptr<Expr>> *args;
		
		public:

		Variable(std::string name): name { mv(name)} {}
		Variable(std::string name, Type t) : 
			name { mv(name) }, t { mv(t) } 
		{}
		const std::string &get_name() const { return this->name; }
		std::string to_string() const;
		Type &get_type() {return this->t; }
		void set_args(Vec<Uptr<Expr>> &args) { this->args = &args; }
		std::string to_l3() {return "%" + this->name; }
	};
	
	class Instruction {
		public:
		
		virtual std::string to_string() const = 0;
		virtual void bind_to_scope(AggregateScope &agg_scope) = 0;
		virtual void resolver(AggregateScope &agg_scope){}
		virtual std::string to_l3_inst(std::string prefix) = 0;
	};
	class InstructionAssignment: public Instruction {
		Opt<Uptr<ItemRef<Variable>>> maybe_dest;
		Uptr<Expr> source;

		public:

		InstructionAssignment(Uptr<Expr> &&expr) : source { mv(expr) } {}
		InstructionAssignment(Uptr<ItemRef<Variable>> &&destination, Uptr<Expr> &&source) :
			maybe_dest { mv(destination) }, source { mv(source) }
		{}
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual std::string to_string() const override;
		virtual std::string to_l3_inst(std::string prefix) override;
	};
	class InstructionDeclaration: public Instruction {
		Uptr<Variable> var;

		public:
		InstructionDeclaration(Uptr<Variable> var): var {mv(var)} {}
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual Opt<Variable *> get_referent() {return this->var.get(); }
		virtual std::string to_string() const override;
		virtual void resolver(AggregateScope &agg_scope) override;
		virtual std::string to_l3_inst(std::string prefix) override;
	};
	class InstructionStore: public Instruction {
		Uptr<MemoryLocation> dest; 
		Uptr<Expr> source;

		public:

		InstructionStore(Uptr<MemoryLocation> dest, Uptr<Expr> source): 
			dest {mv(dest)}, source {mv(source)}
		{}
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual std::string to_string() const override;
		virtual std::string to_l3_inst(std::string prefix) override;
	};
	class InstructionLoad: public Instruction {
		Uptr<ItemRef<Variable>> dest;
		Uptr<MemoryLocation> source; 

		public:

		InstructionLoad(Uptr<ItemRef<Variable>> dest, Uptr<MemoryLocation> source): 
			dest {mv(dest)}, source {mv(source)}
		{}
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual std::string to_string() const override;
		virtual std::string to_l3_inst(std::string prefix) override;
	};
	class InstructionLength: public Instruction {
		Uptr<ItemRef<Variable>> dest;
		Uptr<Length> source; 

		public:

		InstructionLength(Uptr<ItemRef<Variable>> dest, Uptr<Length> source): 
			dest {mv(dest)}, source {mv(source)}
		{}
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual std::string to_string() const override;
		virtual std::string to_l3_inst(std::string prefix) override;
	};
	class InstructionInitializeArray: public Instruction {
		Uptr<ItemRef<Variable>> dest;
		Uptr<ArrayDeclaration> newArray;

		public:

		InstructionInitializeArray(Uptr<ItemRef<Variable>> dest, Uptr<ArrayDeclaration> newArray): 
			dest {mv(dest)}, newArray {mv(newArray)}
		{}
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual std::string to_string() const override;
		virtual std::string to_l3_inst(std::string prefix) override;
	};

	class Terminator {

		public:

		virtual void bind_to_scope(AggregateScope &agg_scope) = 0;
		virtual Vec<Pair<BasicBlock *, double>> get_successor() = 0;
		virtual std::string to_string() const = 0;
		virtual std::string to_l3_terminator(std::string prefix, Trace &my_trace, BasicBlock *my_bb) = 0;
	};
	class TerminatorBranchOne : public Terminator{
		Uptr<ItemRef<BasicBlock>> bb_ref;

		public:
		TerminatorBranchOne(Uptr<ItemRef<BasicBlock>> &&bb_ref): bb_ref { mv(bb_ref) }{}
		virtual void bind_to_scope(AggregateScope &agg_scope);
		virtual std::string to_string() const;
		virtual Vec<Pair<BasicBlock *, double>> get_successor();
		virtual std::string to_l3_terminator(std::string prefix, Trace &my_trace, BasicBlock *my_bb) override;
	};
	class TerminatorBranchTwo : public Terminator{
		Uptr<Expr> condition;
		Uptr<ItemRef<BasicBlock>> branchTrue;
		Uptr<ItemRef<BasicBlock>> branchFalse;

		public:

		TerminatorBranchTwo(
			Uptr<Expr> condition,
			Uptr<ItemRef<BasicBlock>> branchTrue,
			Uptr<ItemRef<BasicBlock>> branchFalse
		):
			condition {mv(condition)},
			branchTrue (mv(branchTrue)),
			branchFalse {mv(branchFalse)}
		{}
		virtual void bind_to_scope(AggregateScope &agg_scope);
		virtual Vec<Pair<BasicBlock *, double>> get_successor();
		virtual std::string to_string() const;
		virtual std::string to_l3_terminator(std::string prefix, Trace &my_trace, BasicBlock *my_bb) override;
	};
	class TerminatorReturnVoid : public Terminator {
		public:
		virtual void bind_to_scope(AggregateScope &agg_scope){}
		virtual Vec<Pair<BasicBlock *, double>> get_successor() { return {}; }
		virtual std::string to_string() const {return "return\n"; }
		virtual std::string to_l3_terminator(std::string prefix, Trace &my_trace, BasicBlock *my_bb) {return "\treturn\n";};
	};
	class TerminatorReturnVar : public Terminator {
		Uptr<Expr> ret_expr;

		public:

		TerminatorReturnVar(Uptr<Expr> ret_expr): ret_expr {mv(ret_expr)} {}
		virtual void bind_to_scope(AggregateScope &agg_scope);
		virtual std::string to_string() const;
		virtual std::string to_l3_terminator(std::string prefix, Trace &my_trace, BasicBlock *my_bb) override;
		virtual Vec<Pair<BasicBlock *, double>> get_successor() { return {};}
	};

	class BasicBlock {
		std::string name;
		Vec<Uptr<Instruction>> inst;
		Uptr<Terminator> te;
		Vec<Pair<BasicBlock *, double>> successors;

		public:

		BasicBlock(
			std::string name,
			Vec<Uptr<Instruction>> &&inst,
			Uptr<Terminator> &&te
		) :
			name { mv(name) },
			inst { mv(inst) },
			te { mv(te) },
			successors {{}}
		{}
		std::string to_string() const;
		const std::string &get_name() const { return this->name; }
		Vec<Pair<BasicBlock *, double>> &get_successors() {return this->successors;}
		Vec<Uptr<Instruction>> &get_inst() { return this->inst; }
		Uptr<Terminator> &get_terminator() { return this->te; }
		void set_successors(Vec<Pair<BasicBlock *, double>> succ) {this->successors = mv(succ); }
		void set_name(std::string new_name) {this->name = mv(new_name); }
		void bind_to_scope(AggregateScope &agg_scope);

		class Builder {
			std::string name;
			Vec<Uptr<Instruction>> inst;
			Uptr<Terminator> te;

			public:

			Builder(){}
			Uptr<BasicBlock> get_result();
			void add_name(std::string name);
			void add_instruction(Uptr<Instruction> &&inst, AggregateScope &agg_scope);
			void add_terminator(Uptr<Terminator> &&te, AggregateScope &agg_scope);
		};
	};

	template<typename Item>
	class Scope {
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

		void set_parent(Scope &parent) {
			if (this->parent) {
				std::cerr << "this scope already has a parent oops\n";
				exit(1);
			}

			this->parent = std::make_optional<Scope *>(&parent);

			for (auto &[name, our_free_refs_vec] : this->free_refs) {
				for (ItemRef<Item> *our_free_ref : our_free_refs_vec) {
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

		// Adds the specified item to this scope under the specified name,
		// resolving all free refs who were depending on that name. Dies if
		// there already exists an item under that name.
		int resolve_item(std::string name, Item *item) {
			int x = 0;
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
					x ++;
				}
				this->free_refs.erase(free_refs_vec_it);
			}
			return x;
		}

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

	struct AggregateScope {
		Scope<Variable> variable_scope;
		Scope<BasicBlock> basic_block_scope;
		Scope<IRFunction> ir_function_scope;
		Scope<ExternalFunction> external_function_scope;

		void set_parent(AggregateScope &parent);
	};

	class Function {
		public:
		virtual const std::string &get_name() const = 0;
		virtual std::string to_string() const = 0;
	};

	class IRFunction : public Function {
		std::string name;
		Type ret_type;
		Vec<Uptr<BasicBlock>> blocks;
		Vec<Uptr<Variable>> vars;
		Vec<Variable *> parameter_vars;
		AggregateScope agg_scope;

		public:

		IRFunction(
			std::string name,
			Type ret_type,
			Vec<Uptr<BasicBlock>> blocks,
			Vec<Uptr<Variable>> vars,
			Vec<Variable *> parameter_vars,
			AggregateScope agg_scope
		) :
			name { mv(name) },
			ret_type {mv(ret_type)},
			blocks { mv(blocks) },
			vars { mv(vars) },
			parameter_vars { mv(parameter_vars) },
			agg_scope {mv(agg_scope)}
		{}
		virtual const std::string &get_name() const override { return this->name; }
		const Vec<Uptr<BasicBlock>> &get_blocks() const { return this->blocks; }
		const Vec<Variable *> &get_parameter_vars() const { return this->parameter_vars; }
		AggregateScope &get_scope() { return this->agg_scope; }
		virtual std::string to_string() const override;

		class Builder {
			std::string name;
			Type ret_type;
			Vec<Uptr<BasicBlock>> basic_blocks;
			Vec<Uptr<Variable>> vars;
			Vec<Variable *> parameter_vars;
			AggregateScope agg_scope;

			public:

			Builder() {};
			Uptr<IRFunction> get_result();
			AggregateScope &get_scope(){return this->agg_scope; }
			void add_name(std::string name);
			void add_ret_type(Type t);
			void add_block(Uptr<BasicBlock> &&bb);
			void add_parameter(Type type, std::string name);
		};
	};

	class ExternalFunction : public Function {
		std::string name;
		Vec<int> num_arguments;

		public:

		ExternalFunction(std::string name, Vec<int> num_arguments) :
			name { mv(name) }, num_arguments { mv(num_arguments) }
		{}

		virtual const std::string &get_name() const override { return this->name; }
		virtual std::string to_string() const override;
	};

	class Program {
		Vec<Uptr<IRFunction>> ir_functions;
		Vec<Uptr<ExternalFunction>> external_functions;

		public:

		Program(
			Vec<Uptr<IRFunction>> &&ir_functions,
			Vec<Uptr<ExternalFunction>> &&external_functions
		) :
			ir_functions { mv(ir_functions) },
			external_functions { mv(external_functions) }
		{}
		std::string to_string() const;
		Vec<Uptr<IRFunction>> &get_ir_functions() { return this->ir_functions; }
		class Builder {
			Vec<Uptr<IRFunction>> ir_functions;
			Vec<Uptr<ExternalFunction>> external_functions;
			AggregateScope agg_scope;
			
			public:
			Builder();
			Vec<Uptr<IRFunction>> &get_ir_functions() { return this->ir_functions; }
			void add_ir_function(Uptr<IRFunction> &&function);
			Uptr<Program> get_result();
		};
	};

	Vec<Uptr<ExternalFunction>> generate_std_functions();
}