/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ToolTips.h"

#include <cstdlib>

#include <algorithm>
#include <array>
#include <span>
#include <string>

#include <spdlog/spdlog.h>

#include "Camera/CameraHelp.h"
#include "Common/FixedFormat.h"
#include "Common/HelpText.h"
#include "Debug/TraceLog.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "GameClock.h"
#include "Graphics/RendererInterface.h"
#include "Help/HelpSystem.h"
#include "Help/InputPromptIcon.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::help::tooltips
{
namespace
{

struct State
{
	int32_t current {-1};        ///< the index of the text shown (text - 0xE73)
	int32_t displayTurns {0};    ///< while > 0 nothing that is not forced takes over
	int32_t afterFocusTurns {0}; ///< the turns it stays once nobody submits it
	bool forced {false};
	uint32_t align {0};
	int32_t action {-1};
	/// The action resolved on submit and its fallback. The InputPromptIcon does not take these: the builder resolves the action
	/// again
	int32_t animType {0};
	int32_t clickType {0};
	int32_t row {0};
	std::u16string keyName;                ///< the key's name of a key binding
	bool alive {false};                    ///< submitted this turn
	float forcedValue {0.0f};              ///< Force's value
	std::optional<Numbers> numbers;        ///< the numbers of the text (the builder reads them from the object)
	std::array<uint8_t, k_Count> shown {}; ///< the times each text took over (up to 80), the fade-in seconds
	int32_t level {2};
	input_prompt::Handle icon {input_prompt::k_None};
	std::function<void()> submitter;
	std::function<std::optional<Numbers>(uint32_t text)> objectNumbers;
	uint32_t traceTurns {0}; ///< OPENBLACK_TOOLTIP_TRACE's turn count
};

/// The tooltip (Locator::scriptState)
State& Get()
{
	return openblack::Locator::scriptState::value().Get<State>();
}

/// The InputPromptIcon deleted and the handle cleared
void DeleteIcon(State& state)
{
	input_prompt::Destroy(state.icon);
	state.icon = input_prompt::k_None;
}

float Priority(int32_t index)
{
	return Locator::infoConstants::value().toolTips.at(static_cast<size_t>(index)).priority;
}

/// A text with its numbers (helptext::Format of the first conversions), then "/" and the places when it has them
std::u16string FormatNumbers(uint32_t text, const Numbers& numbers)
{
	if (numbers.percentAfter)
	{
		// "%s %3.0f%%": the text, then the percent, rounded as the game's C library rounds
		return fixed_format::WithNumber(helptext::Get(text) + u" %3.0f%%", *numbers.percentAfter);
	}
	auto out = helptext::Format(numbers.text.value_or(text),
	                            std::span<const double>(numbers.values.data(), std::min<size_t>(numbers.count, 2)));
	if (numbers.outOf)
	{
		// "%s/%d": the text, then the places
		for (const char c : "/" + std::to_string(*numbers.outOf))
		{
			out.push_back(static_cast<char16_t>(c));
		}
	}
	return out;
}

/// The builder's text: the forced value for 0xEEA / 0xEE1 / 0xEA2 / 0xEEB; for the texts read again from the object,
/// this turn's object's numbers, or the text as it is written with no such object; the submitted numbers for the
/// others that have them; the text as it is otherwise
std::u16string BuildText(uint32_t text)
{
	const auto& state = Get();
	if (text == 0xEEA || text == 0xEE1 || text == 0xEA2 || text == 0xEEB)
	{
		return helptext::Format(text, static_cast<double>(state.forcedValue));
	}
	if (ReadsObjectNumbers(text) && state.objectNumbers)
	{
		const auto read = state.objectNumbers(text);
		return read ? FormatNumbers(text, *read) : helptext::Get(text);
	}
	if (state.numbers)
	{
		return FormatNumbers(text, *state.numbers);
	}
	return helptext::Get(text);
}

} // namespace

void Submit(uint32_t text, int32_t action, uint32_t align, bool force, std::optional<float> value)
{
	Numbers numbers;
	if (value)
	{
		numbers.values[0] = static_cast<double>(*value);
		numbers.count = 1;
	}
	Submit(text, action, align, force, numbers);
}

void Submit(uint32_t text, int32_t action, uint32_t align, bool force, const Numbers& numbers)
{
	const auto value = numbers.count > 0 || numbers.outOf || numbers.percentAfter || numbers.text
	                       ? std::optional<Numbers>(numbers)
	                       : std::nullopt;
	auto& state = Get();
	// only the texts 0xE73..0xF1C
	if (text < k_First || text - k_First >= k_Count || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto index = static_cast<int32_t>(text - k_First);
	const float priority = Priority(index);
	// level 1 keeps the priorities from 0.9
	if (state.level == 1 && priority < 0.9f)
	{
		return;
	}
	if (index == state.current)
	{
		state.alive = true;
		state.numbers = value;
	}
	// while the display timer runs only a forced text takes over
	if (!force && state.displayTurns > 0)
	{
		return;
	}
	// a higher priority stays; the same text again only takes over forced. (not ported) a per-text counter nobody reads
	if (state.current != -1 && (Priority(state.current) > priority || (index == state.current && !force)))
	{
		return;
	}
	const auto& info = Locator::infoConstants::value().toolTips.at(static_cast<size_t>(index));
	state.forced = force;
	state.current = index;
	// trunc(2.5 x time x 10): game turns of 100 ms
	state.displayTurns = static_cast<int32_t>(2.5f * info.displayTime * 10.0f);
	state.afterFocusTurns = static_cast<int32_t>(2.5f * info.displayTimeAfterFocus * 10.0f);
	state.align = align;
	state.action = action;
	// resolve the action; when it fails animType 0, clickType 0 and the mouse row
	if (!input_prompt::ResolveAction(action, state.animType, state.clickType, state.row, state.keyName))
	{
		state.animType = 0;
		state.clickType = 0;
		state.row = input_prompt::MouseRow();
		state.keyName.clear();
	}
	state.alive = true;
	state.numbers = value;
	if (priority < 0.9f)
	{
		state.shown[static_cast<size_t>(index)] =
		    static_cast<uint8_t>(std::min(state.shown[static_cast<size_t>(index)] + 1, 80));
	}
}

void Force(uint32_t text, float value)
{
	Submit(text, -1, 0, true);
	Get().forcedValue = value;
}

void SetStateSubmitter(std::function<void()> submitter)
{
	Get().submitter = std::move(submitter);
}

void SetObjectNumbers(std::function<std::optional<Numbers>(uint32_t text)> reader)
{
	Get().objectNumbers = std::move(reader);
}

void SetLevel(int32_t level)
{
	Get().level = level;
}

int32_t Level()
{
	return Get().level;
}

void ProcessTurn()
{
	auto& state = Get();
	// the builder, then the alive flag cleared for the next turn
	const auto endTurn = [&state]() { state.alive = false; };
	if (state.level == 0 || !Locator::infoConstants::has_value())
	{
		DeleteIcon(state);
		endTurn();
		return;
	}
	// the script's widescreen with its owner: no tooltip
	if (const auto* helpSystem = help::Get();
	    helpSystem != nullptr && helpSystem->GetWideScreen() != 0 && helpSystem->GetWideScreenOwner() != 0)
	{
		if (static const bool trace = std::getenv("OPENBLACK_TOOLTIP_TRACE") != nullptr; debug_env::TraceLogs(trace))
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tooltip trace: none, the script's widescreen is on");
		}
		DeleteIcon(state);
		endTurn();
		return;
	}
	// paused outside the citadel: no tooltip either. The game's turns stop in a pause, so this only happens when the pause
	// begins during a turn; inside the citadel the temple's turn keeps it. Between the turns the icon stays as it is
	if (game_clock::IsPaused() && !game_clock::IsInsideCitadel())
	{
		DeleteIcon(state);
		endTurn();
		return;
	}
	// the builder: the hand's state submits
	if (state.submitter)
	{
		state.submitter();
	}
	// OPENBLACK_TOOLTIP_TRACE=1 (openblack only): every 10 turns the text kept and its timers
	if (static const bool trace = std::getenv("OPENBLACK_TOOLTIP_TRACE") != nullptr; trace)
	{
		if (++state.traceTurns % 10 == 0)
		{
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_INFO(logger, "Tooltip trace: text {:#x} alive {} display {} after {} icon {}",
				                   state.current >= 0 ? state.current + k_First : 0, state.alive, state.displayTurns,
				                   state.afterFocusTurns, state.icon != input_prompt::k_None);
			}
		}
	}
	if (state.afterFocusTurns > 0 && !state.alive)
	{
		state.alive = true;
		--state.afterFocusTurns;
	}
	else if (!state.alive)
	{
		state.displayTurns = 0;
		state.current = -1;
		DeleteIcon(state);
		endTurn();
		return;
	}
	const float priority = Priority(state.current);
	if (state.level == 1 && priority < 0.9f)
	{
		DeleteIcon(state);
		endTurn();
		return;
	}
	auto* icon = input_prompt::Get(state.icon);
	// at level 2 with no icon, the display time over and a priority under 0.9: no icon is made
	if (state.level == 2 && icon == nullptr && state.displayTurns <= 0 && priority < 0.9f)
	{
		endTurn();
		return;
	}
	// otherwise, once the display time is over, a low priority shown at full alpha (current at its target and at 1 or
	// more) fades out in 1 s and stays, invisible, while the same text is submitted
	if (state.level == 2 && icon != nullptr && icon->current == icon->target && state.displayTurns <= 0 && priority < 0.9f &&
	    icon->current >= 1.0f)
	{
		icon->start = icon->current;
		icon->target = 0.0f;
		icon->duration = 1.0f;
		icon->elapsed = 0.0f;
	}
	// the display time runs only while the camera may rotate (CameraHelp::EnabledFeatures & 2)
	if (state.displayTurns > 0 && camera_help::IsFeatureEnabled(camera_help::Feature::Rotate))
	{
		--state.displayTurns;
	}
	const auto text = static_cast<uint32_t>(state.current) + k_First;
	const auto built = BuildText(text);
	// the builder makes some of its texts show at once, as a forced one does
	if (BuilderShowsAtOnce(text, state.numbers.has_value() && state.numbers->count > 0))
	{
		state.forced = true;
	}
	const uint32_t align = state.align | 0x16;
	// the builder resolves the action again into its own locals: -1 first sets animType 0 and row 3; a failed call gives
	// clickType 0, row 3 and animType 0, so no picture
	int32_t animType = 0;
	int32_t clickType = 0;
	int32_t row = 3;
	std::u16string keyName;
	if (!input_prompt::ResolveAction(state.action, animType, clickType, row, keyName))
	{
		animType = 0;
		clickType = 0;
		row = 3;
		keyName.clear();
	}
	// the icon is kept while clickType, row, animType, align | 0x16 and the text are the same. (a key's row is the same
	// pointer for every key: its name is not compared)
	if (icon != nullptr && icon->clickType == clickType && icon->row == row && icon->animType == animType &&
	    icon->align == align && icon->text == built)
	{
		endTurn();
		return;
	}
	DeleteIcon(state);
	// start 0, target 1, the fade-in: the times it was shown in seconds, or at once when forced or at level 3, the
	// builder's animType, clickType, row, the text, x 0, y 0 (offsets from the hand: align | 0x16 anchors there),
	// S = H / 25, align | 0x16, yellow, white, 0x80
	// the screen height; (openblack) 0 before the renderer exists
	const int32_t height = Locator::rendererInterface::has_value() ? Locator::rendererInterface::value().GetResolution().y : 0;
	input_prompt::Desc desc {
	    .start = 0.0f,
	    .target = 1.0f,
	};
	desc.duration =
	    (!state.forced && state.level != 3) ? static_cast<float>(state.shown[static_cast<size_t>(state.current)]) : 0.0f;
	desc.animType = animType;
	desc.clickType = clickType;
	desc.row = row;
	desc.keyName = keyName;
	desc.text = built;
	desc.x = 0;
	desc.y = 0;
	desc.size = IconSize(height);
	desc.align = align;
	desc.c1 = input_prompt::k_Yellow;
	desc.c2 = input_prompt::k_White;
	desc.alpha8 = 0x80;
	state.icon = input_prompt::Create(desc);
	endTurn();
}

