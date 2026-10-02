/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include "Audio/Device/SampleOutput.h"

namespace openblack::audio
{

/// The 16 sample channels on the audio device (Device.h: its one OpenAL context, no second device). Positions are
/// openblack's world (or listener) coordinates (the device swaps x and z for OpenAL). The distance curve up to
/// the max is OpenAL's inverse distance clamped (reference = min, rolloff = scale: QMixer 0x1802CE50) and beyond the max
/// the channel is muted as QMixer does (0x1800ADDF).
class AlSampleOutput final: public SampleOutput
{
public:
	static constexpr size_t k_Channels = 16;

	~AlSampleOutput() override;
	bool Play(size_t channel, Sound& sound, const Start& start) override;
	void Stop(size_t channel) override;
	/// The 20 ms ramp of QMixer's pan rate as four gain steps of 5 ms (approximated), then the stop
	void StopRamped(size_t channel) override;
	[[nodiscard]] int64_t PlayPositionMs(size_t channel) const override;
	[[nodiscard]] bool Playing(size_t channel) const override;
	void SetGain(size_t channel, float gain) override;
	void SetPitch(size_t channel, float ratio) override;
	void SetPosition(size_t channel, glm::vec3 position) override;
	void ReleaseLoop(size_t channel) override;
	void SetListener(glm::vec3 position) override;
	void Update() override;
	[[nodiscard]] size_t Sources() const override;
	void DeleteAll() override;

private:
	struct Slot
	{
		uint32_t source {0};
		bool is3D {false};
		bool relative {false};
		bool looping {false};
		float gain {1.0f};
		float maxDistance {9999.0f};
		glm::vec3 position {0.0f};
		LoopCounter loop;
	};
	void ApplyGain(Slot& slot) const;

	std::array<Slot, k_Channels> _slots;
	glm::vec3 _listener {0.0f};
};

} // namespace openblack::audio
