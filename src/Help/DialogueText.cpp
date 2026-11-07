/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DialogueText.h"

#include <string>
#include <utility>

#include <spdlog/spdlog.h>

#include "ClickCue.h"
#include "ControlIcon.h"
#include "Debug/DebugEnv.h"
#include "GameClock.h"
#include "InputPromptIcon.h"
#include "TextSplitter.h"
#include "ToolTips.h"

namespace openblack::help
{

namespace
{
std::string Utf8(std::u16string_view text)
{
	std::string out;
	out.reserve(text.size());
	for (const char16_t c : text)
	{
		if (c < 0x80)
		{
			out.push_back(c == u'\n' ? '|' : static_cast<char>(c));
		}
		else if (c < 0x800)
		{
			out.push_back(static_cast<char>(0xC0 | (c >> 6)));
			out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
		}
		else
		{
			out.push_back(static_cast<char>(0xE0 | (c >> 12)));
			out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
		}
	}
	return out;
}

/// Delete the icon, then the slot is empty
void DeleteIcon(input_prompt::Handle& icon)
{
	input_prompt::Destroy(icon);
	icon = input_prompt::k_None;
}
} // namespace

bool TextTracing()
{
	static const bool k_Trace = debug_env::TextTrace();
	return k_Trace && spdlog::get("game") != nullptr;
}

uint32_t CountWords(std::u16string_view text)
{
	// Count the non-empty pieces the splitter returns. No $M icon; the colour and font codes only change a state nobody
	// reads here.
	text_splitter::Splitter splitter(text);
	text_splitter::DrawState state;
	std::u16string word;
	uint32_t words = 0;
	while (!splitter.AtEnd())
	{
		splitter.Next(word, state, false);
		if (!word.empty())
		{
			++words;
		}
	}
	return words;
}

float ReadSpeedFactor(float readSpeed)
{
	// Below, equal or unordered (NaN) -> the first branch
	if (!(static_cast<double>(readSpeed) > 0.5))
	{
		const float scaled = readSpeed * 4.0f;
		return 3.0f - scaled;
	}
	// Each result rounded to 24 bits
	float value = static_cast<float>(static_cast<double>(readSpeed) - 0.5);
	value = value + value;
	value = static_cast<float>(1.0 - static_cast<double>(value));
	value = static_cast<float>(static_cast<double>(value) * 0.80000000000000004);
	return static_cast<float>(static_cast<double>(value) + 0.20000000000000001);
}

VoiceRoute RouteOf(int32_t narrator, audio::TextVoice voice)
{
	if (voice.bank == audio::SfxBank::HelpSprites)
	{
		if (narrator == helptext::k_NarratorGoodSpirit)
		{
			return VoiceRoute::GoodSpirit;
		}
		if (narrator == helptext::k_NarratorEvilSpirit)
		{
			return VoiceRoute::EvilSpirit;
		}
	}
	return voice.sample != 0 && voice.bank != audio::SfxBank::None ? VoiceRoute::Narration : VoiceRoute::None;
}

Speaker ApplySpeakerOverride(SpeakerOverride override, int32_t narrator)
{
	const bool good = narrator == helptext::k_NarratorGoodSpirit;
	const bool evil = narrator == helptext::k_NarratorEvilSpirit;
	switch (override)
	{
	case SpeakerOverride::None:
		break;
	case SpeakerOverride::SilenceGood:
		return {.narrator = narrator, .silenced = good};
	case SpeakerOverride::SilenceEvil:
		return {.narrator = narrator, .silenced = evil};
	case SpeakerOverride::ForceGood:
		return {.narrator = evil ? helptext::k_NarratorGoodSpirit : narrator, .silenced = false};
	case SpeakerOverride::ForceEvil:
		return {.narrator = good ? helptext::k_NarratorEvilSpirit : narrator, .silenced = false};
	}
	return {.narrator = narrator, .silenced = false};
}

DialogueText::DialogueText(Info info, Queries queries, Hooks hooks)
    : _info(info)
    , _queries(std::move(queries))
    , _hooks(std::move(hooks))
    // The line height is H / 28 when the language needs bigger text, else H / 30. (pending) re-initialise the text after
    // a resolution change
    , _display(_queries.screenHeight ? _queries.screenHeight() : 480, helptext::NeedsBiggerText())
{
}

void DialogueText::Draw3D(float frameMs, const TextRegion& region, int screenWidth)
{
	// While a film is open the texts are not drawn, so they do not slide in either; the click cue below still is
	if (!_queries.filmPlaying || !_queries.filmPlaying())
	{
		_display.Advance(frameMs, _textDraw);
	}
	if (!_waitClick)
	{
		return;
	}
	const int size = click_cue::CueSize(region.top, region.bottom);
	const int y = click_cue::MiddleRow(region.top, region.bottom);
	if (auto* icon = input_prompt::Get(_clickIcon); icon != nullptr)
	{
		icon->y = y;
		return;
	}
	input_prompt::Desc desc;
	// No icon when the action cannot be resolved
	if (!input_prompt::ResolveAction(1, desc.animType, desc.clickType, desc.row, desc.keyName))
	{
		return;
	}
	if (desc.animType == 0)
	{
		desc.animType = 1; // the button blinks
	}
	// the click cue's label
	desc.text = helptext::Get(click_cue::k_LabelText);
	desc.start = 0.0f;
	desc.target = 1.0f;
	desc.duration = click_cue::k_FadeSeconds;
	desc.x = screenWidth - click_cue::k_RightInset;
	desc.y = y;
	desc.size = size;
	desc.align = 0x11;
	desc.c1 = input_prompt::k_Yellow;
	desc.c2 = input_prompt::k_White;
	desc.alpha8 = click_cue::k_GlowAlpha;
	_clickIcon = input_prompt::Create(desc);
}

void DialogueText::ShowControlIcons(std::span<const int> actions, const TextRegion& box, int screenWidth)
{
	const int size = click_cue::CueSize(box.top, box.bottom);
	for (const int action : actions)
	{
		if (control_icon::IsKeyAction(action))
		{
			input_prompt::Desc desc;
			// Nothing when no key is bound to the action
			if (!input_prompt::ResolveAction(action, desc.animType, desc.clickType, desc.row, desc.keyName) ||
			    desc.animType != -1)
			{
				continue;
			}
			const std::u16string name = desc.keyName;
			const bool made = input_prompt::Get(_controlKeyIcon) != nullptr;
			if (!made || action == _controlKeyAction)
			{
				// The text starts again; a new icon is made only when there is none
				_controlKeyText.clear();
				_controlKeyCount = 0;
				if (!made)
				{
					desc.animType = -1;
					desc.clickType = 0;
					desc.keyName.clear();
					desc.text.reset();
					desc.start = 0.0f;
					desc.target = 1.0f;
					desc.duration = control_icon::k_KeyFadeSeconds;
					desc.x = screenWidth - click_cue::k_RightInset;
					desc.y = control_icon::KeyIconRow(box.top, box.bottom);
					desc.size = size;
					desc.align = 0x11;
					desc.c1 = input_prompt::k_Yellow;
					desc.c2 = input_prompt::k_White;
					desc.alpha8 = click_cue::k_GlowAlpha;
					_controlKeyIcon = input_prompt::Create(desc);
					_controlKeyAction = action;
				}
			}
			control_icon::AppendKeyName(_controlKeyText, _controlKeyCount, name);
			// The icon shows the text as it is now built
			if (auto* icon = input_prompt::Get(_controlKeyIcon); icon != nullptr)
			{
				icon->keyName = _controlKeyText;
			}
			continue;
		}
		// No mouse icon while the text waits for a click: the click cue has that place
		if (action == -1 || _waitClick)
		{
			continue;
		}
		int32_t animType = 0;
		int32_t clickType = 0;
		int32_t row = 3;
		std::u16string keyName;
		if (!input_prompt::ResolveAction(action, animType, clickType, row, keyName))
		{
			continue;
		}
		const int y = click_cue::MiddleRow(box.top, box.bottom);
		if (auto* icon = input_prompt::Get(_clickIcon); icon != nullptr)
		{
			icon->animType = animType;
			icon->clickType = clickType;
			icon->row = row;
			icon->y = y;
			continue;
		}
		input_prompt::Desc desc;
		desc.animType = animType;
		desc.clickType = clickType;
		desc.row = row;
		desc.keyName = keyName;
		desc.start = 0.0f;
		desc.target = 1.0f;
		desc.duration = control_icon::k_KeyFadeSeconds;
		desc.x = screenWidth - click_cue::k_RightInset;
		desc.y = y;
		desc.size = size;
		desc.align = 0x11;
		desc.c1 = input_prompt::k_Yellow;
		desc.c2 = input_prompt::k_White;
		desc.alpha8 = click_cue::k_GlowAlpha;
		_clickIcon = input_prompt::Create(desc);
	}
}

void DialogueText::ProcessIcons()
{
	if (!_display.HasNewestText())
	{
		DeleteIcon(_clickIcon);
		DeleteIcon(_controlKeyIcon);
	}
}

void DialogueText::ResetIcons()
{
	DeleteIcon(_clickIcon);
	help::tooltips::ResetIcon();
	DeleteIcon(_controlKeyIcon);
}

void DialogueText::ResetHistory()
{
	_historyCount = 0;
	_historyNext = 0;
}

bool DialogueText::RunText(bool singleLine, uint32_t textId, int32_t withInteraction)
{
	const bool valid = textId < helptext::k_TextCount;
	RunTextWithNumber(singleLine, textId, 0.0f, withInteraction); // number 0
	return valid;
}

void DialogueText::RunTextWithNumber(bool singleLine, uint32_t textId, float number, int32_t withInteraction)
{
	if (textId >= helptext::k_TextCount) // an invalid text reports an error, then text 0
	{
		if (const auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_WARN(logger, "Invalid text {}", textId);
		}
		textId = 0;
	}
	if (singleLine || _display.IsSingleLine())
	{
		ClearAllText();
	}
	_display.SetSingleLine(singleLine);
	SayText(textId, withInteraction, number);
}

void DialogueText::TempText(bool singleLine, std::u16string_view text, int32_t withInteraction)
{
	TempTextWithNumber(singleLine, text, 0.0f, withInteraction); // number 0
}

void DialogueText::TempTextWithNumber(bool singleLine, std::u16string_view text, float number, int32_t withInteraction)
{
	// "*" + the string. TempText also reports "Development text being used in game!" and the string (debug output, not
	// ported)
	const std::u16string shown = u"*" + std::u16string(text);
	if (singleLine || _display.IsSingleLine())
	{
		ClearAllText();
	}
	_display.SetSingleLine(singleLine);
	AddText(shown, withInteraction, number, 1, 0); // narrator 1, text id 0
}

void DialogueText::CloseDialogue()
{
	_display.Close();
	ClearAllText();
}

void DialogueText::SayText(uint32_t textId, int32_t withInteraction, float number)
{
	if (textId >= helptext::k_TextCount)
	{
		textId = 0;
	}
	const auto entry = _queries.textEntry ? _queries.textEntry(textId) : helptext::GetEntry(textId);
	const auto voice = _queries.textVoice ? _queries.textVoice(textId) : audio::voices::Table().Get(textId);
	const auto speaker = ApplySpeakerOverride(_speakerOverride, entry.narrator);
	AddText(entry.text, withInteraction, number, speaker.narrator, textId);
	AddHistory(textId, withInteraction, number, speaker.narrator);
	const auto route = speaker.silenced ? VoiceRoute::None : RouteOf(speaker.narrator, voice);
	if (TextTracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: {} {} narrator {} voice bank {} sample {} route {}", textId, entry.name,
		                   entry.narrator, static_cast<int>(voice.bank), voice.sample, static_cast<int>(route));
	}
	if (route != VoiceRoute::None && _hooks.sayVoice)
	{
		_hooks.sayVoice(textId, route, voice);
	}
}

