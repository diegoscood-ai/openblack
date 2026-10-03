/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ToolTips.h"

#include <algorithm>
#include <array>
#include <cstdlib>

#include <spdlog/spdlog.h>

#include "Common/HelpText.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::help::tooltips
{
namespace
{

struct IconState
{
	Icon icon;
	float start {0.0f};    ///< +0
	float target {1.0f};   ///< +4
	float duration {0.0f}; ///< +8, real seconds
	float elapsed {0.0f};  ///< +0x10
};

struct State
{
	int32_t current {-1};        ///< [0xBF19C4], the index of the text shown (text - 0xE73)
	int32_t displayTurns {0};    ///< [0xD17BC8]: while > 0 nothing that is not forced takes over
	int32_t afterFocusTurns {0}; ///< [0xD17BCC]: the turns it stays once nobody submits it
	bool forced {false};         ///< [0xD17BC4]
	uint32_t align {0};          ///< [0xD17BD0]
	int32_t action {-1};         ///< [0xBF19C8]
	bool alive {false};          ///< [0xD17BDC]: submitted this turn
	float forcedValue {0.0f};    ///< [0xD17BBC], ForceToolTips' value
	std::optional<float> value;  ///< the number of the text (the builder reads it from the object)
	std::array<uint8_t, k_Count> shown {}; ///< 0xD163C0: the times each text took over (up to 80), the fade-in seconds
	int32_t level {2};           ///< HelpSystem +0x45FC
	std::optional<IconState> icon; ///< HelpSystem +0x2C
	std::function<void()> submitter;
};

State& Get()
{
	static State state;
	return state;
}

float Priority(int32_t index)
{
	return Locator::infoConstants::value().toolTips.at(static_cast<size_t>(index)).priority;
}

/// The builder 0x5C9FC0's text: the forced value for 0xEEA / 0xEE1 / 0xEA2 / 0xEEB, the object's number for the others
/// that have one (helptext::Format: UNICODE_sprintf of the first conversion), the text as it is otherwise
std::u16string BuildText(uint32_t text)
{
	const auto& state = Get();
	if (text == 0xEEA || text == 0xEE1 || text == 0xEA2 || text == 0xEEB)
	{
		return helptext::Format(text, static_cast<double>(state.forcedValue));
	}
	if (state.value)
	{
		return helptext::Format(text, static_cast<double>(*state.value));
	}
	return helptext::Get(text);
}

} // namespace

void Submit(uint32_t text, int32_t action, uint32_t align, bool force, std::optional<float> value)
{
	auto& state = Get();
	// SubmitToolTips 0x5C9A70: only the texts 0xE73..0xF1C
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
		state.value = value;
	}
	// while the display timer runs only a forced text takes over
	if (!force && state.displayTurns > 0)
	{
		return;
	}
	// a higher priority stays; the same text again only takes over forced. (not ported) 0xD16668[cur] += 1: nothing
	// reads it
	if (state.current != -1 && (Priority(state.current) > priority || (index == state.current && !force)))
	{
		return;
	}
	const auto& info = Locator::infoConstants::value().toolTips.at(static_cast<size_t>(index));
	state.forced = force;
	state.current = index;
	// ftol(2.5 x time x 10): game turns of 100 ms
	state.displayTurns = static_cast<int32_t>(2.5f * info.displayTime * 10.0f);
	state.afterFocusTurns = static_cast<int32_t>(2.5f * info.displayTimeAfterFocus * 10.0f);
	state.align = align;
	state.action = action;
	state.alive = true;
	state.value = value;
	if (priority < 0.9f)
	{
		state.shown[static_cast<size_t>(index)] = static_cast<uint8_t>(std::min(state.shown[static_cast<size_t>(index)] + 1, 80));
	}
}

void Force(uint32_t text, float value)
{
	// ForceToolTips 0x5C9C60
	Submit(text, -1, 0, true);
	Get().forcedValue = value;
}

