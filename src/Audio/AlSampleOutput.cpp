/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AlSampleOutput.h"

#include <chrono>
#include <thread>

#include <glm/geometric.hpp>

extern "C" {
#include <AL/al.h>
#include <AL/alc.h>
}

#include "AlCheck.h"
#include "Sound.h"
#include "WaveBuffers.h"

using namespace openblack::audio;

AlSampleOutput::~AlSampleOutput()
{
	DeleteAll();
}

bool AlSampleOutput::Play(size_t channel, Sound& sound, const Start& start)
{
	if (channel >= _slots.size() || alcGetCurrentContext() == nullptr)
	{
		return false;
	}
	const BufferId buffer = wave_buffers::Get(sound);
	if (buffer == 0)
	{
		return false;
	}
	auto& slot = _slots[channel];
	if (slot.source == 0)
	{
		alCheckCall(alGenSources(1, &slot.source));
	}
	const auto source = slot.source;
	alCheckCall(alSourceStop(source));
	alCheckCall(alSourcei(source, AL_BUFFER, 0));
	alCheckCall(alSourcei(source, AL_BUFFER, static_cast<ALint>(buffer)));
	alCheckCall(alSourcef(source, AL_PITCH, start.pitch));
	slot.is3D = start.is3D;
	slot.relative = start.relative || !start.is3D;
	slot.position = start.is3D ? start.position : glm::vec3(0.0f);
	slot.maxDistance = start.maxDistance;
	slot.gain = start.gain;
	alCheckCall(alSourcei(source, AL_SOURCE_RELATIVE, slot.relative ? AL_TRUE : AL_FALSE));
	if (start.is3D)
	{
		alCheckCall(alSourcef(source, AL_REFERENCE_DISTANCE, start.minDistance));
		alCheckCall(alSourcef(source, AL_MAX_DISTANCE, start.maxDistance));
		alCheckCall(alSourcef(source, AL_ROLLOFF_FACTOR, start.scale));
	}
	else
	{
		// 2D (0x100125AB): no position and no distance mapping
		alCheckCall(alSourcef(source, AL_ROLLOFF_FACTOR, 0.0f));
	}
	alCheckCall(alSource3f(source, AL_POSITION, slot.position.z, slot.position.y, slot.position.x));
	slot.looping = start.loops != 0;
	alCheckCall(alSourcei(source, AL_LOOPING, slot.looping ? AL_TRUE : AL_FALSE));
	slot.loop.Start(start.loops);
	ApplyGain(slot);
	alCheckCall(alSourcePlay(source));
	return true;
}

void AlSampleOutput::Stop(size_t channel)
{
	if (channel < _slots.size() && _slots[channel].source != 0)
	{
		alCheckCall(alSourceStop(_slots[channel].source));
		_slots[channel].loop.Start(0);
	}
}

void AlSampleOutput::StopRamped(size_t channel)
{
	if (channel >= _slots.size() || _slots[channel].source == 0 || !Playing(channel))
	{
		Stop(channel);
		return;
	}
	// LHSampleStop 0x10012CCA..0x10012CE5: QSWaveMixSetPanRate(mixer, channel, 0, 20) and QSWaveMixSetVolume(.., 0): the
	// volume ramps to 0 in 20 ms while the DLL sleeps 20 ms; then SetPanRate(100) and the flush. (approximated) OpenAL
	// has no pan rate: four steps of 5 ms down to 0 (OpenAL Soft smooths each gain change), the game thread waiting as
	// the original's does.
	auto& slot = _slots[channel];
	constexpr int k_Steps = 4;
	constexpr auto k_Ramp = std::chrono::milliseconds(20); // push 0x14 (0x10012CCA, 0x10012CE3)
	for (int step = 1; step <= k_Steps; ++step)
	{
		const float gain = slot.gain * static_cast<float>(k_Steps - step) / static_cast<float>(k_Steps);
		alCheckCall(alSourcef(slot.source, AL_GAIN, gain));
		std::this_thread::sleep_for(k_Ramp / k_Steps);
	}
	Stop(channel);
	ApplyGain(slot);
}

