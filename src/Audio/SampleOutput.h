/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/vec3.hpp>

namespace openblack::audio
{
class Sound;

/// What QMixer does with one of LHaudio's 16 sample channels (layer 0 of dev\tmp_dis\audio\PLAN.md §2.1): the device
/// side of audio::sample_play. Each channel owns one OpenAL source (made at its first start, deleted by DeleteAll), out
/// of the ECS registry, so a registry reset leaves no orphan source. The logic of the channels (allocation, modes,
/// priorities, volumes, owners) is in SamplePlay; the tests use a fake output.
class SampleOutput
{
public:
	/// QSWaveMixPlayEx and the channel set-up of LHSamplePlay 0x10011420..0x10012949
	struct Start
	{
		/// QSWaveMixSetVolume's gain (qmixer::Gain of the channel volume and the sample master)
		float gain {1.0f};
		/// QSWaveMixSetFrequency as a ratio of the wave's rate (qmixer::FrequencyRatio of the start pitch)
		float pitch {1.0f};
		/// a 3D channel (0x10012050: QSWaveMixEnableChannel 0x20 with the 3D flags); 2D: on the listener, no distance
		/// mapping (0x100125AB)
		bool is3D {false};
		/// the position is in the listener's frame: (right, up, ahead) in openblack's axes as AudioPlayer sends them
		/// (x <-> z swapped), i.e. already through qmixer::PolarRelative
		bool relative {false};
		glm::vec3 position {0.0f};
		/// QSWaveMixSetDistanceMapping {min, max, scale} (0x10012159)
		float minDistance {1.0f};
		float maxDistance {9999.0f};
		float scale {0.3f};
		/// QSWaveMixPlayEx's iLoops (+0x4C): 0 once, -1 for ever, N > 0 N more passes after the first (inferred: the QMixer
		/// SDK's "number of times to loop"; dev\tmp_dis\audio\PLAN.md §6 question 2)
		int loops {0};
	};

	virtual ~SampleOutput() = default;
	/// The channel starts the sample's wave (restarted if it was playing). False when the wave cannot be played.
	virtual bool Play(size_t channel, Sound& sound, const Start& start) = 0;
	/// QSWaveMixFlushChannel: the channel stops at once
	virtual void Stop(size_t channel) = 0;
	/// LHSampleStop's stop (0x10012CC1..0x10012D09, 0x10012D79..0x10012DCB, 0x10012E6E..): QSWaveMixSetPanRate(20 ms),
	/// QSWaveMixSetVolume(0) (a 20 ms ramp to silence), Sleep(20), SetPanRate(100), QSWaveMixFlushChannel. The caller
	/// waits those 20 ms, as the original does. Default: Stop.
	virtual void StopRamped(size_t channel) { Stop(channel); }
	/// QSWaveMixGetPlayPosition(..., 2) of LHSampleGetPlayPosition 0x10014C00: the channel's play position in
	/// milliseconds, -1 when it is not playing (0x10014C6E). Default: -1.
	[[nodiscard]] virtual int64_t PlayPositionMs(size_t /*channel*/) const { return -1; }
	/// The channel's wave is still sounding (+0x8C is cleared by the end callback 0x100108C0)
	[[nodiscard]] virtual bool Playing(size_t channel) const = 0;
	/// QSWaveMixSetVolume
	virtual void SetGain(size_t channel, float gain) = 0;
	/// QSWaveMixSetFrequency
	virtual void SetPitch(size_t channel, float ratio) = 0;
	/// QSWaveMixSetSourcePosition (a 3D channel)
	virtual void SetPosition(size_t channel, glm::vec3 position) = 0;
	/// QSWaveMixStopChannel(0x1000) 0x10012F20 -> QMixer 0x18007520: the remaining loops go to 0, the current pass ends
	virtual void ReleaseLoop(size_t channel) = 0;
	/// QSWaveMixSetListenerPosition (LHListenerUpdate, once a turn): QMixer mutes a channel beyond its max (0x1800ADDF)
	virtual void SetListener(glm::vec3 position) = 0;
	/// Once a frame: the finite loops are counted (QMixer's +0x44 counter)
	virtual void Update() = 0;
	/// The OpenAL sources alive (a channel's source is made at its first start)
	[[nodiscard]] virtual size_t Sources() const = 0;
	/// Every channel stopped and its source deleted
	virtual void DeleteAll() = 0;
};

/// The finite loops of a channel (QSWaveMixPlayEx iLoops = N > 0): the source loops and every wrap of its play offset is
/// one pass; after N wraps it stops looping, so the current pass ends and the wave plays on to its end (N + 1 passes of
/// the loop section, inferred). Pure, for the tests.
struct LoopCounter
{
	/// The wraps still to come before the looping stops (0: nothing to count)
	int remaining {0};
	int64_t lastOffset {0};

	void Start(int loops)
	{
		remaining = loops > 0 ? loops : 0;
		lastOffset = 0;
	}
	/// The play offset of this frame; true when the looping must stop now
	bool Feed(int64_t offset)
	{
		if (remaining <= 0)
		{
			return false;
		}
		if (offset < lastOffset)
		{
			--remaining;
		}
		lastOffset = offset;
		return remaining == 0;
	}
};

/// No device (AudioManagerNoOp): nothing ever plays
class NullSampleOutput final: public SampleOutput
{
public:
	bool Play(size_t, Sound&, const Start&) override { return false; }
	void Stop(size_t) override {}
	[[nodiscard]] bool Playing(size_t) const override { return false; }
	void SetGain(size_t, float) override {}
	void SetPitch(size_t, float) override {}
	void SetPosition(size_t, glm::vec3) override {}
	void ReleaseLoop(size_t) override {}
	void SetListener(glm::vec3) override {}
	void Update() override {}
	[[nodiscard]] size_t Sources() const override { return 0; }
	void DeleteAll() override {}
};

} // namespace openblack::audio
