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

namespace openblack::audio
{

/// The voices the ambience mixer (atmos_banks) plays its samples on: the starts, the volume changes, the questions and
/// the stops it sends to the audio library. The game's are the sample channels (AtmosVoices); a test gives a fake that
/// records what the mixer sends
class AtmosVoicesInterface
{
public:
	virtual ~AtmosVoicesInterface() = default;

	/// Start a sample: the voice of the start, k_NoChannel when nothing started. A start that finds its sample already
	/// playing (play mode 2) gives that voice back, untouched
	virtual Channel Play(const sample_play::Options& options) = 0;
	/// The voice is still that start and sounds
	[[nodiscard]] virtual bool IsPlaying(Channel voice) const = 0;
	/// The voice's volume 0..127, as its start or its last volume change left it
	[[nodiscard]] virtual int Volume(Channel voice) const = 0;
	/// A new volume, clamped to 0..127; nothing on a voice that no longer sounds or already has it
	virtual void SetVolume(Channel voice, int volume) = 0;
	virtual void Stop(Channel voice) = 0;
};

} // namespace openblack::audio