int64_t AlSampleOutput::PlayPositionMs(size_t channel) const
{
	if (!Playing(channel))
	{
		return -1;
	}
	ALfloat seconds = 0.0f;
	alCheckCall(alGetSourcef(_slots[channel].source, AL_SEC_OFFSET, &seconds));
	return static_cast<int64_t>(seconds * 1000.0f);
}

bool AlSampleOutput::Playing(size_t channel) const
{
	if (channel >= _slots.size() || _slots[channel].source == 0)
	{
		return false;
	}
	ALint state = AL_STOPPED;
	alCheckCall(alGetSourcei(_slots[channel].source, AL_SOURCE_STATE, &state));
	return state == AL_PLAYING || state == AL_PAUSED;
}

void AlSampleOutput::SetGain(size_t channel, float gain)
{
	if (channel < _slots.size() && _slots[channel].source != 0)
	{
		_slots[channel].gain = gain;
		ApplyGain(_slots[channel]);
	}
}

void AlSampleOutput::SetPitch(size_t channel, float ratio)
{
	if (channel < _slots.size() && _slots[channel].source != 0)
	{
		alCheckCall(alSourcef(_slots[channel].source, AL_PITCH, ratio));
	}
}

void AlSampleOutput::SetPosition(size_t channel, glm::vec3 position)
{
	if (channel >= _slots.size() || _slots[channel].source == 0 || !_slots[channel].is3D)
	{
		return;
	}
	auto& slot = _slots[channel];
	slot.position = position;
	alCheckCall(alSource3f(slot.source, AL_POSITION, position.z, position.y, position.x));
	ApplyGain(slot);
}

void AlSampleOutput::ReleaseLoop(size_t channel)
{
	if (channel < _slots.size() && _slots[channel].source != 0)
	{
		auto& slot = _slots[channel];
		slot.looping = false;
		slot.loop.Start(0);
		alCheckCall(alSourcei(slot.source, AL_LOOPING, AL_FALSE));
	}
}

void AlSampleOutput::SetListener(glm::vec3 position)
{
	_listener = position;
	for (auto& slot : _slots)
	{
		if (slot.source != 0 && slot.is3D)
		{
			ApplyGain(slot);
		}
	}
}

void AlSampleOutput::Update()
{
	for (auto& slot : _slots)
	{
		if (slot.source == 0 || !slot.looping || slot.loop.remaining <= 0)
		{
			continue;
		}
		ALint offset = 0;
		alCheckCall(alGetSourcei(slot.source, AL_SAMPLE_OFFSET, &offset));
		if (slot.loop.Feed(offset))
		{
			slot.looping = false;
			alCheckCall(alSourcei(slot.source, AL_LOOPING, AL_FALSE));
		}
	}
}

size_t AlSampleOutput::Sources() const
{
	size_t count = 0;
	for (const auto& slot : _slots)
	{
		count += slot.source != 0 ? 1 : 0;
	}
	return count;
}

void AlSampleOutput::DeleteAll()
{
	const bool context = alcGetCurrentContext() != nullptr;
	for (auto& slot : _slots)
	{
		if (slot.source != 0 && context)
		{
			alCheckCall(alSourceStop(slot.source));
			alCheckCall(alDeleteSources(1, &slot.source));
		}
		slot = {};
	}
}

void AlSampleOutput::ApplyGain(Slot& slot) const
{
	// QMixer 0x1800ADDF: a 3D channel farther than the max of its distance mapping is silent (it keeps playing)
	bool muted = false;
	if (slot.is3D)
	{
		const float distance = slot.relative ? glm::length(slot.position) : glm::distance(slot.position, _listener);
		muted = distance > slot.maxDistance;
	}
	alCheckCall(alSourcef(slot.source, AL_GAIN, muted ? 0.0f : slot.gain));
}
