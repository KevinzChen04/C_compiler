#pragma once

#include <vector>
#include <string>

namespace L1 {

	enum struct RegisterID {
		rax,
		rbx,
		rcx,
		rdx,
		rdi,
		rsi,
		r8,
		r9,
		r10,
		r11,
		r12,
		r13,
		r14,
		r15,
		rbp,
		rsp
	};

	// Every component of the AST is-a Item
	struct Item {
		virtual std::string toString() const;
	};

	struct Register : Item {
		RegisterID id;
		std::string str; // TODO remove

		Register(const std::string &id);

		virtual std::string toString() const override;
	};

	struct Number : Item {
		int64_t value;

		Number(int64_t value);

		virtual std::string toString() const override;
	};

	struct Label : Item {
		std::string name;

		Label(const std::string &name);

		virtual std::string toString() const override;
	};

	struct FunctionName : Item {
		std::string name;

		FunctionName(const std::string &name);

		virtual std::string toString() const override;
	};

	/*
	 * Instruction interface.
	 */
	struct Instruction : Item {};

	/*
	 * Instructions.
	 */
	struct Instruction_ret : Instruction {};

	struct Instruction_assignment : Instruction {
		Item *source;
		Item *destination;
	};

	/*
	 * Function.
	 */
	struct Function : Item {
		std::string name;
		int64_t num_arguments;
		int64_t num_locals;
		// TODO consider changing to value type instead of ptr type
		std::vector<Instruction *> instructions;
	};

	struct Program {
		std::string entryPointLabel;
		std::vector<Function *> functions;
	};
}
