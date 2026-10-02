/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpSystem.h"

#include <cstdlib>

#include <memory>
#include <utility>

#include <spdlog/spdlog.h>

#include "GameClock.h"

namespace openblack::help
{

namespace
{
std::unique_ptr<HelpSystem> s_helpSystem;

bool Tracing()
{
	static const bool k_Trace = std::getenv("OPENBLACK_TEXT_TRACE") != nullptr;
	return k_Trace && spdlog::get("game") != nullptr;
}

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

// fn_005CB0F0
bool IsBlank(char16_t c)
{
	return c == 0x20 || c == 0x09 || c == 0x0D || c == 0x0A || c == 0xF8FE;
}

// fn_005CB190
bool IsEscape(char16_t c)
{
	return c == u'$' || c == u'\\';
}

// fn_005CB1B0
bool IsDigit(char16_t c)
{
	return c >= u'0' && c <= u'9';
}

// fn_005CB1D0: a digit, or fn_005CB220: C/c (fn_005CB200) or one of DFMNPdfmnp (the byte table 0x5CB26C over 'D'..'p')
bool IsCode(char16_t c)
{
	return IsDigit(c) || std::u16string_view(u"CDFMNPcdfmnp").find(c) != std::u16string_view::npos;
}

class Splitter
{
public:
	explicit Splitter(std::u16string_view text)
	    : _text(text)
	{
	}

	[[nodiscard]] bool AtEnd() const { return At(_pos) == 0; }

	/// fn_005CB590(&text, word, 0): the length of the next piece (0 = empty)
	uint32_t Next()
	{
		uint32_t length = 0;
		if (IsBlank(At(_pos)) && SkipBlanks() != 3) // 0x5CB5B3..0x5CB5E2
		{
			return 0;
		}
		if (IsEscape(At(_pos))) // 0x5CB5F9
		{
			if (IsEscape(At(_pos + 1))) // 0x5CB613, 0x5CB68B: the second one is a character of the word
			{
				_pos += 2;
				length = 1;
			}
			else
			{
				const int result = CodeResult(); // fn_005CB2A0
				SkipCode();                      // fn_005CB4E0
				if (result == 0 && IsBlank(At(_pos)))
				{
					SkipBlanks(); // 0x5CB653..0x5CB675
				}
				return 0;
			}
		}
		// 0x5CB6A9..0x5CB703
		while (!IsBlank(At(_pos)) && !IsEscape(At(_pos)) && At(_pos) != 0 && length < 0x2F)
		{
			++length;
			++_pos;
		}
		return length;
	}

private:
	[[nodiscard]] char16_t At(size_t i) const { return i < _text.size() ? _text[i] : u'\0'; }

	/// fn_005CB120: 1 when it went past a line feed, 3 otherwise
	int SkipBlanks()
	{
		if (!IsBlank(At(_pos)))
		{
			return 3;
		}
		while (IsBlank(At(_pos)))
		{
			if (At(_pos) == u'\n')
			{
				++_pos;
				return 1;
			}
			++_pos;
		}
		return 3;
	}

	/// fn_005CB2A0(text, 0) at an escape: 1 for N, 4 for P, 5 for D, 2 for the number 1, 0 for C, F, M, other numbers or
	/// no code (the calls it makes to set colours and fonts change only the display)
	[[nodiscard]] int CodeResult() const
	{
		const char16_t code = At(_pos + 1);
		if (!IsCode(code))
		{
			return 0;
		}
		switch (code)
		{
		case u'N':
		case u'n':
			return 1; // 0x5CB317
		case u'P':
		case u'p':
			return 4; // 0x5CB322
		case u'D':
		case u'd':
			return 5; // 0x5CB32D
		case u'C':
		case u'c':
		case u'F':
		case u'f':
		case u'M':
		case u'm':
			return 0; // 0x5CB2FB, 0x5CB35C, 0x5CB338
		default:
		{
			// 0x5CB378: _wtoi of the digits, fn_005CB4C0 (2 for 1)
			int value = 0;
			for (size_t i = _pos + 1; IsDigit(At(i)); ++i)
			{
				value = value * 10 + (At(i) - u'0');
			}
			return value == 1 ? 2 : 0;
		}
		}
	}

