/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpProfile.h"

#include <cmath>

#include <algorithm>
#include <span>
#include <utility>

#include <spdlog/spdlog.h>

#include "ECS/Systems/ScriptStateInterface.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "Locator.h"

namespace openblack::help::profile
{

namespace
{
/// A kept time's slot, steps back from the next one to be written
constexpr uint32_t SlotBack(uint32_t head, uint32_t back)
{
	return (head + k_TimesKept - back) % k_TimesKept;
}
} // namespace

void EventCount::Trigger(uint32_t clock)
{
	if (_triggeredThisTurn)
	{
		return;
	}
	++_total;
	_triggeredThisTurn = true;
	_times.at(_head) = clock;
	_head = (_head + 1) % k_TimesKept;
	_kept = std::min(_kept + 1, k_TimesKept);
}

void EventCount::EndTurn()
{
	const float happened = _triggeredThisTurn ? 1.0f : 0.0f;
	_triggeredThisTurn = false;
	_smoothedRate += (happened - _smoothedRate) * k_RateFollow;
}

void EventCount::PullTimesBackTo(uint32_t clock)
{
	std::ranges::for_each(_times, [clock](uint32_t& time) { time = std::min(time, clock); });
}

float EventCount::SecondsSince(uint32_t clock)
{
	if (_kept == 0)
	{
		return 0.0f;
	}
	const auto since = static_cast<int32_t>(clock - _times.at(SlotBack(_head, 1)));
	if (since < 0)
	{
		PullTimesBackTo(clock);
		return 0.0f;
	}
	return static_cast<float>(since) * 0.001f;
}

float EventCount::PerSecond(uint32_t clock)
{
	if (_kept < 2)
	{
		return 0.0f;
	}
	const auto since = static_cast<int32_t>(clock - _times.at(SlotBack(_head, _kept)));
	if (since < 0)
	{
		PullTimesBackTo(clock);
		return 0.0f;
	}
	if (since == 0)
	{
		return 0.0f;
	}
	return static_cast<float>((_kept - 1) * 1000) / static_cast<float>(since);
}

EventCount::Kept EventCount::GetKept() const
{
	return {.total = _total,
	        .smoothedRate = _smoothedRate,
	        .head = _head,
	        .kept = _kept,
	        .triggeredThisTurn = _triggeredThisTurn,
	        .times = _times};
}

void EventCount::SetKept(const Kept& kept)
{
	_total = kept.total;
	_smoothedRate = kept.smoothedRate;
	_head = kept.head % k_TimesKept;
	_kept = std::min(kept.kept, k_TimesKept);
	_triggeredThisTurn = kept.triggeredThisTurn;
	_times = kept.times;
}

void HelpProfile::Trigger(uint32_t event)
{
	if (event >= k_EventCount)
	{
		return;
	}
	_counts.at(event).Trigger(_clock);
	if (event > 13 && event < 24)
	{
		_counts.at(24).Trigger(_clock);
	}
	if (event > 0 && event < 43)
	{
		_counts.at(43).Trigger(_clock);
	}
	if (event > 8 && event < 12)
	{
		_counts.at(11).Trigger(_clock);
	}
	_counts.at(k_AnyEvent).Trigger(_clock);
}

void HelpProfile::EndTurn()
{
	std::ranges::for_each(_counts, &EventCount::EndTurn);
	_clock += k_MillisecondsPerTurn;
}

} // namespace openblack::help::profile

