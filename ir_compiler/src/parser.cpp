
#include "parser.h"

#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/analyze.hpp>
#include <tao/pegtl/contrib/raw_string.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>
#include <tao/pegtl/contrib/parse_tree_to_dot.hpp>

namespace pegtl = TAO_PEGTL_NAMESPACE;

namespace IR::parser {

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

		struct VariableRule :
			seq<one<'%'>, NameRule>
		{};

		struct LabelRule :
			seq<one<':'>, NameRule>
		{};

		struct IRFunctionNameRule : 
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

		struct OperatorRule :
			sor<
				TAO_PEGTL_STRING("<<"),
				TAO_PEGTL_STRING(">>"),
				TAO_PEGTL_STRING("<="),
				TAO_PEGTL_STRING(">="),
				TAO_PEGTL_STRING("="),
				TAO_PEGTL_STRING("+"),
				TAO_PEGTL_STRING("-"),
				TAO_PEGTL_STRING("*"),
				TAO_PEGTL_STRING("&"),
				TAO_PEGTL_STRING("<"),
				TAO_PEGTL_STRING(">")
			>
		{};

		struct ArrowRule : TAO_PEGTL_STRING("\x3c-") {};
		
		struct InexplicableURule :
			sor<
				VariableRule,
				IRFunctionNameRule
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
				IRFunctionNameRule
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

		struct DefineArgRule :
			interleaved<
				SpacesRule,
				TypeRule,
				VariableRule
			>
		{};

		struct DefineArgsRule :
			star<
				interleaved<
					SpacesRule,
					DefineArgRule,
					opt<one<','>>
				>,
				SpacesRule
			>
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

		struct InstructionTypeDeclaratioRule :
			interleaved<
				SpacesRule,
				VoidableTypeRule,
				VariableRule 
			>
		{};

		struct InstructionPureAssignmentRule :
			interleaved<
				SpacesRule,
				VariableRule,
				ArrowRule,
				InexplicableSRule 
			>
		{};

		struct InstructionOperatorAssignmentRule :
			interleaved<
				SpacesRule,
				VariableRule,
				ArrowRule,
				InexplicableTRule,
				OperatorRule,
				InexplicableTRule
			>
		{};
		struct ArrayAccess :
			plus<
				interleaved<
					SpacesRule,
					one<'['>,
					InexplicableTRule,
					one<']'>
				>
			>
		{};
		struct InstructionArrayLoadRule :
			interleaved<
				SpacesRule,
				VariableRule,
				ArrowRule,
				VariableRule,
				ArrayAccess
			>
		{};

		struct InstructionArrayStoreRule : 
			interleaved<
				SpacesRule,
				VariableRule,
				ArrayAccess,
				ArrowRule,
				InexplicableSRule
			>
		{};

		struct InstructionLengthArrayRule :
			interleaved<
				SpacesRule,
				VariableRule,
				ArrowRule,
				TAO_PEGTL_STRING("length"),
				VariableRule,
				InexplicableTRule
			>
		{};

		struct InstructionLengthTupleRule :
			interleaved<
				SpacesRule,
				VariableRule,
				ArrowRule,
				TAO_PEGTL_STRING("length"),
				VariableRule
			>
		{};

		struct InstructionFunctionCallRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("call"),
				CalleeRule,
				one<'('>,
				ArgsRule,
				one<')'>
			>
		{};

		struct InstructionFunctionCallValRule :
			interleaved<
				SpacesRule,
				VariableRule,
				ArrowRule,
				TAO_PEGTL_STRING("call"),
				CalleeRule,
				one<'('>,
				ArgsRule,
				one<')'>
			>
		{};

		struct InstructionArrayDeclarationRule :
			interleaved<
				SpacesRule,
				VariableRule,
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
				VariableRule,
				ArrowRule,
				TAO_PEGTL_STRING("new"),
				TAO_PEGTL_STRING("Tuple"),
				one<'('>,
				InexplicableTRule,
				one<')'>
			>
		{};