	/// fn_005CB4E0: past the escape, its code and the code's digits (and one more character after C)
	void SkipCode()
	{
		++_pos; // 0x5CB501
		const char16_t code = At(_pos);
		if (code == u'C' || code == u'c') // fn_005CB200 0x5CB507
		{
			++_pos;
			while (IsDigit(At(_pos)))
			{
				++_pos;
			}
			// 0x5CB53A skips one more character (the original reads past a terminating 0 here; not done)
			if (_pos < _text.size())
			{
				++_pos;
			}
			return;
		}
		if (!IsCode(code)) // 0x5CB54C: only the escape is skipped
		{
			return;
		}
		++_pos;
		while (IsDigit(At(_pos)))
		{
			++_pos;
		}
	}

	std::u16string_view _text;
	size_t _pos {0};
};
} // namespace

uint32_t CountWords(std::u16string_view text)
{
	// fn_005CBEC0: while (*text) { fn_005CB590(&text, word, 0); if (word[0]) ++n; }
	Splitter splitter(text);
	uint32_t words = 0;
	while (!splitter.AtEnd())
	{
		if (splitter.Next() != 0)
		{
			++words;
		}
	}
	return words;
}

double ReadSpeedFactor(float readSpeed)
{
	// fn_005C6CB0 (fcom 0.5, test ah 0x41: the first branch includes 0.5)
	const double r = readSpeed;
	if (r <= 0.5)
	{
		return 3.0 - r * 4.0;
	}
	return (1.0 - (r - 0.5) * 2.0) * 0.8 + 0.2;
}

VoiceRoute RouteOf(int32_t narrator, audio::TextVoice voice)
{
	if (voice.bank == audio::SfxBank::HelpSprites) // 0x5C6025
	{
		if (narrator == helptext::k_NarratorGoodSpirit) // 0x5C602A
		{
			return VoiceRoute::GoodSpirit;
		}
		if (narrator == helptext::k_NarratorEvilSpirit) // 0x5C6061
		{
			return VoiceRoute::EvilSpirit;
		}
	}
	return voice.sample != 0 && voice.bank != audio::SfxBank::None ? VoiceRoute::Narration : VoiceRoute::None; // 0x5C609C
}

int32_t SpiritWhoTalks(int32_t narrator)
{
	// 0x5C6E45..0x5C6E5C
	if (narrator == helptext::k_NarratorGoodSpirit)
	{
		return 1;
	}
	return narrator == helptext::k_NarratorEvilSpirit ? 2 : 0;
}

int32_t ConvertScriptSpiritToHelpSpirit(int32_t type, int discreteAlignment, const std::function<int()>& rand100)
{
	switch (type) // 0x710354: type - 2, 0..3 through 0x710400
	{
	case 2: // 0x710367
		return 2;
	case 3: // 0x71036D..0x7103A7: setl, inc
		return discreteAlignment < 3 ? 2 : 1;
	case 4: // 0x7103A8..0x7103E2: setge, inc
		return discreteAlignment >= 3 ? 2 : 1;
	case 5: // 0x7103E3..0x7103F8: LocalRand(100) > 0x32
		return (rand100 ? rand100() : 0) > 50 ? 2 : 1;
	default: // 0x7103F9
		return 1;
	}
}

HelpSystem::HelpSystem(Info info, Queries queries, Hooks hooks)
    : _info(info)
    , _queries(std::move(queries))
    , _hooks(std::move(hooks))
{
}

void HelpSystem::RunText(bool singleLine, uint32_t textId, int32_t withInteraction)
{
	RunTextWithNumber(singleLine, textId, 0.0f, withInteraction); // 0x6F7E11: number 0
}

void HelpSystem::RunTextWithNumber(bool singleLine, uint32_t textId, float number, int32_t withInteraction)
{
	if (textId >= helptext::k_TextCount) // 0x6F7CE8 / 0x6F7DC5: ScriptErrorMessage "Invalid text", then text 0
	{
		if (const auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_WARN(logger, "Invalid text {}", textId);
		}
		textId = 0;
	}
	if (singleLine || _singleLine) // 0x6F7CFF..0x6F7D21
	{
		ClearAllText();
	}
	_singleLine = singleLine; // 0x6F7D37
	SayText(textId, withInteraction, number);
}

void HelpSystem::TempText(bool singleLine, std::u16string_view text, int32_t withInteraction)
{
	TempTextWithNumber(singleLine, text, 0.0f, withInteraction); // 0x6F7F29: number 0
}

void HelpSystem::TempTextWithNumber(bool singleLine, std::u16string_view text, float number, int32_t withInteraction)
{
	// UNICODE_sprintf(0xD96164, "*%s" 0xC0D420, CHAR2WCHAR(string)) (0x6F7E8D)
	// TempText also reports "Development text being used in game!" (0xC0D3F8, 0x6F7E9E) and the string (0x6F7EE8)
	// (ScriptErrorMessage / ScriptWarningMessage: debug output, not ported)
	const std::u16string shown = u"*" + std::u16string(text);
	if (singleLine || _singleLine) // 0x6F7EF5..0x6F7F12
	{
		ClearAllText();
	}
	_singleLine = singleLine;
	AddText(shown, withInteraction, number, 1, 0); // 0x6F7F25..0x6F7F43: narrator 1, text id 0
}

void HelpSystem::ClearDialogue()
{
	ClearAllText();
}

void HelpSystem::CloseDialogue()
{
	ClearAllText();
}

void HelpSystem::SayText(uint32_t textId, int32_t withInteraction, float number)
{
	if (textId >= helptext::k_TextCount) // 0x5C5FA0
	{
		textId = 0;
	}
	const auto entry = _queries.textEntry ? _queries.textEntry(textId) : helptext::GetEntry(textId);
	const auto voice = _queries.textVoice ? _queries.textVoice(textId) : audio::voices::Table().Get(textId);
	AddText(entry.text, withInteraction, number, entry.narrator, textId); // 0x5C6007
	AddHistory(textId, withInteraction, number, entry.narrator);          // 0x5C6020
	const auto route = RouteOf(entry.narrator, voice);
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: {} {} narrator {} voice bank {} sample {} route {}", textId, entry.name,
		                   entry.narrator, static_cast<int>(voice.bank), voice.sample, static_cast<int>(route));
	}
	if (route != VoiceRoute::None && _hooks.sayVoice)
	{
		_hooks.sayVoice(textId, route, voice);
	}
}

