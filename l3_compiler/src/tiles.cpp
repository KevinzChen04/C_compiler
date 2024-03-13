#include "tiles.h"
#include "std_alias.h"
#include "target_arch.h"
#include "utils.h"
#include <iostream>
#include <algorithm>

namespace L3::code_gen::tiles {
	// TODO add more tiles for CISC instructions

	using namespace L3::program;

	/*
	struct MyTile {
		using Structre = DestCtr<
			MoveCtr<
				BinaryCtr<
					VariableCtr,
					AnyCtr
				>
			>
		>;
	}
	*/

	struct MatchFailError {};
	// the following are functions throwing MatchFailError used for matching
	// purposes
	template<typename NodeType>
	const NodeType &unwrap_node_type(const ComputationNode &node) {
		const NodeType *downcasted = dynamic_cast<const NodeType *>(&node);
		if (downcasted) {
			return *downcasted;
		} else {
			throw MatchFailError {};
		}
	}
	template<typename T>
	const T &unwrap_optional(const Opt<T> &opt) {
		if (opt) {
			return *opt;
		} else {
			throw MatchFailError {};
		}
	}
	void throw_unless(bool success) {
		if (!success) {
			throw MatchFailError {};
		}
	}
	void fail_match() {
		throw MatchFailError {};
	}

	// "CTR" stands for "computation tree rule", and is kind of like a pegtl
	// parsing rule but instead of matching characters it matches parts of
	// a computation tree.

	// FUTURE perhaps each CTR should just be a function template, instead of
	// a struct template with a static `match` method. This would remove one
	// level of indentation and allow us to specify function signatures in the
	// template parameters, so you get slightly more helpful error messages
	// when you write an incorrect rule.

	namespace rules {
		// "Matches" describes what kind of node will be matched by the rule.
		// "Captures" describes the elements of the resulting struct that are
		// taken from the target. this does not include any children CTRs that
		// are passed in the template parameters.

		// Matches: a NoOpCn or an atomic computation node that doens't do anything
		// i.e. has no destination or is a VariableCn
		struct NoOpCtr {
			static NoOpCtr match(const ComputationNode &target) {
				throw_unless(
					is_dynamic_type<NoOpCn, VariableCn>(target)
					|| (
						is_dynamic_type<FunctionCn, NumberCn, LabelCn>(target)
						&& !target.destination.has_value()
					)
				);
				return {};
			}
		};

		// Matches: any node
		// Captures: the node matched
		// This rule allows tiles to capture parts of the tree that they
		// don't technically match, so that they can return those parts of the
		// tree to be matched by other tiles
		struct AnyCtr {
			const ComputationNode *node;

			static AnyCtr match(const ComputationNode &target) {
				return { &target };
			}
		};

		// TODO inexplicableS and inexplicableT should probably return their own
		// variants so that it doesn't seem like Uptr<ComputationNode> is a
		// possibility

		// Matches: A computation node that can be expressed as an "T"
		// in L2 (i.e. a variable or number literal)
		// Captures: the matched node
		struct InexplicableTCtr {
			const ComputationNode *node;

			static InexplicableTCtr match(const ComputationNode &target) {
				throw_unless(target.destination.has_value() || is_dynamic_type<NumberCn>(target));
				return { &target };
			}
		};

		// Matches: A computation node that can be expressed as an "S"
		// in L2 (i.e. a variable, number literal, label, or function name)
		// Captures: the matched node
		struct InexplicableSCtr {
			const ComputationNode *node;

			static InexplicableSCtr match(const ComputationNode &target) {
				throw_unless(target.destination.has_value()
					|| is_dynamic_type<NumberCn, LabelCn, FunctionCn>(target));
				return { &target };
			}
		};

		// Matches: An ATOMIC computation node that can be expressed as an "S"
		// in L2 and is not just a variable (i.e. number literal, label, or
		// function name)
		// Captures: the matched node
		struct ConstantCtr {
			const ComputationNode *node;

