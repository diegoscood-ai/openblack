/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "RandomNumberManagerProduction.h"

#include <ctime>

using namespace openblack;

RandomNumberManagerProduction::RandomNumberManagerProduction()
{
	SetRunSeed(static_cast<uint32_t>(std::time(nullptr)));
}

void RandomNumberManagerProduction::SetRunSeed(uint32_t seed)
{
	_runSeed = seed;
	_generator.seed(run_seed::StreamSeed(seed, RandomStream::General));
	for (size_t i = 0; i < _streams.size(); ++i)
	{
		_streams.at(i).seed(run_seed::StreamSeed(seed, static_cast<RandomStream>(i)));
	}
}

std::mt19937& RandomNumberManagerProduction::Stream(RandomStream stream)
{
	return _streams.at(static_cast<size_t>(stream));
}

std::mt19937& RandomNumberManagerProduction::Generator()
{
	return _generator;
}

std::optional<std::reference_wrapper<std::mutex>> RandomNumberManagerProduction::LockAccess()
{
	return std::nullopt;
}
