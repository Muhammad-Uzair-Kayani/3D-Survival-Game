#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <cstdint>

namespace lastlight {
enum class Feature {
#define FEATURE(name) name,
#include "neural_features.def"
#undef FEATURE
	Count
};

constexpr int FEATURE_COUNT = int(Feature::Count);
constexpr int HIDDEN_COUNT = 24;

struct NeuralFeatures : std::array<float, FEATURE_COUNT> {
	void set(Feature column, float value, float scale = 1.f) {
		(*this)[int(column)] = std::clamp(value / scale, -4.f, 4.f);
	}
};

class NeuralPolicy {
	static constexpr int parameter_count = FEATURE_COUNT * (HIDDEN_COUNT + 1) + HIDDEN_COUNT * 2 + 1;
	std::array<float, parameter_count> weights{};
	bool loaded = false;

public:
	static constexpr int file_size = 16 + parameter_count * 4;

	bool load(const uint8_t *bytes, size_t size) {
		if (size != file_size) {
			return false;
		}
		auto word = [&](int offset) {
			return uint32_t(bytes[offset]) | uint32_t(bytes[offset + 1]) << 8 |
					uint32_t(bytes[offset + 2]) << 16 | uint32_t(bytes[offset + 3]) << 24;
		};
		// LLNP, file format and feature-layout revision 1.
		if (word(0) != 0x504e4c4c || word(4) != 1 || word(8) != FEATURE_COUNT || word(12) != HIDDEN_COUNT) {
			return false;
		}
		std::array<float, parameter_count> candidate;
		for (int n = 0; n < parameter_count; ++n) {
			uint32_t bits = word(16 + n * 4);
			float value;
			static_assert(sizeof(value) == sizeof(bits));
			std::memcpy(&value, &bits, sizeof(value));
			if (!std::isfinite(value) || std::abs(value) > 32.f) {
				return false;
			}
			candidate[n] = value;
		}
		weights = candidate;
		loaded = true;
		return true;
	}

	bool ready() const { return loaded; }

	float score(const NeuralFeatures &features) const {
		float value = weights.back();
		for (int col = 0; col < FEATURE_COUNT; ++col) {
			value += weights[col] * features[col];
		}
		int offset = FEATURE_COUNT;
		for (int unit = 0; unit < HIDDEN_COUNT; ++unit) {
			float activation = weights[offset++];
			for (int col = 0; col < FEATURE_COUNT; ++col) {
				activation += weights[offset++] * features[col];
			}
			value += weights[offset++] * std::tanh(activation);
		}
		return value;
	}
};

struct NeuralChoice {
	NeuralFeatures features;
	float teacher = 0.f;
	int kind = 0;
	int target = 0;
};
} //namespace lastlight
