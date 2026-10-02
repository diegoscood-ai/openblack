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
#include <functional>
#include <string>
#include <string_view>

#include "Audio/Services/Voices.h"
#include "Common/HelpText.h"

// The text part of HelpSystem (runblack.exe W120, g_game+0x25005C), milestone A11 of dev\tmp_dis\audio\PLAN.md: the
// current text and the five before it, the history, ClearAllText, IsTextRead, the click that ends a text, and the CHL
// functions RUN_TEXT, RUN_TEXT_WITH_NUMBER, TEMP_TEXT, TEMP_TEXT_WITH_NUMBER, TEXT_READ, GAME_CLEAR_DIALOGUE and
// GAME_CLOSE_DIALOGUE. Nothing is drawn (the HelpText display, fn_005CCED0, is not ported): OPENBLACK_TEXT_TRACE logs
// each text. The voices (milestone B7) plug in through the hooks and queries below (Game.cpp: audio::voices and
// audio::advisor); with Queries::voiceBankLoaded unset no text has a voice, so IsTextRead takes the reading-time branch,
// as the original does when the dialogue banks are not registered.
// Sources: dev\tmp_dis\audio\voices.md §2.3-2.4, script.md §2.3 and the disassembly of 0x5C5550..0x5C6E00,
// 0x5CB0F0..0x5CBF00 and 0x6F7C70..0x6F8280 (bwdis.py), cited at each step.

namespace openblack::help
{

/// The history of texts: 1024 entries (fn_005C5EE0 0x5C5EE7, 0x5C5F30)
constexpr size_t k_HistorySize = 0x400;
/// What ProcessInterface returns when the click was used by the text (0x5C6A25, 0x5C6ABA)
constexpr int k_ClickTaken = 0x14;

/// fn_005CBEC0: the number of words of a text, the non-empty pieces its splitter fn_005CB590 returns. Words end at a
/// blank (space, tab, CR, LF, 0xF8FE: fn_005CB0F0), at a '$' or '\' (fn_005CB190) or after 47 characters (0x5CB6E3);
/// "$$" / "\\" (any two of them) is a literal character; '$' or '\' and a code (a digit or one of CDFMNP in either case:
/// fn_005CB1D0 / fn_005CB220) with its digits are not words (fn_005CB2A0 / fn_005CB4E0; after C one more character is
/// skipped, 0x5CB53A); any other character after them is the start of a word.
[[nodiscard]] uint32_t CountWords(std::u16string_view text);

/// fn_005C6CB0: the factor of READ_SPEED r on the reading time, r <= 0.5 ? 3 - 4r : (1 - 2(r - 0.5)) * 0.8 + 0.2
/// (constants 0.5 0x8AB260, 4 0x8AB418, 3 0x8C2C50, 1 0x8AB680, 0.8 0x900AC8, 0.2 0x8C7C68)
/// (in the x87 registers: kept in double here, as the original multiplies it in fn_005C61B0 without rounding to float)
[[nodiscard]] double ReadSpeedFactor(float readSpeed);

/// Where fn_005C5F90 sends the voice of a text (0x5C6025..0x5C60DB)
enum class VoiceRoute : uint8_t
{
	None,       ///< no bank or no sample: the text is only shown
	GoodSpirit, ///< HelpSprites and narrator 2: StopSentence of both advisors, HelpDudeControl::Say(0, sample, 0)
	EvilSpirit, ///< HelpSprites and narrator 3: StopSentence of both advisors, HelpDudeControl::Say(1, sample, 0)
	Narration,  ///< any other: GAudio::PlaySoundEffect 2D, owner 0x270F, +0x164 = 1
};
[[nodiscard]] VoiceRoute RouteOf(int32_t narrator, audio::TextVoice voice);

/// HelpSystem::GetSpiritWhoTalks 0x5C6E20(text): the narrator of HelpTextDatabase[0 < text < count ? text : 0]: 2 (the
/// good spirit) -> 1, 3 (the evil one) -> 2, any other -> 0
[[nodiscard]] int32_t SpiritWhoTalks(int32_t narrator);

/// GScript::ConvertScriptSpiritToHelpSpirit 0x710350 (the jump table 0x710400): SCRIPT_SPIRIT_TYPE (ScriptEnums.h
/// SpiritType) -> the help spirit 1 (good) / 2 (evil): 2 (EVIL) -> 2; 3 (ALIGNMENT) -> discrete alignment < 3 ? 2 : 1;
/// 4 (ANTI_ALIGNMENT) -> >= 3 ? 2 : 1; 5 (RANDOM) -> LocalRand(100) > 50 ? 2 : 1; any other (NONE, GOOD) -> 1.
/// `discreteAlignment`: GAlignment::GetDiscreteAlignmentValue 0x414730 of the local player's GPlayer::GetAlignmentValue
/// (g_game+0x205A59); `rand100`: GRand::LocalRand(100) (asked only for RANDOM)
[[nodiscard]] int32_t ConvertScriptSpiritToHelpSpirit(int32_t type, int discreteAlignment,
                                                      const std::function<int()>& rand100);

/// One entry of the history (fn_005C5EE0: 16 bytes at +0x5C4)
struct HistoryEntry
{
	uint32_t textId;
	int32_t withInteraction;
	float number; ///< the number of RUN_TEXT_WITH_NUMBER (0 for RUN_TEXT)
	int32_t narrator;
};

class HelpSystem
{
public:
	/// HelpSystemInfo (info.dat, global 0xD16160)
	struct Info
	{
		uint32_t readDefaultAdjustGTTime; ///< +0x18 [0xD16178] (8 in info.dat)
		uint32_t readDefaultWordGTTime;   ///< +0x1C [0xD1617C] (5 in info.dat)
	};

