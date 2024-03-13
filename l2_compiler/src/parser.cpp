#include "parser.h"
#include "program.h"
#include "utils.h"
#include <typeinfo>
#include <sched.h>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <set>
#include <iterator>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <stdint.h>
#include <assert.h>
#include <fstream>
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/analyze.hpp>
#include <tao/pegtl/contrib/raw_string.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>
#include <tao/pegtl/contrib/parse_tree_to_dot.hpp>

namespace pegtl = TAO_PEGTL_NAMESPACE;

namespace L2::parser {
	struct ParseNode;

	namespace rules {
		using namespace pegtl; // for convenience of reading the rules

		template<typename Result, typename Separator, typename...Rules>
		struct interleaved_impl;
		template<typename... Results, typename Separator, typename Rule0, typename... RulesRest>
		struct interleaved_impl<seq<Results...>, Separator, Rule0, RulesRest...> :
			interleaved_impl<seq<Results..., Rule0, Separator>, Separator, RulesRest...>
		{};
		template<typename... Results, typename Separator, typename Rule0>
		struct interleaved_impl<seq<Results...>, Separator, Rule0> {
			using type = seq<Results..., Rule0>;
		};
		template<typename Separator, typename... Rules>
		using interleaved = typename interleaved_impl<seq<>, Separator, Rules...>::type;

		struct str_return : TAO_PEGTL_STRING("return") {};
		struct str_arrow : TAO_PEGTL_STRING("\x3c-") {};
		struct str_rax : TAO_PEGTL_STRING("rax") {};
		struct str_rcx : TAO_PEGTL_STRING("rcx") {};
		struct str_rdx : TAO_PEGTL_STRING("rdx") {};
		struct str_rdi : TAO_PEGTL_STRING("rdi") {};
		struct str_rsi : TAO_PEGTL_STRING("rsi") {};
		struct str_r8 : TAO_PEGTL_STRING("r8") {};
		struct str_r9 : TAO_PEGTL_STRING("r9") {};
		struct str_rsp : TAO_PEGTL_STRING("rsp") {};
		struct str_plus : TAO_PEGTL_STRING("\x2b\x3d") {};
		struct str_minus : TAO_PEGTL_STRING("\x2d\x3d") {};
		struct str_times : TAO_PEGTL_STRING("\x2a\x3d") {};
		struct str_bitwise_and : TAO_PEGTL_STRING("\x26\x3d") {};
		struct str_lshift : TAO_PEGTL_STRING("\x3c\x3c=") {};
		struct str_rshift : TAO_PEGTL_STRING(">>=") {};
		struct str_lt : TAO_PEGTL_STRING("\x3c") {};
		struct str_le : TAO_PEGTL_STRING("\x3c=") {};
		struct str_eq : TAO_PEGTL_STRING("=") {};
		struct str_mem : TAO_PEGTL_STRING("mem") {};
		struct str_stack_arg : TAO_PEGTL_STRING("stack-arg") {};
		struct str_goto : TAO_PEGTL_STRING("goto") {};
		struct str_cjump : TAO_PEGTL_STRING("cjump") {};
		struct str_call : TAO_PEGTL_STRING("call") {};
		struct str_print : TAO_PEGTL_STRING("print") {};
		struct str_input : TAO_PEGTL_STRING("input") {};
		struct str_allocate : TAO_PEGTL_STRING("allocate") {};
		struct str_tuple_error : TAO_PEGTL_STRING("tuple-error") {};
		struct str_tensor_error : TAO_PEGTL_STRING("tensor-error") {};

		struct CommentRule :
			disable<
				TAO_PEGTL_STRING("//"),
				until<eolf>
			>
		{};

		struct SpacesRule :
			star<sor<one<' '>, one<'\t'>>>
		{};

		struct LineSeparatorsRule :
			star<seq<SpacesRule, eol>>
		{};

		struct LineSeparatorsWithCommentsRule :
			star<
				seq<
					SpacesRule,
					sor<
						eol,
						CommentRule
					>
				>
			>
		{};

		struct NumberRule :
			sor<
				seq<
					opt<sor<one<'-'>, one<'+'>>>,
					range<'1', '9'>,
					star<digit>
				>,
				one<'0'>
			>
		{};

