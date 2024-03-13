#include "L1.h"
#include <map>

namespace L1 {
	// Item methods

	std::string Item::toString() const {
		return "(unimplemented)";
	}

	// Register methods

	std::map<std::string, RegisterID> strToRegId {
		{ "rax", RegisterID::rax },
		{ "rbx", RegisterID::rbx },
		{ "rcx", RegisterID::rcx },
		{ "rdx", RegisterID::rdx },
		{ "rdi", RegisterID::rdi },
		{ "rsi", RegisterID::rsi },
		{ "r8", RegisterID::r8 },
		{ "r9", RegisterID::r9 },
		{ "r10", RegisterID::r10 },
		{ "r11", RegisterID::r11 },
		{ "r12", RegisterID::r12 },
		{ "r13", RegisterID::r13 },
		{ "r14", RegisterID::r14 },
		{ "r15", RegisterID::r15 },
		{ "rbp", RegisterID::rbp },
		{ "rsp", RegisterID::rsp }
	};

	Register::Register(const std::string &id) : id {strToRegId[id]}, str {id} {}

	std::string Register::toString() const {
		return std::string("Register ") + this->str;
	}

	// Number methods

	Number::Number(int64_t value) : value {value} {}

	std::string Number::toString() const {
		return std::string("Number ") + std::to_string(this->value);
	}

	// Label methods

	Label::Label(const std::string &name) : name {name} {}

	std::string Label::toString() const {
		return std::string("Label :") + this->name;
	}

	// FunctionName methods

	FunctionName::FunctionName(const std::string &name) : name {name} {}

	std::string FunctionName::toString() const {
		return std::string("FunctionName @") + this->name;
	}
}