	/// What the help system reads from the rest of the game; unset gives the value written next to each
	struct Queries
	{
		/// HelpTextDatabase[id] (0xD17CA8). Unset: helptext::GetEntry.
		std::function<helptext::Entry(uint32_t textId)> textEntry;
		/// The voice table 0x915D40. Unset: audio::voices::Table().
		std::function<audio::TextVoice(uint32_t textId)> textVoice;
		/// g_game+0x205A40, the game turn. Unset: 0.
		std::function<uint32_t()> turn;
		/// (GetTickCount() - [0xEA1C78]) * [0xEA1C80] + [0xEA1C7C], the scaled clock in milliseconds. Unset: 0.
		std::function<int32_t()> nowMs;
		/// g_game+0x205A28 == 1 && (g_game+0x14 & 4): inside the citadel the times are in milliseconds (0x5C6463,
		/// 0x5C68E8). Unset: false.
		std::function<bool()> citadelClock;
		/// ScriptDLL::GetScriptType 0x6F6C50: the VMScriptType of a task (1 Script, 2 Help, ...; 1 when there is no such
		/// task, ScriptLibraryR.dll 0x100051F0). Unset: 1.
		std::function<uint32_t(uint32_t taskNumber)> taskScriptType;
		/// [0xE85410] & 0xFF, the key that skips a text (ProcessInterface 0x5C69B2; which key: not read). Unset: false.
		std::function<bool()> skipKey;
		/// GGame::MyInterface()->IsPlayBack(0) (0x5C69ED). Unset: false.
		std::function<bool()> playBack;
		/// GAudio+0x3A8 + 4 * bank != 0: the bank is registered (fn_005C62F0 0x5C631E). Unset: false, so no text has a
		/// voice (milestone B7).
		std::function<bool(audio::SfxBank)> voiceBankLoaded;
		/// HelpSystem+0x10 (HelpDudeControl): +0x74 && fn_005BB730(+4) || +0x78 && fn_005BB730(+8), an advisor talks or
		/// stopped less than 200 ms ago (0x5C6372..0x5C63A0). Unset: false.
		std::function<bool()> advisorsTalking;
		/// GAudio fn_0042A280(owner, sample, bank) = LHSampleIsPlaying (0x5C63CA). Unset: false.
		std::function<bool(audio::SfxBank, audio::VoiceOwner, uint32_t sample)> isPlaying;
	};