void SetStateSubmitter(std::function<void()> submitter)
{
	Get().submitter = std::move(submitter);
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
	// fn_005C9D00, then [0xD17BDC] = 0 (HelpSystem::Process 0x5C8FE0)
	const auto endTurn = [&state]() { state.alive = false; };
	if (state.level == 0 || !Locator::infoConstants::has_value())
	{
		state.icon.reset();
		endTurn();
		return;
	}
	// HelpSystem +0x45E8 && +0x45EC (the script's widescreen and its owner): no tooltip
	if (const auto* helpSystem = help::Get();
	    helpSystem != nullptr && helpSystem->GetWideScreen() != 0 && helpSystem->GetWideScreenOwner() != 0)
	{
		if (static const bool trace = std::getenv("OPENBLACK_TOOLTIP_TRACE") != nullptr; trace)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tooltip trace: none, the script's widescreen is on");
		}
		state.icon.reset();
		endTurn();
		return;
	}
	// [0xD17BB8] <= 0 (it is only written 0, 0x54A86C) with g_game +0x14 & 4 (the pause, PauseGame 0x54AE20) outside the
	// citadel deletes it too (0x5C9D44..0x5C9D5E): see Current
	// the builder 0x5C9FC0: the hand's state submits (fn_005D78D0)
	if (state.submitter)
	{
		state.submitter();
	}
	// OPENBLACK_TOOLTIP_TRACE=1 (openblack only): every 10 turns the text kept and its timers
	if (static const bool trace = std::getenv("OPENBLACK_TOOLTIP_TRACE") != nullptr; trace)
	{
		static uint32_t turns = 0;
		if (++turns % 10 == 0)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tooltip trace: text {:#x} alive {} display {} after {} icon {}",
			                   state.current >= 0 ? state.current + k_First : 0, state.alive, state.displayTurns,
			                   state.afterFocusTurns, state.icon.has_value());
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
		state.icon.reset();
		endTurn();
		return;
	}
	const float priority = Priority(state.current);
	if (state.level == 1 && priority < 0.9f)
	{
		state.icon.reset();
		endTurn();
		return;
	}
	// fn_005C9C80 at level 2 returns 0 with no icon, the display time over and a priority under 0.9 (0x5C9CC3): no icon
	// is made (0x5C9E1A)
	if (state.level == 2 && !state.icon && state.displayTurns <= 0 && priority < 0.9f)
	{
		endTurn();
		return;
	}
	// otherwise, once the display time is over, a low priority shown at full alpha fades out in 1 s and stays, invisible,
	// while the same text is submitted
	if (state.level == 2 && state.icon && state.icon->icon.alpha == state.icon->target && state.displayTurns <= 0 &&
	    priority < 0.9f && state.icon->icon.alpha >= 1.0f)
	{
		state.icon->start = state.icon->icon.alpha;
		state.icon->target = 0.0f;
		state.icon->duration = 1.0f;
		state.icon->elapsed = 0.0f;
	}
	// (CameraHelp::EnabledFeatures & 2, default 0x1BF)
	if (state.displayTurns > 0)
	{
		--state.displayTurns;
	}
	const auto text = static_cast<uint32_t>(state.current) + k_First;
	const auto built = BuildText(text);
	const uint32_t align = state.align | 0x16;
	if (state.icon && state.icon->icon.action == state.action && state.icon->icon.align == align &&
	    state.icon->icon.text == built)
	{
		endTurn();
		return;
	}
	// fn_004489D0(start 0, target 1, the fade-in: the times it was shown in seconds, or at once when forced or at
	// level 3)
	IconState icon;
	icon.icon = {built, state.action, align, 0.0f};
	icon.start = 0.0f;
	icon.target = 1.0f;
	icon.duration =
	    (!state.forced && state.level != 3) ? static_cast<float>(state.shown[static_cast<size_t>(state.current)]) : 0.0f;
	icon.elapsed = 0.0f;
	if (icon.duration <= 0.0f)
	{
		icon.icon.alpha = 1.0f;
	}
	state.icon = icon;
	endTurn();
}

void Frame(float realSeconds)
{
	auto& state = Get();
	if (!state.icon)
	{
		return;
	}
	// KMIcon: alpha = clamp(lerp(start, target, elapsed / duration))
	auto& icon = *state.icon;
	icon.elapsed += realSeconds;
	const float t = icon.duration > 0.0f ? std::clamp(icon.elapsed / icon.duration, 0.0f, 1.0f) : 1.0f;
	icon.icon.alpha = std::clamp(icon.start + (icon.target - icon.start) * t, 0.0f, 1.0f);
}

std::optional<Icon> Current()
{
	auto& state = Get();
	// paused (g_game +0x14 & 4) the icon is deleted (fn_005C9D00 0x5C9D44). (not ported) inside the citadel it stays
	if (game_clock::IsPaused())
	{
		state.icon.reset();
	}
	if (!state.icon)
	{
		return std::nullopt;
	}
	return state.icon->icon;
}

void Reset()
{
	// (inferred) a new land starts with no tooltip; the show counts stay (0xD163C0 is a dword array capped at 0x50,
	// 0x5C9C32)
	auto& state = Get();
	state.current = -1;
	state.displayTurns = 0;
	state.afterFocusTurns = 0;
	state.alive = false;
	state.icon.reset();
}

} // namespace openblack::help::tooltips
