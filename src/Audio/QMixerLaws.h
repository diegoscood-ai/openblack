/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <glm/vec3.hpp>

// The arithmetic of LHaudiodllR and QMixer that turns a channel's LHaudio values into what is heard (layer 1 of
// dev\tmp_dis\audio\PLAN.md §2.1). Pure functions: test_audio_laws checks them against a Unicorn emulation of the DLLs
// (dev\tmp_dis\agua\re\emu_qmixer.py, emu_polar.py).

namespace openblack::audio::qmixer
{

/// LHSampleSetMasterVolume's range and the default of LH_AudioSystem+0x3C (ctor 0x10015290; BWSetup
/// AudioSampleMasterVolume when the registry has it, GAudio fn_00428250)
inline constexpr int k_MaxVolume = 127;

/// The gain QMixer applies for an LHaudio channel volume v (0..127) under the sample master volume m (0..127):
/// LHSampleSetVolume 0x10013400 / LHSamplePlay 0x100133C1..0x100133E3 send floor(m * v / 127) * 258 (the magic
/// 0x2040811 is the division by 127, then * 129 * 2) to QSWaveMixSetVolume (0..32766), which QMixer keeps as vol / 32767
/// (0x18007AE5). LHSampleSetMasterVolume 0x100150E0 re-sends the same product for every channel in use.
[[nodiscard]] float Gain(int volume, int master);

/// QMixer's distance gain (0x1800ACDF..0x1800AE1A with the channel flags of LHSamplePlay 0x10012065, 0x103 / 0x111:
/// neither 0x800 "clamp at max" nor 0x1000 "linear"; fn 0x1802CE50): 1 up to min or with scale 0,
/// min / (min + scale (d - min)) up to max, 0 beyond max
[[nodiscard]] float DistanceGain(float minDistance, float maxDistance, float scale, float distance);

/// The listener-space point QMixer hears a relative LHaudio position at (0x10012269: LHaudio's (x, y, z) -> azimuth
/// atan2(x, y) and elevation atan(z / |(x, y)|) in degrees with pi taken as 1 / 0.318471, range |(x, y, z)|;
/// QSWaveMixSetPolarPosition -> QMixer 0x1800AA85: right = r cos(el) sin(az), up = r sin(el), ahead = r cos(el) cos(az)).
/// So LHaudio's relative x is right, y ahead and z up. Returns (right, up, ahead).
[[nodiscard]] glm::vec3 PolarRelative(glm::vec3 position);

/// QSWaveMixSetFrequency(rate * percent / 100) as a ratio of the wave's rate: the unsigned integer division of
/// LHaudiodllR 0x10012820 (start) and LHSampleSetPitch 0x10013520
[[nodiscard]] float FrequencyRatio(int sampleRate, int percent);

/// The pitch of a starting sample (LHaudiodllR 0x1001278B..0x1001283B, unsigned integers): p = pitch (0 -> 100),
/// d = deviation * p / 100, p = p - d + rand * 2d / 32767 (rand = MSVC rand(), 0..32767), and 0 -> 100 again
[[nodiscard]] int StartPitch(int pitch, int deviation, int rand15);

} // namespace openblack::audio::qmixer
