#pragma once

#include "std_alias.h"
#include <string>
#include <string_view>
#include <iostream>

namespace Lb::hir {
	using namespace std_alias;

	template<typename Item> class ItemRef;

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

		// Adds the specified item to this scope under the specified name, and
		// if `bind_existing` is set, also bind all free refs who were
		// depending on that name; otherwise those refs will remain free. Dies
		// if there already exists an item under that name.
		void resolve_item(std::string name, Item *item, bool bind_existing) {
			auto existing_item_it = this->dict.find(name);
			if (existing_item_it != this->dict.end()) {
				std::cerr << "name conflict: " << name << std::endl;
				exit(-1);
			}

			const auto [item_it, _] = this->dict.insert(std::make_pair(name, item));
			if (bind_existing) {
				auto free_refs_vec_it = this->free_refs.find(name);
				if (free_refs_vec_it != this->free_refs.end()) {
					for (ItemRef<Item> *item_ref_ptr : free_refs_vec_it->second) {
						item_ref_ptr->bind(item_it->second);
					}
					this->free_refs.erase(free_refs_vec_it);
				}
			}
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

	// something that can be referred to by a simple name
	struct Nameable {
		virtual const std::string &get_name() const = 0;
	};

	struct Variable : Nameable {
		std::string name;
		std::string type_name;

		Variable(std::string name, std::string type_name) : name { mv(name) }, type_name { mv(type_name) } {}

		const std::string &get_name() const override { return this->name; }
	};

	// abstract class
	struct Expr {
		virtual void bind_to_scope(Scope<Nameable> &scope) = 0;
		virtual std::string to_string() const = 0;
	};

	// instantiations must implement the virtual methods
	template<typename Item>
	class ItemRef : public Expr {
		std::string free_name; // the original name
		Item *referent_nullable;

		public:

		ItemRef(std::string free_name) :
			free_name { mv(free_name) },
			referent_nullable { nullptr }
		{}

		void bind_to_scope(Scope<Nameable> &scope) override;
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
		std::string to_string() const override;
	};

	struct NumberLiteral : Expr {
		int64_t value;

		NumberLiteral(int64_t value) : value { value } {}

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
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
	std::string to_string(Operator op);
	Operator str_to_op(std::string_view str);

	struct BinaryOperation : Expr {
		Uptr<Expr> lhs;
		Uptr<Expr> rhs;
		Operator op;

		BinaryOperation(Uptr<Expr> lhs, Uptr<Expr> rhs, Operator op) :
			lhs { mv(lhs) }, rhs { mv(rhs) }, op { op }
		{}

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
	};

	struct IndexingExpr : Expr {
		Uptr<Expr> target;
		Vec<Uptr<Expr>> indices;

		IndexingExpr(Uptr<Expr> target, Vec<Uptr<Expr>> indices) :
			target { mv(target) }, indices { mv(indices) }
		{}

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
	};

	struct LengthGetter : Expr {
		Uptr<Expr> target;
		Opt<Uptr<Expr>> dimension;

		LengthGetter(Uptr<Expr> target, Opt<Uptr<Expr>> dimension) :
			target { mv(target) }, dimension { mv(dimension) }
		{}

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
	};

	struct FunctionCall : Expr {
		Uptr<Expr> callee;
		Vec<Uptr<Expr>> arguments;

		FunctionCall(Uptr<Expr> callee, Vec<Uptr<Expr>> arguments) :
			callee { mv(callee) }, arguments { mv(arguments) }
		{}

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
	};

	struct NewArray : Expr {
		Vec<Uptr<Expr>> dimension_lengths;

		NewArray(Vec<Uptr<Expr>> dimension_lengths) : dimension_lengths { mv(dimension_lengths) } {}

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
	};

	struct NewTuple : Expr {
		Uptr<Expr> length;

		NewTuple(Uptr<Expr> length) : length { mv(length) } {}

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
	};

	class StatementBlock;
	class StatementDeclaration;
	class StatementAssignment;
	class StatementLabel;
	class StatementReturn;
	class StatementContinue;
	class StatementBreak;
	class StatementGoto;
	class StatementIf;
	class StatementWhile;

	// interface
	class StatementVisitor {
		public:
		virtual void visit(StatementBlock &stmt) = 0;
		virtual void visit(StatementDeclaration &stmt) = 0;
		virtual void visit(StatementAssignment &stmt) = 0;
		virtual void visit(StatementLabel &stmt) = 0;
		virtual void visit(StatementReturn &stmt) = 0;
		virtual void visit(StatementContinue &stmt) = 0;
		virtual void visit(StatementBreak &stmt) = 0;
		virtual void visit(StatementGoto &stmt) = 0;
		virtual void visit(StatementIf &stmt) = 0;
		virtual void visit(StatementWhile &stmt) = 0;
	};

	// interface
	struct Statement {
		virtual void bind_to_scope(Scope<Nameable> &scope) = 0;
		virtual std::string to_string() const = 0;
		virtual void accept(StatementVisitor &v) = 0;
	};

	struct StatementBlock : Statement {
		Vec<Uptr<Statement>> statements;
		Vec<Uptr<Variable>> vars;
		Scope<Nameable> scope;

		explicit StatementBlock() {}

		void bind_to_scope(Scope<Nameable> &scope) override; // after binding, the block is frozen
		void add_next_statement(Uptr<Statement> stmt);
		std::string to_string() const override;
		void accept(StatementVisitor &v) override { v.visit(*this); }
	};

	struct StatementDeclaration : Statement {
		std::string type_name; // FUTURE we don't really care about representing the type
		Vec<Pair<std::string, Uptr<ItemRef<Nameable>>>> variables;

		StatementDeclaration(Vec<std::string> variable_names, std::string type_name) :
			variables {}, type_name { mv(type_name) }
		{
			for (const std::string &var_name : variable_names) {
				this->variables.push_back({ var_name, mkuptr<ItemRef<Nameable>>(var_name) });
			}
		}

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
		void accept(StatementVisitor &v) override { v.visit(*this); }
	};

	struct StatementAssignment : Statement {
		// the destination is optional only for the pure call instruction
		Opt<Uptr<IndexingExpr>> maybe_dest;
		Uptr<Expr> source;

		StatementAssignment(Uptr<Expr> source) : maybe_dest {}, source { mv(source) } {}
		StatementAssignment(Uptr<Expr> source, Uptr<IndexingExpr> dest) : maybe_dest { mv(dest) }, source { mv(source) } {}

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
		void accept(StatementVisitor &v) override { v.visit(*this); }
	};

	struct StatementLabel : Statement {
		std::string label_name;

		StatementLabel(std::string label_name) : label_name { mv(label_name) } {}

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
		void accept(StatementVisitor &v) override { v.visit(*this); }
	};

	struct StatementReturn : Statement {
		Opt<Uptr<Expr>> return_value;

		StatementReturn(Opt<Uptr<Expr>> return_value) : return_value { mv(return_value) } {}

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
		void accept(StatementVisitor &v) override { v.visit(*this); }
	};

	struct StatementContinue : Statement {
		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
		void accept(StatementVisitor &v) override { v.visit(*this); }
	};

	struct StatementBreak : Statement {
		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
		void accept(StatementVisitor &v) override { v.visit(*this); }
	};

	struct StatementGoto : Statement {
		std::string label_name; // TODO consider making it an ItemRef

		StatementGoto(std::string label_name) : label_name { mv(label_name) } {}

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
		void accept(StatementVisitor &v) override { v.visit(*this); }
	};

	struct StatementIf : Statement {
		Uptr<Expr> condition; // LB grammar forces this to be a BinaryOperation
		std::string then_label_name; // TODO consider making these into ItemRefs
		std::string else_label_name;

		StatementIf(Uptr<Expr> condition, std::string then_label_name, std::string else_label_name) :
			condition { mv(condition) }, then_label_name { mv(then_label_name) }, else_label_name { mv(else_label_name) }
		{}

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
		void accept(StatementVisitor &v) override { v.visit(*this); }
	};

	struct StatementWhile : Statement {
		Uptr<Expr> condition; // LB grammar forces this to be a BinaryOperation
		std::string body_label_name; // TODO consider making these into ItemRefs
		std::string end_label_name;

		StatementWhile(Uptr<Expr> condition, std::string body_label_name, std::string end_label_name) :
			condition { mv(condition) }, body_label_name { mv(body_label_name) }, end_label_name { mv(end_label_name) }
		{}

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
		void accept(StatementVisitor &v) override { v.visit(*this); }
	};

	struct LbFunction : Nameable {
		std::string name;
		std::string return_type_name;

		Uptr<StatementBlock> body;
		Vec<Uptr<Variable>> parameter_vars;
		Scope<Nameable> scope;

		explicit LbFunction(std::string name, std::string return_type_name, Uptr<StatementBlock> body);

		const std::string &get_name() const override { return this->name; }
		std::string to_string() const;
		void add_parameter_variable(std::string name, std::string type_name);
	};

	// just a wrapper so that it can inherit from Nameable
	struct ExternalFunction : Nameable {
		std::string name;

		ExternalFunction(std::string name) : name { mv(name) } {}

		const std::string &get_name() const override { return this->name; }
	};

	struct Program {
		Vec<Uptr<LbFunction>> lb_functions;
		Vec<Uptr<ExternalFunction>> external_functions;
		Scope<Nameable> scope;

		std::string to_string() const;
		void add_lb_function(Uptr<LbFunction> lb_function);
		void add_external_function(Uptr<ExternalFunction> external_function);
	};

	// adds the standard library functions to the program's scope
	void link_std(Program &program);
}