	/// Where the parts that are not ported plug in
	struct Hooks
	{
		/// HelpText fn_005CCED0(text, number, narrator) from fn_005C6100: show the text
		std::function<void(const std::u16string& text, float number, int32_t narrator)> showText;
		/// The voice branches of fn_005C5F90 (milestone B7)
		std::function<void(uint32_t textId, VoiceRoute route, audio::TextVoice voice)> sayVoice;
		/// ProcessInterface 0x5C6A9E..0x5C6AAD: GAudio::StopPlayingSoundEffect(0, 0x270F, VILLAGERS) (audio::voices::
		/// CutByClick), after fn_005C6720(spirit 1, 1) and fn_005C6720(spirit 2, 1) (0x5C6A88 / 0x5C6A93: spiritStop)
		std::function<void()> stopVoicesOnClick;
		/// HelpSystem::SpiritHome 0x5C6670(spirit, arg) -> fn_005C5200 on the spirit (fn_005C68A0: 1 -> HelpSystem+0xC,
		/// any other -> +8): spirit+0x58 / +0x5C = 0 and HelpDudeControl (HelpSystem+0x10) fn_005C3540(dude) when arg != 0
		/// (the dude's state +0xC = 1 at once, 0x5C357A), fn_005C3590(dude) when arg == 0 (fn_005C2E90 / fn_005BBDD0
		/// first: inferred, the advisor flies off); dude = spirit+0x54 != 1 (fn_005C5250). The advisors are not ported.
		std::function<void(int32_t spirit, int32_t arg)> spiritHome;
		/// fn_005C6720(spirit, arg) -> fn_005C4C20 -> HelpDudeControl fn_005C3780(dude, arg) (its W120 symbol
		/// MacAdjustHelpID is wrong: it reads HelpDude::IsTalking 0x5BB760): audio::advisor::Interrupt
		std::function<void(int32_t spirit, int32_t arg)> spiritStop;
		/// The rest of HelpSystem::SetWideScreen 0x5C6AD0 when +0x45E8 changes: DialogBoxBase::HideAll (on),
		/// GInterface::SetActive(!(on && owner)) and the bars' timer +0x45F0 (wideScreenTime 0xD16174 * 1000 * the part
		/// already done, 0x5C6B3F..0x5C6B4E): openblack's bars are ScreenFade::SetWideScreen
		std::function<void(bool on)> wideScreen;
	};

	HelpSystem(Info info, Queries queries, Hooks hooks);

	/// GScript::RunText 0x6F7D60 (CHL 13 RUN_TEXT)
	void RunText(bool singleLine, uint32_t textId, int32_t withInteraction);
	/// GScript::RunTextWithNumber 0x6F7C70 (CHL 232 RUN_TEXT_WITH_NUMBER)
	void RunTextWithNumber(bool singleLine, uint32_t textId, float number, int32_t withInteraction);
	/// GScript::TempText 0x6F7E40 (CHL 14 TEMP_TEXT): u"*" + the string, narrator 1, text id 0, no voice, no history
	void TempText(bool singleLine, std::u16string_view text, int32_t withInteraction);
	/// GScript::TempTextWithNumber 0x6F7F50 (CHL 231 TEMP_TEXT_WITH_NUMBER)
	void TempTextWithNumber(bool singleLine, std::u16string_view text, float number, int32_t withInteraction);
	/// GScript::GameClearDialogue 0x6FF6F0 (CHL 411): ClearAllText
	void ClearDialogue();
	/// GScript::GameCloseDialogue 0x6FF700 (CHL 412): HelpText fn_005CB010 (+0xB4 = 0, +0xAC = 1: display only, and
	/// HelpText::Reset clears both again) and ClearAllText
	void CloseDialogue();

	/// fn_005C5F90: show a help text (fn_005C6100), keep it in the history (fn_005C5EE0) and say its voice
	void SayText(uint32_t textId, int32_t withInteraction, float number);
	/// fn_005C6100: show a text, start its reading time and make it the current one
	void AddText(const std::u16string& text, int32_t withInteraction, float number, int32_t narrator, uint32_t textId);
	/// HelpSystem::ClearAllText 0x5C5550: HelpText::Reset(1) (0x5CB020, which also clears the single line flag +0xB0),
	/// the six text ids and ClearTextDisplayed
	void ClearAllText();
	/// HelpSystem::ClearTextDisplayed 0x5C54E0: +0x580, +0x57C, +0x45DC and +0x45E4 = 0 (the KMIcons +0x24 / +0x28 it
	/// deletes, 0x5C54F1..0x5C5522, are display only: not ported)
	void ClearTextDisplayed();
	/// The text part of HelpSystem::Reset 0x5C5580 (from GScript::Reset 0x6EB340): ClearAllText, SetWideScreen(0, 0)
	/// (0x5C55D6) and the history emptied (+0x45C8 / +0x45C4 = 0, 0x5C55EA). The rest (+0x578, +0x568, the spirits'
	/// arrays +0x78 / +0x2FC, fn_005C6C40, ResetIcons) is not ported. +0x2D8 (9 category turns) is zeroed and +0x45F8 = 1
	/// (0x5C55FC). It does not touch +0x45CC
	void Reset();
	/// HelpSystem::IsTextRead 0x5C64E0 (CHL 15 TEXT_READ)
	[[nodiscard]] bool IsTextRead() const;
	/// HelpSystem::ProcessInterface 0x5C69B0 from GInterface (0x5D11C0: `click` is bit 5 of GInterface+0x39) (inferred:
	/// that bit is the left button going down; openblack calls it on that event). 1, or k_ClickTaken.
	int ProcessInterface(bool click);