			static ConstantCtr match(const ComputationNode &target) {
				throw_unless(is_dynamic_type<NumberCn, LabelCn, FunctionCn>(target));
				return { &target };
			}
		};

		// Matches: A computation node that is a number literal in L2
		// Captures: the value
		struct NumberCtr {
			int64_t value;

			static NumberCtr match(const ComputationNode &target) {
				const NumberCn &number_node = unwrap_node_type<NumberCn>(target);
				return { number_node.value };
			}
		};

		// Matches: a computation node that can be expressed as a "U" (i.e. a
		// variable or function name) or is one of the std functions in L2
		// Captures: the matched node
		struct CallableCtr {
			const ComputationNode *node;

			static CallableCtr match(const ComputationNode &target) {
				throw_unless(target.destination.has_value()
					|| is_dynamic_type<FunctionCn>(target));
				return { &target };
			}
		};

		// Matches: A computation node that can be expressed as a variable
		// in L2 and satisfies the passed-in CTR.
		// Captures: the destination variable
		template<typename NodeCtr>
		struct VariableCtr {
			Variable *var;
			NodeCtr node;

			static VariableCtr match(const ComputationNode &target) {
				throw_unless(target.destination.has_value());
				return { *target.destination, NodeCtr::match(target) };
			}
		};

		// Matches: any computation node
		// Captures: the destination variable IF IT EXISTS
		template<typename NodeCtr>
		struct MaybeVariableCtr {
			Opt<Variable *> maybe_var;
			NodeCtr node;

			static MaybeVariableCtr match(const ComputationNode &target) {
				return { target.destination, NodeCtr::match(target) };
			}
		};

		// Matches: a MoveCn
		template<typename SourceCtr>
		struct MoveCtr {
			SourceCtr source;

			static MoveCtr match(const ComputationNode &target) {
				const MoveCn &move_node = unwrap_node_type<MoveCn>(target);
				return { SourceCtr::match(*move_node.source) };
			}
		};

		// Matches: a BinaryCn, but also tries the reverse order of operands if
		// the operator can be flipped
		// Captures: the Operator used
		template<typename LhsCtr, typename RhsCtr>
		struct CommutativeBinaryCtr {
			Operator op;
			LhsCtr lhs;
			RhsCtr rhs;

			static CommutativeBinaryCtr match(const ComputationNode &target) {
				const BinaryCn &bin_node = unwrap_node_type<BinaryCn>(target);
				try {
					return {
						bin_node.op,
						LhsCtr::match(*bin_node.lhs),
						RhsCtr::match(*bin_node.rhs)
					};
				} catch (MatchFailError &e) {
					Operator flipped_op = unwrap_optional(flip_operator(bin_node.op));
					return {
						flipped_op,
						LhsCtr::match(*bin_node.rhs),
						RhsCtr::match(*bin_node.lhs)
					};
				}
			}
		};

		// Matches: a BinaryCn where the order of the operands DOES matter
		// Captures: the Operator used
		template<typename LhsCtr, typename RhsCtr>
		struct NoncommutativeBinaryCtr {
			Operator op;
			LhsCtr lhs;
			RhsCtr rhs;

			static NoncommutativeBinaryCtr match(const ComputationNode &target) {
				const BinaryCn &bin_node = unwrap_node_type<BinaryCn>(target);
				return {
					bin_node.op,
					LhsCtr::match(*bin_node.lhs),
					RhsCtr::match(*bin_node.rhs)
				};
			}
		};

		// Matches: a LoadCn
		template<typename AddressCtr>
		struct LoadCtr {
			AddressCtr address;

			static LoadCtr match(const ComputationNode &target) {
				const LoadCn &load_node = unwrap_node_type<LoadCn>(target);
				return {
					AddressCtr::match(*load_node.address)
				};
			}
		};

