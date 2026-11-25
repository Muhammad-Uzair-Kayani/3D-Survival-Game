// Standalone native scorer used to verify the trainer's export format.
#include "neural_policy.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <vector>

int main(int argc, char **argv) {
	if (argc != 2) {
		return 2;
	}
	std::ifstream checkpoint(argv[1], std::ios::binary);
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(checkpoint)), {});
	lastlight::NeuralPolicy policy;
	if (!policy.load(bytes.data(), bytes.size())) {
		return 3;
	}
	std::cout << std::setprecision(9);
	lastlight::NeuralFeatures row{};
	while (std::cin >> row[0]) {
		for (int col = 1; col < lastlight::FEATURE_COUNT; ++col) {
			if (!(std::cin >> row[col])) {
				return 4;
			}
		}
		std::cout << policy.score(row) << '\n';
	}
}
