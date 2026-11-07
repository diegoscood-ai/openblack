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
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "Audio/Services/Voices.h"
#include "Common/HelpText.h"
#include "DialogueText.h"
#include "HelpTextDisplay.h"
#include "SpeedIndicator.h"

// The help system: the wide screen and its owner, the help switch and level, the category turns and the message sets,
// with the dialogue's texts (DialogueText, which it makes and forwards to; the help texts slot owns it in the game).
// See docs/wiki/scripts/land1-intro.md#the-help-systems-turn.

namespace openblack::ecs::systems
{
class HelpTextSystemInterface;
}

namespace openblack::help
{

class HelpSystem
{
public:
	/// The help system's values from info.dat
	using Info = DialogueText::Info;

	/// What the help system reads from the rest of the game; unset gives the value written next to each
	struct Queries
	{
		/// The help text database entry. Unset: helptext::GetEntry.
		std::function<helptext::Entry(uint32_t textId)> textEntry;
		/// The voice table. Unset: audio::voices::Table().
		std::function<audio::TextVoice(uint32_t textId)> textVoice;
		/// The game turn. Unset: 0.
		std::function<uint32_t()> turn;
		/// The scaled clock in milliseconds. Unset: 0.
		std::function<int32_t()> nowMs;
		/// Inside the citadel the times are in milliseconds. Unset: false.
		std::function<bool()> citadelClock;
		/// The VMScriptType of a task (1 Script, 2 Help, ...; 1 when there is no such task). Unset: 1.
		std::function<uint32_t(uint32_t taskNumber)> taskScriptType;
		/// The key that skips a text is down (which key: not read). Unset: false.
		std::function<bool()> skipKey;
		/// The interface is playing back a recording. Unset: false.
		std::function<bool()> playBack;
		/// A full screen film is open, from its start to the end of its fade: the dialogue is not drawn and its texts do
		/// not slide in. Unset: false.
		std::function<bool()> filmPlaying;
		/// The bank is registered. Unset: false, so no text has a voice.
		std::function<bool(audio::SfxBank)> voiceBankLoaded;
		/// An advisor talks or stopped less than 200 ms ago. Unset: false.
		std::function<bool()> advisorsTalking;
		/// The sample is playing. Unset: false.
		std::function<bool(audio::SfxBank, audio::VoiceOwner, uint32_t sample)> isPlaying;
		/// The screen height the text display reads once (its line height is H / 30). Unset: 480 (inferred)
		std::function<int()> screenHeight;
	};

	/// Where the parts that are not ported plug in
	struct Hooks
	{
		/// Show the text
		std::function<void(const std::u16string& text, float number, int32_t narrator)> showText;
		/// openblack only (test hooks): a text with this id was made the current one
		std::function<void(uint32_t textId)> textStarted;
		/// The voice branches of SayText
		std::function<void(uint32_t textId, VoiceRoute route, audio::TextVoice voice)> sayVoice;
		/// A click that cuts a text stops the narration (audio::voices::CutByClick), after interruptAdvisors
		std::function<void()> stopVoicesOnClick;
		/// Send a spirit home: with arg != 0 the advisor leaves at once, with arg == 0 it flies off first (inferred).
		/// Game.cpp: help::spirits.
		std::function<void(int32_t spirit, int32_t arg)> spiritHome;
		/// Whether a spirit (type 1 or 2) is out of its home. Game.cpp: help::spirits' IsHome. Unset: home
		std::function<bool(int32_t spirit)> spiritOut;
		/// Whether a script holds the dialogue control. Unset: none
		std::function<bool()> dialogueControlled;
		/// Both advisors are cut short in what they say, the good one first (the help texts' InterruptAdvisors)
		std::function<void()> interruptAdvisors;
		/// The rest of SetWideScreen when the wide screen changes: the dialog boxes hidden (on), the interface made
		/// inactive while a script owns the wide screen, and the bars' timer: openblack's bars are CinemaBars::Set
		std::function<void(bool on)> wideScreen;
		/// The bars put at once where the wide screen is going (their slide's timer at its end), as the help system's
		/// reset does after its SetWideScreen(0, 0): openblack's bars are CinemaBars::Snap
		std::function<void()> snapWideScreen;
	};

