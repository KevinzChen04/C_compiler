#pragma once
#include "program.h"
#include "std_alias.h"
#include <iostream>
#include <string>

namespace L3::code_gen::tiles {
	using namespace std_alias;

	// interface
	struct Tile {
		virtual Vec<std::string> to_l2_instructions() const = 0;
		virtual Vec<const L3::program::ComputationNode *> get_unmatched() const = 0;
	};

	// outputs a vector of matched tiles. these tiles have the same lifetime
	// as the vector of computation tree boxes
	Vec<Uptr<Tile>> tile_trees(const Vec<L3::program::ComputationTreeBox> &trees);
}