		// Matches: a StoreCn
		template<typename AddressCtr, typename SourceCtr>
		struct StoreCtr {
			AddressCtr address;
			SourceCtr source;

			static StoreCtr match(const ComputationNode &target) {
				const StoreCn &store_node = unwrap_node_type<StoreCn>(target);
				return {
					AddressCtr::match(*store_node.address),
					SourceCtr::match(*store_node.value)
				};
			}
		};

		// Matches: a BranchCn with no condition
		// Captures: the jmp_dest
		struct UnconditionalBranchCtr {
			BasicBlock *jmp_dest;

			static UnconditionalBranchCtr match(const ComputationNode &target) {
				const BranchCn &branch_node = unwrap_node_type<BranchCn>(target);
				throw_unless(!branch_node.condition.has_value());
				return {
					branch_node.jmp_dest
				};
			}
		};

		// Matches: a BranchCn with a condition
		// Captures: the jmp_dest
		template<typename ConditionCtr>
		struct ConditionalBranchCtr {
			BasicBlock *jmp_dest;
			ConditionCtr condition;

			static ConditionalBranchCtr match(const ComputationNode &target) {
				const BranchCn &branch_node = unwrap_node_type<BranchCn>(target);
				const Uptr<ComputationNode> &condition = unwrap_optional(branch_node.condition);
				return {
					branch_node.jmp_dest,
					ConditionCtr::match(*condition)
				};
			}
		};

		// Matches: a ReturnCn without a value
		struct ReturnVoidCtr {
			static ReturnVoidCtr match(const ComputationNode &target) {
				const ReturnCn &return_node = unwrap_node_type<ReturnCn>(target);
				throw_unless(!return_node.value.has_value());
				return {};
			}
		};

		// Matches: a ReturnCn with a value
		template<typename ValueCtr>
		struct ReturnValCtr {
			ValueCtr value;

			static ReturnValCtr match(const ComputationNode &target) {
				const ReturnCn &return_node = unwrap_node_type<ReturnCn>(target);
				const Uptr<ComputationNode> &value = unwrap_optional(return_node.value);
				return {
					ValueCtr::match(*value)
				};
			}
		};

		// Matches: a CallCn
		// Captures: a vector with pointers to all the argument ComputationNodes
		template<typename CalleeCtr>
		struct CallCtr {
			CalleeCtr callee;
			Vec<const ComputationNode *> arguments;

			static CallCtr match(const ComputationNode &target) {
				const CallCn &call_node = unwrap_node_type<CallCn>(target);

				Vec<const ComputationNode *> arguments;
				for (const Uptr<ComputationNode> &arg : call_node.arguments) {
					arguments.push_back(arg.get());
				}

				return {
					CalleeCtr::match(*call_node.callee),
					mv(arguments)
				};
			}
		};
	}

	// To be used for matching, a Tile subclass must have:
	// - member type Structure where Structure::match(const ComputationTree &) works (can throw)
	// - a constructor that takes a Structure and returns a Tile (can throw)
	// - static member int cost
	// - static member int munch

	namespace tile_patterns {
		using namespace rules;
		using L3::code_gen::target_arch::to_l2_expr;

		struct NoOp : Tile {
			using Structure = NoOpCtr;
			NoOp(Structure s) {}

			static const int munch = 0;
			static const int cost = 0;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return {};
			}
		};

		struct PureAssignment : Tile {
			Variable *dest;
			const ComputationNode *source;

			using Structure = VariableCtr<MoveCtr<InexplicableSCtr>>;
			PureAssignment(Structure s) :
				dest { s.var },
				source { s.node.source.node }
			{}