	/// HelpSystem::IsDialogueControlled 0x5C6740: a script task has the dialogue (+0x45CC != 0) or holds the wide screen
	/// (+0x45E8 && +0x45EC). IS_DIALOGUE_READY pushes its negation (0x710846)
	[[nodiscard]] bool IsDialogueControlled() const { return _dialogueOwner != 0 || IsScriptWideScreen(); }
	/// +0x45CC: the script task that has the dialogue (0: none)
	[[nodiscard]] uint32_t GetDialogueOwner() const { return _dialogueOwner; }
	/// HelpSystem::SetCurrentControl 0x5C6780: +0x45CC = task
	void SetCurrentControl(uint32_t task) { _dialogueOwner = task; }
	/// HelpSystem::DialogueControlRequest 0x5C6790: nothing (false) while IsDialogueControlled; else the task takes the
	/// dialogue and ClearAllText (true)
	bool DialogueControlRequest(uint32_t task);
	/// HelpSystem::ClearDialogueControl 0x5C67E0: +0x45CC = 0 and HelpText fn_005CB010 (+0xB4 = 0, +0xAC = 1: display
	/// only, not ported)
	void ClearDialogueControl();
	/// fn_005C6800(task) (no W120 symbol; END_DIALOGUE 0x7107F9 and the task-stop callback 0x6EC6E9 call it): when the
	/// task has the dialogue, give it back, take the wide screen away and send both advisors home
	void ReleaseDialogueControl(uint32_t task);
	/// HelpSystem::SpiritHome 0x5C6670 (Hooks::spiritHome)
	void SpiritHome(int32_t spirit, int32_t arg);
	/// HelpSystem::SetWideScreen 0x5C6AD0(on, owner): when +0x45E8 changes, +0x45E8 = on and +0x45EC = on ? owner : 0
	/// (0x5C6B17..0x5C6B31), then Hooks::wideScreen
	void SetWideScreen(int32_t on, uint32_t owner);
	/// +0x45E8 && +0x45EC: a script task holds the wide screen (IsDialogueControlled 0x5C674A, ProcessInterface 0x5C6A2E,
	/// GAudio::PlaySoundEffect userParam 1, ProcessAlignmentMusic 0x4279E9)
	[[nodiscard]] bool IsScriptWideScreen() const { return _wideScreen != 0 && _wideScreenOwner != 0; }
	/// +0x45E8: the wide screen is on
	[[nodiscard]] int32_t GetWideScreen() const { return _wideScreen; }
	/// +0x45EC: the script task that set it (0: none, or set by the game)
	[[nodiscard]] uint32_t GetWideScreenOwner() const { return _wideScreenOwner; }

	/// fn_005C6C90: HelpSystem+0x4610, the profile's READ_SPEED (fn_005C6CF0 0x5C6DC6; 0.5 without a profile, 0x5C6DEC)
	void SetReadSpeed(float readSpeed) { _readSpeed = readSpeed; }
	[[nodiscard]] float GetReadSpeed() const { return _readSpeed; }

	/// +0x45F8: the help switch (HelpSystem::Reset 0x5C55FC sets 1; SET_HELP_SYSTEM 0x6FC03D stores the popped value)
	void SetHelpOn(uint32_t on) { _helpOn = on; }
	[[nodiscard]] uint32_t GetHelpOn() const { return _helpOn; }
	/// +0x45F4, fn_005C79C0 (its ResetFOV 0x5C9300 is not ported): the profile's HELP_LEVEL, 3 when the profile has none
	/// (fn_005C6CF0 0x5C6DB6; openblack has no profiles)
	void SetHelpLevel(int32_t level) { _helpLevel = level; }
	[[nodiscard]] int32_t GetHelpLevel() const { return _helpLevel; }
	/// HELP_SYSTEM_ON (GScript::HelpSystemOn 0x6FBFD0): +0x45F8 && +0x45F4 != 0
	[[nodiscard]] bool IsHelpSystemOn() const { return _helpOn != 0 && _helpLevel != 0; }
	/// The level GGuidance::PlayNow reads (0x71AF99..0x71AFB1): +0x45F8 ? +0x45F4 : 0
	[[nodiscard]] int32_t GetGuidanceLevel() const { return _helpOn != 0 ? _helpLevel : 0; }
	/// HelpSystem::TriggerCategory 0x5C8280(category): +0x2D8 + 4 category = the turn (Reset zeroes the 9, 0x5C55BE)
	void TriggerCategory(int32_t category);
	[[nodiscard]] uint32_t GetCategoryTurn(int32_t category) const;
	/// +0x560: the turn of the last help message started (RunMessage 0x5C8D0C; SetToZero 0x5C54A4 zeroes it)
	void SetMessageTurn(uint32_t turn) { _messageTurn = turn; }
	[[nodiscard]] uint32_t GetMessageTurn() const { return _messageTurn; }

