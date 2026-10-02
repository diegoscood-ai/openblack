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

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <entt/core/fwd.hpp>

#include "Audio/GAudio/BankTables.h"

// The banks of openblack's audio: LHBankRegister 0x10002240 as GAudio calls it (src/Audio/GAudio, layer 2 of
// dev\tmp_dis\audio\PLAN.md §2.1, milestone B11a). The only place that reads a .sad: the sample banks (k_SfxBankPaths
// 0x9CB3F8 and the rest of Audio\, GAudio
// fn_00429CB0 / fn_0042A350, InitAtmos 0x428EF0 -> fn_00428F30), their anim effect tables (0x10002778..0x100029AB),
// the waves of the dialogue banks read at their first play (0x10011420 -> fn_100032D0) and the music banks of
// MUSIC_TYPE (0x9C9748, the GAudio ctor 0x426D40). audio::Init loads the sample banks (banks::LoadAll); the music banks
// are registered at their first use (music::GetBank).

namespace openblack::audio
{
class MusicBank;
class Sound;

/// Any registered bank: GAudio's LH_AudioBank* (the 11 of 0x9CB3F8, the 14 atmos ones, a creature's). 0 = none.
using BankId = uint16_t;
inline constexpr BankId k_NoBank = 0;

/// LHBankRegister 0x10002240 as openblack loads the .sad: every bank Game reads gets an id, by its path (the original
/// file system ignores case, so "audio/dialogue/Villagers.sad" of 0x9CB488 is villagers.sad on disk). `group` is the
/// sound group of its samples ("<file>.sad", their ids "<file>.sad/<n>"). The 11 types of 0x9CB3F8 are recognised by
/// path (fn_0042A390, 0x42A39D..0x42A3BA: GAudio+0x3A8 + 4 * type).
BankId RegisterBank(const std::filesystem::path& path, std::string_view group);
/// The size of a registered bank's sample table, as Game read it (the samples are 1..count)
void SetBankSampleCount(BankId bank, int samples);
/// LHBankGetNumberOfSamples (HelpDude::SaySentence 0x5BB389): 0 for no bank
[[nodiscard]] int BankSampleCount(BankId bank);
/// GAudio+0x3A8 + 4 * type (k_NoBank for type 0 or a bank not loaded)
[[nodiscard]] BankId Bank(SfxBank type);
/// The bank registered for a path (case-insensitive, any separator; matched on its end), k_NoBank if none
[[nodiscard]] BankId FindBank(std::string_view path);
/// The sound group of a bank ("InGame.sad"), empty for k_NoBank
[[nodiscard]] std::string BankGroup(BankId bank);
/// The sound id of sample `number` (1-based, .sad +0x104) of a bank: "<group>/<number>" (0 when the bank is unknown)
[[nodiscard]] entt::id_type SampleId(BankId bank, int number);

namespace banks
{

/// fn_00429CB0 / fn_0042A350 (the 11 types of 0x9CB3F8), InitAtmos fn_00428F30 (the 14 atmos banks) and the creature
/// banks: every sample bank (.sad) under Audio\, each through LHBankRegister(path, 0) 0x10002240, all from audio::Init
/// (approximated: InitAtmos 0x428EF0 runs later, from GGame::FinishInitialisation; nothing plays in between). The
/// dialogue banks (types 6..10, Audio\Dialogue) keep only their headers, each wave read from the file at its first play
/// (ReadWave); the others keep their bytes in memory (approximated: the original reads every bank that way, 0x426EEE).
/// A music bank (its waves are ".mpg") is left to MusicBankOf. Nothing without the file system and the resources (the
/// tests).
void LoadAll();

/// The banks registered, 1..Count()
[[nodiscard]] size_t Count();
/// A registered bank's path as LHBankRegister got it (lower case, '/' separators), empty for k_NoBank
[[nodiscard]] std::string Path(BankId bank);
/// The sound ids of a bank's samples LoadAll loaded (its empty records left out), in the .sad's order
[[nodiscard]] const std::vector<entt::id_type>& Samples(BankId bank);

/// The bytes of a wave left in its .sad (Sound::waveFile, the dialogue banks: LHBankRegister(path, 0) 0x10002240 reads
/// only the headers and a wave at its first play, 0x10011420 -> fn_100032D0). False when the sound has no such wave or
/// the file cannot be read.
[[nodiscard]] bool ReadWave(const Sound& sound, std::vector<uint8_t>& out);

/// The bank of a MUSIC_TYPE (0x9C9748, GAudio+0x2C + 4 * type), registered on its first use (LHBankRegister(path, 0)
/// 0x10002240 through MusicBank::Register; the original registers the 85 at once in the GAudio constructor 0x426D40:
/// milestone A5). nullptr if it is not installed (WELCOME_DANCE). `registeredNow` is set when this call registered it.
MusicBank* MusicBankOf(MusicType type, bool& registeredNow);
/// The music banks released (LHMusicClose 0x1000E7A0, after the music thread)
void ReleaseMusicBanks();

} // namespace banks
} // namespace openblack::audio