		struct ArgumentNumberRule :
			seq<NumberRule>
		{};

		struct LeaFactorRule :
			rematch<
				NumberRule,
				sor<one<'1'>, one<'2'>, one<'4'>, one<'8'>>
			>
		{};

		struct NameRule :
			ascii::identifier // the rules for L2 rules are the same as for C identifiers
		{};

		struct LabelRule :
			seq<one<':'>, NameRule>
		{};

		struct FunctionNameRule :
			seq<one<'@'>, NameRule>
		{};

		struct VariableRule :
			seq<one<'%'>, NameRule>
		{};

		struct RegisterRule :
			sor<
				str_rax,
				str_rcx,
				str_rdx,
				str_rdi,
				str_rsi,
				str_r8,
				str_r9,
				str_rsp
			>
		{};

		struct InexplicableSxRule :
			sor<
				rematch<
					RegisterRule,
					str_rcx
				>,
				VariableRule
			>
		{};

		struct InexplicableARule :
			sor<
				InexplicableSxRule,
				rematch<
					RegisterRule,
					sor<
						str_rdi,
						str_rsi,
						str_rdx,
						str_r8,
						str_r9
					>
				>
			>
		{};

		struct InexplicableWRule :
			sor<
				InexplicableARule,
				rematch<
					RegisterRule,
					str_rax
				>
			>
		{};

		struct InexplicableXRule :
			sor<
				InexplicableWRule,
				rematch<
					RegisterRule,
					str_rsp
				>
			>
		{};

		struct InexplicableTRule :
			sor<
				InexplicableXRule,
				NumberRule
			>
		{};

		struct InexplicableSRule :
			sor<
				InexplicableTRule,
				LabelRule,
				FunctionNameRule
			>
		{};

		struct InexplicableURule :
			sor<
				InexplicableWRule,
				FunctionNameRule
			>
		{};

		struct StackArgRule :
			interleaved<
				SpacesRule,
				str_stack_arg,
				NumberRule
			>
		{};

		struct MemoryLocationRule :
			interleaved<
				SpacesRule,
				str_mem,
				InexplicableXRule,
				NumberRule
			>
		{};

		struct ArithmeticOperatorRule :
			sor<
				str_plus,
				str_minus,
				str_times,
				str_bitwise_and
			>
		{};

		struct ShiftOperatorRule :
			sor<
				str_lshift,
				str_rshift
			>
		{};

		struct ComparisonOperatorRule :
			sor<
				str_le,
				str_lt,
				str_eq
			>
		{};

		struct InstructionAssignmentRule :
			seq<
				InexplicableWRule,
				SpacesRule,
				str_arrow,
				SpacesRule,
				InexplicableSRule
			>
		{};

		struct InstructionReturnRule :
			seq<
				str_return
			>
		{};

		struct InstructionMemoryReadRule :
			interleaved<
				SpacesRule,
				InexplicableWRule,
				str_arrow,
				MemoryLocationRule
			>
		{};

		struct InstructionMemoryWriteRule :
			interleaved<
				SpacesRule,
				MemoryLocationRule,
				str_arrow,
				InexplicableSRule
			>
		{};

		struct InstructionArithmeticOperationRule :
			interleaved<
				SpacesRule,
				InexplicableWRule,
				ArithmeticOperatorRule,
				InexplicableTRule
			>
		{};

		struct InstructionStackArgRule :
			interleaved<
				SpacesRule,
				InexplicableWRule,
				str_arrow,
				StackArgRule
			>
		{};

		struct InstructionShiftOperationRule :
			interleaved<
				SpacesRule,
				InexplicableWRule,
				ShiftOperatorRule,
				sor<
					NumberRule,
					InexplicableSxRule
				>
			>
		{};

		struct InstructionPlusWriteMemoryRule :
			interleaved<
				SpacesRule,
				MemoryLocationRule,
				str_plus,
				InexplicableTRule
			>
		{};

		struct InstructionMinusWriteMemoryRule :
			interleaved<
				SpacesRule,
				MemoryLocationRule,
				str_minus,
				InexplicableTRule
			>
		{};