std::optional<uint32_t> ShownText()
{
	if (!openblack::Locator::scriptState::has_value())
	{
		return std::nullopt;
	}
	const auto& state = Get();
	return state.current >= 0 ? std::optional(static_cast<uint32_t>(state.current) + k_First) : std::nullopt;
}

bool IsShownForced()
{
	return openblack::Locator::scriptState::has_value() && Get().current >= 0 && Get().forced;
}

bool HasIcon()
{
	return openblack::Locator::scriptState::has_value() && input_prompt::Get(Get().icon) != nullptr;
}

void ResetIcon()
{
	// the tooltip's InputPromptIcon deleted and its handle cleared
	DeleteIcon(Get());
}

void Reset()
{
	// a new land starts with no tooltip and every field back to its start value; only the show counts stay (capped at
	// 80), so a text seen before still fades in slowly. The icon stays drawn: the next turn's pass keeps it when the
	// same text comes back, and deletes or replaces it otherwise
	auto& state = Get();
	state.current = -1;
	state.displayTurns = 0;
	state.afterFocusTurns = 0;
	state.forced = false;
	state.align = 0;
	state.action = -1;
	state.animType = 0;
	state.clickType = 0;
	state.row = 3;
	state.keyName.clear();
	state.alive = false;
	state.forcedValue = 0.0f;
	state.numbers.reset();
}

} // namespace openblack::help::tooltips
