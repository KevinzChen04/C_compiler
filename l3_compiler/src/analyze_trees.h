#pragma once
#include "program.h"

namespace L3::program::analyze {
	void generate_data_flow(L3Function &l3_function);

	// Takes the completed program and generates computation trees
	// for each instruction, then updates all the basic blocks to have correct
	// in and out sets.
	void generate_data_flow(Program &program);

	// Assumes that data flow has already been generated for this block.
	// Merges trees whenever possible.
	void merge_trees(BasicBlock &block);

	// Assumes that data flow has already been generated for the program.
	// Merges trees in all the basic blocks.
	void merge_trees(Program &program);
}
