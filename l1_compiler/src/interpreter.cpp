#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <set>
#include <iterator>
#include <iostream>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <stdint.h>
#include <unistd.h>
#include <iostream>

#include <parser.h>
#include <code_generator.h>

using namespace std;

void print_help(char *progName) {
	std::cerr << "Usage: " << progName << " SOURCE" << std::endl;
	return;
}

int main(
	int argc,
	char **argv
) {
	auto enable_code_generator = false;
	int32_t optLevel = 0;
	bool verbose;

	/*
	 * Check the compiler arguments.
	 */
	if (argc < 2) {
		print_help(argv[0]);
		return 1;
	}

	/*
	 * Parse the input file.
	 */
	auto p = L1::parse_file(argv[optind]);

	/*
	 * Interpret the L1 program.
	 */
	//TODO

	return 0;
}
