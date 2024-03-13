#include "target_arch.h"
#include <assert.h>

namespace IR::code_gen::target_arch {

	std::string new_variable_names(IRFunction &fun, BasicBlock& bb){
		return fun.get_name() + bb.get_name();
	}

    void mangle_label_names(Program &program) {
		for (const Uptr<IRFunction> &ir_function : program.get_ir_functions()) {
			for (const Uptr<BasicBlock> &block : ir_function->get_blocks()) {
				block->set_name("_" + ir_function->get_name() + block->get_name());
			}
		}
	}
}