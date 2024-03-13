#include "std_alias.h"
#include "parser.h"
#include "utils.h"
#include "mir.h"
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

namespace La::parser {
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

		template<typename... Rules>
		using spaces_interleaved = interleaved<SpacesRule, Rules...>;

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
			ascii::identifier // the rules for LA names are the same as for C identifiers
		{};

		struct LabelRule :
			seq<one<':'>, NameRule>
		{};

		struct OperatorRule :
			sor<
				TAO_PEGTL_STRING("+"),
				TAO_PEGTL_STRING("-"),
				TAO_PEGTL_STRING("*"),
				TAO_PEGTL_STRING("&"),
				TAO_PEGTL_STRING("<<"),
				TAO_PEGTL_STRING("<="),
				TAO_PEGTL_STRING("<"),
				TAO_PEGTL_STRING(">>"),
				TAO_PEGTL_STRING(">="),
				TAO_PEGTL_STRING(">"),
				TAO_PEGTL_STRING("=")
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

		struct InexplicableTRule :
			sor<
				NameRule,
				NumberRule
			>
		{};

		struct CallArgsRule :
			opt<list<
				InexplicableTRule,
				one<','>,
				SpaceRule
			>>
		{};

		struct Int64TypeRule : TAO_PEGTL_STRING("int64") {};
		struct ArrayTypeIndicator : TAO_PEGTL_STRING("[]") {};
		struct TupleTypeRule : TAO_PEGTL_STRING("tuple") {};
		struct CodeTypeRule : TAO_PEGTL_STRING("code") {};
		struct VoidTypeRule : TAO_PEGTL_STRING("void") {};

		struct TypeRule :
			sor<
				seq<
					Int64TypeRule,
					star<ArrayTypeIndicator>
				>,
				TupleTypeRule,
				CodeTypeRule,
				VoidTypeRule
			>
		{};

		struct NonVoidTypeRule :
			minus<TypeRule, VoidTypeRule>
		{};

		struct IndexingExpressionRule :
			spaces_interleaved<
				NameRule,
				star<
					one<'['>,
					InexplicableTRule,
					one<']'>
				>
			>
		{};

		struct CallingExpressionRule :
			spaces_interleaved<
				NameRule,
				one<'('>,
				CallArgsRule,
				one<')'>
			>
		{};

		struct InstructionDeclarationRule :
			spaces_interleaved<
				NonVoidTypeRule,
				NameRule
			>
		{};

		struct ArrowSymbolRule : TAO_PEGTL_STRING("\x3c-") {};

		struct InstructionOpAssignmentRule :
			spaces_interleaved<
				NameRule,
				ArrowSymbolRule,
				InexplicableTRule,
				OperatorRule,
				InexplicableTRule
			>
		{};

		struct InstructionReadTensorRule :
			spaces_interleaved<
				NameRule,
				ArrowSymbolRule,
				IndexingExpressionRule
			>
		{};

		struct InstructionWriteTensorRule :
			spaces_interleaved<
				IndexingExpressionRule,
				ArrowSymbolRule,
				InexplicableTRule
			>
		{};

		struct InstructionGetLengthRule :
			spaces_interleaved<
				NameRule,
				ArrowSymbolRule,
				TAO_PEGTL_STRING("length"),
				NameRule,
				opt<InexplicableTRule>
			>
		{};

		struct InstructionCallVoidRule :
			spaces_interleaved<
				CallingExpressionRule
			>
		{};

		struct InstructionCallValRule :
			spaces_interleaved<
				NameRule,
				ArrowSymbolRule,
				CallingExpressionRule
			>
		{};

		struct InstructionNewArrayRule :
			spaces_interleaved<
				NameRule,
				ArrowSymbolRule,
				TAO_PEGTL_STRING("new"),
				TAO_PEGTL_STRING("Array"),
				one<'('>,
				CallArgsRule,
				one<')'>
			>
		{};

		struct InstructionNewTupleRule :
			spaces_interleaved<
				NameRule,
				ArrowSymbolRule,
				TAO_PEGTL_STRING("new"),
				TAO_PEGTL_STRING("Tuple"),
				one<'('>,
				CallArgsRule,
				one<')'>
			>
		{};

		struct InstructionLabelRule :
			spaces_interleaved<
				LabelRule
			>
		{};

		struct InstructionBranchUncondRule :
			spaces_interleaved<
				TAO_PEGTL_STRING("br"),
				LabelRule
			>
		{};

		struct InstructionBranchCondRule :
			spaces_interleaved<
				TAO_PEGTL_STRING("br"),
				InexplicableTRule,
				LabelRule,
				LabelRule
			>
		{};

		struct InstructionReturnRule :
			spaces_interleaved<
				TAO_PEGTL_STRING("return"),
				opt<InexplicableTRule>
			>
		{};

