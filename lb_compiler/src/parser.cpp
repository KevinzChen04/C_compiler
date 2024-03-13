#include "parser.h"

#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/analyze.hpp>
#include <tao/pegtl/contrib/raw_string.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>
#include <tao/pegtl/contrib/parse_tree_to_dot.hpp>

namespace pegtl = TAO_PEGTL_NAMESPACE;

namespace Lb::parser {

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
			std::string_view strview {
				this->begin.data,
				10
			};
			// std::cout << "Starting to parse " << pegtl::demangle<Rule>() << " " << in.position() << ": \"" << strview << "\"\n";
		}

		template<typename Rule, typename ParseInput, typename... States>
		void success(const ParseInput &in, States &&...) {
			this->end = in.inputerator();
			this->rule = &typeid(Rule);
			this->type = pegtl::demangle<Rule>();
			this->type.remove_prefix(this->type.find_last_of(':') + 1);
			// std::cout << "Successfully parsed " << pegtl::demangle<Rule>() << " at " << in.position() << ": \"" << this->string_view() << "\"\n";
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
    namespace rules {
        using namespace pegtl;
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
			ascii::identifier
		{};

        struct LabelRule :
			seq<one<':'>, NameRule>
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

        struct OperatorRule :
			sor<
				TAO_PEGTL_STRING("<<"),
				TAO_PEGTL_STRING(">>"),
				TAO_PEGTL_STRING("+"),
				TAO_PEGTL_STRING("-"),
				TAO_PEGTL_STRING("*"),
				TAO_PEGTL_STRING("&"),
				TAO_PEGTL_STRING("<="),
				TAO_PEGTL_STRING(">="),
				TAO_PEGTL_STRING("="),
				TAO_PEGTL_STRING("<"),
				TAO_PEGTL_STRING(">")
			>
		{};

        struct InexplicableTRule :
			sor<
				NameRule,
				NumberRule
			>
		{};

        struct TypeRule :
			sor<
				seq<
					TAO_PEGTL_STRING("int64"),
					star<
						TAO_PEGTL_STRING("[]")
					>
				>,
				TAO_PEGTL_STRING("tuple"),
				TAO_PEGTL_STRING("code")
			>
		{};

        struct VoidableTypeRule :
			sor<
				TypeRule,
				TAO_PEGTL_STRING("void")
			>
		{};

        struct ArgsRule :
			opt<list<
				InexplicableTRule,
				one<','>,
				SpaceRule
			>>
		{};

        struct NamesRule :
            seq<
                SpacesRule,
                NameRule,
                star<
                    interleaved<
                        SpacesRule,
                        one<','>,
                        NameRule
                    >
                >
            >
        {};

        struct ArrowRule : TAO_PEGTL_STRING("\x3c-") {};

        struct ConditionRule :
            interleaved<
                SpacesRule,
                InexplicableTRule,
                OperatorRule,
                InexplicableTRule
            >
        {};

		struct CallingExpressionRule :
			interleaved<
				SpacesRule,
				NameRule,
				one<'('>,
				ArgsRule,
				one<')'>
			>
		{};

        struct ArrayAccess :
			interleaved<
				SpacesRule,
				NameRule,
				plus<
					interleaved<
						SpacesRule,
						one<'['>,
						InexplicableTRule,
						one<']'>
					>,
					SpacesRule
				>
			>
		{};

        struct InstructionTypeDeclarationRule :
            interleaved<
				SpacesRule,
				VoidableTypeRule,
				NamesRule
			>
        {};

        struct InstructionPureAssignmentRule :
			interleaved<
				SpacesRule,
				NameRule,
				ArrowRule,
				InexplicableTRule
			>
		{};

        struct InstructionOperatorAssignmentRule :
			interleaved<
				SpacesRule,
				NameRule,
				ArrowRule,
				InexplicableTRule,
				OperatorRule,
				InexplicableTRule
			>
		{};

        struct InstructionLabelRule :
            seq<
                SpacesRule,
                LabelRule
            >
        {};

        struct InstructionIfStatementRule :
            interleaved<
                SpacesRule,
                TAO_PEGTL_STRING("if"),
                one<'('>,
                ConditionRule,
                one<')'>,
                LabelRule,
                LabelRule
            >
        {};

        struct InstructionGotoRule :
            interleaved<
                SpacesRule,
                TAO_PEGTL_STRING("goto"),
                LabelRule
            >
        {};

        struct InstructionReturnRule :
            interleaved<
                SpacesRule,
                TAO_PEGTL_STRING("return"),
                opt<InexplicableTRule>
            >
        {};

        struct InstructionWhileStatementRule :
            interleaved<
                SpacesRule,
                TAO_PEGTL_STRING("while"),
                one<'('>,
                ConditionRule,
                one<')'>,
                LabelRule,
                LabelRule
            >
        {};

        struct InstructionContinueRule :
            seq<
                SpacesRule,
                TAO_PEGTL_STRING("continue")
            >
        {};

        struct InstructionBreakRule :
            seq<
                SpacesRule,
                TAO_PEGTL_STRING("break")
            >
        {};

        struct InstructionArrayLoadRule :
			interleaved<
				SpacesRule,
				NameRule,
				ArrowRule,
				ArrayAccess
			>
		{};

        struct InstructionArrayStoreRule :
			interleaved<
				SpacesRule,
				ArrayAccess,
				ArrowRule,
				InexplicableTRule
			>
		{};

        struct InstructionLengthRule :
			interleaved<
				SpacesRule,
				NameRule,
				ArrowRule,
				TAO_PEGTL_STRING("length"),
				NameRule,
				opt<InexplicableTRule>
			>
		{};

        struct InstructionFunctionCallRule :
            interleaved<
                SpacesRule,
				CallingExpressionRule
            >
        {};

        struct InstructionFunctionCallAssignmentRule :
            interleaved<
                SpacesRule,
				NameRule,
				ArrowRule,
				CallingExpressionRule
            >
        {};

        struct InstructionArrayDeclarationRule :
			interleaved<
				SpacesRule,
				NameRule,
				ArrowRule,
				TAO_PEGTL_STRING("new"),
				TAO_PEGTL_STRING("Array"),
				one<'('>,
				ArgsRule,
				one<')'>
			>
		{};

        struct InstructionTupleDeclarationRule :
			interleaved<
				SpacesRule,
				NameRule,
				ArrowRule,
				TAO_PEGTL_STRING("new"),
				TAO_PEGTL_STRING("Tuple"),
				one<'('>,
				InexplicableTRule,
				one<')'>
			>
		{};

		struct InstructionScopeRule;

        struct InstructionRule :
			sor<
				InstructionFunctionCallRule,
                InstructionFunctionCallAssignmentRule,
				InstructionTypeDeclarationRule,
				InstructionOperatorAssignmentRule,
                InstructionLabelRule,
                InstructionIfStatementRule,
                InstructionGotoRule,
                InstructionReturnRule,
                InstructionWhileStatementRule,
                InstructionContinueRule,
                InstructionBreakRule,
                InstructionArrayLoadRule,
                InstructionArrayStoreRule,
                InstructionLengthRule,
                InstructionArrayDeclarationRule,
                InstructionTupleDeclarationRule,
                InstructionScopeRule,
				InstructionPureAssignmentRule
			>
		{};

        struct InstructionScopeRule :
            interleaved<
                SpacesOrNewLines,
                one<'{'>,
                star<
                    seq<
                        LineSeparatorsWithCommentsRule,
						SpacesRule,
                        InstructionRule,
						LineSeparatorsWithCommentsRule
                    >
                >,
                one<'}'>
            >
        {};

		struct DefineArgRule :
			interleaved<
				SpacesRule,
				VoidableTypeRule,
				NameRule
			>
		{};

		struct DefineArgsRule :
			opt<list<
				DefineArgRule,
				one<','>,
				SpaceRule
			>>
		{};

        struct FunctionRule :
            interleaved<
                SpacesRule,
                VoidableTypeRule,
				NameRule,
                one<'('>,
                DefineArgsRule,
                one<')'>,
				LineSeparatorsWithCommentsRule,
                InstructionScopeRule
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
				CallingExpressionRule,
				NameRule,
                LabelRule,
                NumberRule,
                OperatorRule,
                TypeRule,
                VoidableTypeRule,
                ArgsRule,
                NamesRule,
                ConditionRule,
                ArrayAccess,
				DefineArgRule,
				DefineArgsRule,
                InstructionTypeDeclarationRule,
                InstructionPureAssignmentRule,
                InstructionOperatorAssignmentRule,
                InstructionLabelRule,
                InstructionIfStatementRule,
                InstructionGotoRule,
                InstructionReturnRule,
                InstructionWhileStatementRule,
                InstructionContinueRule,
                InstructionBreakRule,
                InstructionArrayLoadRule,
                InstructionArrayStoreRule,
                InstructionLengthRule,
                InstructionFunctionCallRule,
                InstructionFunctionCallAssignmentRule,
                InstructionArrayDeclarationRule,
                InstructionTupleDeclarationRule,
                InstructionScopeRule,
				FunctionRule,
				ProgramRule
			>
		>
        {};
    }

	namespace node_processor {
		using namespace Lb::hir;

		std::string extract_name(const ParseNode &n) {
			assert(*n.rule == typeid(rules::NameRule));
			return std::string(n.string_view());
		}

		std::string make_type(const ParseNode &n) {
			std::cout << std::string(n.string_view()) << std::endl;
			assert(*n.rule == typeid(rules::TypeRule));
			return (std::string(n.string_view()));
		}

		Operator make_operator(const ParseNode &n) {
			assert(*n.rule == typeid(rules::OperatorRule));
			return str_to_op(n.string_view());
		}

		std::string make_voidable_type(const ParseNode &n) {
			assert(*n.rule == typeid(rules::VoidableTypeRule));
			return (std::string(n.string_view()));
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

		Vec<Uptr<Expr>> make_call_args(const ParseNode &n) {
			assert(*n.rule == typeid(rules::ArgsRule));
			Vec<Uptr<Expr>> arguments;
			for (const Uptr<ParseNode> &call_arg : n.children) {
				arguments.emplace_back(convert_inexplicable_t_rule(*call_arg));
			}
			return arguments;
		}

		Uptr<BinaryOperation> make_condition_expr(const ParseNode &n) {
			assert(*n.rule == typeid(rules::ConditionRule));
			return mkuptr<BinaryOperation>(
				convert_inexplicable_t_rule(n[0]),
				convert_inexplicable_t_rule(n[2]),
				make_operator(n[1])
			);
		}

		Uptr<IndexingExpr> make_indexing_expr(const ParseNode &n) {
			Uptr<ItemRef<Nameable>> target;
			Vec<Uptr<Expr>> indices;

			if (*n.rule == typeid(rules::ArrayAccess)) {
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
			return expr;
		}

		Uptr<FunctionCall> make_function_call(const ParseNode &n) {
			assert(*n.rule == typeid(rules::CallingExpressionRule));
			auto function = mkuptr<FunctionCall>(
				make_name_ref(n[0]),
				make_call_args(n[1])
			);
			return function;
		}

		Uptr<StatementDeclaration> convert_instruction_declaration(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionTypeDeclarationRule));
			Vec<std::string> variable_names;
			const ParseNode &names_rule = n[1];
			assert(*names_rule.rule == typeid(rules::NamesRule));
			for (const Uptr<ParseNode> &name: names_rule.children){
				variable_names.push_back(extract_name(*name));
			}
			return mkuptr<StatementDeclaration>(
				mv(variable_names),
				make_voidable_type(n[0])
			);
		}

		Uptr<StatementAssignment> convert_instruction_pure_assignment_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionPureAssignmentRule));
			return mkuptr<StatementAssignment>(
				convert_inexplicable_t_rule(n[1]),
				make_indexing_expr(n[0])
			);
		}

		Uptr<StatementAssignment> convert_instruction_operation_assignment_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionOperatorAssignmentRule));
			return mkuptr<StatementAssignment>(
				mkuptr<BinaryOperation>(
					convert_inexplicable_t_rule(n[1]),
					convert_inexplicable_t_rule(n[3]),
					make_operator(n[2])
				),
				make_indexing_expr(n[0])
			);
		}

		Uptr<StatementLabel> convert_instruction_label_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionLabelRule));
			return mkuptr<StatementLabel>(make_label_ref(n[0]));
		}

		Uptr<StatementIf> convert_instruction_if_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionIfStatementRule));
			return mkuptr<StatementIf>(
				make_condition_expr(n[0]),
				make_label_ref(n[1]),
				make_label_ref(n[2])
			);
		}

		Uptr<StatementGoto> convert_instruction_goto_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionGotoRule));
			return mkuptr<StatementGoto>(
				make_label_ref(n[0])
			);
		}

		Uptr<StatementReturn> convert_instruction_return_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionReturnRule));
			return mkuptr<StatementReturn>(
				n.children.size() > 0 ? (convert_inexplicable_t_rule(n[0])) : Opt<Uptr<Expr>>()
			);
		}

		Uptr<StatementWhile> convert_instruction_while_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionWhileStatementRule));
			return mkuptr<StatementWhile>(
				make_condition_expr(n[0]),
				make_label_ref(n[1]),
				make_label_ref(n[2])
			);
		}

		Uptr<StatementContinue> convert_instruction_continue_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionContinueRule));
			return mkuptr<StatementContinue>();
		}

		Uptr<StatementBreak> convert_instruction_break_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionBreakRule));
			return mkuptr<StatementBreak>();
		}

		Uptr<StatementAssignment> convert_instruction_array_load_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionArrayLoadRule));
			return mkuptr<StatementAssignment>(
				make_indexing_expr(n[1]),
				make_indexing_expr(n[0])
			);
		}

		Uptr<StatementAssignment> convert_instruction_array_store_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionArrayStoreRule));
			return mkuptr<StatementAssignment>(
				convert_inexplicable_t_rule(n[1]),
				make_indexing_expr(n[0])
			);
		}

		Uptr<StatementAssignment> convert_instruction_length_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionLengthRule));
			return mkuptr<StatementAssignment>(
				mkuptr<LengthGetter>(
					make_name_ref(n[1]),
					n.children.size() > 2 ? (convert_inexplicable_t_rule(n[2])) : (Opt<Uptr<Expr>>())
				),
				make_indexing_expr(n[0])
			);
		}

		Uptr<StatementAssignment> convert_instruction_function_call_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionFunctionCallRule));
			return mkuptr<StatementAssignment>(
				make_function_call(n[0])
			);
		}

		Uptr<StatementAssignment> convert_instruction_function_call_assignment_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionFunctionCallAssignmentRule));
			return mkuptr<StatementAssignment>(
				make_function_call(n[1]),
				make_indexing_expr(n[0])
			);
		}

		Uptr<StatementAssignment> convert_instruction_array_declaration_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionArrayDeclarationRule));
			return mkuptr<StatementAssignment>(
				mkuptr<NewArray>(make_call_args(n[1])),
				make_indexing_expr(n[0])
			);
		}

		Uptr<StatementAssignment> convert_instruction_tuple_declaration_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionTupleDeclarationRule));
			return mkuptr<StatementAssignment>(
				mkuptr<NewTuple>(convert_inexplicable_t_rule(n[1])),
				make_indexing_expr(n[0])
			);
		}
		Uptr<StatementBlock> convert_statement_block_rule(const ParseNode &n);

		Uptr<Statement> convert_statement(const ParseNode &n){
			const std::type_info &rule = *n.rule;
			if (rule == typeid(rules::InstructionTypeDeclarationRule)) {
				return convert_instruction_declaration(n);
			} else if (rule == typeid(rules::InstructionPureAssignmentRule)) {
				return convert_instruction_pure_assignment_rule(n);
			} else if (rule == typeid(rules::InstructionOperatorAssignmentRule)) {
				return convert_instruction_operation_assignment_rule(n);
			} else if (rule == typeid(rules::InstructionLabelRule)) {
				return convert_instruction_label_rule(n);
			} else if (rule == typeid(rules::InstructionIfStatementRule)) {
				return convert_instruction_if_rule(n);
			} else if (rule == typeid(rules::InstructionGotoRule)){
				return convert_instruction_goto_rule(n);
			} else if (rule == typeid(rules::InstructionReturnRule)){
				return convert_instruction_return_rule(n);
			} else if (rule == typeid(rules::InstructionWhileStatementRule)) {
				return convert_instruction_while_rule(n);
			} else if (rule == typeid(rules::InstructionContinueRule)) {
				return convert_instruction_continue_rule(n);
			} else if (rule == typeid(rules::InstructionBreakRule)) {
				return convert_instruction_break_rule(n);
			} else if (rule == typeid(rules::InstructionArrayLoadRule)) {
				return convert_instruction_array_load_rule(n);
			} else if (rule == typeid(rules::InstructionArrayStoreRule)) {
				return convert_instruction_array_store_rule(n);
			} else if (rule == typeid(rules::InstructionLengthRule)) {
				return convert_instruction_length_rule(n);
			} else if (rule == typeid(rules::InstructionFunctionCallRule)){
				return convert_instruction_function_call_rule(n);
			} else if (rule == typeid(rules::InstructionFunctionCallAssignmentRule)) {
				return convert_instruction_function_call_assignment_rule(n);
			} else if (rule == typeid(rules::InstructionArrayDeclarationRule)) {
				return convert_instruction_array_declaration_rule(n);
			} else if (rule == typeid(rules::InstructionTupleDeclarationRule)) {
				return convert_instruction_tuple_declaration_rule(n);
			} else if (rule == typeid(rules::InstructionScopeRule)) {
				return convert_statement_block_rule(n);
			} else {
				std::cerr << "Cannot make Instruction from this parse node.";
				exit(1);
			}
		}

		Uptr<StatementBlock> convert_statement_block_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionScopeRule));
			Uptr<StatementBlock> newBB = mkuptr<StatementBlock>();
			for (const Uptr<ParseNode> &child : n.children) {
				newBB->add_next_statement(convert_statement(*child));
			}
			return newBB;
		}

		Uptr<LbFunction> convert_lb_function(const ParseNode &n) {
			assert(*n.rule == typeid(rules::FunctionRule));

			Uptr<LbFunction> function = mkuptr<LbFunction>(
				extract_name(n[1]),
				make_voidable_type(n[0]),
				mv(convert_statement_block_rule(n[3]))
			);

			const ParseNode &def_args_node = n[2];
			assert(*def_args_node.rule == typeid(rules::DefineArgsRule));
			for (const Uptr<ParseNode> &child : def_args_node.children) {
				const ParseNode &def_arg_node = *child;
				assert(*def_arg_node.rule == typeid(rules::DefineArgRule));
				function->add_parameter_variable(
					extract_name(def_arg_node[1]),
					make_voidable_type(def_arg_node[0])
				);
			}

			return function;
		}

		Uptr<Program> convert_program(const ParseNode &n) {
			assert(*n.rule == typeid(rules::ProgramRule));
			Uptr<Program> program = mkuptr<Program>();
			for (const Uptr<ParseNode> &child : n.children) {
				program->add_lb_function(convert_lb_function(*child));
			}
			link_std(*program);
			return program;
		}
	}
    Uptr<Lb::hir::Program> parse_file(char *fileName, Opt<std::string> parse_tree_output) {
        using EntryPointRule = pegtl::must<rules::ProgramRule>;
        if (pegtl::analyze<EntryPointRule>() != 0) {
			std::cerr << "There are problems with the grammar" << std::endl;
			exit(1);
		}
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
		Uptr<Lb::hir::Program> ptr = node_processor::convert_program((*root)[0]);
		return ptr;
    }
}