		struct InstructionPlusReadMemoryRule :
			interleaved<
				SpacesRule,
				InexplicableWRule,
				str_plus,
				MemoryLocationRule
			>
		{};

		struct InstructionMinusReadMemoryRule :
			interleaved<
				SpacesRule,
				InexplicableWRule,
				str_minus,
				MemoryLocationRule
			>
		{};

		struct InstructionAssignmentCompareRule :
			interleaved<
				SpacesRule,
				InexplicableWRule,
				str_arrow,
				InexplicableTRule,
				ComparisonOperatorRule,
				InexplicableTRule

			>
		{};

		struct InstructionCJumpRule :
			interleaved<
				SpacesRule,
				str_cjump,
				InexplicableTRule,
				ComparisonOperatorRule,
				InexplicableTRule,
				LabelRule
			>
		{};

		struct InstructionLabelRule :
			interleaved<
				SpacesRule,
				LabelRule
			>
		{};

		struct InstructionGotoLabelRule :
			interleaved<
				SpacesRule,
				str_goto,
				LabelRule
			>
		{};

		struct InstructionFunctionCallRule :
			interleaved<
				SpacesRule,
				str_call,
				InexplicableURule,
				NumberRule
			>
		{};

		struct StdFunctionNameRule :
			sor<
				str_print,
				str_input,
				str_allocate,
				str_tuple_error,
				str_tensor_error
			>
		{};

		struct InstructionStdCallRule :
			interleaved<
				SpacesRule,
				str_call,
				StdFunctionNameRule,
				NumberRule
			>
		{};

		struct InstructionIncrementRule :
			interleaved<
				SpacesRule,
				InexplicableWRule,
				seq<one<'+'>, one<'+'>>
			>
		{};

		struct InstructionDecrementRule :
			interleaved<
				SpacesRule,
				InexplicableWRule,
				seq<one<'-'>, one<'-'>>
			>
		{};

		struct InstructionLeaRule :
			interleaved<
				SpacesRule,
				InexplicableWRule,
				one<'@'>,
				InexplicableWRule,
				InexplicableWRule,
				LeaFactorRule
			>
		{};

		struct InstructionRule :
			sor<
				InstructionLeaRule,
				InstructionAssignmentCompareRule,
				InstructionAssignmentRule,
				InstructionReturnRule,
				InstructionMemoryReadRule,
				InstructionMemoryWriteRule,
				InstructionArithmeticOperationRule,
				InstructionStackArgRule,
				InstructionShiftOperationRule,
				InstructionPlusWriteMemoryRule,
				InstructionMinusWriteMemoryRule,
				InstructionPlusReadMemoryRule,
				InstructionMinusReadMemoryRule,
				InstructionCJumpRule,
				InstructionLabelRule,
				InstructionGotoLabelRule,
				InstructionFunctionCallRule,
				InstructionStdCallRule,
				InstructionIncrementRule,
				InstructionDecrementRule
			>
		{};

		struct InstructionsRule :
			plus<
				seq<
					LineSeparatorsWithCommentsRule,
					bol,
					SpacesRule,
					InstructionRule,
					LineSeparatorsWithCommentsRule
				>
			>
		{};

		struct FunctionRule :
			interleaved<
				LineSeparatorsWithCommentsRule,
				seq<SpacesRule, one<'('>>,
				seq<SpacesRule, FunctionNameRule>,
				seq<SpacesRule, ArgumentNumberRule>,
				InstructionsRule,
				seq<SpacesRule, one<')'>>
			>
		{};

		struct SpillFunctionRule :
			interleaved<
				LineSeparatorsWithCommentsRule,
				FunctionRule,
				VariableRule,
				VariableRule
			>
		{};

		struct FunctionsRule :
			list<
				FunctionRule,
				LineSeparatorsWithCommentsRule
			>
		{};


		struct ProgramRule :
			seq<
				LineSeparatorsWithCommentsRule,
				interleaved<
					LineSeparatorsWithCommentsRule,
					seq<SpacesRule, one<'('>>,
					FunctionNameRule,
					FunctionsRule,
					seq<SpacesRule, one<')'>>
				>,
				LineSeparatorsWithCommentsRule
			>
		{};

		struct EntryPointRule :	must<ProgramRule> {};

