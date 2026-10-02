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

#include "Device.h"
#include "Sound.h"
#include "WaveBuffers.h"

using namespace openblack::audio;

AlSampleOutput::~AlSampleOutput()
{
	DeleteAll();
}

bool AlSampleOutput::Play(size_t channel, Sound& sound, const Start& start)
{
	if (channel >= _slots.size() || !device::IsOpen())
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
		slot.source = device::CreateSource();
	}
	const auto source = slot.source;
	device::StopSource(source);
	device::SetSourceBuffer(source, 0);
	device::SetSourceBuffer(source, buffer);
	device::SetSourcePitch(source, start.pitch);
	slot.is3D = start.is3D;
	slot.relative = start.relative || !start.is3D;
	slot.position = start.is3D ? start.position : glm::vec3(0.0f);
	slot.maxDistance = start.maxDistance;
	slot.gain = start.gain;
	device::SetSourceRelative(source, slot.relative);
	if (start.is3D)
	{
		device::SetSourceDistance(source, start.minDistance, start.maxDistance, start.scale);
	}
	else
	{
		// 2D (0x100125AB): no position and no distance mapping
		device::SetSourceRolloff(source, 0.0f);
	}
	device::SetSourcePosition(source, slot.position);
	slot.looping = start.loops != 0;
	device::SetSourceLooping(source, slot.looping);
	slot.loop.Start(start.loops);
	ApplyGain(slot);
	device::PlaySource(source);
	return true;
}

void AlSampleOutput::Stop(size_t channel)
{
	if (channel < _slots.size() && _slots[channel].source != 0)
	{
		device::StopSource(_slots[channel].source);
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
		device::SetSourceGain(slot.source, gain);
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
	const float seconds = device::SourceSecondOffset(_slots[channel].source);
	return static_cast<int64_t>(seconds * 1000.0f);
}

bool AlSampleOutput::Playing(size_t channel) const
{
	if (channel >= _slots.size() || _slots[channel].source == 0)
	{
		return false;
	}
	const auto state = device::SourceStatus(_slots[channel].source);
	return state == AudioStatus::Playing || state == AudioStatus::Paused;
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
		device::SetSourcePitch(_slots[channel].source, ratio);
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
	device::SetSourcePosition(slot.source, position);
	ApplyGain(slot);
}

void AlSampleOutput::ReleaseLoop(size_t channel)
{
	if (channel < _slots.size() && _slots[channel].source != 0)
	{
		auto& slot = _slots[channel];
		slot.looping = false;
		slot.loop.Start(0);
		device::SetSourceLooping(slot.source, false);
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
		const auto offset = device::SourceSampleOffset(slot.source);
		if (slot.loop.Feed(offset))
		{
			slot.looping = false;
			device::SetSourceLooping(slot.source, false);
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
	const bool context = device::IsOpen();
	for (auto& slot : _slots)
	{
		if (slot.source != 0 && context)
		{
			device::StopSource(slot.source);
			device::DeleteSource(slot.source);
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
	device::SetSourceGain(slot.source, muted ? 0.0f : slot.gain);
}
