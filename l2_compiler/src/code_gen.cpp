#include "code_gen.h"
#include "program.h"
#include "register_allocator.h"
#include <iostream>
#include <fstream>

namespace L2::code_gen {
	using namespace L2::program;

	class ExprCodeGenVisitor : public ExprVisitor {
		private:

		L2Function &function;
        Program &program;
        std::ostream &o;
        int spill_overflow;
		const analyze::RegAllocMap &reg_alloc_map;

		public:

		ExprCodeGenVisitor(L2Function &function, Program &program, std::ostream &o, int spill_overflow, const analyze::RegAllocMap &reg_alloc_map):
            function {function},
            program {program},
            o {o},
            spill_overflow {spill_overflow},
			reg_alloc_map {reg_alloc_map}
        {};

		virtual void visit(RegisterRef &expr) {
			o << expr.get_ref_name();
		}
		virtual void visit(NumberLiteral &expr) {
			o << std::to_string(expr.value);
		}
		virtual void visit(StackArg &expr) {
			int64_t byte_offset = this->spill_overflow * 8 + expr.stack_num->value;
			o << "mem rsp " << std::to_string(byte_offset);
		}
		virtual void visit(MemoryLocation &expr) {
			o << "mem ";
			expr.base->accept(*this);
			o << " ";
			expr.offset->accept(*this);
		}
		virtual void visit(LabelRef &expr) {
			o << ":" << expr.get_ref_name();
		}
		virtual void visit(VariableRef &expr) {
			o << this->reg_alloc_map.at(expr.get_referent())->name;
		}
		virtual void visit(L2FunctionRef &expr) {
			o << "@" << expr.get_referent()->get_name();
		}
		virtual void visit(ExternalFunctionRef &expr) {
			o << expr.get_referent()->get_name();
		}
	};

	class InstructionCodeGenVisitor : public InstructionVisitor {
		private:

        ExprCodeGenVisitor expr_v;
		L2Function &function;
        Program &program;
        std::ostream &o;
        int spill_overflow;
		const analyze::RegAllocMap &reg_alloc_map;

		public:

		InstructionCodeGenVisitor(L2Function &function, Program &program, std::ostream &o, int spill_overflow, const analyze::RegAllocMap &reg_alloc_map) :
            expr_v(function, program, o, spill_overflow, reg_alloc_map),
			function {function},
            program {program},
            o{o},
            spill_overflow {spill_overflow},
			reg_alloc_map {reg_alloc_map}
        {};

		virtual void visit(InstructionReturn &inst) {
            o << "\t\treturn\n";
		}
		virtual void visit(InstructionAssignment &inst) {
            o << "\t\t";
            inst.destination->accept(expr_v);
            o << " " << program::to_string(inst.op) <<  " ";
            inst.source->accept(expr_v);
            o << "\n";
		}
		virtual void visit(InstructionCompareAssignment &inst) {
            o << "\t\t";
            inst.destination->accept(expr_v);
            o << " <- ";
            inst.lhs->accept(expr_v);
            o << " " << program::to_string(inst.op) << " ";
            inst.rhs->accept(expr_v);
            o << "\n";
		}
		virtual void visit(InstructionCompareJump &inst) {
            o << "\t\t cjump ";
            inst.lhs->accept(expr_v);
            o << " " << program::to_string(inst.op) << " ";
            inst.rhs->accept(expr_v);
            o << " ";
            inst.label->accept(expr_v);
			o << "\n";
		}
		virtual void visit(InstructionLabel &inst) {
            o << "\t\t:" << inst.label_name << "\n";
		}
		virtual void visit(InstructionGoto &inst) {
            o << "\t\tgoto ";
            inst.label->accept(expr_v);
            o << "\n";
		}
		virtual void visit(InstructionCall &inst) {
            o << "\t\tcall ";
            inst.callee->accept(expr_v);
            o << " " << std::to_string(inst.num_arguments);
            o << "\n";
		}
		virtual void visit(InstructionLeaq &inst) {
            o << "\t\t";
            inst.destination->accept(expr_v);
            o << " @ ";
            inst.base->accept(expr_v);
            o << " ";
            inst.offset->accept(expr_v);
            o << " " << std::to_string(inst.scale) << "\n";
		}
	};

	int get_spill_overflow(L2Function &f){
		int sol = 0;
		for (const auto &i: f.agg_scope.variable_scope.get_all_items()) {
			if (i->spillable) {
				continue;
			}
			sol++;
		}
		return sol;
	}

	void generate_code(Program &p){
		std::ofstream o;
		o.open("prog.L1");

		o << "(@" << p.get_entry_function_ref().get_referent()->get_name() << "\n";

		for (const std::unique_ptr<L2Function> &f : p.get_l2_functions()) {
			analyze::RegAllocMap reg_alloc_map =
				analyze::allocate_and_spill_with_backup(*f);
            int spill_overflow = get_spill_overflow(*f);
			InstructionCodeGenVisitor v(*f, p, o, spill_overflow, reg_alloc_map);
			o << "\t(@" << f->get_name();
			o << " " << f->get_num_arguments();
			o << " " << std::to_string(spill_overflow) << "\n";
            for (const auto &inst : f->instructions) {
                inst->accept(v);
            }
			o << "\t ) \n";
		}
		o << ")\n";

		o.close();
	}
}