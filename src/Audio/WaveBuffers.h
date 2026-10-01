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

#include "Sound.h"

// The waves of the .sad banks, decoded at their first use (layer 0/1 of dev\tmp_dis\audio\PLAN.md §2.1, milestone B0).
//
// The original registers the banks with only their headers (LHBankRegister 0x10002240 with inMemory 0, every call of
// the game) and reads a wave from the open file when a sample first starts (0x10011420 -> fn_100032D0), keeping it in a
// FIFO cache of RAM / 8 (0x10042F80, [0x100383B8]); QMixer converts ADPCM and MPEG with ACM (QSWaveMixOpenWaveEx,
// engine.md §1.4). openblack keeps the .sad bytes of each sample (Sound::buffer; the dialogue banks of Audio\Dialogue
// are read as the original does, headers only, and a wave is read from the file when it is decoded: Sound::waveFile),
// decodes them once to PCM at the first use and keeps one OpenAL buffer per sample until the audio closes (no eviction: the budget of the original only
// matters with less than 1 GB of RAM) (approximated: one buffer per sample record, while the original caches per wave
// +0x108, so the clones of a wave are decoded once each).

namespace openblack::audio::wave_buffers
{

/// A decoded wave
struct Pcm
{
	std::vector<int16_t> samples;
	ChannelLayout layout {ChannelLayout::Mono};
	int sampleRate {0};
	[[nodiscard]] size_t Frames() const
	{
		return layout == ChannelLayout::Stereo ? samples.size() / 2 : samples.size();
	}
};

/// The bytes of a wave left in its .sad (Sound::waveFile, the dialogue banks: LHBankRegister(path, 0) 0x10002240 reads
/// only the headers and a wave at its first play, 0x10011420 -> fn_100032D0). False when the sound has no such wave or
/// the file cannot be read.
[[nodiscard]] bool ReadWave(const Sound& sound, std::vector<uint8_t>& out);

/// The sample's wave as PCM (read from its .sad first when it was left there). A .sad wave is a RIFF file (LHaudio opens it as memory with QSWaveMixOpenWaveEx,
/// 0x10011CB3): wFormatTag 1 (PCM) and 2 (MS-ADPCM) go to dr_wav; 0x50 (MPEG-1/2 layer II: all of HelpSprites and
/// villagers, most of Guidance) has its "data" chunk decoded by dr_mp3, as ACM does. A wave that is not RIFF is tried as
/// raw MPEG (the music segments). False when nothing decodes (an empty sample: InGame 165, spells 31).
[[nodiscard]] bool Decode(const Sound& sound, Pcm& out);

/// The sample's OpenAL buffer, decoded and made at the first call and kept in `sound.bufferId` (with its duration and
/// size); 0 when the wave does not decode or there is no OpenAL context. With a loop section (+0x138 < +0x13C, both set)
/// the buffer gets it as AL_LOOP_POINTS_SOFT, so a looping channel repeats only that section after its first pass, as
/// QMIXPLAYPARAMS +0x18 / +0x1C do (0x10012949).
BufferId Get(Sound& sound);

/// The buffer of a sample that is going away (a music bank Sound erased by AudioManager::StopMusic)
void Release(Sound& sound);

/// Every buffer made, deleted (the audio closes: after every source that used them)
void DeleteAll();

/// Statistics for the traces and the debug panel: the buffers alive and the ones ever made (alGenBuffers calls)
[[nodiscard]] size_t Alive();
[[nodiscard]] size_t Made();

} // namespace openblack::audio::wave_buffers