void DialogueText::AddText(const std::u16string& text, int32_t withInteraction, float number, int32_t narrator, uint32_t textId)
{
	// The TEXT_DRAW gate reads the entry of the text being made current here (inferred: TEMP_TEXT's id 0 reads entry 0)
	const auto entry = _queries.textEntry ? _queries.textEntry(textId) : helptext::GetEntry(textId);
	_display.Add(text, number, narrator, entry.arg0 != 0);
	if (_hooks.showText)
	{
		_hooks.showText(text, number, narrator);
	}
	if (_hooks.textStarted)
	{
		_hooks.textStarted(textId);
	}
	DeleteIcon(_clickIcon);
	DeleteIcon(_controlKeyIcon);
	StartReadingTime(text);
	_waitClick = withInteraction == 1;
	_noClick = withInteraction == 2;
	for (size_t i = _texts.size() - 1; i > 0; --i) // shift the five older texts down
	{
		_texts[i] = _texts[i - 1];
	}
	_texts[0] = textId;
	// The key combination icon is not ported
	if (TextTracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: show {} narrator {} interaction {} words {} turns {}..{} \"{}\"", textId,
		                   narrator, withInteraction, CountWords(text), _startTurn, _endTurn, Utf8(text));
	}
}

void DialogueText::Restore(const Held& held)
{
	_texts = held.texts;
	_waitClick = held.waitClick;
	_noClick = held.noClick;
}

