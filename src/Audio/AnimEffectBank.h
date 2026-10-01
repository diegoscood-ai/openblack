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

#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/core/fwd.hpp>

namespace openblack::pack
{
class PackFile;
}

namespace openblack::audio
{

/// The "anim effect" tables of a .sad bank (LHaudiodllR.dll): LHAudioAnimArrayTable rows of 5 attribute columns plus an
/// index into LHAudioWaveNumTable, whose lists are {count, sample numbers...}. The game passes 5 attributes and the DLL
/// plays one sample of the best matching row's list: editor.sad for the animation sounds (AnimationSounds), spells.sad
/// for the particle system sounds (SpellSounds).
struct AnimEffectBank
{
	static constexpr int32_t k_Wildcard = 0x0FFF0000;

	/// The per-sample playback fields that matter to the callers, from the 0x280-byte LH_BankSample (only counted when
	/// their override bit at +0x244 is set; defaults from the LH_SamplePlayOptions ctor 0x10010E90)
	struct Sample
	{
		int32_t loops {0};     ///< +0x248 (bit 0x40): -1 = forever
		int32_t playMode {3};  ///< +0x274 (bit 0x400): 1 new channel, 2 nothing if already playing for that object, 3 restart
		float maxDistance {0}; ///< +0x26C
	};

	std::string name;                         ///< the sound ids are "<name>/<sample number>"
	std::vector<std::array<int32_t, 6>> rows; ///< LHAudioAnimArrayTable
	std::vector<int32_t> waves;               ///< LHAudioWaveNumTable
	std::unordered_map<int32_t, Sample> samples;

	/// Reads the tables of the bank file (nothing when it is missing or has none)
	void Load(const std::filesystem::path& path);
	void Load(const pack::PackFile& file);
	/// LHFindAttribRow (LHaudiodllR 0x10014420): the list of the matching row with the most exact columns (the later
	/// one on a tie): its sample numbers, empty if no row matches
	[[nodiscard]] std::vector<int32_t> FindList(const std::array<int32_t, 5>& key) const;
	/// The resource id of one of its samples ("<name>/<sample>")
	[[nodiscard]] entt::id_type SoundId(int32_t sample) const;
	[[nodiscard]] const Sample* FindSample(int32_t sample) const;
};

} // namespace openblack::audio
