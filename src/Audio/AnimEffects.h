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

#include "AudioSystem.h"

namespace openblack::pack
{
class PackFile;
}

// LHaudio's anim effects (layer 1 of dev\tmp_dis\audio\PLAN.md §2.1, milestone B2): the "anim effect" tables of each
// bank, read once when the bank is registered (LHBankRegister 0x10002240: LHFileSegmentAnimArray 0x10002778 into
// bank +0x124 / +0x12C width / +0x130 rows, LHFileSegmentAnimArrayWaveNumbers 0x100028E8), and the two
// LHSamplePlayAnimEffect of LHaudiodllR.dll. GAudio's filters in front of them are audio::SamplePlayAnimEffect
// (0x42A4B0, Audio.h). Research: dev\tmp_dis\anim\sounds_props.md A.3..A.5.

namespace openblack::audio
{

/// The 5 attributes a caller passes (long[5]): {voice, 2, group, surface, soundId} for the animations
/// (fn_00516510), {size, alignment, 1, surface, action} for PSysSound
using AnimKey = std::array<int32_t, 5>;
/// The 4th argument of GAudio::SamplePlayAnimEffect 0x42A4B0: 0 plays one sample of the row (0x42A4EC), any other value
/// goes to the key variant 0x100146F0: 1 stops the row's samples (0x1001491C), the others release their loops
/// (0x10014990)
enum class AnimAction : int32_t
{
	Play = 0,
	Stop = 1,
	Release = 2
};

/// The anim effect tables of a .sad bank: LHAudioAnimArrayTable rows of 5 attribute columns plus an index into
/// LHAudioWaveNumTable, whose lists are {count, sample numbers...} (the miracles' AnimEffectBank, moved here unchanged:
/// AnimEffectBank.h keeps that name as an alias)
struct AnimEffectTable
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
	/// one on a tie, fn_10014610): its sample numbers, empty if no row matches
	[[nodiscard]] std::vector<int32_t> FindList(const std::array<int32_t, 5>& key) const;
	/// The resource id of one of its samples ("<name>/<sample>")
	[[nodiscard]] entt::id_type SoundId(int32_t sample) const;
	[[nodiscard]] const Sample* FindSample(int32_t sample) const;
};

namespace anim_effects
{

/// LHSampleRegister3DObjectFunction(fn_00427200, 800.0) at 0x426E6B: LH_AudioSystem+0x44, the farthest an anim effect
/// plays, stops or releases (0x1001475A, 0x10014A8A: a distance, compared with the caller's)
inline constexpr float k_MaxDistance = 800.0f;

/// LHBankRegister 0x10002778..0x100029AB: the bank's tables, read from its file once as the bank is registered (Game's
/// bank loop). A bank without the two blocks has none.
void RegisterTables(BankId bank, const pack::PackFile& file);
/// The tables of a registered bank (nullptr when it has none)
[[nodiscard]] const AnimEffectTable* Tables(BankId bank);
/// Every bank's tables forgotten (the audio closing, the tests)
void Clear();

/// LHSampleGetAnimEffectNumber 0x10014670: the row of the key (LHFindAttribRow 0x10014420), then its list: 0 for none
/// or an empty one, the only sample of a list of 1, else list[LH_AudioSystem::Rand(count)] (0x100146CF)
[[nodiscard]] int Number(const AnimKey& key, BankId bank);

/// LHSamplePlayAnimEffect(owner, dist, sample, track, bank, min, max) 0x10014A20: nothing while the audio is off
/// (+0x14), for no bank, farther than k_MaxDistance (0x10014A8A) or than the sample's max distance (.sad +0x26C raw,
/// 0x10014ABD); options is3D 1 (+0x08), +0x10 0, track +0x0C, owner +0x20, the sample +0x24, min +0x54 / max +0x58 with
/// their caller bits 0x80 / 0x100 only when > 0 (0x10014ACC..0x10014B4F); the channel allocated (0x10011020) and the
/// point of the game's 3D function for the owner (fn_00427200, the camera for none; nothing for the atmos owner or an
/// unavailable thing, 0x10014B91..0x10014B9D); then LHSamplePlay. `distance` is the caller's (the camera's distance to
/// the object), not measured here.
Channel Play(Owner owner, float distance, int sample, bool track, BankId bank, float minDistance, float maxDistance);

/// LHSamplePlayAnimEffect(owner, dist, key, action, track, bank, min, max) 0x100146F0: the same gates (+0x14, bank,
/// k_MaxDistance 0x1001475A), the key's row (none: nothing); action 0 plays one of its samples as Play (0x100147D0;
/// GAudio never calls it with 0); 1 stops, for each sample of the list, the first channel of (bank, owner, sample)
/// (0x1001491C..0x10014989: LHSampleStop); any other value releases its loop (0x10014990..0x100149F5:
/// LHSampleReleaseLoop)
Channel PlayKey(Owner owner, float distance, const AnimKey& key, AnimAction action, bool track, BankId bank,
                float minDistance, float maxDistance);

} // namespace anim_effects
} // namespace openblack::audio
