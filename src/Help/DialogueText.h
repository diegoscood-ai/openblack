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
#include <span>
#include <string>
#include <string_view>

#include "Audio/Services/Voices.h"
#include "Common/HelpText.h"
#include "HelpTextDisplay.h"
#include "InputPromptIcon.h"

// The text part of the help system: the current text and the five before it, the history, ClearAllText, IsTextRead,
// the click that ends a text, and the CHL functions RUN_TEXT, RUN_TEXT_WITH_NUMBER, TEMP_TEXT, TEMP_TEXT_WITH_NUMBER,
// TEXT_READ, GAME_CLEAR_DIALOGUE and GAME_CLOSE_DIALOGUE. The text display is HelpTextDisplay, drawn by
// Renderer::DrawHelpText; OPENBLACK_TEXT_TRACE logs each text. The voices plug in through the hooks and queries below
// (Game.cpp: audio::voices and help::AdvisorVoices); with Queries::voiceBankLoaded unset no text has a voice, so IsTextRead
// takes the reading-time branch, as the original does when the dialogue banks are not registered.
// See docs/wiki/audio/voices.md#narration-and-reading-time.

namespace openblack::help
{

/// How many help texts the scripts can refer to by number
constexpr uint32_t k_HelpTextCount = helptext::k_TextCount;
/// The narrators the help texts name that are the advisors
constexpr int32_t k_NarratorGoodAdvisor = helptext::k_NarratorGoodSpirit;
constexpr int32_t k_NarratorEvilAdvisor = helptext::k_NarratorEvilSpirit;

/// The history of texts: 1024 entries
constexpr size_t k_HistorySize = 0x400;
/// What ProcessInterface returns when the click was used by the text
constexpr int k_ClickTaken = 0x14;

/// The number of words of a text: the non-empty pieces the text splitter returns. Words end at a blank (space, tab,
/// CR, LF, U+F8FE), at a '$' or '\' or after 47 characters; "$$" / "\\" (any two of them) is a literal character;
/// '$' or '\' and a code (a digit or one of CDFMNP in either case) with its digits are not words (after C one more
/// character is skipped); any other character after them is the start of a word.
[[nodiscard]] uint32_t CountWords(std::u16string_view text);

/// The factor of READ_SPEED r on the reading time, r <= 0.5 ? 3 - 4r : (1 - 2(r - 0.5)) * 0.8 + 0.2. NaN takes the
/// first branch. The game thread's FPU runs at 24 bits: every step rounds to a float, the double constants used whole
/// (docs/wiki/audio/voices.md#narration-and-reading-time)
[[nodiscard]] float ReadSpeedFactor(float readSpeed);

/// Where the voice of a text is sent
enum class VoiceRoute : uint8_t
{
	None,       ///< no bank or no sample: the text is only shown
	GoodSpirit, ///< HelpSprites and narrator 2: both advisors stop their sentence, the good advisor says the sample
	EvilSpirit, ///< HelpSprites and narrator 3: both advisors stop their sentence, the evil advisor says the sample
	Narration,  ///< any other: a 2D sound effect with the narration owner
};
[[nodiscard]] VoiceRoute RouteOf(int32_t narrator, audio::TextVoice voice);

/// A debug override of the spirits' texts, set from the debug GUI's Consciences window: a spirit silenced keeps its
/// texts but says none of them, a spirit forced says the other one's texts as well. None leaves every text as it is.
enum class SpeakerOverride : uint8_t
{
	None,
	SilenceGood,
	SilenceEvil,
	ForceGood,
	ForceEvil,
};

/// Who says a text under the override, and whether its voice is silenced
struct Speaker
{
	int32_t narrator;
	bool silenced;
};
/// The narrator of a text under the override: only the two spirits' texts change
[[nodiscard]] Speaker ApplySpeakerOverride(SpeakerOverride override, int32_t narrator);

/// One entry of the history
struct HistoryEntry
{
	uint32_t textId;
	int32_t withInteraction;
	float number; ///< the number of RUN_TEXT_WITH_NUMBER (0 for RUN_TEXT)
	int32_t narrator;
};

/// OPENBLACK_TEXT_TRACE is set and the game log exists: the texts and the wide screen are logged
[[nodiscard]] bool TextTracing();

/// The dialogue the scripts show at the bottom of the screen: the texts shown, how long each takes to read or whether
/// it waits for a click, the voice saying it, and the player clicking it on. The help system holds it and keeps the
/// rest (the wide screen, the help level, the message sets and the category turns).
class DialogueText
{
public:
	/// The reading time's values from info.dat
	struct Info
	{
		uint32_t readDefaultAdjustGTTime; ///< 8 in info.dat
		uint32_t readDefaultWordGTTime;   ///< 5 in info.dat
	};