void HelpSystem::AddText(const std::u16string& text, int32_t withInteraction, float number, int32_t narrator, uint32_t textId)
{
	if (_hooks.showText) // HelpText fn_005CCED0 (0x5C6116)
	{
		_hooks.showText(text, number, narrator);
	}
	StartReadingTime(text);                        // 0x5C611E
	_waitClick = withInteraction == 1;             // 0x5C6129, 0x5C6140
	_noClick = withInteraction == 2;               // 0x5C6131, 0x5C614D
	for (size_t i = _texts.size() - 1; i > 0; --i) // memmove(+0x588, +0x584, 0x14)
	{
		_texts[i] = _texts[i - 1];
	}
	_texts[0] = textId; // 0x5C615C
	// 0x5C615E..0x5C6196 delete the KMIcons +0x24 / +0x28 (display only: not ported)
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: show {} narrator {} interaction {} words {} turns {}..{} \"{}\"", textId,
		                   narrator, withInteraction, CountWords(text), _startTurn, _endTurn, Utf8(text));
	}
}

void HelpSystem::StartReadingTime(std::u16string_view text)
{
	// fn_005C61B0
	const uint32_t words = CountWords(text);           // 0x5C61C0
	const double factor = ReadSpeedFactor(_readSpeed); // 0x5C61C9
	const auto gameTurns =
	    static_cast<int32_t>(_info.readDefaultWordGTTime * words + _info.readDefaultAdjustGTTime); // 0x5C61DA
	const uint32_t turn = Turn();
	_startTurn = turn; // 0x5C61FB
	// 0x5C6211..0x5C6225: fild gt, fimul [0xD01A38], fmul 0.001f (0x8AA3B0), fmul factor, fstp float (the FPU at 24 bits)
	const float seconds = static_cast<float>(gameTurns) * static_cast<float>(game_clock::MsPerTurn()) *
	                      game_clock::k_SecondsPerMs * static_cast<float>(factor);
	// 0x5C61F6..0x5C623E: 1000 / [0xD01A38] (div) * seconds, ftol: the NumGameTicksPerSecond 0x711630 conversion
	_endTurn = turn + static_cast<uint32_t>(game_clock::TicksForSeconds(seconds));
	const int32_t now = NowMs();
	_startMs = now;                                                                                  // 0x5C628F
	_endMs = static_cast<int32_t>(static_cast<double>(seconds) * 1000.0 + static_cast<double>(now)); // 0x5C629B
}

bool HelpSystem::HasVoice(uint32_t textId) const
{
	// fn_005C62F0
	if (textId >= helptext::k_TextCount)
	{
		return false;
	}
	const auto voice = _queries.textVoice ? _queries.textVoice(textId) : audio::voices::Table().Get(textId);
	return voice.HasVoice() && _queries.voiceBankLoaded && _queries.voiceBankLoaded(voice.bank);
}

