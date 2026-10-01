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

#include <string>
#include <string_view>
#include <vector>

#include "BankTables.h"

// The voices of the help texts (milestone A10 of dev\tmp_dis\audio\PLAN.md): the table HELP_TEXT -> {bank, sample} of
// runblack.exe W120, 0x915D40 (identical copies at 0x942B38, 0x957310 and 0x96BA30), rebuilt from the data because it is
// not in any data file: the name of a text is the name of the wave of its sample (dev\tmp_dis\audio\voices.md §2.2, §3.2;
// voices_namerule.py). Playing them (RunTextVoice, SaySound, the advisors) is milestone B7.

namespace openblack::audio
{

/// The owners ("attached object", +0x20 of LH_SamplePlayOptions) of the voice channels (voices.md §2.3, §3.4)
enum class VoiceOwner : uint32_t
{
	Advisor = 0x270C,   ///< HelpDude::SaySentence 0x5BB340 (the two spirits)
	SayExtra = 0x270D,  ///< GAME_PLAY_SAY_SOUND_EFFECT with alt (SaySoundEffect 0x70F8E0)
	StopOnly = 0x270E,  ///< only STOP_SOUND_EFFECT(isSay) 0x70FA50 stops it; nothing plays it in W120
	Narration = 0x270F, ///< fn_005C5F90 (RUN_TEXT not by a spirit) and GAME_PLAY_SAY_SOUND_EFFECT without alt
};

/// One entry of 0x915D40 ({u32 id, u32 bank, u32 sample}, 12 bytes): bank 0 = no voice; sample 1-based in the bank
struct TextVoice
{
	SfxBank bank {SfxBank::None};
	uint32_t sample {0};

	/// The part of fn_005C62F0 that reads the table (0x5C630A..0x5C631C): a bank and a sample
	[[nodiscard]] bool HasVoice() const { return bank != SfxBank::None && sample != 0; }
};

/// The key a wave name is matched by: the last component of AudioBankSampleHeader::name (e.g.
/// "K:\4frosty\Spanish\1622p\HELP_TEXT_X.wav") without ".wav", in upper case
[[nodiscard]] std::string VoiceSampleKey(std::string_view waveName);

class VoiceTable
{
public:
	/// The wave names of a bank (AudioBankSampleHeader::name), in sample order: sample n is element n - 1
	using SampleNames = std::vector<std::string>;

	/// The rule of voices.md §3.2: text i (the i-th ADD_TEXT) has the first sample whose wave is named like the text,
	/// looked up in villagers (7), then HelpSprites (6), then Guidance (10). It gives 0x915D40 exactly (6974 texts, 1922 in
	/// HelpSprites, 1328 in villagers, 227 in Guidance). The only name in two banks, HELP_TEXT_LAND_2_WORKSHOP_10
	/// (HelpSprites 801 and villagers 399), is villagers 399 in the exe: hence villagers first (inferred: one case). Never
	/// VillagersBanter (8) or SpellDialogue (9): the exe gives none of their samples to a text (35 names would match).
	[[nodiscard]] static VoiceTable Build(const std::vector<std::string>& textNames, const SampleNames& villagers,
	                                      const SampleNames& helpSprites, const SampleNames& guidance);

	/// The entry of a text; no voice for an id out of the table
	[[nodiscard]] TextVoice Get(uint32_t textId) const;
	[[nodiscard]] size_t Size() const { return _voices.size(); }

private:
	std::vector<TextVoice> _voices;
};

namespace voices
{
/// The dialogue banks of the table: villagers (7), HelpSprites (6) and Guidance (10)
[[nodiscard]] bool IsTableBank(SfxBank bank);
/// The wave names of a dialogue bank, as Game reads the .sad of k_SfxBankPaths (call before BuildTable)
void SetBankSampleNames(SfxBank bank, VoiceTable::SampleNames names);
/// Builds the game's table from the help texts (helptext::GetEntry names) and the names set so far
void BuildTable();
/// The game's table (empty until BuildTable)
[[nodiscard]] const VoiceTable& Table();
} // namespace voices

} // namespace openblack::audio