void DialogueText::ShowAgain()
{
	_display.Hide();
	// The current text, the newest, decides whether the texts are drawn (the TEXT_DRAW gate)
	const auto entryOf = [this](uint32_t textId) {
		return _queries.textEntry ? _queries.textEntry(textId) : helptext::GetEntry(textId);
	};
	const bool drawable = entryOf(_texts[0]).arg0 != 0;
	// The oldest first, so that the newest is on top again; a text that showed a number shows none now
	for (size_t i = _texts.size(); i-- > 0;)
	{
		const auto textId = _texts.at(i);
		if (textId == 0)
		{
			continue;
		}
		const auto entry = entryOf(textId < helptext::k_TextCount ? textId : 0);
		_display.Add(entry.text, 0.0f, entry.narrator, drawable);
	}
	if (_texts[0] != 0)
	{
		StartReadingTime(entryOf(_texts[0] < helptext::k_TextCount ? _texts[0] : 0).text);
	}
}

void DialogueText::StartReadingTime(std::u16string_view text)
{
	const uint32_t words = CountWords(text);
	const float factor = ReadSpeedFactor(_readSpeed);
	const auto gameTurns = static_cast<int32_t>(_info.readDefaultWordGTTime * words + _info.readDefaultAdjustGTTime);
	const uint32_t turn = Turn();
	_startTurn = turn;
	// Each step rounded to a float (the FPU at 24 bits)
	const float seconds =
	    static_cast<float>(gameTurns) * static_cast<float>(game_clock::MsPerTurn()) * game_clock::k_SecondsPerMs * factor;
	// The seconds back to game turns
	_endTurn = turn + static_cast<uint32_t>(game_clock::TicksForSeconds(seconds));
	const int32_t now = NowMs();
	_startMs = now;
	// Each step rounded to a float (the FPU at 24 bits: past 2^24 ms the sum loses its last bits, as in the original)
	_endMs = static_cast<int32_t>(static_cast<float>(static_cast<double>(seconds * 1000.0f) + static_cast<double>(now)));
}