bool HelpSystem::IsTextRead() const
{
	if (_waitClick) // 0x5C64E0
	{
		return false;
	}
	// 0x5C6340
	const uint32_t textId = _texts[0];
	if (HasVoice(textId))
	{
		const auto voice = _queries.textVoice ? _queries.textVoice(textId) : audio::voices::Table().Get(textId);
		if (voice.bank == audio::SfxBank::HelpSprites) // 0x5C636D: the advisors
		{
			return !(_queries.advisorsTalking && _queries.advisorsTalking());
		}
		if (_queries.isPlaying && _queries.isPlaying(voice.bank, audio::VoiceOwner::Narration, voice.sample)) // 0x5C63CA
		{
			_endMs = NowMs() + 450; // 0x5C640A: add 0x1C2
			return false;
		}
		return static_cast<uint32_t>(NowMs()) >= static_cast<uint32_t>(_endMs); // 0x5C6454
	}
	if (CitadelClock()) // 0x5C6468..0x5C6475
	{
		return static_cast<uint32_t>(_endMs) < static_cast<uint32_t>(NowMs()); // 0x5C64AD
	}
	return _endTurn < Turn(); // 0x5C64C3: cmp +0x45DC, turn; sbb; neg
}

void HelpSystem::ClearAllText()
{
	_singleLine = false; // HelpText::Reset 0x5CB053
	_texts.fill(0);      // 0x5C556F
	ClearTextDisplayed();
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: clear all");
	}
}

void HelpSystem::Reset()
{
	ClearAllText();          // 0x5C558D
	_categoryTurns.fill(0);  // 0x5C55BE..0x5C55C9: +0x2D8, 9 dwords
	SetWideScreen(0, 0);     // 0x5C55D6
	_historyCount = 0;       // 0x5C55EA
	_historyNext = 0;        // 0x5C55F0
	_helpOn = 1;             // 0x5C55FC: +0x45F8
}

void HelpSystem::TriggerCategory(int32_t category)
{
	// 0x5C8280: +0x2D8 + 4 category = g_game+0x205A40 (unchecked in the original)
	if (category >= 0 && static_cast<size_t>(category) < _categoryTurns.size())
	{
		_categoryTurns.at(static_cast<size_t>(category)) = Turn();
	}
}

uint32_t HelpSystem::GetCategoryTurn(int32_t category) const
{
	return category >= 0 && static_cast<size_t>(category) < _categoryTurns.size()
	           ? _categoryTurns.at(static_cast<size_t>(category))
	           : 0;
}

bool HelpSystem::DialogueControlRequest(uint32_t task)
{
	if (IsDialogueControlled()) // 0x5C6793
	{
		return false;
	}
	SetCurrentControl(task); // 0x5C67A9
	ClearAllText();          // 0x5C67B0
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: dialogue control to task {}", task);
	}
	return true;
}

void HelpSystem::ClearDialogueControl()
{
	_dialogueOwner = 0; // 0x5C67E0
	// 0x5C67EA..0x5C67F1: HelpText (+0x14) fn_005CB010, display only (not ported)
}

void HelpSystem::ReleaseDialogueControl(uint32_t task)
{
	if (_dialogueOwner != task) // 0x5C6807
	{
		return;
	}
	// 0x5C681B..0x5C682B: the advisors go home at once (arg 1) when the task is a Help script (VMScriptType 2)
	const uint32_t type = _queries.taskScriptType ? _queries.taskScriptType(task) : 1;
	const int32_t helpScript = type == 2 ? 1 : 0;
	ClearDialogueControl(); // 0x5C682C
	// 0x5C6831 / 0x5C683B: fn_005C5200(0) on the spirits +0xC (type 1) and +8 (type 2), as SpiritHome(1 / 2, 0)
	SpiritHome(1, 0);
	SpiritHome(2, 0);
	SetWideScreen(0, 0); // 0x5C684B
	if (_hooks.spiritStop) // 0x5C6856 / 0x5C6861
	{
		_hooks.spiritStop(1, 1);
		_hooks.spiritStop(2, 1);
	}
	SpiritHome(1, helpScript); // 0x5C6874
	SpiritHome(2, helpScript); // 0x5C6888
	ClearAllText();            // 0x5C688F
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: dialogue control of task {} released", task);
	}
}

void HelpSystem::SpiritHome(int32_t spirit, int32_t arg)
{
	if (_hooks.spiritHome)
	{
		_hooks.spiritHome(spirit, arg);
	}
}