namespace openblack::help_profile
{
namespace
{
/// ProcessSpecialTriggers' constants
constexpr float k_TooCloseDistance = 15.0f;
constexpr float k_TooClosePitch = 0.55f;
constexpr float k_SkyPitch = -0.3f;
/// |yaw| is compared with the float 0.01 widened to double
constexpr double k_RotateThreshold = 0.0099999997764825821;

/// The camera help's own tables: the inputs of the 0x3nn reasons, the 0x2nn reasons and the 0x1nn reasons. Only the
/// camera help's debug page and its tooltips read them: kept so that the callback is whole
constexpr size_t k_InputCount = 5;
constexpr size_t k_MoveCount = 4;
constexpr size_t k_ExclusionCount = 3;

using help::profile::EventCount;

struct HelpProfileState
{
	help::profile::HelpProfile profile;
	std::array<EventCount, k_InputCount> inputs {};
	std::array<EventCount, k_MoveCount> moves {};
	std::array<EventCount, k_ExclusionCount> exclusions {};
	Queries queries;
};

/// The HelpProfile state (Locator::scriptState)
HelpProfileState& HelpProfileData()
{
	return openblack::Locator::scriptState::value().Get<HelpProfileState>();
}

bool Paused()
{
	auto& state = HelpProfileData();
	return state.queries.paused ? state.queries.paused() : game_clock::IsPaused();
}

bool ScriptWideScreen()
{
	auto& state = HelpProfileData();
	if (state.queries.scriptWideScreen)
	{
		return state.queries.scriptWideScreen();
	}
	const auto* helpSystem = help::Get();
	return helpSystem != nullptr && helpSystem->IsScriptWideScreen();
}

bool Blocked()
{
	return Paused() || ScriptWideScreen();
}

template <size_t N>
void EndTurn(std::array<EventCount, N>& table)
{
	std::ranges::for_each(table, &EventCount::EndTurn);
}

template <size_t N>
void ResetAll(std::array<EventCount, N>& table)
{
	table.fill({});
}

/// The count a script asks about, none for a number outside 1..48
EventCount* Valid(int32_t type)
{
	if (!help::profile::HelpProfile::ScriptMayAsk(type))
	{
		return nullptr;
	}
	return &HelpProfileData().profile.Count(static_cast<uint32_t>(type));
}
} // namespace

void SetQueries(Queries queries)
{
	HelpProfileData().queries = std::move(queries);
}

void Trigger(Event type)
{
	if (Blocked())
	{
		return;
	}
	HelpProfileData().profile.Trigger(static_cast<uint32_t>(type));
}

std::optional<CameraView> PlayerCameraView()
{
	const auto& state = HelpProfileData();
	return state.queries.playerCamera ? state.queries.playerCamera() : std::nullopt;
}

void ProcessSpecialTriggers()
{
	auto& state = HelpProfileData();
	// the camera's current mode, only when it is the player's; none -> nothing
	if (!state.queries.playerCamera)
	{
		return;
	}
	const auto view = state.queries.playerCamera();
	if (!view)
	{
		return;
	}
	if (view->screenCentreOnLand)
	{
		Trigger(Event::LookAtLand);
		// below 15 and above 0.55
		if (view->headingDistance < k_TooCloseDistance && view->pitch > k_TooClosePitch)
		{
			Trigger(Event::LookAtLandTooClose);
		}
		return;
	}
	if (view->pitch < k_SkyPitch)
	{
		Trigger(Event::LookAtSky);
	}
}

void Process()
{
	auto& state = HelpProfileData();
	// paused, a script's wide screen, or blocked
	if (Blocked() || (state.queries.processBlocked && state.queries.processBlocked()))
	{
		return;
	}
	ProcessSpecialTriggers();
	state.profile.EndTurn();
	// the camera help's three tables, the same step
	EndTurn(state.exclusions);
	EndTurn(state.moves);
	EndTurn(state.inputs);
}

void CameraHelpCallback(CameraReason reason, uint32_t inputs)
{
	auto& state = HelpProfileData();
	const auto value = static_cast<uint32_t>(reason);
	const auto entry = value & 0xFFu;
	const auto now = state.profile.Clock();
	switch (value & 0xF00u)
	{
	case 0x300:
		// A readout for the log, at the trace level of the input log only (-L input=trace): each move of the camera the
		// help hears, with its input mask and the game turn
		if (const auto logger = spdlog::get("input"); logger != nullptr && logger->should_log(spdlog::level::trace))
		{
			logger->trace("Camera help: 0x{:03X} from the input 0x{:02X} in game turn {}", value, inputs, game_clock::Turn());
		}
		Trigger(static_cast<Event>(entry + 0x19));
		for (size_t bit = 0; bit < k_InputCount; ++bit)
		{
			if ((inputs & (1u << bit)) != 0)
			{
				state.inputs.at(bit).Trigger(now); // the camera help's own: no pause test
			}
		}
		break;
	case 0x200:
		if (entry < k_MoveCount)
		{
			state.moves.at(entry).Trigger(now);
		}
		break;
	case 0x100:
		if (entry < k_ExclusionCount)
		{
			state.exclusions.at(entry).Trigger(now);
		}
		break;
	default:
		break;
	}
}

uint32_t InputMask(const CameraInputSources& sources)
{
	const auto bit = [](CameraInput input) { return static_cast<uint32_t>(input); };
	uint32_t mask = sources.wheel ? bit(CameraInput::WheelSpin) : 0u;
	if (sources.middleButton)
	{
		mask |= bit(CameraInput::WheelDown);
	}
	// Any other input of the frame, but a movement key alone while the land is gripped (the grip drops that move)
	if ((sources.turnTiltOrZoom || (sources.moveKeys && !sources.landGripped)) && !sources.middleButton)
	{
		mask |= bit(CameraInput::Keyboard);
	}
	if (sources.landGripped)
	{
		mask |= bit(CameraInput::MouseButton);
	}
	if (sources.bothButtons)
	{
		mask |= bit(CameraInput::BothMouseButtons);
	}
	return mask;
}

PlayerCameraReports ReportsOfPlayerCamera(float yaw, float pitch, float zoom)
{
	PlayerCameraReports reports;
	const auto report = [&reports](CameraReason reason) { reports.reasons.at(reports.count++) = reason; };
	if (zoom != 0.0f)
	{
		report(CameraReason::Zoom);
	}
	if (yaw != 0.0f && std::fabs(static_cast<double>(yaw)) > k_RotateThreshold)
	{
		report(CameraReason::Rotate);
		if (yaw > 0.0f)
		{
			report(CameraReason::RotateCW);
		}
		if (yaw < 0.0f)
		{
			report(CameraReason::RotateCCW);
		}
	}
	if (pitch != 0.0f)
	{
		report(CameraReason::Pitch);
	}
	return reports;
}

void OnPlayerCameraMove(float yaw, float pitch, float zoom, uint32_t inputs)
{
	const auto reports = ReportsOfPlayerCamera(yaw, pitch, zoom);
	for (const auto reason : std::span(reports.reasons).first(reports.count))
	{
		CameraHelpCallback(reason, inputs);
	}
}

std::optional<float> TotalEvents(int32_t type)
{
	const auto* count = Valid(type);
	if (count == nullptr)
	{
		return std::nullopt;
	}
	return static_cast<float>(count->Total());
}

std::optional<float> EventsPerSecond(int32_t type)
{
	auto* count = Valid(type);
	if (count == nullptr)
	{
		return std::nullopt;
	}
	return count->PerSecond(HelpProfileData().profile.Clock());
}

std::optional<float> TimeSince(int32_t type)
{
	auto* count = Valid(type);
	if (count == nullptr)
	{
		return std::nullopt;
	}
	return count->SecondsSince(HelpProfileData().profile.Clock());
}

const EventCount& Get(Event type)
{
	return HelpProfileData().profile.Count(static_cast<uint32_t>(type));
}

help::profile::HelpProfile& Profile()
{
	return HelpProfileData().profile;
}

uint32_t AccumulatedTime()
{
	return HelpProfileData().profile.Clock();
}

void SetToZero()
{
	auto& state = HelpProfileData();
	for (uint32_t event = 0; event < help::profile::k_EventCount; ++event)
	{
		state.profile.Count(event) = {};
	}
	// the camera help's tables: 0x1nn, 0x2nn, then the inputs
	ResetAll(state.exclusions);
	ResetAll(state.moves);
	ResetAll(state.inputs);
}

void Reset()
{
	HelpProfileData() = {};
}

} // namespace openblack::help_profile
