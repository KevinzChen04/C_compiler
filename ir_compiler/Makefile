CPP_FILES			:= $(wildcard src/*.cpp)
OBJ_FILES			:= $(addprefix obj/,$(notdir $(CPP_FILES:.cpp=.o)))
CC_FLAGS			:= --std=c++17 -I./src -I../lib/PEGTL/include -I../lib -g3 -DDEBUG -pedantic -pedantic-errors -Werror=pedantic
LD_FLAGS			:= 
CC						:= g++
PL_CLASS			:= IR
DST_PL_CLASS 	:= L3
EXT_CLASS			:= $(PL_CLASS)
COMPILER			:= bin/$(PL_CLASS)
OPT_LEVEL			:=
CC_CLASS			:= $(PL_CLASS)c

run: all
	./IRc my_tests/parse_test.IR
	
svg_tree: all
	./bin/IR my_tests/parse_test.IR -p
	dot -T svg -o parse_tree.svg parse_tree.dot


all: dirs $(COMPILER)

dirs: obj bin

obj:
	mkdir -p $@

bin:
	mkdir -p $@

$(COMPILER): $(OBJ_FILES)
	$(CC) $(LD_FLAGS) -o $@ $^

obj/%.o: src/%.cpp
	$(CC) $(CC_FLAGS) -c -o $@ $<

oracle: $(COMPILER)
	../scripts/generateOutput.sh $(EXT_CLASS) $(CC_CLASS) "tests"

oracle_new: $(COMPILER)
	../scripts/generateOutput.sh $(EXT_CLASS) $(CC_CLASS) "tests/new"

rm_tests_without_oracle:
	../scripts/rm_tests_without_oracle.sh $(EXT_CLASS)

test: dirs $(COMPILER)
	../scripts/test.sh $(EXT_CLASS) $(CC_CLASS) "tests"

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
	rm -fr *.$(DST_PL_CLASS)

.PHONY: dirs $(COMPILER) oracle oracle_new rm_tests_without_oracle test test_new test_programs performance clean