bool DialogueText::HasVoice(uint32_t textId) const
{
	if (textId >= helptext::k_TextCount)
	{
		return false;
	}
	const auto voice = _queries.textVoice ? _queries.textVoice(textId) : audio::voices::Table().Get(textId);
	return voice.HasVoice() && _queries.voiceBankLoaded && _queries.voiceBankLoaded(voice.bank);
}

bool DialogueText::IsTextRead() const
{
	if (_waitClick)
	{
		return false;
	}
	const uint32_t textId = _texts[0];
	if (HasVoice(textId))
	{
		const auto voice = _queries.textVoice ? _queries.textVoice(textId) : audio::voices::Table().Get(textId);
		if (voice.bank == audio::SfxBank::HelpSprites) // the advisors
		{
			return !(_queries.advisorsTalking && _queries.advisorsTalking());
		}
		if (_queries.isPlaying && _queries.isPlaying(voice.bank, audio::VoiceOwner::Narration, voice.sample))
		{
			_endMs = NowMs() + 450;
			return false;
		}
		return static_cast<uint32_t>(NowMs()) >= static_cast<uint32_t>(_endMs);
	}
	if (CitadelClock())
	{
		return static_cast<uint32_t>(_endMs) < static_cast<uint32_t>(NowMs());
	}
	return _endTurn < Turn();
}