	/// What the dialogue reads from the rest of the game; unset gives the value written next to each
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
		/// A script task holds the wide screen (the help system's). Unset: false.
		std::function<bool()> scriptWideScreen;
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
		/// Both advisors are cut short in what they say, the good one first (the help texts' InterruptAdvisors)
		std::function<void()> interruptAdvisors;
	};

	DialogueText(Info info, Queries queries, Hooks hooks);

	/// CHL 13 RUN_TEXT; false for an invalid text, which shows text 0
	bool RunText(bool singleLine, uint32_t textId, int32_t withInteraction);
	/// CHL 232 RUN_TEXT_WITH_NUMBER
	void RunTextWithNumber(bool singleLine, uint32_t textId, float number, int32_t withInteraction);
	/// CHL 14 TEMP_TEXT: u"*" + the string, narrator 1, text id 0, no voice, no history
	void TempText(bool singleLine, std::u16string_view text, int32_t withInteraction);
	/// CHL 231 TEMP_TEXT_WITH_NUMBER
	void TempTextWithNumber(bool singleLine, std::u16string_view text, float number, int32_t withInteraction);
	/// CHL 412 GAME_CLOSE_DIALOGUE: closes the text display (display only; its Reset clears that again) and ClearAllText
	void CloseDialogue();
	/// The text display closed, its texts kept (the dialogue's release)
	void CloseBox() { _display.Close(); }

	/// The last six texts and the click waits of the newest, which the temple keeps while the player is inside
	struct Held
	{
		/// The text last shown, then the five before it; 0 for none
		std::array<uint32_t, 6> texts {};
		bool waitClick {false};
		bool noClick {false};
	};
	[[nodiscard]] Held Hold() const { return {.texts = _texts, .waitClick = _waitClick, .noClick = _noClick}; }
	/// The texts and click waits the temple kept come back; the box shows nothing of them until ShowAgain
	void Restore(const Held& held);
	/// The box hides, then shows each of the six texts it keeps again, the oldest first, with no number, and the newest's
	/// reading time starts again
	void ShowAgain();

	/// Show a help text (AddText), keep it in the history and say its voice
	void SayText(uint32_t textId, int32_t withInteraction, float number);
	/// Show a text, start its reading time and make it the current one
	void AddText(const std::u16string& text, int32_t withInteraction, float number, int32_t narrator, uint32_t textId);
	/// Reset the text display (which also clears the single line flag), the six text ids and ClearTextDisplayed
	void ClearAllText();
	/// Delete the click icon (while waiting for a click) and the key icon, then clear the no-click and wait flags and
	/// the end times
	void ClearTextDisplayed();
	/// Once a turn from Process: the icons go when the display has no current text
	void ProcessIcons();
	/// Delete the click icon, the tooltips' icon and the key icon
	void ResetIcons();
	/// The history emptied
	void ResetHistory();
	/// The debug override of the spirits' texts
	void SetSpeakerOverride(SpeakerOverride override) { _speakerOverride = override; }
	[[nodiscard]] SpeakerOverride GetSpeakerOverride() const { return _speakerOverride; }
	/// CHL 15 TEXT_READ
	[[nodiscard]] bool IsTextRead() const;
	/// Once a frame with the frame's game time in ms (the frame delta in the citadel): the slide-in of the newest text,
	/// then while a text waits for a click the "click to continue" icon: created once (help::input_prompt, a 1.0 s
	/// fade), its y moved every frame. `region` and `screenWidth` are this frame's
	void Draw3D(float frameMs, const TextRegion& region, int screenWidth);
	/// As the newest text is drawn, its $M<n> codes in order ask for their control icons, at the right of `box`:
	/// - an action from 6 to 15, the bound key: one key icon (made when there is none, its text reset when the code is
	///   the one it was made for), the key's name added to its text (control_icon::AppendKeyName);
	/// - any other action but -1, while the text does not wait for a click, the bound mouse button: the click cue's
	///   icon, moved and changed if there is one, else made.
	/// Both go when there is no newest text (ProcessIcons)
	void ShowControlIcons(std::span<const int> actions, const TextRegion& box, int screenWidth);
	/// The text display
	[[nodiscard]] const HelpTextDisplay& GetDisplay() const { return _display; }
	[[nodiscard]] HelpTextDisplay& Display() { return _display; }
	/// The frame's box and words for a screen with cinema bars of `barPixels`, with the profile's draw flags
	[[nodiscard]] const TextFrame& Layout(int width, int height, int barPixels, const WidthFn& widthFn) const
	{
		return _display.CachedLayout(width, height, barPixels, _textDraw, _textTopToBottom, widthFn);
	}
	/// The profile value TEXT_DRAW (1 when the profile has none; openblack has no profiles)
	[[nodiscard]] int GetTextDraw() const { return _textDraw; }
	/// The profile's TEXT_TOPTOBOTTOM (0 when missing)
	[[nodiscard]] bool GetTextTopToBottom() const { return _textTopToBottom; }
	/// The player's choices in the options, from their profile: which texts are shown (TEXT_DRAW), where the newest goes
	/// (TEXT_TOPTOBOTTOM) and how fast they read (READ_SPEED)
	void SetPlayerChoices(int textDraw, bool topToBottom, float readSpeed)
	{
		_textDraw = textDraw;
		_textTopToBottom = topToBottom;
		_readSpeed = readSpeed;
	}

