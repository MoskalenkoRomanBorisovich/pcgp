#include <iostream>

#include "pcgpp.hpp"

int main (int argc, char* argv[]) {
	try {
		if (!pcgpp_app_main(argc, argv)) {
			std::cerr << "pcgpp finished with errors.\n";
			return 1;
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Error: Unhandled exception: " << e.what() << "\n";
		return 1;
	}	
	return 0;
}