#pragma once
#include <random>

// tier0's RandomFloat/RandomInt share their sequence with the game.
namespace utils
{
	inline std::mt19937 randomEngine {std::random_device {}()};

	inline float RandomFloat(float min, float max)
	{
		return std::uniform_real_distribution<float>(min, max)(randomEngine);
	}

	inline int RandomInt(int min, int max)
	{
		return std::uniform_int_distribution<int>(min, max)(randomEngine);
	}
} // namespace utils