		struct InstructionRule :
			sor<
				InstructionDeclarationRule,
				InstructionGetLengthRule,
				InstructionNewArrayRule,
				InstructionNewTupleRule,
				InstructionCallVoidRule,
				InstructionCallValRule,
				InstructionOpAssignmentRule,
				InstructionReadTensorRule,
				InstructionWriteTensorRule,
				InstructionLabelRule,
				InstructionBranchUncondRule,
				InstructionBranchCondRule,
				InstructionReturnRule
			>
		{};

		struct InstructionsRule :
			opt<list<
				seq<bol, SpacesRule, InstructionRule>,
				LineSeparatorsWithCommentsRule
			>>
		{};

		struct DefArgRule :
			spaces_interleaved<
				NonVoidTypeRule,
				NameRule
			>
		{};

		struct DefArgsRule :
			opt<list<
				DefArgRule,
				one<','>,
				SpaceRule
			>>
		{};

		struct FunctionDefinitionRule :
			interleaved<
				LineSeparatorsWithCommentsRule,
				interleaved<
					SpacesOrNewLines,
					TypeRule,
					NameRule,
					one<'('>,
					DefArgsRule,
					one<')'>,
					one<'{'>
				>,
				InstructionsRule,
				seq<SpacesRule, one<'}'>>
			>
		{};

		struct ProgramRule :
			list<
				seq<SpacesRule, FunctionDefinitionRule>,
				LineSeparatorsWithCommentsRule
			>
		{};

		struct ProgramFile :
			seq<
				bof,
				LineSeparatorsWithCommentsRule,
				ProgramRule,
				LineSeparatorsWithCommentsRule,
				eof
			>
		{};

