#pragma once
#include "program.h"

namespace L2::program::spiller {

    class Spiller {
        private:
        program::L2Function &function;
        std::string prefix;
        int prefix_count;
        int spill_calls;

        public:
        Spiller(program::L2Function &function, std::string prefix):
            function {function},
            prefix {prefix},
            prefix_count {0},
            spill_calls {0}
        {};

        void spill(const Variable *var);
        void spill_all();
        std::string printDaSpiller();
    };
}