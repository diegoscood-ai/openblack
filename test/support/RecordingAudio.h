/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// An audio service for the tests that records the sounds played with their options and the counters drawn, in order;
// it plays nothing. The other ways in (owner, sample, ...) go through the game's own audio functions, which end in the
// options variant

#include <vector>

#include "Audio/Audio.h"
#include "Audio/AudioManagerNoOp.h"

namespace openblack::test
{
/// Every sound played (its options) and every counter drawn, in order; each counter gives `next`
class RecordingAudio final: public audio::AudioManagerNoOp
{
public:
	using AudioManagerNoOp::PlaySoundEffect;
	audio::Channel PlaySoundEffect(const audio::PlayOptions& options) override
	{
		played.push_back(options);
		return audio::k_NoChannel;
	}
	[[nodiscard]] int NextCounter(audio::Counter counter) override
	{
		counters.push_back(counter);
		return next;
	}

	int next {0};
	std::vector<audio::PlayOptions> played;
	std::vector<audio::Counter> counters;
};
} // namespace openblack::test
