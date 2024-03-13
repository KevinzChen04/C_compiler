#pragma once

#include "std_alias.h"
#include <variant>
#include <string>

// The MIR, or "mid-level intermediate representation", describes the imperative
// instructions of the LA program at a type-aware level. Each function is
// a control flow graph of BasicBlocks which contain lists of elementary
// type-aware instructions as well as transitions to other BasicBlocks. It is
// also meant to closely reflect CS 322's IR language.
namespace mir {
	using namespace std_alias;

	struct Operand;

	struct Type {
		struct VoidType {};
		struct ArrayType { int num_dimensions; };
		struct TupleType {};
		struct CodeType {};
		using Variant = std::variant<VoidType, ArrayType, TupleType, CodeType>;
		Variant type;

		std::string to_ir_syntax() const;
		Uptr<Operand> get_default_value() const; // the value to initialize the variable to
	};

	// any function-local location in memory, including user-defined local
	// variables as well as compiler-defined temporaries
	struct LocalVar {
		bool is_user_declared;
		std::string name; // empty means anonymous
		Type type;

		LocalVar(bool is_user_declared, std::string name, Type type) :
			is_user_declared { is_user_declared }, name { mv(name) }, type { type }
		{}

		std::string to_ir_syntax() const;
		std::string get_unambiguous_name() const;
		std::string get_declaration() const;
	};

	// a value that can be used as the right-hand side of an
	// InstructionAssignment
	// closely resembles hir::Expr
	struct Rvalue {
		virtual std::string to_ir_syntax() const = 0;
	};

	struct Operand : Rvalue {};

	// a "place" in memory, which can be assigned to as the left-hand side of
	// an InstructionAssignment.
	// closely resembles hir::IndexingExpr but is more limited in the allowable'
	// target expressions
	struct Place : Operand {
		LocalVar *target;
		Vec<Uptr<Operand>> indices;

		Place(LocalVar *target) : target { target }, indices {} {}
		Place(LocalVar *target, Vec<Uptr<Operand>> indices) :
			target { target }, indices { mv(indices) }
		{}

		std::string to_ir_syntax() const override;
	};

	struct Int64Constant : Operand {
		int64_t value;

		Int64Constant(int64_t value) : value { value } {}

		std::string to_ir_syntax() const override;
	};

	struct FunctionDef;

	struct CodeConstant : Operand {
		FunctionDef *value;

		CodeConstant(FunctionDef *value) : value { value } {}

		std::string to_ir_syntax() const override;
	};

	struct ExternalFunction;

	struct ExtCodeConstant : Operand {
		ExternalFunction *value;

		ExtCodeConstant(ExternalFunction *value) : value { value } {}

		std::string to_ir_syntax() const override;
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

	struct BinaryOperation : Rvalue {
		Uptr<Operand> lhs;
		Uptr<Operand> rhs;
		Operator op;

		BinaryOperation(Uptr<Operand> lhs, Uptr<Operand> rhs, Operator op) :
			lhs { mv(lhs) }, rhs { mv(rhs) }, op { op }
		{}

		std::string to_ir_syntax() const override;
	};

	struct LengthGetter : Rvalue {
		Uptr<Operand> target;
		Opt<Uptr<Operand>> dimension;

		LengthGetter(Uptr<Operand> target, Opt<Uptr<Operand>> dimension) :
			target { mv(target) }, dimension { mv(dimension) }
		{}

		std::string to_ir_syntax() const override;
	};

	struct FunctionCall : Rvalue {
		Uptr<Operand> callee;
		Vec<Uptr<Operand>> arguments;

		FunctionCall(Uptr<Operand> callee, Vec<Uptr<Operand>> arguments) :
			callee { mv(callee) }, arguments { mv(arguments) }
		{}

		std::string to_ir_syntax() const override;
	};

	struct NewArray : Rvalue {
		Vec<Uptr<Operand>> dimension_lengths;

		NewArray(Vec<Uptr<Operand>> dimension_lengths) : dimension_lengths { mv(dimension_lengths) } {}

		std::string to_ir_syntax() const override;
	};

	struct NewTuple : Rvalue {
		Uptr<Operand> length;

		NewTuple(Uptr<Operand> length) : length { mv(length) } {}

		std::string to_ir_syntax() const override;
	};

	// mir::Instruction represents an elementary type-aware option, unlike
	// hir::Instruction which more closely resembles the syntactic construct of
	// an LA instruction
	// interface
	struct Instruction {
		Opt<Uptr<Place>> destination;
		Uptr<Rvalue> rvalue;

		Instruction(Opt<Uptr<Place>> destination, Uptr<Rvalue> rvalue) :
			destination { mv(destination) }, rvalue { mv(rvalue) }
		{}

		std::string to_ir_syntax() const;
	};

	struct BasicBlock {
		struct ReturnVoid {};
		struct ReturnVal { Uptr<Operand> return_value; };
		struct Goto { BasicBlock* successor; };
		struct Branch {
			Uptr<Operand> condition;
			BasicBlock *then_block;
			BasicBlock *else_block;
		};
		using Terminator = std::variant<ReturnVoid, ReturnVal, Goto, Branch>;

		// data fields start here
		bool user_labeled; // whether the block was given a label by the user
		std::string label_name;
		Vec<Uptr<Instruction>> instructions;
		Terminator terminator;

		BasicBlock(bool user_labeled, std::string label_name) :
			user_labeled { user_labeled },
			label_name { mv(label_name) },
			instructions {},
			terminator { ReturnVoid {} }
		{}

		std::string to_ir_syntax(Opt<Vec<LocalVar *>> vars_to_declare) const;
		std::string get_unambiguous_name() const;
	};

	struct FunctionDef {
		std::string user_given_name; // empty means anonymous
		mir::Type return_type;
		Vec<Uptr<LocalVar>> local_vars;
		Vec<LocalVar *> parameter_vars;
		Vec<Uptr<BasicBlock>> basic_blocks; // the first block is always the entry block

		explicit FunctionDef(std::string user_given_name, mir::Type return_type) :
			user_given_name { mv(user_given_name) }, return_type { return_type }
		{}

		std::string to_ir_syntax() const;
		std::string get_unambiguous_name() const;
	};

	struct ExternalFunction {
		std::string name;
		int num_parameters;
		bool returns_val;
		// TODO consider adding richer information about the function's signature

		ExternalFunction(std::string name, int num_parameters, bool returns_val) :
			name { mv(name) }, num_parameters { num_parameters }, returns_val { returns_val }
		{}
	};

	extern ExternalFunction tensor_error; // FUTURE handle overloads
	extern ExternalFunction tuple_error;

	struct Program {
		Vec<Uptr<FunctionDef>> function_defs;
		Vec<Uptr<ExternalFunction>> external_functions;

		std::string to_ir_syntax() const;
	};
}