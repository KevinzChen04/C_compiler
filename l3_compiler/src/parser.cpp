#include "std_alias.h"
#include "parser.h"
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

namespace L3::parser {
	using namespace std_alias;

	namespace rules {
		// for convenience of reading the rules
		using namespace pegtl;

		// for convenience of adding whitespace
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

		struct CommentRule :
			disable<
				TAO_PEGTL_STRING("//"),
				until<eolf>
			>
		{};

		struct SpaceRule :
			sor<one<' '>, one<'\t'>>
		{};

		struct SpacesRule :
			star<SpaceRule>
		{};

		struct LineSeparatorsRule :
			star<seq<SpacesRule, eol>>
		{};

		struct LineSeparatorsWithCommentsRule :
			star<
				seq<
					SpacesRule,
					sor<eol, CommentRule>
				>
			>
		{};

		struct SpacesOrNewLines :
			star<sor<SpaceRule, eol>>
		{};

		struct NameRule :
			ascii::identifier // the rules for L3 names are the same as for C identifiers
		{};

		struct VariableRule :
			seq<one<'%'>, NameRule>
		{};

		struct LabelRule :
			seq<one<':'>, NameRule>
		{};

		// aka. "l" in the grammar
		struct L3FunctionNameRule :
			seq<one<'@'>, NameRule>
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

		struct ComparisonOperatorRule :
			sor<
				TAO_PEGTL_STRING("<="),
				TAO_PEGTL_STRING("<"),
				TAO_PEGTL_STRING("="),
				TAO_PEGTL_STRING(">="),
				TAO_PEGTL_STRING(">")
			>
		{};

		struct ArithmeticOperatorRule :
			sor<
				TAO_PEGTL_STRING("+"),
				TAO_PEGTL_STRING("-"),
				TAO_PEGTL_STRING("*"),
				TAO_PEGTL_STRING("&"),
				TAO_PEGTL_STRING("<<"),
				TAO_PEGTL_STRING(">>")
			>
		{};

		struct InexplicableURule :
			sor<
				VariableRule,
				L3FunctionNameRule
			>
		{};

		struct InexplicableTRule :
			sor<
				VariableRule,
				NumberRule
			>
		{};

		struct InexplicableSRule :
			sor<
				InexplicableTRule,
				LabelRule,
				L3FunctionNameRule
			>
		{};

		struct CallArgsRule :
			opt<list<
				InexplicableTRule,
				one<','>,
				SpaceRule
			>>
		{};

		struct DefArgsRule :
			opt<list<
				VariableRule,
				one<','>,
				SpaceRule
			>>
		{};

		struct StdFunctionNameRule :
			sor<
				TAO_PEGTL_STRING("print"),
				TAO_PEGTL_STRING("allocate"),
				TAO_PEGTL_STRING("input"),
				TAO_PEGTL_STRING("tuple-error"),
				TAO_PEGTL_STRING("tensor-error")
			>
		{};

		struct CalleeRule :
			sor<
				InexplicableURule,
				StdFunctionNameRule
			>
		{};

		struct ArrowRule : TAO_PEGTL_STRING("\x3c-") {};

		struct InstructionPureAssignmentRule :
			interleaved<
				SpacesRule,
				VariableRule, //
				ArrowRule,
				InexplicableSRule //
			>
		{};

		struct InstructionOpAssignmentRule :
			interleaved<
				SpacesRule,
				VariableRule, // 0
				ArrowRule,
				InexplicableTRule, // 1
				ArithmeticOperatorRule, // 2
				InexplicableTRule // 3
			>
		{};

		struct InstructionCompareAssignmentRule :
			interleaved<
				SpacesRule,
				VariableRule, // 0
				ArrowRule,
				InexplicableTRule, // 1
				ComparisonOperatorRule, // 2
				InexplicableTRule // 3
			>
		{};

		struct InstructionLoadAssignmentRule :
			interleaved<
				SpacesRule,
				VariableRule, // 0
				ArrowRule,
				TAO_PEGTL_STRING("load"),
				VariableRule // 1
			>
		{};

		struct InstructionStoreAssignmentRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("store"),
				VariableRule, // 0
				ArrowRule,
				InexplicableSRule // 1
			>
		{};

		struct InstructionReturnRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("return"),
				opt<InexplicableTRule> // maybe 0
			>
		{};