	/// Makes the dialogue into `dialogue`, its owner, with the dialogue's share of the queries and hooks; the owner keeps
	/// it as long as the help system
	HelpSystem(Info info, Queries queries, Hooks hooks, std::unique_ptr<DialogueText>& dialogue);
	// The dialogue asks it whether a script holds the wide screen, so it stays where it was made
	HelpSystem(const HelpSystem&) = delete;
	HelpSystem& operator=(const HelpSystem&) = delete;
	HelpSystem(HelpSystem&&) = delete;
	HelpSystem& operator=(HelpSystem&&) = delete;
	~HelpSystem() = default;

	/// The dialogue's texts, made by this help system
	[[nodiscard]] DialogueText& GetDialogue() { return _dialogue; }
	[[nodiscard]] const DialogueText& GetDialogue() const { return _dialogue; }

	/// CHL 13 RUN_TEXT
	void RunText(bool singleLine, uint32_t textId, int32_t withInteraction)
	{
		static_cast<void>(_dialogue.RunText(singleLine, textId, withInteraction));
	}
	/// CHL 232 RUN_TEXT_WITH_NUMBER
	void RunTextWithNumber(bool singleLine, uint32_t textId, float number, int32_t withInteraction)
	{
		_dialogue.RunTextWithNumber(singleLine, textId, number, withInteraction);
	}
	/// CHL 14 TEMP_TEXT: u"*" + the string, narrator 1, text id 0, no voice, no history
	void TempText(bool singleLine, std::u16string_view text, int32_t withInteraction)
	{
		_dialogue.TempText(singleLine, text, withInteraction);
	}
	/// CHL 231 TEMP_TEXT_WITH_NUMBER
	void TempTextWithNumber(bool singleLine, std::u16string_view text, float number, int32_t withInteraction)
	{
		_dialogue.TempTextWithNumber(singleLine, text, number, withInteraction);
	}
	/// CHL 411 GAME_CLEAR_DIALOGUE: ClearAllText
	void ClearDialogue() { _dialogue.ClearAllText(); }
	/// CHL 412 GAME_CLOSE_DIALOGUE: closes the text display (display only; its Reset clears that again) and ClearAllText
	void CloseDialogue() { _dialogue.CloseDialogue(); }

	/// Show a help text (AddText), keep it in the history and say its voice
	void SayText(uint32_t textId, int32_t withInteraction, float number) { _dialogue.SayText(textId, withInteraction, number); }
	/// Show a text, start its reading time and make it the current one
	void AddText(const std::u16string& text, int32_t withInteraction, float number, int32_t narrator, uint32_t textId)
	{
		_dialogue.AddText(text, withInteraction, number, narrator, textId);
	}
	/// Reset the text display (which also clears the single line flag), the six text ids and ClearTextDisplayed
	void ClearAllText() { _dialogue.ClearAllText(); }
	/// Delete the click icon (while waiting for a click) and the key icon, then clear the no-click and wait flags and
	/// the end times
	void ClearTextDisplayed() { _dialogue.ClearTextDisplayed(); }
	/// Once a turn from Process: the icons go when the display has no current text
	void ProcessIcons() { _dialogue.ProcessIcons(); }
	/// Delete the click icon, the tooltips' icon and the key icon. (pending) its callers on load and when the advisors
	/// are shut down
	void ResetIcons() { _dialogue.ResetIcons(); }
	/// The text part of the help system's reset (from the script reset): ClearAllText, SetWideScreen(0, 0) and the bars
	/// snapped out (Hooks::snapWideScreen), ResetIcons
	/// and the history emptied. The rest is not ported. The 9 category turns and the message sets' counters are zeroed and
	/// the help is switched on. It does not touch the dialogue owner
	void Reset();
	/// The scripts' restart (a new game, a playground's start): Reset, then both advisors sent home, the evil one first,
	/// flying off (not a Help script's vanish). A land change does neither
	void ResetForScriptsRestart();
	/// The debug override of the spirits' texts; Reset puts it back to None
	void SetSpeakerOverride(SpeakerOverride override) { _dialogue.SetSpeakerOverride(override); }
	[[nodiscard]] SpeakerOverride GetSpeakerOverride() const { return _dialogue.GetSpeakerOverride(); }
	/// CHL 15 TEXT_READ
	[[nodiscard]] bool IsTextRead() const { return _dialogue.IsTextRead(); }
	/// The text part of the help system's 3D draw (DialogueText::Draw3D)
	void Draw3D(float frameMs, const TextRegion& region, int screenWidth) { _dialogue.Draw3D(frameMs, region, screenWidth); }
	/// The text display
	[[nodiscard]] const HelpTextDisplay& GetDisplay() const { return _dialogue.GetDisplay(); }
	/// The game speed's words near the top of the screen, which the help text draws after the dialogue texts
	[[nodiscard]] SpeedIndicator& GetSpeedIndicator() { return _speedIndicator; }
	[[nodiscard]] const SpeedIndicator& GetSpeedIndicator() const { return _speedIndicator; }
	/// The profile value TEXT_DRAW (1 when the profile has none; openblack has no profiles)
	[[nodiscard]] int GetTextDraw() const { return _dialogue.GetTextDraw(); }
	/// The profile's TEXT_TOPTOBOTTOM (0 when missing)
	[[nodiscard]] bool GetTextTopToBottom() const { return _dialogue.GetTextTopToBottom(); }