void DialogueText::ClearAllText()
{
	_display.Reset(true); // also clears the single line flag
	_texts.fill(0);
	ClearTextDisplayed();
	if (TextTracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: clear all");
	}
}

void DialogueText::ClearTextDisplayed()
{
	if (_waitClick)
	{
		DeleteIcon(_clickIcon);
	}
	DeleteIcon(_controlKeyIcon);
	_noClick = false;
	_waitClick = false;
	_endTurn = 0;
	_endMs = 0;
}

bool DialogueText::ShownLongEnough() const
{
	// 1 s while it waits for the click, 0.5 s otherwise
	const float limit = _waitClick ? 1.0f : 0.5f;
	// the FPU at 24 bits: float
	float seconds;
	if (CitadelClock())
	{
		seconds = static_cast<float>(static_cast<uint32_t>(NowMs() - _startMs)) * game_clock::k_SecondsPerMs;
	}
	else
	{
		seconds = static_cast<float>(static_cast<int32_t>(Turn() - _startTurn)) *
		          static_cast<float>(static_cast<int32_t>(game_clock::MsPerTurn())) * game_clock::k_SecondsPerMs;
	}
	return !(seconds < limit);
}

void DialogueText::InterruptAdvisors() const
{
	if (_hooks.interruptAdvisors)
	{
		_hooks.interruptAdvisors();
	}
}

int DialogueText::ProcessClick(bool click)
{
	const bool key = _queries.skipKey && _queries.skipKey();
	if (_noClick)
	{
		return 1;
	}
	if (!click && !key)
	{
		return 1;
	}
	if (_queries.playBack && _queries.playBack())
	{
		return 1;
	}
	if (IsTextRead())
	{
		return 1;
	}
	if (!ShownLongEnough() && !key)
	{
		return 1;
	}
	if (_clickPending)
	{
		_clickPending = false;
		return k_ClickTaken;
	}
	if (ScriptWideScreen() || key)
	{
		if (_texts[0] != 0)
		{
			InterruptAdvisors();
			if (_hooks.stopVoicesOnClick)
			{
				_hooks.stopVoicesOnClick();
			}
		}
		ClearTextDisplayed();
		if (TextTracing())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: click cuts {}", _texts[0]);
		}
		return k_ClickTaken;
	}
	if (_waitClick)
	{
		_waitClick = false;
		DeleteIcon(_clickIcon);
		if (TextTracing())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: click ends the wait of {}", _texts[0]);
		}
	}
	return 1;
}

void DialogueText::AddHistory(uint32_t textId, int32_t withInteraction, float number, int32_t narrator)
{
	if (++_historyCount > static_cast<int32_t>(k_HistorySize))
	{
		_historyCount = static_cast<int32_t>(k_HistorySize);
	}
	_history[static_cast<size_t>(_historyNext)] = {textId, withInteraction, number, narrator};
	_historyNext = (_historyNext + 1) % static_cast<int32_t>(k_HistorySize);
}

const HistoryEntry* DialogueText::GetHistory(int32_t i) const
{
	if (i < 0 || i >= _historyCount)
	{
		return nullptr;
	}
	const auto index = (_historyNext - i + 0x3FF) & 0x3FF;
	return &_history[static_cast<size_t>(index)];
}

} // namespace openblack::help