		struct InstructionLabelRule :
			interleaved<
				SpacesRule,
				LabelRule
			>
		{};

		struct InstructionBranchUncondRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("br"),
				LabelRule
			>
		{};

		struct InstructionBranchCondRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("br"),
				InexplicableTRule,
				LabelRule
			>
		{};

		struct FunctionCallRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("call"),
				CalleeRule,
				one<'('>,
				CallArgsRule,
				one<')'>
			>
		{};

		struct InstructionCallVoidRule :
			interleaved<
				SpacesRule,
				FunctionCallRule
			>
		{};

		struct InstructionCallValRule :
			interleaved<
				SpacesRule,
				VariableRule,
				ArrowRule,
				FunctionCallRule
			>
		{};

		struct InstructionRule :
			sor<
				InstructionOpAssignmentRule,
				InstructionCompareAssignmentRule,
				InstructionLoadAssignmentRule,
				InstructionPureAssignmentRule,
				InstructionStoreAssignmentRule,
				InstructionReturnRule,
				InstructionLabelRule,
				InstructionBranchUncondRule,
				InstructionBranchCondRule,
				InstructionCallVoidRule,
				InstructionCallValRule
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
			seq<
				interleaved<
					SpacesOrNewLines,
					TAO_PEGTL_STRING("define"),
					L3FunctionNameRule,
					one<'('>,
					DefArgsRule,
					one<')'>,
					one<'{'>
				>,
				InstructionsRule,
				SpacesRule,
				one<'}'>
			>
		{};

		struct ProgramRule :
			seq<
				LineSeparatorsWithCommentsRule,
				SpacesRule,
				list<
					seq<
						SpacesRule,
						FunctionRule
					>,
					LineSeparatorsWithCommentsRule
				>,
				LineSeparatorsWithCommentsRule
			>
		{};

		template<typename Rule>
		struct Selector : pegtl::parse_tree::selector<
			Rule,
			pegtl::parse_tree::store_content::on<
				NameRule,
				VariableRule,
				LabelRule,
				L3FunctionNameRule,
				NumberRule,
				ComparisonOperatorRule,
				ArithmeticOperatorRule,
				CallArgsRule,
				DefArgsRule,
				StdFunctionNameRule,
				FunctionCallRule,
				InstructionPureAssignmentRule,
				InstructionOpAssignmentRule,
				InstructionCompareAssignmentRule,
				InstructionLoadAssignmentRule,
				InstructionStoreAssignmentRule,
				InstructionReturnRule,
				InstructionLabelRule,
				InstructionBranchUncondRule,
				InstructionBranchCondRule,
				InstructionCallVoidRule,
				InstructionCallValRule,
				InstructionsRule,
				FunctionRule,
				ProgramRule
			>
		> {};

	}

	struct ParseNode {
		// members
		Vec<Uptr<ParseNode>> children;
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
		void emplace_back(Uptr<ParseNode> &&child, States &&...) {
			children.emplace_back(mv(child));
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


	namespace node_processor {
		using namespace L3::program;

		std::string_view convert_name_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::NameRule));
			return n.string_view();
		}

		Uptr<Expr> make_expr(const ParseNode &n);

		Uptr<ItemRef<Variable>> make_variable_ref(const ParseNode &n) {
			assert(*n.rule == typeid(rules::VariableRule));
			return mkuptr<ItemRef<Variable>>(std::string(convert_name_rule(n[0])));
		}

		Uptr<ItemRef<L3Function>> make_l3_function_ref(const ParseNode &n) {
			assert(*n.rule == typeid(rules::L3FunctionNameRule));
			return mkuptr<ItemRef<L3Function>>(std::string(convert_name_rule(n[0])));
		}

		Uptr<ItemRef<ExternalFunction>> make_external_function_ref(const ParseNode &n) {
			assert(*n.rule == typeid(rules::StdFunctionNameRule));
			return mkuptr<ItemRef<ExternalFunction>>(std::string(n.string_view()));
		}

		Uptr<ItemRef<BasicBlock>> make_label_ref(const ParseNode &n) {
			assert(*n.rule == typeid(rules::LabelRule));
			return mkuptr<ItemRef<BasicBlock>>(std::string(convert_name_rule(n[0])));
		}

