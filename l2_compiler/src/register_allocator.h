#pragma once
#include "register_allocator.h"
#include "interference_graph.h"
#include "program.h"
#include "spiller.h"

namespace L2::program::analyze {
	std::vector<const Register *> create_register_color_table(RegisterScope &register_scope);

	using RegAllocMap = std::map<const Variable *, const Register *>;

	int get_next_prefix(L2Function &l2_function, std::string prefix);

	// Attempts to do register allocation with the function. If we get stuck,
	// then go back spill all variables, even those that were spilled before.
	RegAllocMap allocate_and_spill_with_backup(L2Function &l2_functions);

	// returns a mapping from Variable *'s to Register *'s, or none if there
	// was an error allocating registers. If there was an error, the user should
	// call allocate_and_spill_all on a backup to get a guaranteed solution
	std::optional<RegAllocMap> allocate_and_spill(L2Function &l2_function, program::spiller::Spiller &spill_man);

	RegAllocMap allocate_and_spill_all(L2Function &l2_function, program::spiller::Spiller &spill_man);
}