	/// The player's click (inferred: the left button going down; openblack calls it on that event), or the skip key:
	/// it may end the wait for a click or cut the text short. 1, or k_ClickTaken.
	int ProcessClick(bool click);

	/// The profile's READ_SPEED (0.5 without a profile)
	void SetReadSpeed(float readSpeed) { _readSpeed = readSpeed; }
	[[nodiscard]] float GetReadSpeed() const { return _readSpeed; }

	/// The i-th most recent text of the history (0 = the last), nullptr past the ones kept
	[[nodiscard]] const HistoryEntry* GetHistory(int32_t i) const;
	[[nodiscard]] int32_t GetHistoryCount() const { return _historyCount; }

	/// The current text (0 after ClearAllText), then the five before it
	[[nodiscard]] const std::array<uint32_t, 6>& GetTexts() const { return _texts; }
	/// The text last shown, 0 once cleared
	[[nodiscard]] uint32_t GetCurrentText() const { return _texts[0]; }
	/// withInteraction == 1, the text waits for a click
	[[nodiscard]] bool IsWaitingForClick() const { return _waitClick; }
	/// The turn the text started and the turn after which it is read
	[[nodiscard]] uint32_t GetStartTurn() const { return _startTurn; }
	[[nodiscard]] uint32_t GetEndTurn() const { return _endTurn; }
	/// The time in ms after which a text with a voice, or one in the citadel, is read
	[[nodiscard]] int32_t GetEndMs() const { return _endMs; }

	/// The game turn (Queries::turn; 0 unset)
	[[nodiscard]] uint32_t Turn() const { return _queries.turn ? _queries.turn() : 0; }
	/// Hooks::interruptAdvisors
	void InterruptAdvisors() const;

private:
	/// The reading time of a text without voice
	void StartReadingTime(std::u16string_view text);
	/// The text has a voice to wait for (table entry, bank, sample and the bank registered)
	[[nodiscard]] bool HasVoice(uint32_t textId) const;
	/// The text has been shown long enough for a click to count
	[[nodiscard]] bool ShownLongEnough() const;
	void AddHistory(uint32_t textId, int32_t withInteraction, float number, int32_t narrator);

	[[nodiscard]] int32_t NowMs() const { return _queries.nowMs ? _queries.nowMs() : 0; }
	[[nodiscard]] bool CitadelClock() const { return _queries.citadelClock && _queries.citadelClock(); }
	[[nodiscard]] bool ScriptWideScreen() const { return _queries.scriptWideScreen && _queries.scriptWideScreen(); }

	Info _info;
	Queries _queries;
	Hooks _hooks;
	SpeakerOverride _speakerOverride {SpeakerOverride::None};

	HelpTextDisplay _display;
	int _textDraw {1};
	bool _textTopToBottom {false};
	input_prompt::Handle _clickIcon {input_prompt::k_None};      ///< the click cue (or the $M<n> action icon)
	input_prompt::Handle _controlKeyIcon {input_prompt::k_None}; ///< the $M<6..15> key combination
	int _controlKeyAction {-1};                                  ///< the action the key icon was made for
	int _controlKeyCount {0};                                    ///< the key names in its text
	std::u16string _controlKeyText;                              ///< its text, the key names
	bool _waitClick {false};
	bool _noClick {false}; ///< withInteraction == 2, a click does nothing
	std::array<uint32_t, 6> _texts {};
	std::array<HistoryEntry, k_HistorySize> _history {};
	int32_t _historyNext {0};
	int32_t _historyCount {0};
	uint32_t _startTurn {0};
	uint32_t _endTurn {0};
	int32_t _startMs {0};
	mutable int32_t _endMs {0}; ///< IsTextRead moves it while the narration plays
	/// Set from an interface flag when the interface's active state is set, cleared by ProcessClick. openblack never
	/// sets it, so it stays false (not ported)
	bool _clickPending {false};
	float _readSpeed {0.5f};
};

} // namespace openblack::help