		Uptr<NumberLiteral> make_number_literal(const ParseNode &n) {
			assert(*n.rule == typeid(rules::NumberRule));
			return mkuptr<NumberLiteral>(n.string_view());
		}

		Uptr<FunctionCall> make_function_call(const ParseNode &n) {
			assert(*n.rule == typeid(rules::FunctionCallRule));

			// make the callee
			Uptr<Expr> callee = make_expr(n[0]);

			// add the arguments
			const ParseNode &call_args = n[1];
			assert(*call_args.rule == typeid(rules::CallArgsRule));
			Vec<Uptr<Expr>> arguments;
			for (const Uptr<ParseNode> &call_arg : call_args.children) {
				arguments.emplace_back(make_expr(*call_arg));
			}

			return mkuptr<FunctionCall>(mv(callee), mv(arguments));
		}

		Uptr<Expr> make_expr(const ParseNode &n) {
			const std::type_info &rule = *n.rule;
			if (rule == typeid(rules::VariableRule)) {
				return make_variable_ref(n);
			} else if (rule == typeid(rules::LabelRule)) {
				return make_label_ref(n);
			} else if (rule == typeid(rules::L3FunctionNameRule)) {
				return make_l3_function_ref(n);
			} else if (rule == typeid(rules::StdFunctionNameRule)) {
				return make_external_function_ref(n);
			} else if (rule == typeid(rules::NumberRule)) {
				return make_number_literal(n);
			} else {
				std::cerr << "Cannot make Expr from this parse node of type " << n.type << "\n";
				exit(1);
			}
		}