	/// Called by the interface on a click (DialogueText::ProcessClick). 1, or k_ClickTaken.
	int ProcessInterface(bool click) { return _dialogue.ProcessClick(click); }

	/// The advisors' part of the dialogue control's released hook (ecs::systems::DialogueControlSystemInterface, which
	/// holds the owner), from END_DIALOGUE and when a task stops: the text display closed, the wide screen taken away
	/// and both advisors sent home; `helpScript` when the task was a Help script. The hook then clears the texts through
	/// the help texts (ecs::systems::HelpTextSystemInterface::ClearAllText), as the taken hook does
	void OnDialogueReleased(bool helpScript);
	/// The VMScriptType of a task (Queries::taskScriptType; 1 unset)
	[[nodiscard]] uint32_t TaskScriptType(uint32_t task) const { return _taskScriptType ? _taskScriptType(task) : 1; }
	/// When a task stops: when the wide screen is on and this task owns it, SetWideScreen(0, 0)
	void ReleaseWideScreenOf(uint32_t task);
	/// Hooks::spiritHome
	void SpiritHome(int32_t spirit, int32_t arg);
	/// When the wide screen changes, store it and its owner (0 when off), then Hooks::wideScreen
	void SetWideScreen(int32_t on, uint32_t owner);
	/// A script task holds the wide screen (also read by the sound effects and the alignment music)
	[[nodiscard]] bool IsScriptWideScreen() const { return _wideScreen != 0 && _wideScreenOwner != 0; }
	/// The wide screen is on
	[[nodiscard]] int32_t GetWideScreen() const { return _wideScreen; }
	/// The script task that set it (0: none, or set by the game)
	[[nodiscard]] uint32_t GetWideScreenOwner() const { return _wideScreenOwner; }

	/// The profile's READ_SPEED (0.5 without a profile)
	void SetReadSpeed(float readSpeed) { _dialogue.SetReadSpeed(readSpeed); }
	[[nodiscard]] float GetReadSpeed() const { return _dialogue.GetReadSpeed(); }

