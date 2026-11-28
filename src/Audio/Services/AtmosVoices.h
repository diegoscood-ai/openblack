/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Audio/Engine/SamplePlay.h"
#include "Audio/Services/AtmosVoicesInterface.h"

namespace openblack::audio
{

/// The game's ambience voices: the audio library's sample channels (sample_play)
class AtmosVoices final: public AtmosVoicesInterface
{
public:
	Channel Play(const sample_play::Options& options) override { return sample_play::Start(options); }
	[[nodiscard]] bool IsPlaying(Channel voice) const override { return sample_play::IsPlaying(voice); }
	[[nodiscard]] int Volume(Channel voice) const override { return sample_play::Volume(voice); }
	void SetVolume(Channel voice, int volume) override { sample_play::SetVolume(voice, volume); }
	void Stop(Channel voice) override { sample_play::Stop(voice); }
};

} // namespace openblack::audio