		struct InstructionRule : 
			sor<
				InstructionTypeDeclaratioRule,
				InstructionOperatorAssignmentRule,
				InstructionLengthArrayRule,
				InstructionArrayLoadRule,
				InstructionArrayStoreRule,
				InstructionFunctionCallValRule,
				InstructionFunctionCallRule,
				InstructionLengthTupleRule,
				InstructionArrayDeclarationRule,
				InstructionTupleDeclarationRule,
				InstructionPureAssignmentRule
			>
		{};

		struct InstructionsRule :
			star<
				seq<
					LineSeparatorsWithCommentsRule,
					bol,
					SpacesRule,
					InstructionRule,
					LineSeparatorsWithCommentsRule
				>
			>
		{};

		struct TerminatorBranchOneRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("br"),
				LabelRule
			>
		{};

		struct TerminatorBranchTwoRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("br"),
				InexplicableTRule,
				LabelRule,
				LabelRule
			>
		{};

		struct TerminatorReturnVoidRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("return")
			>
		{};

		struct TerminatorReturnVarRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("return"),
				InexplicableTRule
			>
		{};

		struct TerminatorRule :
			sor<
				TerminatorReturnVarRule,
				TerminatorReturnVoidRule,
				TerminatorBranchOneRule,
				TerminatorBranchTwoRule
			>
		{};

		struct BasicBlockRule :
			interleaved<
				LineSeparatorsWithCommentsRule,
				SpacesRule,
				LabelRule,
				InstructionsRule,
				SpacesRule,
				TerminatorRule
			>
		{};

		struct BasicBlocksRule :
			plus<
				seq<
					LineSeparatorsWithCommentsRule,
					bol,
					SpacesRule,
					BasicBlockRule,
					LineSeparatorsWithCommentsRule
				>
			>
		{};

		struct FunctionRule : 
			seq<
				interleaved<
					SpacesOrNewLines,
					TAO_PEGTL_STRING("define"),
					VoidableTypeRule,
					IRFunctionNameRule,
					one<'('>,
					DefineArgsRule,
					one<')'>,
					one<'{'>
				>,
				BasicBlocksRule,
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
				IRFunctionNameRule,
				NumberRule,
				OperatorRule,
				ArgsRule,
				DefineArgRule,
				TypeRule,
				VoidableTypeRule,
				DefineArgsRule,
				ArrayAccess,
				StdFunctionNameRule,
				InstructionTypeDeclaratioRule,
				InstructionOperatorAssignmentRule,
				InstructionLengthArrayRule,
				InstructionArrayLoadRule,
				InstructionArrayStoreRule,
				InstructionFunctionCallValRule,
				InstructionFunctionCallRule,
				InstructionLengthTupleRule,
				InstructionArrayDeclarationRule,
				InstructionTupleDeclarationRule,
				InstructionPureAssignmentRule,
				InstructionsRule,
				TerminatorRule,
				TerminatorBranchOneRule,
				TerminatorBranchTwoRule,
				TerminatorReturnVarRule,
				TerminatorReturnVoidRule,
				BasicBlockRule,
				BasicBlocksRule,
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
	
	namespace node_processor{
		using namespace IR::program;