void HelpSystem::SetWideScreen(int32_t on, uint32_t owner)
{
	if (_wideScreen == on) // 0x5C6ADE
	{
		return;
	}
	_wideScreen = on;                       // 0x5C6B17
	_wideScreenOwner = on != 0 ? owner : 0; // 0x5C6B23 / 0x5C6B31
	if (_hooks.wideScreen)
	{
		_hooks.wideScreen(on != 0);
	}
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: wide screen {} (task {})", on, _wideScreenOwner);
	}
}

void HelpSystem::ClearTextDisplayed()
{
	_noClick = false;   // 0x5C5526
	_waitClick = false; // 0x5C552C
	_endTurn = 0;       // 0x5C5532
	_endMs = 0;         // 0x5C5538
}

bool HelpSystem::ShownLongEnough() const
{
	// fn_005C68C0: 1 s ([0x92A444]) while it waits for the click, 0.5 s ([0x915D1C]) otherwise
	const float limit = _waitClick ? 1.0f : 0.5f;
	// the FPU at 24 bits: float
	float seconds;
	if (CitadelClock()) // 0x5C68ED..0x5C6949
	{
		seconds = static_cast<float>(static_cast<uint32_t>(NowMs() - _startMs)) * game_clock::k_SecondsPerMs;
	}
	else // 0x5C695F..0x5C698B
	{
		seconds = static_cast<float>(static_cast<int32_t>(Turn() - _startTurn)) *
		          static_cast<float>(static_cast<int32_t>(game_clock::MsPerTurn())) * game_clock::k_SecondsPerMs;
	}
	return !(seconds < limit);
}

int HelpSystem::ProcessInterface(bool click)
{
	const bool key = _queries.skipKey && _queries.skipKey(); // [0xE85410] & 0xFF
	if (_noClick)                                            // 0x5C69C6
	{
		return 1;
	}
	if (!click && !key) // 0x5C69D2..0x5C69D8
	{
		return 1;
	}
	if (_queries.playBack && _queries.playBack()) // 0x5C69ED
	{
		return 1;
	}
	if (IsTextRead()) // 0x5C69F8
	{
		return 1;
	}
	if (!ShownLongEnough() && !key) // 0x5C6A03..0x5C6A0E
	{
		return 1;
	}
	if (_clickPending) // 0x5C6A10
	{
		_clickPending = false;
		return k_ClickTaken;
	}
	if (IsScriptWideScreen() || key) // 0x5C6A2E..0x5C6A44
	{
		if (_texts[0] != 0) // 0x5C6A7E
		{
			if (_hooks.spiritStop) // 0x5C6A88 / 0x5C6A93: fn_005C6720(1, 1), fn_005C6720(2, 1)
			{
				_hooks.spiritStop(1, 1);
				_hooks.spiritStop(2, 1);
			}
			if (_hooks.stopVoicesOnClick) // 0x5C6AAD
			{
				_hooks.stopVoicesOnClick();
			}
		}
		ClearTextDisplayed(); // 0x5C6AB4
		if (Tracing())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: click cuts {}", _texts[0]);
		}
		return k_ClickTaken;
	}
	if (_waitClick) // 0x5C6A46..0x5C6A50 (and deletes the KMIcon +0x24, 0x5C6A56..0x5C6A6D: display only, not ported)
	{
		_waitClick = false;
		if (Tracing())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: click ends the wait of {}", _texts[0]);
		}
	}
	return 1;
}

void HelpSystem::AddHistory(uint32_t textId, int32_t withInteraction, float number, int32_t narrator)
{
	// fn_005C5EE0
	if (++_historyCount > static_cast<int32_t>(k_HistorySize))
	{
		_historyCount = static_cast<int32_t>(k_HistorySize);
	}
	_history[static_cast<size_t>(_historyNext)] = {textId, withInteraction, number, narrator};
	_historyNext = (_historyNext + 1) % static_cast<int32_t>(k_HistorySize);
}

const HistoryEntry* HelpSystem::GetHistory(int32_t i) const
{
	// fn_005C5F50
	if (i < 0 || i >= _historyCount)
	{
		return nullptr;
	}
	const auto index = (_historyNext - i + 0x3FF) & 0x3FF;
	return &_history[static_cast<size_t>(index)];
}

HelpSystem* Get()
{
	return s_helpSystem.get();
}

void Start(HelpSystem::Info info, HelpSystem::Queries queries, HelpSystem::Hooks hooks)
{
	s_helpSystem = std::make_unique<HelpSystem>(info, std::move(queries), std::move(hooks));
}

void Shutdown()
{
	s_helpSystem.reset();
}

} // namespace openblack::help
