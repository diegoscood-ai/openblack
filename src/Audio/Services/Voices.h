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

#include <glm/vec3.hpp>

#include "Audio/GAudio/BankTables.h"
#include "Audio/LH/SamplePlay.h"

// The voices of the help texts (milestone A10 of dev\tmp_dis\audio\PLAN.md): the table HELP_TEXT -> {bank, sample} of
// runblack.exe W120, 0x915D40 (identical copies at 0x942B38, 0x957310 and 0x96BA30), rebuilt from the data because it is
// not in any data file: the name of a text is the name of the wave of its sample (dev\tmp_dis\audio\voices.md §2.2, §3.2;
// voices_namerule.py). Playing them (milestone B7): RunTextVoice, Say, IsSaying, CutByClick below; the advisors in
// Advisor.h.

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
/// The table of the tests
void SetTable(VoiceTable table);

/// fn_005C62F0 0x5C631E: GAudio+0x3A8 + 4 * bank != 0, the bank of a voice is registered
[[nodiscard]] bool BankRegistered(SfxBank bank);

/// The voice part of fn_005C5F90 (0x5C6025..0x5C60DB), after the text is shown and kept in the history:
///  - HelpSprites (6) and narrator 2 (the good spirit) / 3 (the evil one): fn_005C3750 (advisor::Stop) of dude 1, then
///    of dude 0, then HelpDudeControl::Say(0 / 1, sample, 0) (advisor::Say: owner 0x270C, LHSamplePlay directly);
///  - any other voice with a sample and a bank: a default LH_SamplePlayOptions (ctor 0x5C60A8) with the bank +0x04 =
///    GAudio+0x3A8 + 4 * bank, the sample +0x24, the owner +0x20 = 0x270F and +0x164 = 1: 2D, the .sad decides the
///    rest; GAudio::PlaySoundEffect 0x429E30 (with its filters: inside the citadel only user parameter 2 plays, and after
///    SET_GAME_SOUND false only HelpSprites / villagers).
/// Returns the channel of the second branch (the advisor's sentence starts in advisor::Update).
Channel RunTextVoice(int32_t narrator, TextVoice voice);

/// GScript::SaySoundEffect 0x70F8E0(text, withPosition, alt, LHPoint*) (GAME_PLAY_SAY_SOUND_EFFECT 0x70F9B0): text 0 for
/// one >= 6974 (0x70F8EA); the say table 0x942B38: nothing without a sample (0x70F90A), the bank 0 when the entry's id
/// is not the text (0x70F910, never in W120's table); a default LH_SamplePlayOptions with the bank, the sample, the owner
/// alt ? 0x270D : 0x270F (0x70F931..0x70F938), is3D +0x08 = withPosition, the point +0x30 only when withPosition
/// (0x70F970..0x70F986), track +0x0C = 0 (0x70F98F), +0x164 = 1; GAudio::PlaySoundEffect 0x429E30. No text and no
/// advisor, even for HelpSprites.
Channel Say(uint32_t textId, bool withPosition, bool alt, glm::vec3 position);

/// SAY_SOUND_EFFECT_PLAYING 0x710280(alt, text): text 0 for one >= 6974 (0x7102A7); false without a sample in the say
/// table (0x7102C3); else fn_0042A280(alt ? 0x270D : 0x270F, sample, the entry's bank) = LHSampleIsPlaying (the id is
/// not compared here)
[[nodiscard]] bool IsSaying(bool alt, uint32_t textId);

/// HelpSystem::ProcessInterface 0x5C6A9E..0x5C6AAD: GAudio::StopPlayingSoundEffect(0, 0x270F, VILLAGERS) 0x42A210 =
/// LHSampleStop(villagers, 0x270F, 0): every narration of villagers.sad, each with the 20 ms ramp. (The narration of
/// the other banks is not cut; the advisors are cut by fn_005C6720, advisor::Interrupt.)
void CutByClick();
} // namespace voices

} // namespace openblack::audio