		Operator make_operator_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::ComparisonOperatorRule)
				|| *n.rule == typeid(rules::ArithmeticOperatorRule));
			return str_to_op(n.string_view());
		}

		Uptr<Instruction> convert_instruction_pure_assignment_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionPureAssignmentRule));
			return mkuptr<InstructionAssignment>(
				make_expr(n[1]),
				make_variable_ref(n[0])
			);
		}

		Uptr<Instruction> convert_instruction_op_assignment_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionOpAssignmentRule));
			return mkuptr<InstructionAssignment>(
				mkuptr<BinaryOperation>(
					make_expr(n[1]),
					make_expr(n[3]),
					make_operator_rule(n[2])
				),
				make_variable_ref(n[0])
			);
		}

		Uptr<Instruction> convert_instruction_compare_assignment_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionCompareAssignmentRule));
			return mkuptr<InstructionAssignment>(
				mkuptr<BinaryOperation>(
					make_expr(n[1]),
					make_expr(n[3]),
					make_operator_rule(n[2])
				),
				make_variable_ref(n[0])
			);
		}

		Uptr<Instruction> convert_instruction_load_assignment_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionLoadAssignmentRule));
			return mkuptr<InstructionAssignment>(
				mkuptr<MemoryLocation>(
					make_variable_ref(n[1])
				),
				make_variable_ref(n[0])
			);
		}

		Uptr<Instruction> convert_instruction_store_assignment_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionStoreAssignmentRule));
			return mkuptr<InstructionStore>(
				make_expr(n[1]),
				make_variable_ref(n[0])
			);
		}

		Uptr<Instruction> convert_instruction_return_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionReturnRule));
			if (n.children.empty()) {
				// return without value
				return mkuptr<InstructionReturn>(Opt<Uptr<Expr>>());
			} else {
				// return with value
				return mkuptr<InstructionReturn>(
					make_expr(n[0])
				);
			}
		}

		Uptr<Instruction> convert_instruction_label_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionLabelRule));
			return mkuptr<InstructionLabel>(
				std::string(convert_name_rule(n[0][0]))
			);
		}

		Uptr<Instruction> convert_instruction_branch_uncond_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionBranchUncondRule));
			return mkuptr<InstructionBranch>(
				make_label_ref(n[0])
			);
		}

		Uptr<Instruction> convert_instruction_branch_cond_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionBranchCondRule));
			return mkuptr<InstructionBranch>(
				make_label_ref(n[1]),
				make_expr(n[0])
			);
		}

		Uptr<Instruction> convert_instruction_call_void_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionCallVoidRule));
			return mkuptr<InstructionAssignment>(
				make_function_call(n[0])
			);
		}

		Uptr<Instruction> convert_instruction_call_val_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionCallValRule));
			return mkuptr<InstructionAssignment>(
				make_function_call(n[1]),
				make_variable_ref(n[0])
			);
		}

		Uptr<Instruction> make_instruction(const ParseNode &n) {
			const std::type_info &rule = *n.rule;
			if (rule == typeid(rules::InstructionPureAssignmentRule)) {
				return convert_instruction_pure_assignment_rule(n);
			} else if (rule == typeid(rules::InstructionOpAssignmentRule)) {
				return convert_instruction_op_assignment_rule(n);
			} else if (rule == typeid(rules::InstructionCompareAssignmentRule)) {
				return convert_instruction_compare_assignment_rule(n);
			} else if (rule == typeid(rules::InstructionLoadAssignmentRule)) {
				return convert_instruction_load_assignment_rule(n);
			} else if (rule == typeid(rules::InstructionStoreAssignmentRule)) {
				return convert_instruction_store_assignment_rule(n);
			} else if (rule == typeid(rules::InstructionReturnRule)) {
				return convert_instruction_return_rule(n);
			} else if (rule == typeid(rules::InstructionLabelRule)) {
				return convert_instruction_label_rule(n);
			} else if (rule == typeid(rules::InstructionBranchUncondRule)) {
				return convert_instruction_branch_uncond_rule(n);
			} else if (rule == typeid(rules::InstructionBranchCondRule)) {
				return convert_instruction_branch_cond_rule(n);
			} else if (rule == typeid(rules::InstructionCallVoidRule)) {
				return convert_instruction_call_void_rule(n);
			} else if (rule == typeid(rules::InstructionCallValRule)) {
				return convert_instruction_call_val_rule(n);
			} else {
				std::cerr << "Cannot make Instruction from this parse node.";
				exit(1);
			}
		}

		Pair<Uptr<L3Function>, AggregateScope> make_l3_function_with_scope(const ParseNode &n) {
			assert(*n.rule == typeid(rules::FunctionRule));
			L3Function::Builder builder;

			// add function name
			builder.add_name(std::string(convert_name_rule(n[0][0])));

			// add function parameters
			const ParseNode &def_args = n[1];
			assert(*def_args.rule == typeid(rules::DefArgsRule));
			for (const Uptr<ParseNode> &def_arg : def_args.children) {
				builder.add_parameter(std::string(convert_name_rule((*def_arg)[0])));
			}

			// add instructions
			const ParseNode &instructions = n[2];
			assert(*instructions.rule == typeid(rules::InstructionsRule));
			for (const Uptr<ParseNode> &inst : instructions.children) {
				builder.add_next_instruction(make_instruction(*inst));
			}

			return builder.get_result();
		}

		Uptr<Program> make_program(const ParseNode &n) {
			assert(*n.rule == typeid(rules::ProgramRule));
			Program::Builder builder;
			for (const Uptr<ParseNode> &child : n.children) {
				auto [function, agg_scope] = make_l3_function_with_scope(*child);
				builder.add_l3_function(mv(function), agg_scope);
			}
			return builder.get_result();
		}
	}

	Uptr<L3::program::Program> parse_file(char *fileName, Opt<std::string> parse_tree_output) {
		using EntryPointRule = pegtl::must<rules::ProgramRule>;

		// Check the grammar for some possible issues.
		// TODO move this to a separate file bc it's performance-intensive
		if (pegtl::analyze<EntryPointRule>() != 0) {
			std::cerr << "There are problems with the grammar" << std::endl;
			exit(1);
		}

		// Parse
		pegtl::file_input<> fileInput(fileName);
		auto root = pegtl::parse_tree::parse<EntryPointRule, ParseNode, rules::Selector>(fileInput);
		if (!root) {
			std::cerr << "ERROR: Parser failed" << std::endl;
			exit(1);
		}
		if (parse_tree_output) {
			std::ofstream output_fstream(*parse_tree_output);
			if (output_fstream.is_open()) {
				pegtl::parse_tree::print_dot(output_fstream, *root);
				output_fstream.close();
			}
		}

		Uptr<L3::program::Program> ptr = node_processor::make_program((*root)[0]);
		return ptr;

		// auto p = node_processor::make_program((*root)[0]);
		// p->get_scope().fake_bind_frees(); // If you want to allow unbound name
		// p->get_scope().ensure_no_frees(); // If you want to error on unbound name
		// return p;
		// return {};
	}
}