		template<typename Rule>
		struct Selector : pegtl::parse_tree::selector<
			Rule,
			pegtl::parse_tree::store_content::on<
				NameRule,
				OperatorRule,
				NumberRule
			>,
			pegtl::parse_tree::remove_content::on<
				LabelRule,
				CallArgsRule,
				ArrayTypeIndicator,
				TypeRule,
				VoidTypeRule,
				Int64TypeRule,
				ArrayTypeIndicator,
				TupleTypeRule,
				CodeTypeRule,
				IndexingExpressionRule,
				CallingExpressionRule,
				DefArgRule,
				DefArgsRule,
				InstructionDeclarationRule,
				InstructionOpAssignmentRule,
				InstructionReadTensorRule,
				InstructionWriteTensorRule,
				InstructionGetLengthRule,
				InstructionCallVoidRule,
				InstructionCallValRule,
				InstructionNewArrayRule,
				InstructionNewTupleRule,
				InstructionLabelRule,
				InstructionBranchUncondRule,
				InstructionBranchCondRule,
				InstructionReturnRule,
				InstructionsRule,
				FunctionDefinitionRule,
				ProgramRule
			>
		> {};
	}

	struct ParseNode {
		// members
		Vec<Uptr<ParseNode>> children;
		pegtl::internal::inputerator begin;
		pegtl::internal::inputerator end;
		Opt<pegtl::position> position;
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
			this->position = in.position();
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

		template<typename... States>
		void remove_content(States &&... /*unused*/) {
			this->end = TAO_PEGTL_NAMESPACE::internal::inputerator();
		}

		bool is_root() const noexcept {
			return static_cast<bool>(this->rule);
		}
	};


	namespace node_processor {
		using namespace La::hir;
		using mir::Type;
		using mir::Operator;

		std::string extract_name(const ParseNode &n) {
			assert(*n.rule == typeid(rules::NameRule));
			return std::string(n.string_view());
		}

		Type make_type(const ParseNode &n) {
			assert(*n.rule == typeid(rules::TypeRule));
			const std::type_info &type_rule = *n[0].rule;
			if (type_rule == typeid(rules::VoidTypeRule)) {
				return { Type::VoidType {} };
			} else if (type_rule == typeid(rules::Int64TypeRule)) {
				// the rest of the children must be ArrayTypeIndicators
				return { Type::ArrayType { static_cast<int>(n.children.size() - 1) } };
			} else if (type_rule == typeid(rules::TupleTypeRule)) {
				return { Type::TupleType {} };
			} else if (type_rule == typeid(rules::CodeTypeRule)) {
				return { Type::CodeType {} };
			} else {
				std::cerr << "Logic error: inexhaustive over TypeRule node's children\n";
				exit(1);
			}
		}

		Uptr<ItemRef<Nameable>> make_name_ref(const ParseNode &n) {
			assert(*n.rule == typeid(rules::NameRule));
			return mkuptr<ItemRef<Nameable>>(extract_name(n));
		}

		std::string make_label_ref(const ParseNode &n) {
			assert(*n.rule == typeid(rules::LabelRule));
			return extract_name(n[0]);
		}

		Uptr<Expr> convert_inexplicable_t_rule(const ParseNode &n) {
			const std::type_info &rule = *n.rule;
			if (rule == typeid(rules::NameRule)) {
				return make_name_ref(n);
			} else if (rule == typeid(rules::NumberRule)) {
				return mkuptr<NumberLiteral>(utils::string_view_to_int<int64_t>(n.string_view()));
			} else {
				std::cerr << "Logic error: inexhaustive over InexplicableT node possibilities\n";
				exit(1);
			}
		}

		Uptr<IndexingExpr> make_indexing_expr(const ParseNode &n) {
			Uptr<ItemRef<Nameable>> target;
			Vec<Uptr<Expr>> indices;

			if (*n.rule == typeid(rules::IndexingExpressionRule)) {
				target = make_name_ref(n[0]);
				for (auto it = n.children.begin() + 1; it != n.children.end(); ++it) {
					const ParseNode &index_node = **it;
					indices.push_back(convert_inexplicable_t_rule(index_node));
				}
			} else if (*n.rule == typeid(rules::NameRule)) {
				target = make_name_ref(n);
			} else {
				std::cerr << "Logic error: can't convert this ParseNode into an IndexingExpr\n";
				exit(1);
			}

			auto expr = mkuptr<IndexingExpr>(
				mv(target),
				mv(indices)
			);
			expr->src_pos = n.position;
			return expr;
		}

		Vec<Uptr<Expr>> make_call_args(const ParseNode &n) {
			assert(*n.rule == typeid(rules::CallArgsRule));
			Vec<Uptr<Expr>> arguments;
			for (const Uptr<ParseNode> &call_arg : n.children) {
				arguments.emplace_back(convert_inexplicable_t_rule(*call_arg));
			}
			return arguments;
		}

		Uptr<FunctionCall> make_function_call(const ParseNode &n) {
			assert(*n.rule == typeid(rules::CallingExpressionRule));
			auto function = mkuptr<FunctionCall>(
				make_name_ref(n[0]),
				make_call_args(n[1])
			);
			function->src_pos = n.position;
			return function;
		}

		Uptr<InstructionDeclaration> convert_instruction_declaration_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionDeclarationRule));
			return mkuptr<InstructionDeclaration>(
				extract_name(n[1]),
				make_type(n[0])
			);
		}

		Operator make_operator(const ParseNode &n) {
			assert(*n.rule == typeid(rules::OperatorRule));
			return str_to_op(n.string_view());
		}

		Uptr<InstructionAssignment> convert_instruction_op_assignment_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionOpAssignmentRule));
			return mkuptr<InstructionAssignment>(
				mkuptr<BinaryOperation>(
					convert_inexplicable_t_rule(n[1]),
					convert_inexplicable_t_rule(n[3]),
					make_operator(n[2])
				),
				make_indexing_expr(n[0])
			);
		}

		Uptr<InstructionAssignment> convert_instruction_read_tensor_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionReadTensorRule));
			return mkuptr<InstructionAssignment>(
				make_indexing_expr(n[1]),
				make_indexing_expr(n[0])
			);
		}

		Uptr<InstructionAssignment> convert_instruction_write_tensor_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionWriteTensorRule));
			return mkuptr<InstructionAssignment>(
				convert_inexplicable_t_rule(n[1]),
				make_indexing_expr(n[0])
			);
		}

		Uptr<InstructionAssignment> convert_instruction_get_length_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionGetLengthRule));
			return mkuptr<InstructionAssignment>(
				mkuptr<LengthGetter>(
					make_name_ref(n[1]),
					n.children.size() > 2 ? (convert_inexplicable_t_rule(n[2])) : (Opt<Uptr<Expr>>())
				),
				make_indexing_expr(n[0])
			);
		}

		Uptr<InstructionAssignment> convert_instruction_call_void_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionCallVoidRule));
			return mkuptr<InstructionAssignment>(
				make_function_call(n[0])
			);
		}

		Uptr<InstructionAssignment> convert_instruction_call_val_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionCallValRule));
			return mkuptr<InstructionAssignment>(
				make_function_call(n[1]),
				make_indexing_expr(n[0])
			);
		}

		Uptr<InstructionAssignment> convert_instruction_new_array_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionNewArrayRule));
			return mkuptr<InstructionAssignment>(
				mkuptr<NewArray>(make_call_args(n[1])),
				make_indexing_expr(n[0])
			);
		}

		Uptr<InstructionAssignment> convert_instruction_new_tuple_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionNewTupleRule));
			Vec<Uptr<Expr>> call_args = make_call_args(n[1]);
			if (call_args.size() != 1) {
				std::cout << "Compiliation Error: new Tuple(...) expression expects exactly one argument\n";
				exit(1);
			}
			return mkuptr<InstructionAssignment>(
				mkuptr<NewTuple>(mv(call_args[0])),
				make_indexing_expr(n[0])
			);
		}

		Uptr<InstructionLabel> convert_instruction_label_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionLabelRule));
			return mkuptr<InstructionLabel>(
				make_label_ref(n[0])
			);
		}

		Uptr<InstructionBranchUnconditional> convert_instruction_branch_uncond_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionBranchUncondRule));
			return mkuptr<InstructionBranchUnconditional>(
				make_label_ref(n[0])
			);
		}

		Uptr<InstructionBranchConditional> convert_instruction_branch_cond_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionBranchCondRule));
			return mkuptr<InstructionBranchConditional>(
				convert_inexplicable_t_rule(n[0]),
				make_label_ref(n[1]),
				make_label_ref(n[2])
			);
		}

		Uptr<InstructionReturn> convert_instruction_return_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionReturnRule));
			return mkuptr<InstructionReturn>(
				n.children.size() > 0 ? (convert_inexplicable_t_rule(n[0])) : Opt<Uptr<Expr>>()
			);
		}

		Uptr<Instruction> make_instruction(const ParseNode &n) {
			const std::type_info &rule = *n.rule;
			if (rule == typeid(rules::InstructionDeclarationRule)) {
				return convert_instruction_declaration_rule(n);
			} else if (rule == typeid(rules::InstructionOpAssignmentRule)) {
				return convert_instruction_op_assignment_rule(n);
			} else if (rule == typeid(rules::InstructionReadTensorRule)) {
				return convert_instruction_read_tensor_rule(n);
			} else if (rule == typeid(rules::InstructionWriteTensorRule)) {
				return convert_instruction_write_tensor_rule(n);
			} else if (rule == typeid(rules::InstructionGetLengthRule)) {
				return convert_instruction_get_length_rule(n);
			} else if (rule == typeid(rules::InstructionCallVoidRule)) {
				return convert_instruction_call_void_rule(n);
			} else if (rule == typeid(rules::InstructionCallValRule)) {
				return convert_instruction_call_val_rule(n);
			} else if (rule == typeid(rules::InstructionNewArrayRule)) {
				return convert_instruction_new_array_rule(n);
			} else if (rule == typeid(rules::InstructionNewTupleRule)) {
				return convert_instruction_new_tuple_rule(n);
			} else if (rule == typeid(rules::InstructionLabelRule)) {
				return convert_instruction_label_rule(n);
			} else if (rule == typeid(rules::InstructionBranchUncondRule)) {
				return convert_instruction_branch_uncond_rule(n);
			} else if (rule == typeid(rules::InstructionBranchCondRule)) {
				return convert_instruction_branch_cond_rule(n);
			} else if (rule == typeid(rules::InstructionReturnRule)) {
				return convert_instruction_return_rule(n);
			} else {
				std::cerr << "Cannot make Instruction from this parse node.";
				exit(1);
			}
		}

		Uptr<LaFunction> make_la_function(const ParseNode &n) {
			assert(*n.rule == typeid(rules::FunctionDefinitionRule));

			Uptr<LaFunction> function = mkuptr<LaFunction>(
				extract_name(n[1]),
				make_type(n[0])
			);

			const ParseNode &def_args_node = n[2];
			assert(*def_args_node.rule == typeid(rules::DefArgsRule));
			for (const Uptr<ParseNode> &child : def_args_node.children) {
				const ParseNode &def_arg_node = *child;
				assert(*def_arg_node.rule == typeid(rules::DefArgRule));
				function->add_variable(
					extract_name(def_arg_node[1]),
					make_type(def_arg_node[0]),
					true // is a paramter variable
				);
			}

			const ParseNode &instructions_node = n[3];
			assert(*instructions_node.rule == typeid(rules::InstructionsRule));
			for (const Uptr<ParseNode> &child : instructions_node.children) {
				function->add_next_instruction(make_instruction(*child));
			}

			return function;
		}

		Uptr<Program> make_program(const ParseNode &n) {
			assert(*n.rule == typeid(rules::ProgramRule));
			Uptr<Program> program = mkuptr<Program>();
			for (const Uptr<ParseNode> &child : n.children) {
				program->add_la_function(make_la_function(*child));
			}
			link_std(*program);
			return program;
		}
	}

	Uptr<La::hir::Program> parse_file(char *fileName, Opt<std::string> parse_tree_output) {
		using EntryPointRule = pegtl::must<rules::ProgramFile>;

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

		Uptr<La::hir::Program> ptr = node_processor::make_program((*root)[0]);
		return ptr;
	}
}