	/// fn_005C5F50: the i-th most recent text of the history (0 = the last), nullptr past the ones kept
	[[nodiscard]] const HistoryEntry* GetHistory(int32_t i) const;
	[[nodiscard]] int32_t GetHistoryCount() const { return _historyCount; }

	/// +0x584: the current text (0 after ClearAllText); +0x588..+0x598 the five before it
	[[nodiscard]] const std::array<uint32_t, 6>& GetTexts() const { return _texts; }
	/// +0x57C: withInteraction == 1, the text waits for a click
	[[nodiscard]] bool IsWaitingForClick() const { return _waitClick; }
	/// +0x45D8 / +0x45DC: the turn the text started and the turn after which it is read
	[[nodiscard]] uint32_t GetStartTurn() const { return _startTurn; }
	[[nodiscard]] uint32_t GetEndTurn() const { return _endTurn; }

private:
	/// fn_005C61B0: the reading time of a text without voice
	void StartReadingTime(std::u16string_view text);
	/// fn_005C62F0: the text has a voice to wait for (table entry, bank, sample and the bank registered)
	[[nodiscard]] bool HasVoice(uint32_t textId) const;
	/// fn_005C68C0: the text has been shown long enough for a click to count
	[[nodiscard]] bool ShownLongEnough() const;
	/// fn_005C5EE0
	void AddHistory(uint32_t textId, int32_t withInteraction, float number, int32_t narrator);

	[[nodiscard]] uint32_t Turn() const { return _queries.turn ? _queries.turn() : 0; }
	[[nodiscard]] int32_t NowMs() const { return _queries.nowMs ? _queries.nowMs() : 0; }
	[[nodiscard]] bool CitadelClock() const { return _queries.citadelClock && _queries.citadelClock(); }

	Info _info;
	Queries _queries;
	Hooks _hooks;

	bool _singleLine {false};                            ///< HelpText +0xB0 (helpText = HelpSystem+0x14)
	bool _waitClick {false};                             ///< +0x57C
	bool _noClick {false};                               ///< +0x580: withInteraction == 2, a click does nothing
	std::array<uint32_t, 6> _texts {};                   ///< +0x584..+0x598
	std::array<HistoryEntry, k_HistorySize> _history {}; ///< +0x5C4
	int32_t _historyNext {0};                            ///< +0x45C4
	int32_t _historyCount {0};                           ///< +0x45C8
	uint32_t _startTurn {0};                             ///< +0x45D8
	uint32_t _endTurn {0};                               ///< +0x45DC
	int32_t _startMs {0};                                ///< +0x45E0
	mutable int32_t _endMs {0};                          ///< +0x45E4 (IsTextRead moves it while the narration plays, 0x5C640F)
	/// +0x460C: GInterface::SetActive 0x5CEDC0 sets it to bit 0 of GInterface+0x39 (0x5CEDD2..0x5CEDDF), ProcessInterface
	/// clears it. openblack has no GInterface::SetActive, so it stays false (not ported)
	bool _clickPending {false};
	float _readSpeed {0.5f};       ///< +0x4610
	uint32_t _helpOn {1};          ///< +0x45F8 (Reset 0x5C55FC)
	int32_t _helpLevel {3};        ///< +0x45F4 (fn_005C6CF0 0x5C6DB6 without a profile)
	std::array<uint32_t, 9> _categoryTurns {}; ///< +0x2D8 (Reset 0x5C55BE: 9 dwords)
	uint32_t _messageTurn {0};     ///< +0x560
	uint32_t _dialogueOwner {0};   ///< +0x45CC (SetToZero 0x5C547B -> ClearDialogueControl)
	int32_t _wideScreen {0};       ///< +0x45E8 (Reset 0x5C55D6: SetWideScreen(0, 0))
	uint32_t _wideScreenOwner {0}; ///< +0x45EC
};

/// The game's help system (g_game+0x25005C): nullptr before Start
[[nodiscard]] HelpSystem* Get();
void Start(HelpSystem::Info info, HelpSystem::Queries queries, HelpSystem::Hooks hooks);
void Shutdown();

} // namespace openblack::help