		std::string convert_name_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::NameRule));
			return std::string(n.string_view());
		}
		Type convert_type_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::TypeRule));
			return Type(std::string(n.string_view()));
		}
		Type convert_voidable_type_rule(const ParseNode &n) {
			assert(*n.rule == typeid(rules::VoidableTypeRule));
			return Type(std::string(n.string_view()));
		}
		Uptr<ItemRef<Variable>> convert_variable_ref(const ParseNode &n) {
			assert(*n.rule == typeid(rules::VariableRule));
			return mkuptr<ItemRef<Variable>>(convert_name_rule(n[0]));
		}
		Uptr<ItemRef<BasicBlock>> convert_basic_block_ref(const ParseNode &n) {
			assert(*n.rule == typeid(rules::LabelRule));
			return mkuptr<ItemRef<BasicBlock>>(std::string(convert_name_rule(n[0])));
		}
		Uptr<ItemRef<IRFunction>> convert_ir_function_ref(const ParseNode &n) {
			assert(*n.rule == typeid(rules::IRFunctionNameRule));
			return mkuptr<ItemRef<IRFunction>>(std::string(convert_name_rule(n[0])));
		}
		Uptr<ItemRef<ExternalFunction>> convert_external_function_ref(const ParseNode &n) {
			assert(*n.rule == typeid(rules::StdFunctionNameRule));
			return mkuptr<ItemRef<ExternalFunction>>(std::string(n.string_view()));
		}
		Uptr<NumberLiteral> convert_number_literal(const ParseNode &n) {
			assert(*n.rule == typeid(rules::NumberRule));
			return mkuptr<NumberLiteral>(std::stoll(std::string(n.string_view())));
		}
		Uptr<Expr> convert_expr(const ParseNode &n) {
			const std::type_info &rule = *n.rule;
			if (rule == typeid(rules::VariableRule)) {
				return convert_variable_ref(n);
			} else if (rule == typeid(rules::LabelRule)) {
				return convert_basic_block_ref(n);
			} else if (rule == typeid(rules::IRFunctionNameRule)) {
				return convert_ir_function_ref(n);
			} else if (rule == typeid(rules::StdFunctionNameRule)) {
				return convert_external_function_ref(n);
			} else if (rule == typeid(rules::NumberRule)) {
				return convert_number_literal(n);
			} else {
				std::cerr << "Cannot make Expr from this parse node of type " << n.type << "\n";
				exit(1);
			}
		}
		Vec<Uptr<Expr>> convert_array_access(const ParseNode &n){
			assert(*n.rule == typeid(rules::ArrayAccess));
			Vec<Uptr<Expr>> sol;
			for (const Uptr<ParseNode> &child: n.children){
				sol.push_back(convert_expr(*child));
			}
			return sol;
		}
		Vec<Uptr<Expr>> convert_args(const ParseNode &n){
			assert(*n.rule == typeid(rules::ArgsRule));
			Vec<Uptr<Expr>> sol;
			for (const Uptr<ParseNode> &child: n.children){
				sol.push_back(convert_expr(*child));
			}
			return sol;
		}
		Uptr<Instruction> convert_instruction_var_declaration(const ParseNode &n){
			assert(*n.rule == typeid(rules::InstructionTypeDeclaratioRule));
			return mkuptr<InstructionDeclaration>(
				mkuptr<Variable>(
					convert_name_rule(n[1][0]),
					convert_type_rule(n[0][0])
				)
			);
		}
		Uptr<Instruction> convert_instruction_array_load(const ParseNode &n){
			assert(*n.rule == typeid(rules::InstructionArrayLoadRule));
			return mkuptr<InstructionLoad>(
				convert_variable_ref(n[0]),
				mkuptr<MemoryLocation>(
					convert_variable_ref(n[1]),
					convert_array_access(n[2])
				)
			);
		}
		Uptr<Instruction> convert_instruction_array_store(const ParseNode &n){
			assert(*n.rule == typeid(rules::InstructionArrayStoreRule));
			return mkuptr<InstructionStore>(
				mkuptr<MemoryLocation>(
					convert_variable_ref(n[0]),
					convert_array_access(n[1])
				),
				convert_expr(n[2])
			);
		}
		Uptr<Instruction> convert_instruction_function_call(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionFunctionCallRule));
			return mkuptr<InstructionAssignment>(
				mkuptr<FunctionCall>(
					convert_expr(n[0]),
					convert_args(n[1])
				)
			);
		}
		Uptr<Instruction> convert_instruction_function_val_call(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionFunctionCallValRule));
			return mkuptr<InstructionAssignment>(
				convert_variable_ref(n[0]),
				mkuptr<FunctionCall>(
					convert_expr(n[1]),
					convert_args(n[2])
				)
			);
		}
		Uptr<Instruction> convert_instruction_length_array(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionLengthArrayRule));
			return mkuptr<InstructionLength>(
				convert_variable_ref(n[0]),
				mkuptr<Length>(
					convert_variable_ref(n[1]),
					convert_number_literal(n[2]).get()->get_value()
				)
			);
		}
		Uptr<Instruction> convert_instruction_length_tuple(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionLengthTupleRule));
			return mkuptr<InstructionLength>(
				convert_variable_ref(n[0]),
				mkuptr<Length>(
					convert_variable_ref(n[1])
				)
			);
		}
		Uptr<Instruction> convert_instruction_operator_assignment(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionOperatorAssignmentRule));
			return mkuptr<InstructionAssignment>(
				convert_variable_ref(n[0]),
				mkuptr<BinaryOperation>(
					convert_expr(n[1]),
					convert_expr(n[3]),
					str_to_op(std::string(n[2].string_view()))
				)
			);
		}
		Uptr<Instruction> convert_instruction_pure_assignment(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionPureAssignmentRule));
			return mkuptr<InstructionAssignment>(
				convert_variable_ref(n[0]),
				convert_expr(n[1])
			);
		}
		Uptr<Instruction> convert_instruction_array_declaration(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionArrayDeclarationRule));
			return mkuptr<InstructionInitializeArray>(
				convert_variable_ref(n[0]),
				mkuptr<ArrayDeclaration>(
					convert_args(n[1])
				)
			);
		}
		Uptr<Instruction> convert_instruction_tuple_declaration(const ParseNode &n) {
			assert(*n.rule == typeid(rules::InstructionTupleDeclarationRule));
			Vec<Uptr<Expr>> sol;
			sol.push_back(mv(convert_expr(n[1])));
			return mkuptr<InstructionInitializeArray>(
				convert_variable_ref(n[0]),
				mkuptr<ArrayDeclaration>(
					mv(sol)
				)
			);
		}
		Uptr<Instruction> convert_instruction(const ParseNode &n){
			const std::type_info &rule = *n.rule;
			if (rule == typeid(rules::InstructionArrayDeclarationRule)) {
				return convert_instruction_array_declaration(n);
			} else if (rule == typeid(rules::InstructionArrayLoadRule)) {
				return convert_instruction_array_load(n);
			} else if (rule == typeid(rules::InstructionArrayStoreRule)) {
				return convert_instruction_array_store(n);
			} else if (rule == typeid(rules::InstructionFunctionCallRule)) {
				return convert_instruction_function_call(n);
			} else if (rule == typeid(rules::InstructionFunctionCallValRule)) {
				return convert_instruction_function_val_call(n);
			} else if (rule == typeid(rules::InstructionLengthArrayRule)) {
				return convert_instruction_length_array(n);
			} else if (rule == typeid(rules::InstructionLengthTupleRule)) {
				return convert_instruction_length_tuple(n);
			} else if (rule == typeid(rules::InstructionOperatorAssignmentRule)) {
				return convert_instruction_operator_assignment(n);
			} else if (rule == typeid(rules::InstructionPureAssignmentRule)) {
				return convert_instruction_pure_assignment(n);
			} else if (rule == typeid(rules::InstructionTypeDeclaratioRule)) {
				return convert_instruction_var_declaration(n);
			} else if (rule == typeid(rules::InstructionTupleDeclarationRule)) {
				return convert_instruction_tuple_declaration(n);
			} else{
				std::cerr << "unknown instruction: " << std::string(n.string_view()) << std::endl;
				exit(-1);
			}
		}

		Uptr<Terminator> convert_terminator_branch_one(const ParseNode &n) {
			assert(*n.rule == typeid(rules::TerminatorBranchOneRule));
			return mkuptr<TerminatorBranchOne>(convert_basic_block_ref(n[0]));
		}
		Uptr<Terminator> convert_terminator_branch_two(const ParseNode &n) {
			assert(*n.rule == typeid(rules::TerminatorBranchTwoRule));
			return mkuptr<TerminatorBranchTwo>(
				convert_expr(n[0]),
				convert_basic_block_ref(n[1]),
				convert_basic_block_ref(n[2])
			);
		}
		Uptr<Terminator> convert_terminator_return_void(const ParseNode &n) {
			assert(*n.rule == typeid(rules::TerminatorReturnVoidRule));
			return mkuptr<TerminatorReturnVoid>();
		}
		Uptr<Terminator> convert_terminator_return_var(const ParseNode &n) {
			assert(*n.rule == typeid(rules::TerminatorReturnVarRule));
			return mkuptr<TerminatorReturnVar>(convert_expr(n[0]));
		}
		Uptr<Terminator> convert_terminator(const ParseNode &n){
			const std::type_info &rule = *n[0].rule;
			if (rule == typeid(rules::TerminatorBranchOneRule)) {
				return convert_terminator_branch_one(n[0]);
			} else if (rule == typeid(rules::TerminatorBranchTwoRule)) {
				return convert_terminator_branch_two(n[0]);
			} else if (rule == typeid(rules::TerminatorReturnVoidRule)) {
				return convert_terminator_return_void(n[0]);
			} else if (rule == typeid(rules::TerminatorReturnVarRule)) {
				return convert_terminator_return_var(n[0]);
			} else {
				std::cerr << "Not a valid terminator: " << std::string(n.string_view()) << std::endl;
				exit(-1);
			}
		}
		Uptr<IRFunction> convert_ir_function(const ParseNode& n) {
			assert(*n.rule == typeid(rules::FunctionRule));
			IRFunction::Builder f_builder;

			// add function name
			f_builder.add_name(std::string(convert_name_rule(n[1][0])));

			// add function return type
			f_builder.add_ret_type(convert_voidable_type_rule(n[0]));
			
			// add function parameters
			const ParseNode &def_args = n[2];
			assert(*def_args.rule == typeid(rules::DefineArgsRule));
			for (const Uptr<ParseNode> &def_arg : def_args.children) {
				Type t = convert_type_rule((*def_arg)[0]);
				std::string var_name = convert_name_rule((*def_arg)[1][0]);
				f_builder.add_parameter(
					convert_type_rule((*def_arg)[0]),
					convert_name_rule((*def_arg)[1][0])
				);
			}
			// add BasicBlocks
			const ParseNode &basic_blocks = n[3];
			assert(*basic_blocks.rule == typeid(rules::BasicBlocksRule));
			for (const Uptr<ParseNode> &bb : basic_blocks.children) {
				assert(*(*bb).rule == typeid(rules::BasicBlockRule));
				BasicBlock::Builder b_builder;
				b_builder.add_name(convert_name_rule((*bb)[0][0]));
				b_builder.add_terminator(convert_terminator((*bb)[2]), f_builder.get_scope());
				const ParseNode& inst_nodes = (*bb)[1];
				assert(*inst_nodes.rule == typeid(rules::InstructionsRule));
				for (const Uptr<ParseNode> &inst : inst_nodes.children) {
					b_builder.add_instruction(convert_instruction((*inst)), f_builder.get_scope());
				}
				f_builder.add_block(mv(b_builder.get_result()));
			}
			return f_builder.get_result();
		}
		
		Uptr<Program> convert_program(const ParseNode &n) {
			assert(*n.rule == typeid(rules::ProgramRule));
			Program::Builder p_builder;
			for (const Uptr<ParseNode> &child : n.children) {
				auto function = convert_ir_function(*child);
				p_builder.add_ir_function(mv(function));
			}
			return p_builder.get_result();
		}
	}

	Uptr<IR::program::Program> parse_input(char *fileName, Opt<std::string> parse_tree_output) {
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
		Uptr<IR::program::Program> ptr = node_processor::convert_program((*root)[0]);
		// std::cout << ptr->to_string() << std::endl;

		return ptr;
	}
}