	/// The help switch (Reset sets 1; SET_HELP_SYSTEM stores the popped value)
	void SetHelpOn(uint32_t on) { _helpOn = on; }
	[[nodiscard]] uint32_t GetHelpOn() const { return _helpOn; }
	/// The profile's HELP_LEVEL, 3 when the profile has none (openblack has no profiles)
	/// The help level the profile keeps; setting it resets the help's field of view (ResetFieldOfView)
	void SetHelpLevel(int32_t level);
	/// Unless a script holds the dialogue control or a script's wide screen is on, each spirit out of its home is sent
	/// home (flying off). (not read) the field it also clears
	void ResetFieldOfView();
	[[nodiscard]] int32_t GetHelpLevel() const { return _helpLevel; }
	/// HELP_SYSTEM_ON: the help is on and the level is not 0
	[[nodiscard]] bool IsHelpSystemOn() const { return _helpOn != 0 && _helpLevel != 0; }
	/// The level the guidance reads: the help level while the help is on, else 0
	[[nodiscard]] int32_t GetGuidanceLevel() const { return _helpOn != 0 ? _helpLevel : 0; }
	/// Store the turn the category was triggered (Reset zeroes the 9)
	void TriggerCategory(int32_t category);
	[[nodiscard]] uint32_t GetCategoryTurn(int32_t category) const;
	/// The turn the last help message started (zeroed when the game is reset)
	void SetMessageTurn(uint32_t turn) { _messageTurn = turn; }
	/// The message sets' state (Help/HelpMessageSets.h)
	struct MessageSets
	{
		std::array<uint32_t, 18> conditions {};  ///< this turn's
		std::array<uint32_t, 0x98> sentTurn {};  ///< the turn each set was last sent
		std::array<uint32_t, 0x98> sentCount {}; ///< how often each set was sent
		uint32_t lastSet {0};
		uint16_t banterCount {0}; ///< banter sets sent since the last restart
	};
	[[nodiscard]] MessageSets& GetMessageSets() { return _messageSets; }
	[[nodiscard]] uint32_t GetMessageTurn() const { return _messageTurn; }

	/// The i-th most recent text of the history (0 = the last), nullptr past the ones kept
	[[nodiscard]] const HistoryEntry* GetHistory(int32_t i) const { return _dialogue.GetHistory(i); }
	[[nodiscard]] int32_t GetHistoryCount() const { return _dialogue.GetHistoryCount(); }

	/// The current text (0 after ClearAllText), then the five before it
	[[nodiscard]] const std::array<uint32_t, 6>& GetTexts() const { return _dialogue.GetTexts(); }
	/// withInteraction == 1, the text waits for a click
	[[nodiscard]] bool IsWaitingForClick() const { return _dialogue.IsWaitingForClick(); }
	/// The turn the text started and the turn after which it is read
	[[nodiscard]] uint32_t GetStartTurn() const { return _dialogue.GetStartTurn(); }
	[[nodiscard]] uint32_t GetEndTurn() const { return _dialogue.GetEndTurn(); }

private:
	/// The dialogue's share of the queries and hooks, moved out of them; the wide screen query reads this help system
	[[nodiscard]] DialogueText::Queries DialogueQueries(Queries& queries) const;
	[[nodiscard]] static DialogueText::Hooks DialogueHooks(Hooks& hooks);

	std::function<uint32_t(uint32_t taskNumber)> _taskScriptType;
	std::function<void(int32_t spirit, int32_t arg)> _spiritHome;
	std::function<bool(int32_t spirit)> _spiritOut;
	std::function<bool()> _dialogueControlled;
	std::function<void(bool on)> _wideScreenHook;
	std::function<void()> _snapWideScreen;

	DialogueText& _dialogue;
	SpeedIndicator _speedIndicator; ///< kept for the whole game, never reset
	uint32_t _helpOn {1};
	int32_t _helpLevel {3}; ///< 3 without a profile
	std::array<uint32_t, 9> _categoryTurns {};
	uint32_t _messageTurn {0};
	MessageSets _messageSets;
	int32_t _wideScreen {0};
	uint32_t _wideScreenOwner {0};
};

namespace script_control
{
struct Vm;
} // namespace script_control

/// The game's help system: nullptr before Start
[[nodiscard]] HelpSystem* Get();
/// Once a game turn, after the script: the hand state, the icons, both spirits (the evil one first), the tooltips'
/// turn and, with the help on, the conditions and the banter (Help/HelpMessageSets.h); `vm` and `turn` for the
/// banter's scripts
void Process(const script_control::Vm& vm, uint32_t turn);
/// The help system made, with its dialogue handed to `texts` (the help texts slot), which keeps it until Shutdown
void Start(ecs::systems::HelpTextSystemInterface& texts, HelpSystem::Info info, HelpSystem::Queries queries,
           HelpSystem::Hooks hooks);
/// The help system goes, then its dialogue
void Shutdown();

} // namespace openblack::help
