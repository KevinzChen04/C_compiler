#pragma once
#include "program.h"
#include "utils.h"
#include <vector>
#include <map>
#include <set>

namespace L2::program::analyze {
	struct InstructionAnalysisResult {
		std::vector<Instruction *> successors;
		utils::set<const Variable *> gen_set;
		utils::set<const Variable *> kill_set;
		utils::set<const Variable *> in_set;
		utils::set<const Variable *> out_set;
	};

	using InstructionsAnalysisResult = std::map<Instruction *, InstructionAnalysisResult>;

	InstructionsAnalysisResult analyze_instructions(const L2Function &function);

	void print_liveness(const L2Function &function, InstructionsAnalysisResult &liveness_results);
}
