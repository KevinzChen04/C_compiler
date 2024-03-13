#include "analyze_trees.h"
#include "std_alias.h"
#include <algorithm>

namespace L3::program {
	using namespace std_alias;

	void BasicBlock::generate_computation_trees() {
		// generate the computation trees
		for (const Uptr<Instruction> &inst : this->raw_instructions) {
			this->tree_boxes.emplace_back(*inst);
		}

		// generate the gen and kill set
		// the algorithm starts at the end of the block
		VarLiveness &l = this->var_liveness;
		for (auto it = this->tree_boxes.rbegin(); it != this->tree_boxes.rend(); ++it) {
			const Opt<Variable *> &var_written = it->get_var_written();
			if (var_written) {
				l.kill_set.insert(*var_written);
				l.gen_set.erase(*var_written);
			}
			l.gen_set += it->get_variables_read();
		}

		// set the initial value of the in and out sets to satisfy the liveness equations
		l.in_set = l.gen_set;
		l.out_set = {}; // should already be empty but just in case
	}
	bool BasicBlock::update_in_out_sets() {
		VarLiveness &l = this->var_liveness;
		bool sets_changed = false;

		// out[i] = UNION (s in successors(i)) {in[s]}
		Set<Variable *> new_out_set;
		for (BasicBlock *succ : this->succ_blocks) {
			new_out_set += succ->var_liveness.in_set;
		}
		if (l.out_set != new_out_set) {
			sets_changed = true;
			l.out_set = mv(new_out_set);
		}

		// in[i] = gen[i] UNION (out[i] MINUS kill[i])
		// Everything currently in in[i] is either there because it was
		// in gen[i] or because it's in out[i]. out[i] is the only thing
		// that might have changed. Assuming that this equation is
		// upheld in the previous iteration, these operations will
		// satisfy the formula again.
		// - remove all existing elements that are not in out[i] nor gen[i]
		// - add all elements that are in out[i] but not kill[i]
		Set<Variable *> new_in_set;
		for (Variable *var : l.in_set) {
			if (l.out_set.find(var) != l.out_set.end()
				|| l.gen_set.find(var) != l.gen_set.end())
			{
				new_in_set.insert(var);
			}
		}
		for (Variable *var : l.out_set) {
			if (l.kill_set.find(var) == l.kill_set.end()) {
				new_in_set.insert(var);
			}
		}
		if (l.in_set != new_in_set) {
			sets_changed = true;
			l.in_set = mv(new_in_set);
		}

		return sets_changed;
	}
	using Iter = Vec<ComputationTreeBox>::reverse_iterator;
	// helper function; attempts to merge the ComputationTreeBox at the
	// child_iter and modifies the passed-in data structures to reflect that the
	// specified tree has been encountered.
	// returns the new location of the child; either the same if merge did not
	// qualify, otherwise the location of the parent that it merged into.
	Iter attempt_merge(
		Iter child_iter,
		Map<Variable *, Opt<Iter>> &alive_until,
		Map<Variable *, Iter> &earliest_write,
		Opt<Iter> &earliest_store
	) {
		Iter result_iter = child_iter;

		// attempt to merge on the variable that the tree writes to
		const Uptr<ComputationNode> &tree = child_iter->get_tree();
		if (!tree->destination) { return result_iter; }
		Variable *merge_var = *tree->destination;

		// the variable must still be alive after this write
		auto alive_until_it = alive_until.find(merge_var);
		if (alive_until_it != alive_until.end()) {
			// the variable must be alive up until a tree within the same basic
			// block (i.e. not until the end of the block)
			if (alive_until_it->second.has_value()) {
				Iter parent_iter = *alive_until_it->second;

				// there must be no instructions between the child and parent
				// that write to the child's read variables
				bool has_conflict = false;
				for (Variable *read_var : child_iter->get_variables_read()) {
					if (auto earliest_write_it = earliest_write.find(read_var);
						earliest_write_it != earliest_write.end()
						&& earliest_write_it->second > parent_iter)
					{
						has_conflict = true;
						break;
					}
				}
				if (!has_conflict) {
					// if the child is a load, there must be no stores between
					// the child and parent
					if (!child_iter->get_has_load()
						|| !earliest_store
						|| *earliest_store <= parent_iter)
					{
						// finally, we know it's okay to merge
						bool success = parent_iter->merge(*child_iter);
						if (success) {
							result_iter = parent_iter;
						}
						// TODO update earliest_write when a merge happens
					}
				}
			}

			// writing to merge_var kills its lifetime
			alive_until.erase(alive_until_it);
		}

		// no matter what, prevent merge_var from being used in another merge
		// until another tree reads from it
		earliest_write.insert_or_assign(merge_var, result_iter);

		return result_iter;
	}
	void BasicBlock::merge_trees() {
		// This map stores all the variables alive at the current moment of
		// iteration, and maps them to a possible T1 merge candidate (that is, a
		// tree that uses that variable; this is the earliest tree seen so far
		// in which the variable is used).
		// - Maps to None (empty optional) if there are no merge
		// candiates, such as if the Variable is alive until the end of the
		// basic block, or if multiple instructions depend on that variable
		// - Entry does not exist if the variable is not alive
		Map<Variable *, Opt<Iter>> alive_until;
		for (Variable *var : this->var_liveness.out_set) {
			alive_until.insert({ var, Opt<Iter>() });
		}

		// Maps a variable to its earliest write seen so far within this basic block
		Map<Variable *, Iter> earliest_write;

		// Stores the earliest store tree seen so far within this basic block
		Opt<Iter> earliest_store;

		for (Iter it = this->tree_boxes.rbegin(); it != this->tree_boxes.rend(); ++it) {
			if (!it->has_value()) {
				std::cerr << "should not have encountered an empty tree box\n";
				exit(1);
			}

			Iter new_it = attempt_merge(it, alive_until, earliest_write, earliest_store);

			if (new_it->get_has_store()) {
				earliest_store = it;
			}

			// add the current tree as a merge candidate for the variables it reads
			for (Variable *var : new_it->get_variables_read()) {
				auto alive_until_it = alive_until.find(var);
				if (alive_until_it == alive_until.end()) {
					// the variable is dead after this, so add this instruction
					// as as merge candidate
					alive_until.insert({ var, new_it });
				} else /* if (alive_until_it->second != new_it) */ { // including this condition allows the merged trees to become bigger, but it makes it harder to figure out which order the tiles go since now parts of the tree can interfere with each other (if we consider the trees to be perfect pipelines of data flow, then the resulting trees are still correct)
					// the variable is alive after this tree so no one can EVER
					// be a merge candidate until the variable is written to
					alive_until_it->second = Opt<Iter>();
				}
			}
		}

		// remove all the null boxes
		auto remove_begin = std::remove_if(
			this->tree_boxes.begin(),
			this->tree_boxes.end(),
			[](const ComputationTreeBox &box) { return !box.has_value(); }
		);
		this->tree_boxes.erase(remove_begin, this->tree_boxes.end());
	}
}

namespace L3::program::analyze {
	using namespace std_alias;

	void generate_data_flow(L3Function &l3_function) {
		Vec<Uptr<BasicBlock>> &basic_blocks = l3_function.get_blocks();

		// generate computation trees for this block
		for (Uptr<BasicBlock> &block : basic_blocks) {
			block->generate_computation_trees();
		}

		// update the in and out sets for all the blocks until a fixed point
		// is reached
		bool sets_changed;
		do {
			sets_changed = false;
			for (Uptr<BasicBlock> &block : basic_blocks) {
				if (block->update_in_out_sets()) {
					sets_changed = true;
				}
			}
		} while (sets_changed);
	}

	// basically take the completed program and generate computation trees
	// for each instruction, then update all the basic blocks to have proper
	// in and out sets
	void generate_data_flow(Program &program) {
		for (Uptr<L3Function> &l3_function : program.get_l3_functions()) {
			generate_data_flow(*l3_function);
		}
	}

	void merge_trees(Program &program) {
		for (Uptr<L3Function> &l3_function : program.get_l3_functions()) {
			for (Uptr<BasicBlock> &basic_block : l3_function->get_blocks()) {
				basic_block->merge_trees();
			}
		}
	}
}