			static const int munch = 1;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {
					to_l2_expr(this->dest) + " <- " + to_l2_expr(*this->source)
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->source };
			}
		};

		struct ConstantAssignment : Tile {
			// these two fields will always refer to the same thing because of
			// the way we match twice on the same node
			Variable *dest;
			const ComputationNode *source;

			using Structure = VariableCtr<ConstantCtr>;
			ConstantAssignment(Structure s) :
				dest { s.var },
				source { s.node.node }
			{}

			static const int munch = 1;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {
					to_l2_expr(this->dest) + " <- " + to_l2_expr(*this->source, true)
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				// because the source is a constant, it is considered to have
				// already been tiled, so don't return it.
				return {};
			}
		};

		struct BinaryArithmeticAssignment : Tile {
			Variable *dest;
			Operator op;
			const ComputationNode *lhs;
			const ComputationNode *rhs;

			using Structure = VariableCtr<
				NoncommutativeBinaryCtr<
					InexplicableTCtr,
					InexplicableTCtr
				>
			>;
			BinaryArithmeticAssignment(Structure s) :
				dest { s.var },
				op { s.node.op },
				lhs { s.node.lhs.node },
				rhs { s.node.rhs.node }
			{
				throw_unless(
					this->op == Operator::plus
					|| this->op == Operator::minus
					|| this->op == Operator::times
					|| this->op == Operator::bitwise_and
					|| this->op == Operator::lshift
					|| this->op == Operator::rshift
				);
			}

			static const int munch = 1;
			static const int cost = 3;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {
					"%_ <- " + to_l2_expr(*this->lhs),
					"%_ " + program::to_string(this->op) + "= " + to_l2_expr(*this->rhs),
					to_l2_expr(this->dest) + " <- %_"
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->lhs, this->rhs };
			}
		};

		struct BinaryArithmeticAssignmentDistinct : Tile {
			Variable *dest;
			Operator op;
			const ComputationNode *lhs;
			const ComputationNode *rhs;

			using Structure = VariableCtr<
				NoncommutativeBinaryCtr<
					InexplicableTCtr,
					InexplicableTCtr
				>
			>;
			BinaryArithmeticAssignmentDistinct(Structure s) :
				dest { s.var },
				op { s.node.op },
				lhs { s.node.lhs.node },
				rhs { s.node.rhs.node }
			{
				throw_unless(
					this->op == Operator::plus
					|| this->op == Operator::minus
					|| this->op == Operator::times
					|| this->op == Operator::bitwise_and
					|| this->op == Operator::lshift
					|| this->op == Operator::rshift
				);
				throw_unless(!this->rhs->destination.has_value() || this->dest != *this->rhs->destination);
			}

			static const int munch = 1;
			static const int cost = 2;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {
					to_l2_expr(this->dest) + " <- " + to_l2_expr(*this->lhs),
					to_l2_expr(this->dest) + " " + program::to_string(this->op) + "= " + to_l2_expr(*this->rhs)
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->lhs, this->rhs };
			}
		};

		struct BinaryArithmeticAssignmentInPlace : Tile {
			Variable *dest;
			Operator op;
			const ComputationNode *lhs;
			const ComputationNode *rhs;

			using Structure = VariableCtr<
				NoncommutativeBinaryCtr<
					InexplicableTCtr,
					InexplicableTCtr
				>
			>;
			BinaryArithmeticAssignmentInPlace(Structure s) :
				dest { s.var },
				op { s.node.op },
				lhs { s.node.lhs.node },
				rhs { s.node.rhs.node }
			{
				throw_unless(
					this->op == Operator::plus
					|| this->op == Operator::minus
					|| this->op == Operator::times
					|| this->op == Operator::bitwise_and
					|| this->op == Operator::lshift
					|| this->op == Operator::rshift
				);
				bool assign_to_lhs = this->lhs->destination.has_value() && this->dest == *this->lhs->destination;
				if (!assign_to_lhs) {
					// see if we assign to rhs instead by swapping the operands
					if (
						this->rhs->destination.has_value()
						&& this->dest == *this->rhs->destination // the rhs must equal the destination
						&& !( // and the operator must not be noncommutative
							this->op == Operator::minus
							|| this->op == Operator::lshift
							|| this->op == Operator::rshift
						)
					) {
						// swap lhs and rhs in this tile
						const ComputationNode *temp = this->rhs;
						this->rhs = this->lhs;
						this->lhs = temp;
					} else {
						fail_match();
					}
				}
			}

			static const int munch = 1;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {
					to_l2_expr(this->dest) + " " + program::to_string(this->op) + "= " + to_l2_expr(*this->rhs)
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->lhs, this->rhs };
			}
		};

		struct LeaShift : Tile {
			Variable *dest;
			const ComputationNode *base;
			const ComputationNode *offset;
			int64_t shift_amt;

			using Structure = VariableCtr<
				CommutativeBinaryCtr<
					VariableCtr<AnyCtr>,
					CommutativeBinaryCtr<
						VariableCtr<AnyCtr>,
						NumberCtr
					>
				>
			>;
			LeaShift(Structure s) :
				dest { s.var },
				base { s.node.lhs.node.node },
				offset { s.node.rhs.lhs.node.node },
				shift_amt { s.node.rhs.rhs.value }
			{
				throw_unless(
					s.node.op == Operator::plus && (
						(s.node.rhs.op == Operator::lshift && (
							this->shift_amt == 0
							|| this->shift_amt == 1
							|| this->shift_amt == 2
							|| this->shift_amt == 3
						)) || (s.node.rhs.op == Operator::rshift && (
							this->shift_amt == 0
						))
					)
				);
			}

			static const int munch = 2;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				int64_t scale = 1 << this->shift_amt;
				return {
					to_l2_expr(this->dest) +
					" @ " + to_l2_expr(*this->base) +
					" " + to_l2_expr(*this->offset) +
					" " + to_l2_expr(scale)
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->base, this->offset };
			}
		};

		struct LeaMultiply : Tile {
			Variable *dest;
			const ComputationNode *base;
			const ComputationNode *offset;
			int64_t scale;

			using Structure = VariableCtr<
				CommutativeBinaryCtr<
					VariableCtr<AnyCtr>,
					CommutativeBinaryCtr<
						VariableCtr<AnyCtr>,
						NumberCtr
					>
				>
			>;
			LeaMultiply(Structure s) :
				dest { s.var },
				base { s.node.lhs.node.node },
				offset { s.node.rhs.lhs.node.node },
				scale { s.node.rhs.rhs.value }
			{
				throw_unless(
					s.node.op == Operator::plus
					&& s.node.rhs.op == Operator::times
					&& (this->scale == 1
						|| this->scale == 2
						|| this->scale == 4
						|| this->scale == 8)
				);
			}

			static const int munch = 2;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {
					to_l2_expr(this->dest) +
					" @ " + to_l2_expr(*this->base) +
					" " + to_l2_expr(*this->offset) +
					" " + to_l2_expr(this->scale)
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->base, this->offset };
			}
		};

		struct BinaryCompareAssignment : Tile {
			Variable *dest;
			Operator op;
			const ComputationNode *lhs;
			const ComputationNode *rhs;

			using Structure = VariableCtr<
				NoncommutativeBinaryCtr<
					InexplicableTCtr,
					InexplicableTCtr
				>
			>;
			BinaryCompareAssignment(Structure s) :
				dest { s.var },
				op { s.node.op },
				lhs { s.node.lhs.node },
				rhs { s.node.rhs.node }
			{
				throw_unless(
					this->op == Operator::lt
					|| this->op == Operator::le
					|| this->op == Operator::eq
					|| this->op == Operator::ge
					|| this->op == Operator::gt
				);
			}

			static const int munch = 1;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				// if we use gt or ge, mirror the operator and swap the operands
				const ComputationNode *lhs_ptr = this->lhs;
				const ComputationNode *rhs_ptr = this->rhs;
				Operator l2_op = this->op;
				switch (this->op) {
					case Operator::gt:
						l2_op = Operator::lt;
						lhs_ptr = this->rhs;
						rhs_ptr = this->lhs;
						break;
					case Operator::ge:
						l2_op = Operator::le;
						lhs_ptr = this->rhs;
						rhs_ptr = this->lhs;
						break;
					// default cause is to do nothing
				}

				return {
					to_l2_expr(this->dest) + " <- "
					+ to_l2_expr(*lhs_ptr) + " "
					+ program::to_string(l2_op) + " "
					+ to_l2_expr(*rhs_ptr)
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->lhs, this->rhs };
			}
		};

		struct BinaryCompareJump : Tile {
			BasicBlock *jmp_dest;
			Operator op;
			const ComputationNode *lhs;
			const ComputationNode *rhs;

			using Structure = ConditionalBranchCtr<
				NoncommutativeBinaryCtr<
					InexplicableTCtr,
					InexplicableTCtr
				>
			>;

			BinaryCompareJump(Structure s) :
				jmp_dest { s.jmp_dest },
				op { s.condition.op },
				lhs { s.condition.lhs.node },
				rhs { s.condition.rhs.node }
			{
				throw_unless(
					this->op == Operator::lt
					|| this->op == Operator::le
					|| this->op == Operator::eq
					|| this->op == Operator::ge
					|| this->op == Operator::gt
				);
			}

			static const int munch = 2;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				// if we use gt or ge, mirror the operator and swap the operands
				const ComputationNode *lhs_ptr = this->lhs;
				const ComputationNode *rhs_ptr = this->rhs;
				Operator l2_op = this->op;
				switch (this->op) {
					case Operator::gt:
						l2_op = Operator::lt;
						lhs_ptr = this->rhs;
						rhs_ptr = this->lhs;
						break;
					case Operator::ge:
						l2_op = Operator::le;
						lhs_ptr = this->rhs;
						rhs_ptr = this->lhs;
						break;
					// default cause is to do nothing
				}

				return {
					"cjump "
					+ to_l2_expr(*lhs_ptr) + " "
					+ program::to_string(l2_op) + " "
					+ to_l2_expr(*rhs_ptr) + " "
					+ to_l2_expr(this->jmp_dest)
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->lhs, this->rhs };
			}
		};

		struct PureLoad : Tile {
			Variable *dest;
			const ComputationNode *address;

			using Structure = VariableCtr<
				LoadCtr<
					VariableCtr<AnyCtr>
				>
			>;
			PureLoad(Structure s) :
				dest { s.var },
				address { s.node.address.node.node }
			{}

			static const int munch = 1;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {
					to_l2_expr(this->dest) + " <- mem " + to_l2_expr(*this->address) + " 0"
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->address };
			}
		};

		struct LoadWithOffset : Tile {
			Variable *dest;
			const ComputationNode *base;
			int64_t offset;

			using Structure = VariableCtr<
				LoadCtr<
					CommutativeBinaryCtr<
						VariableCtr<AnyCtr>,
						NumberCtr
					>
				>
			>;
			LoadWithOffset(Structure s) :
				dest { s.var },
				base { s.node.address.lhs.node.node },
				offset { s.node.address.rhs.value }
			{
				throw_unless(this->offset % 8 == 0);
				switch (s.node.address.op) {
					case Operator::plus:
						break;
					case Operator::minus:
						this->offset *= -1;
						break;
					default:
						fail_match();
				}
			}

			static const int munch = 2;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {
					to_l2_expr(this->dest) + " <- mem " + to_l2_expr(*this->base) + " " + to_l2_expr(this->offset)
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->base };
			}
		};

		struct PureStore : Tile {
			const ComputationNode *address;
			const ComputationNode *source;

			using Structure = StoreCtr<
				VariableCtr<AnyCtr>,
				InexplicableSCtr
			>;
			PureStore(Structure s) :
				address { s.address.node.node },
				source { s.source.node }
			{}

			static const int munch = 1;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {
					"mem " + to_l2_expr(*this->address) + " 0 <- " + to_l2_expr(*this->source)
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->address, this->source };
			}
		};

		struct StoreWithOffset : Tile {
			const ComputationNode *base;
			int64_t offset;
			const ComputationNode *source;

			using Structure = StoreCtr<
				CommutativeBinaryCtr<
					VariableCtr<AnyCtr>,
					NumberCtr
				>,
				InexplicableSCtr
			>;
			StoreWithOffset(Structure s) :
				base { s.address.lhs.node.node },
				offset { s.address.rhs.value },
				source { s.source.node }
			{
				throw_unless(this->offset % 8 == 0);
				switch (s.address.op) {
					case Operator::plus:
						break;
					case Operator::minus:
						this->offset *= -1;
						break;
					default:
						fail_match();
				}
			}

			static const int munch = 2;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {
					"mem " + to_l2_expr(*this->base) + " " + to_l2_expr(this->offset) + " <- " + to_l2_expr(*this->source)
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->base, this->source };
			}
		};

		struct GotoStatement : Tile {
			BasicBlock *jmp_dest;

			using Structure = UnconditionalBranchCtr;
			GotoStatement(Structure s) :
				jmp_dest { s.jmp_dest }
			{}

			static const int munch = 1;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				return { "goto " + to_l2_expr(this->jmp_dest) };
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return {};
			}
		};

		struct PureConditionalBranch : Tile {
			BasicBlock *jmp_dest;
			const ComputationNode *condition;

			using Structure = ConditionalBranchCtr<
				InexplicableTCtr
			>;
			PureConditionalBranch(Structure s) :
				jmp_dest { s.jmp_dest },
				condition { s.condition.node }
			{}

			static const int munch = 1;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {
					"cjump " + to_l2_expr(*this->condition) + " = 1 " + to_l2_expr(this->jmp_dest)
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->condition };
			}
		};

		struct ReturnVoid : Tile {
			using Structure = ReturnVoidCtr;
			ReturnVoid(Structure s) {}

			static const int munch = 1;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				return { "return" };
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return {};
			}
		};

		struct ReturnVal : Tile {
			const ComputationNode *value;

			// We use InexplicableSCtr here because we care about what's allowed
			// in the L2 grammar, not L3's, and L2 allows use to put an "S" into
			// rax.
			using Structure = ReturnValCtr<
				InexplicableSCtr
			>;
			ReturnVal(Structure s) :
				value { s.value.node }
			{}

			static const int munch = 1;
			static const int cost = 2;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {
					"rax <- " + to_l2_expr(*this->value),
					"return"
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->value };
			}
		};

		struct Call : Tile {
			Opt<Variable *> maybe_dest;
			const ComputationNode *callee;
			Vec<const ComputationNode *> arguments;

			using Structure = MaybeVariableCtr<
				CallCtr<CallableCtr>
			>;
			Call(Structure s) :
				maybe_dest { s.maybe_var },
				callee { s.node.callee.node },
				arguments { mv(s.node.arguments) }
			{}

			static const int munch = 1;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				static int num_call_return_labels = 0; // the number of call-return labels we've seen so far
				static const std::string call_return_label_prefix = ":callret";

				Vec<std::string> result;

				// add the instructions preparing the arguments
				for (int i = 0; i < this->arguments.size(); ++i) {
					result.push_back(target_arch::get_argument_prepping_instruction(
						to_l2_expr(*this->arguments[i]),
						i
					));
				}

				// add the actual call instruction
				result.push_back("call " + to_l2_expr(*this->callee) + " " + std::to_string(this->arguments.size()));

				// wrap in return label if the function is not an std function
				const FunctionCn *maybe_fun_cn_ptr = dynamic_cast<const FunctionCn *>(this->callee);
				bool is_std = maybe_fun_cn_ptr && dynamic_cast<const ExternalFunction *>(maybe_fun_cn_ptr->function);
				if (!is_std) {
					std::string return_label = call_return_label_prefix + std::to_string(num_call_return_labels);
					num_call_return_labels += 1;

					result.insert(
						result.end() - 1, // insert before the call instruction
						"mem rsp -8 <- " + return_label
					);
					result.push_back(mv(return_label));
				}

				// store the return value if the call returns something
				if (this->maybe_dest) {
					result.push_back(to_l2_expr(*this->maybe_dest) + " <- rax");
				}

				return result;
			}
			virtual Vec<const ComputationNode *> get_unmatched() const override {
				Vec<const ComputationNode *> result = this->arguments;
				result.push_back(this->callee);
				return result;
			}
		};
	}

	namespace tp = tile_patterns;

	template<typename TP>
	Opt<Uptr<TP>> attempt_tile_match(const ComputationNode &target) {
		try {
			return mkuptr<TP>(TP::Structure::match(target));
		} catch (MatchFailError &e) {
			return {};
		}
	}

	template<typename TP>
	void attempt_tile_match(const ComputationNode &tree, Opt<Uptr<Tile>> &out, int &best_munch, int &best_cost) {
		if (TP::munch > best_munch || (TP::munch == best_munch && TP::cost <= best_cost)) {
			Opt<Uptr<TP>> result = attempt_tile_match<TP>(tree);
			if (result) {
				out = mv(*result);
				best_munch = TP::munch;
				best_cost = TP::cost;
			}
		}
	}
	template<typename... TPs>
	void attempt_tile_matches(const ComputationNode &tree, Opt<Uptr<Tile>> &out, int &best_munch, int &best_cost) {
		(attempt_tile_match<TPs>(tree, out, best_munch, best_cost), ...);
	}

	Opt<Uptr<Tile>> find_best_tile(const ComputationNode &tree) {
		Opt<Uptr<Tile>> best_match;
		int best_munch = 0;
		int best_cost = 0;
		attempt_tile_matches<
			tp::NoOp,
			tp::PureAssignment,
			tp::ConstantAssignment,
			tp::BinaryArithmeticAssignment,
			tp::BinaryArithmeticAssignmentDistinct,
			tp::BinaryArithmeticAssignmentInPlace,
			tp::LeaMultiply,
			tp::LeaShift,
			tp::BinaryCompareAssignment,
			tp::BinaryCompareJump,
			tp::PureLoad,
			tp::LoadWithOffset,
			tp::PureStore,
			tp::StoreWithOffset,
			tp::GotoStatement,
			tp::PureConditionalBranch,
			tp::ReturnVoid,
			tp::ReturnVal,
			tp::Call
		>(tree, best_match, best_munch, best_cost);
		return best_match;
	}

	Vec<Uptr<Tile>> tile_trees(const Vec<ComputationTreeBox> &tree_boxes) {
		// build a stack to hold pointers to the currently untiled trees.
		// the top of the stack is for trees that must be executed later
		Vec<Uptr<Tile>> tiles; // stored in REVERSE order of execution
		Vec<const ComputationNode *> untiled_trees;
		for (const ComputationTreeBox &tree_box : tree_boxes) {
			untiled_trees.push_back(tree_box.get_tree().get());
		}
		while (!untiled_trees.empty()) {
			// try to tile the top tree
			const ComputationNode *top_tree = untiled_trees.back();
			untiled_trees.pop_back();
			Opt<Uptr<Tile>> best_match = find_best_tile(*top_tree);
			if (!best_match) {
				std::cerr << "Couldn't find a tile for this tree! " << program::to_string(*top_tree) << "\n";
				// TODO reject if you can't find a tile
				continue;
				// exit(1);
			}
			for (const ComputationNode *unmatched: (*best_match)->get_unmatched()) {
				untiled_trees.push_back(unmatched);
			}
			tiles.push_back(mv(*best_match));
		}
		std::reverse(tiles.begin(), tiles.end()); // now stored in FORWARD order
		return tiles;
	}
}