		template<typename Rule>
		struct Selector : pegtl::parse_tree::selector<
			Rule,
			pegtl::parse_tree::store_content::on<
				NumberRule,
				NameRule,
				LabelRule,
				FunctionNameRule,
				VariableRule,
				RegisterRule,
				StackArgRule,
				MemoryLocationRule,
				StdFunctionNameRule,
				ArithmeticOperatorRule,
				ShiftOperatorRule,
				ComparisonOperatorRule,
				InstructionAssignmentCompareRule,
				InstructionAssignmentRule,
				InstructionReturnRule,
				InstructionMemoryReadRule,
				InstructionMemoryWriteRule,
				InstructionArithmeticOperationRule,
				InstructionStackArgRule,
				InstructionShiftOperationRule,
				InstructionPlusWriteMemoryRule,
				InstructionMinusWriteMemoryRule,
				InstructionPlusReadMemoryRule,
				InstructionMinusReadMemoryRule,
				InstructionCJumpRule,
				InstructionLabelRule,
				InstructionGotoLabelRule,
				InstructionFunctionCallRule,
				InstructionStdCallRule,
				InstructionIncrementRule,
				InstructionDecrementRule,
				InstructionLeaRule,
				InstructionsRule,
				FunctionRule,
				SpillFunctionRule,
				FunctionsRule,
				ProgramRule
			>
		> {};
	}

	struct ParseNode {
		// members
		std::vector<std::unique_ptr<ParseNode>> children;
		pegtl::internal::inputerator begin;
		pegtl::internal::inputerator end;
		const std::type_info *rule; // which rule this node matched on
		std::string_view type;// only used for displaying parse tree

		// special methods
		ParseNode() = default;
		ParseNode(const ParseNode &) = delete;
		ParseNode(ParseNode &&) = delete;
		ParseNode &operator=(const ParseNode &) = delete;
		ParseNode &operator=(ParseNode &&) = delete;
		~ParseNode() = default;

		// methods used for parsing

		template<typename Rule, typename ParseInput, typename... States>
		void start(const ParseInput &in, States &&...) {
			this->begin = in.inputerator();
		}

		template<typename Rule, typename ParseInput, typename... States>
		void success(const ParseInput &in, States &&...) {
			this->end = in.inputerator();
			this->rule = &typeid(Rule);
			this->type = pegtl::demangle<Rule>();
			this->type.remove_prefix(this->type.find_last_of(':') + 1);
		}

		template<typename Rule, typename ParseInput, typename... States>
		void failure(const ParseInput &in, States &&...) {}

		template<typename... States>
		void emplace_back(std::unique_ptr<ParseNode> &&child, States &&...) {
			children.emplace_back(std::move(child));
		}

		std::string_view string_view() const {
			return {
				this->begin.data,
				static_cast<std::size_t>(this->end.data - this->begin.data)
			};
		}

		const ParseNode &operator[](int index) const {
			return *this->children.at(index);
		}

		// methods used to display the parse tree

		bool has_content() const noexcept {
			return this->end.data != nullptr;
		}

		bool is_root() const noexcept {
			return static_cast<bool>(this->rule);
		}
	};

	using namespace L2::program;

	namespace node_processor {
		template<typename T>
		using ptr = std::unique_ptr<T>;

