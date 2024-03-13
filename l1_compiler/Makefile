CPP_FILES			   	:= $(wildcard src/*.cpp)
CPP_FILES_CC  	 	:= $(filter-out src/interpreter.cpp,$(CPP_FILES))
CPP_FILES_INTERP 	:= $(filter-out src/compiler.cpp,$(CPP_FILES))
OBJ_FILES			   	:= $(addprefix obj/,$(notdir $(CPP_FILES:.cpp=.o)))
OBJ_FILES_CC		 	:= $(addprefix obj/,$(notdir $(CPP_FILES_CC:.cpp=.o)))
OBJ_FILES_INTERP 	:= $(addprefix obj/,$(notdir $(CPP_FILES_INTERP:.cpp=.o)))
CC_FLAGS			   	:= --std=c++17 -I./src -I../lib/PEGTL/include -I../lib -g3 -DDEBUG -pedantic -pedantic-errors -Werror=pedantic
LD_FLAGS		   	 	:= 
CC								:= g++
PL_CLASS          := L1
DST_PL_CLASS      := S
EXT_CLASS					:= $(PL_CLASS)
COMPILER					:= bin/$(PL_CLASS)
INTERP        		:= bin/$(PL_CLASS)i
OPT_LEVEL         :=
CC_CLASS					:= $(PL_CLASS)c

compiler: dirs $(COMPILER)

all: dirs $(COMPILER) $(INTERP)

interp: dirs $(INTERP)

dirs: obj bin

obj:
	mkdir -p $@

bin:
	mkdir -p $@

$(COMPILER): $(OBJ_FILES_CC)
	$(CC) $(LD_FLAGS) -o $@ $^

$(INTERP): $(OBJ_FILES_INTERP)
	$(CC) $(LD_FLAGS) -o $@ $^

obj/%.o: src/%.cpp
	$(CC) $(CC_FLAGS) -c -o $@ $<

oracle: $(COMPILER)
	../scripts/generateOutput.sh $(EXT_CLASS) $(CC_CLASS) "tests"

oracle_new: $(COMPILER)
	../scripts/generateOutput.sh $(EXT_CLASS) $(CC_CLASS) "tests/new"

oracle_broken: $(COMPILER)
	../scripts/generateOutput.sh $(EXT_CLASS) $(CC_CLASS) "tests/broken"

rm_tests_without_oracle:
	../scripts/rm_tests_without_oracle.sh $(EXT_CLASS)

test: dirs $(COMPILER)
	../scripts/test.sh $(EXT_CLASS) $(CC_CLASS) "tests"

test_interp: dirs $(INTERP)
	../scripts/test_interp.sh $(EXT_CLASS) $(INTERP) "tests" "1" "0"

test_interp_broken: dirs $(INTERP)
	../scripts/test_interp.sh $(EXT_CLASS) $(INTERP) "tests/broken" "0" "0"

rm_tests_that_fail_with_interp: dirs $(INTERP)
	../scripts/test_interp.sh $(EXT_CLASS) $(INTERP) "tests" "1" "1" 

test_new: dirs $(COMPILER)
	../scripts/test.sh $(EXT_CLASS) $(CC_CLASS) "tests/new"

test_programs: dirs $(COMPILER)
	../scripts/test_programs.sh $(EXT_CLASS) $(CC_CLASS)

performance: dirs $(COMPILER)
	if ! test -f ./a.out ; then ./$(CC_CLASS) $(OPT_LEVEL) tests/competition2020.$(EXT_CLASS) ; fi ; /usr/bin/time -f'%E' ./a.out

copy_simone_bin:
	mkdir -p bin ;
	cp .bin/* bin/ ;

clean:
	rm -fr bin obj *.out *.o core.* `find tests -iname *.tmp`
	rm -fr `find tests -iname *\.out\.interp`
	rm -fr *.$(DST_PL_CLASS)

.PHONY: dirs compiler interp $(COMPILER) $(INTERP) oracle oracle_new rm_tests_without_oracle test test_new test_programs performance clean
