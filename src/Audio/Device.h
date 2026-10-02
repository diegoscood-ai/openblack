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

#include <vector>

#include <glm/vec3.hpp>

#include "Sound.h"

// The audio device of openblack: layer 0 of dev\tmp_dis\audio\PLAN.md §2.1 (milestone B11a, "one engine"). What
// LH_AudioSystem::Create opened (QMixer's wave device, LHWaveIsInstalled) and what QMixer did with it is OpenAL here, and
// this is the only place that calls OpenAL: one device, one context, every call checked (alGetError) in Device.cpp. Its
// users are the 16 sample channels (AlSampleOutput, the device side of audio::sample_play), the waves' buffers
// (wave_buffers), the 6 music channels (MusicStream) and, through the channels, the voices and the advisors.
//
// Positions are openblack's world (or listener) coordinates. openblack's world is left-handed (GLM_FORCE_LEFT_HANDED:
// right = up x forward) and OpenAL's right-handed (right = at x up): every position, velocity and direction goes to
// OpenAL with x and z swapped, which turns one into the other; ListenerPosition gives the point back in openblack's axes.

namespace openblack::audio
{
class SampleOutput;
}

namespace openblack::audio::device
{

/// LH_AudioSystem::Create's wave device: the OpenAL device and its context made current (Locator's InitializeEngine).
/// False when there is none: the audio runs with a NullSampleOutput (nothing plays, LHWaveIsInstalled is false).
bool Open();
/// The channels' sources, then the waves' buffers, then the context and the device (Locator's ShutDownServices)
void Close();
/// The device is open (GAudio::IsInstalled 0x426D30 -> LHWaveIsInstalled 0x10015D20 as openblack sees it)
[[nodiscard]] bool IsOpen();
/// The 16 sample channels' device side: AlSampleOutput when the device is open, a NullSampleOutput when Open failed,
/// nullptr when Open was never called (the tests, which give sample_play an output of their own)
[[nodiscard]] SampleOutput* Output();

/// QSWaveMixSetListenerPosition 0x1000398E / Orientation 0x100039A7 (LHListenerUpdate 0x10003850) and Velocity (0 once,
/// at 0x10015C1A)
void SetListener(glm::vec3 position, glm::vec3 velocity, glm::vec3 forward, glm::vec3 up);
/// The listener's point as OpenAL has it (openblack's axes)
[[nodiscard]] glm::vec3 ListenerPosition();

// ---- sources (a QMixer channel each) --------------------------------------------------------------------------------

[[nodiscard]] SourceId CreateSource();
void DeleteSource(SourceId source);
/// The source's one buffer (0: none; on a stopped source that also unqueues every queued buffer)
void SetSourceBuffer(SourceId source, BufferId buffer);
/// AL_PITCH: the ratio to the wave's own rate (QSWaveMixSetFrequency)
void SetSourcePitch(SourceId source, float pitch);
/// AL_GAIN (QSWaveMixSetVolume's gain)
void SetSourceGain(SourceId source, float gain);
void SetSourceLooping(SourceId source, bool looping);
/// The position is in the listener's frame (2D, or a polar position)
void SetSourceRelative(SourceId source, bool relative);
void SetSourcePosition(SourceId source, glm::vec3 position);
/// QSWaveMixSetDistanceMapping {min, max, scale}: AL_REFERENCE_DISTANCE, AL_MAX_DISTANCE, AL_ROLLOFF_FACTOR
void SetSourceDistance(SourceId source, float minDistance, float maxDistance, float rolloff);
/// AL_ROLLOFF_FACTOR alone (0: no distance law of OpenAL's)
void SetSourceRolloff(SourceId source, float rolloff);
void PlaySource(SourceId source);
void StopSource(SourceId source);
void PauseSource(SourceId source);
/// AL_SOURCE_STATE (Stopped when it cannot be read)
[[nodiscard]] AudioStatus SourceStatus(SourceId source);
/// AL_SAMPLE_OFFSET: frames from the start of the first buffer still queued
[[nodiscard]] int32_t SourceSampleOffset(SourceId source);
/// AL_SEC_OFFSET
[[nodiscard]] float SourceSecondOffset(SourceId source);
/// AL_BUFFERS_PROCESSED
[[nodiscard]] int32_t SourceBuffersProcessed(SourceId source);
void QueueSourceBuffer(SourceId source, BufferId buffer);
/// The oldest processed buffer, taken off the queue
[[nodiscard]] BufferId UnqueueSourceBuffer(SourceId source);

// ---- buffers ----------------------------------------------------------------------------------------------------------

/// A buffer of 16-bit PCM (`count` samples, interleaved when stereo) at `rate` Hz
[[nodiscard]] BufferId CreateBuffer(ChannelLayout layout, const int16_t* samples, size_t count, int rate);
/// AL_LOOP_POINTS_SOFT {start, end} in frames, when OpenAL has AL_SOFT_loop_points; false without it
bool SetBufferLoopPoints(BufferId buffer, int32_t start, int32_t end);
void DeleteBuffer(BufferId buffer);
void DeleteBuffers(const std::vector<BufferId>& buffers);

} // namespace openblack::audio::device