		ptr<NumberLiteral> convert_number_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::NumberRule));
			return std::make_unique<NumberLiteral>(
				utils::string_view_to_int<int64_t>(n.string_view())
			);
		}

		std::string_view convert_name_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::NameRule));
			return n.string_view();
		}

		ptr<LabelRef> make_label_ref(const ParseNode &n) {
			assert(*n.rule == typeid(rules::LabelRule));
			return std::make_unique<LabelRef>(convert_name_rule(n[0]));
		}

		ptr<L2FunctionRef> make_l2_function_ref(const ParseNode &n) {
			assert(*n.rule == typeid(rules::FunctionNameRule));
			return std::make_unique<L2FunctionRef>(convert_name_rule(n[0]));
		}

		ptr<VariableRef> convert_variable_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::VariableRule));
			return std::make_unique<VariableRef>(convert_name_rule(n[0]));
		}

		ptr<RegisterRef> convert_register_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::RegisterRule));
			return std::make_unique<RegisterRef>(n.string_view());
		}

		ptr<Expr> make_expr(const ParseNode &n);

		ptr<StackArg> convert_stack_arg_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::StackArgRule));
			return std::make_unique<StackArg>(convert_number_rule(n[0]));
		}

		ptr<MemoryLocation> convert_memory_location_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::MemoryLocationRule));
			return std::make_unique<MemoryLocation>(
				make_expr(n[0]),
				convert_number_rule(n[1])
			);
		}

		ptr<ExternalFunctionRef> convert_std_function_name_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::StdFunctionNameRule));
			return std::make_unique<ExternalFunctionRef>(n.string_view());
		}

		ptr<Expr> make_expr(const ParseNode &n) {
			const std::type_info &rule = *n.rule;
			if (rule == typeid(rules::RegisterRule)) {
				return convert_register_rule(n);
			} else if (rule == typeid(rules::NumberRule)) {
				return convert_number_rule(n);
			} else if (rule == typeid(rules::LabelRule)) {
				return make_label_ref(n);
			} else if (rule == typeid(rules::VariableRule)) {
				return convert_variable_rule(n);
			} else if (rule == typeid(rules::FunctionNameRule)) {
				return make_l2_function_ref(n);
			} else if (rule == typeid(rules::StdFunctionNameRule)) {
				return convert_std_function_name_rule(n);
			} else if (rule == typeid(rules::MemoryLocationRule)) {
				return convert_memory_location_rule(n);
			} else if (rule == typeid(rules::StackArgRule)) {
				return convert_stack_arg_rule(n);
			} else {
				std::cerr << "Cannot make Expr from this parse node of type " << n.type << "\n";
				exit(1);
			}
		}

		AssignOperator make_assign_operator(const ParseNode &n) {
			assert(*n.rule == typeid(rules::ArithmeticOperatorRule)
				|| *n.rule == typeid(rules::ShiftOperatorRule)
				|| *n.rule == typeid(rules::ComparisonOperatorRule));
			return str_to_ass_op(n.string_view());
		}

		ComparisonOperator convert_comparison_operator_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::ComparisonOperatorRule));
			return str_to_cmp_op(n.string_view());
		}

		ptr<InstructionCompareAssignment> convert_instruction_assignment_compare(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionAssignmentCompareRule));
			return std::make_unique<InstructionCompareAssignment>(
				make_expr(n[0]),
				convert_comparison_operator_rule(n[2]),
				make_expr(n[1]),
				make_expr(n[3])
			);
		}

		std::unique_ptr<InstructionAssignment> make_pure_instruction_assignment(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionAssignmentRule)
				|| *n.rule == typeid(rules::InstructionMemoryReadRule)
				|| *n.rule == typeid(rules::InstructionMemoryWriteRule)
				|| *n.rule == typeid(rules::InstructionStackArgRule));
			return std::make_unique<InstructionAssignment>(
				AssignOperator::pure,
				make_expr(n[1]),
				make_expr(n[0])
			);
		}

		std::unique_ptr<InstructionAssignment> make_plus_instruction_assignment(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionPlusReadMemoryRule)
				|| *n.rule == typeid(rules::InstructionPlusWriteMemoryRule));
			return std::make_unique<InstructionAssignment>(
				AssignOperator::add,
				make_expr(n[1]),
				make_expr(n[0])
			);
		}

		std::unique_ptr<InstructionAssignment> make_minus_instruction_assignment(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionMinusReadMemoryRule)
				|| *n.rule == typeid(rules::InstructionMinusWriteMemoryRule));
			return std::make_unique<InstructionAssignment>(
				AssignOperator::subtract,
				make_expr(n[1]),
				make_expr(n[0])
			);
		}

		ptr<InstructionReturn> convert_instruction_return_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionReturnRule));
			return std::make_unique<InstructionReturn>();
		}

		std::unique_ptr<InstructionAssignment> make_custom_op_instruction_assignment(const ParseNode &n){
			assert(*n.rule == typeid(rules::InstructionArithmeticOperationRule)
				|| *n.rule == typeid(rules::InstructionShiftOperationRule));
			return std::make_unique<InstructionAssignment>(
				make_assign_operator(n[1]),
				make_expr(n[2]),
				make_expr(n[0])
			);
		}

		ptr<InstructionCompareJump> convert_instruction_cjump_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionCJumpRule));
			return std::make_unique<InstructionCompareJump>(
				convert_comparison_operator_rule(n[1]),
				make_expr(n[0]),
				make_expr(n[2]),
				make_label_ref(n[3])
			);
		}

		ptr<InstructionLabel> convert_instruction_label_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionLabelRule));
			return std::make_unique<InstructionLabel>(convert_name_rule(n[0][0]));
		}

		ptr<InstructionGoto> convert_instruction_goto_label_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionGotoLabelRule));
			return std::make_unique<InstructionGoto>(make_label_ref(n[0]));
		}

		std::unique_ptr<InstructionCall> make_instruction_call(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionFunctionCallRule)
				|| *n.rule == typeid(rules::InstructionStdCallRule));
			return std::make_unique<InstructionCall>(
				make_expr(n[0]),
				convert_number_rule(n[1])->value
			);
		}

		std::unique_ptr<InstructionAssignment> convert_instruction_increment_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionIncrementRule));
			return std::make_unique<InstructionAssignment>(
				AssignOperator::add,
				std::make_unique<NumberLiteral>(1),
				make_expr(n[0])
			);
		}

		std::unique_ptr<InstructionAssignment> convert_instruction_decrement_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionDecrementRule));
			return std::make_unique<InstructionAssignment>(
				AssignOperator::subtract,
				std::make_unique<NumberLiteral>(1),
				make_expr(n[0])
			);
		}

		std::unique_ptr<InstructionLeaq> convert_instruction_lea_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionLeaRule));
			return std::make_unique<InstructionLeaq>(
				make_expr(n[0]),
				make_expr(n[1]),
				make_expr(n[2]),
				convert_number_rule(n[3])->value
			);
		}

		std::unique_ptr<Instruction> make_instruction(const ParseNode &n) {
			const std::type_info &rule = *n.rule;
			if (rule == typeid(rules::InstructionAssignmentCompareRule)) {
				return convert_instruction_assignment_compare(n);
			} else if (
				rule == typeid(rules::InstructionAssignmentRule)
				|| rule == typeid(rules::InstructionMemoryReadRule)
				|| rule == typeid(rules::InstructionMemoryWriteRule)
				|| rule == typeid(rules::InstructionStackArgRule)
			) {
				return make_pure_instruction_assignment(n);
			} else if (
				rule == typeid(rules::InstructionPlusReadMemoryRule)
				|| rule == typeid(rules::InstructionPlusWriteMemoryRule)
			) {
				return make_plus_instruction_assignment(n);
			} else if (
				rule == typeid(rules::InstructionMinusReadMemoryRule)
				|| rule == typeid(rules::InstructionMinusWriteMemoryRule)
			) {
				return make_minus_instruction_assignment(n);
			} else if (rule == typeid(rules::InstructionReturnRule)) {
				return convert_instruction_return_rule(n);
			} else if (
				rule == typeid(rules::InstructionArithmeticOperationRule)
				|| rule == typeid(rules::InstructionShiftOperationRule)
			) {
				return make_custom_op_instruction_assignment(n);
			} else if (rule == typeid(rules::InstructionCJumpRule)) {
				return convert_instruction_cjump_rule(n);
			} else if (rule == typeid(rules::InstructionLabelRule)) {
				return convert_instruction_label_rule(n);
			} else if (rule == typeid(rules::InstructionGotoLabelRule)) {
				return convert_instruction_goto_label_rule(n);
			} else if (
				rule == typeid(rules::InstructionFunctionCallRule)
				|| rule == typeid(rules::InstructionStdCallRule)
			) {
				return make_instruction_call(n);
			} else if (rule == typeid(rules::InstructionIncrementRule)) {
				return convert_instruction_increment_rule(n);
			} else if (rule == typeid(rules::InstructionDecrementRule)) {
				return convert_instruction_decrement_rule(n);
			} else if (rule == typeid(rules::InstructionLeaRule)) {
				auto x = convert_instruction_lea_rule(n);
				return x;
			} else {
				std::cerr << "Cannot make Instruction from this parse node.";
				exit(1);
			}
		}

		ptr<L2Function> make_l2_function(const ParseNode &n) {
			assert(*n.rule == typeid(rules::FunctionRule));
			std::unique_ptr<L2Function> function = std::make_unique<L2Function>(
				convert_name_rule(n[0][0]),
				convert_number_rule(n[1])->value
			);

			const ParseNode &instructions_rule = n[2];
			assert(*instructions_rule.rule == typeid(rules::InstructionsRule));
			for (const auto &child : instructions_rule.children) {
				auto inst_thing = make_instruction(*child);
				function->add_instruction(std::move(inst_thing));
			}
			return function;
		}

		std::unique_ptr<Program> convert_program_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::ProgramRule));
			std::unique_ptr<Program> program = std::make_unique<Program>(
				make_l2_function_ref(n[0])
			);
			add_predefined_registers_and_std(*program);

			const ParseNode &functions_rule = n[1];
			assert(*functions_rule.rule == typeid(rules::FunctionsRule));
			for (const auto &function : functions_rule.children) {
				program->add_l2_function(make_l2_function(*function));
			}
			return program;
		}
	}

	std::unique_ptr<Program> parse_file(char *fileName, std::optional<std::string> parse_tree_output) {
		// Check the grammar for some possible issues.
		// TODO move this to a separate file bc it's performance-intensive
		if (pegtl::analyze<rules::EntryPointRule>() != 0) {
			std::cerr << "There are problems with the grammar" << std::endl;
			exit(1);
		}

		// Parse
		pegtl::file_input<> fileInput(fileName);
		auto root = pegtl::parse_tree::parse<rules::EntryPointRule, ParseNode, rules::Selector>(fileInput);
		if (root) {
			if (parse_tree_output.has_value()) {
				std::ofstream output_fstream(*parse_tree_output);
				if (output_fstream.is_open()) {
					pegtl::parse_tree::print_dot(output_fstream, *root);
					output_fstream.close();
				}
			}

			auto p = node_processor::convert_program_rule((*root)[0]);
			p->get_scope().fake_bind_frees(); // If you want to allow unbound name
			// p->get_scope().ensure_no_frees(); // If you want to error on unbound name
			return p;
		}
		exit(1);
	}
	std::unique_ptr<Program> parse_function_file(char *fileName) {
		pegtl::file_input<> fileInput(fileName);
		auto root = pegtl::parse_tree::parse<pegtl::must<rules::FunctionRule>, ParseNode, rules::Selector>(fileInput);
		if (root) {
			auto function = node_processor::make_l2_function((*root)[0]);
			auto program = std::make_unique<Program>(
				std::make_unique<L2FunctionRef>(function->get_name())
			);
			add_predefined_registers_and_std(*program);
			program->add_l2_function(std::move(function));
			program->get_scope().fake_bind_frees();
			return program;
		}
		return {};
	}

	std::unique_ptr<SpillProgram> parse_spill_file(char *fileName) {
		pegtl::file_input<> fileInput(fileName);
		auto root = pegtl::parse_tree::parse<pegtl::must<rules::SpillFunctionRule>, ParseNode, rules::Selector>(fileInput);
		if (root) {
			auto function = node_processor::make_l2_function((*root)[0][0]);
			auto program = std::make_unique<Program>(
				std::make_unique<L2FunctionRef>(function->get_name())
			);
			add_predefined_registers_and_std(*program);
			program->add_l2_function(std::move(function));
			program->get_scope().fake_bind_frees();
			std::string_view var_string_view = (*root)[0][1].string_view();
			std::string_view prefix_view = (*root)[0][2].string_view();
			std::string var_string = std::string(var_string_view);
			std::string prefix_string = std::string(prefix_view);
			var_string = var_string.substr(1);
			std::string prefix = prefix_string.substr(1);
			Variable *var = program->get_l2_function(0)->agg_scope.variable_scope.get_item_or_create(var_string);
			std::unique_ptr<SpillProgram> spillProgram = std::make_unique<SpillProgram>(
				std::move(program),
				var,
				prefix
			);
			return spillProgram;
		}
		return {};
	}
}

