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

#include <utility>

#include "GameClock.h"
#include "Help/HelpSystem.h"

namespace openblack::help_profile
{
namespace
{
/// HelpProfile::ProcessSpecialTriggers' constants
constexpr float k_TooCloseDistance = 15.0f; ///< [0x915448]
constexpr float k_TooClosePitch = 0.55f;    ///< [0x91544C]
constexpr float k_SkyPitch = -0.3f;         ///< [0x915450]
/// CameraModeNew3::Update 0x45C727: `fabs; fcomp qword [0x8C7A10]` (the float 0.01 as a double)
constexpr double k_RotateThreshold = 0.0099999997764825821;

/// CameraHelp's own tables: the inputs of the 0x3nn reasons (0xC5A320..0xC5A85C), the 0x2nn reasons (0xC5A860..
/// 0xC5AC90) and the 0x1nn reasons (0xC5AC90..0xC5AFB4). Only CameraHelp's debug page (fn_005C47A0) and its tooltips
/// read them: kept so that the callback is whole
constexpr size_t k_InputCount = 5;
constexpr size_t k_MoveCount = 4;
constexpr size_t k_ExclusionCount = 3;

struct State
{
	std::array<Accumulator, k_EventCount> profile {}; ///< g_game +0x250060 +8
	std::array<Accumulator, k_InputCount> inputs {};
	std::array<Accumulator, k_MoveCount> moves {};
	std::array<Accumulator, k_ExclusionCount> exclusions {};
	uint32_t accumulatedTime {0}; ///< HelpProfile::AccumulatedTime [0xC5AFD8]
	Queries queries;
};
State g_State;

bool Paused()
{
	return g_State.queries.paused ? g_State.queries.paused() : game_clock::IsPaused();
}

bool ScriptWideScreen()
{
	if (g_State.queries.scriptWideScreen)
	{
		return g_State.queries.scriptWideScreen();
	}
	const auto* helpSystem = help::Get();
	return helpSystem != nullptr && helpSystem->IsScriptWideScreen();
}

bool Blocked()
{
	return Paused() || ScriptWideScreen();
}

Accumulator& At(Event type)
{
	return g_State.profile.at(static_cast<size_t>(type));
}

template <size_t N>
void EndTurn(std::array<Accumulator, N>& table)
{
	for (auto& accumulator : table)
	{
		accumulator.EndTurn();
	}
}

template <size_t N>
void ResetAll(std::array<Accumulator, N>& table)
{
	for (auto& accumulator : table)
	{
		accumulator.Reset();
	}
}

const Accumulator* Valid(int32_t type)
{
	// 0x70B924..0x70B941: `test esi, esi; jle` / `cmp esi, 0x31; jl`: 1..48
	if (type <= 0 || type >= k_EventCount)
	{
		return nullptr;
	}
	return &g_State.profile.at(static_cast<size_t>(type));
}
} // namespace

void Accumulator::Reset()
{
	totalTriggerCount = 0;
	used = 0;
	head = 0;
	rate = 0.0f;
	triggeredThisTurn = false;
}

void Accumulator::Trigger(uint32_t now)
{
	if (triggeredThisTurn) // 0x449040..0x449045
	{
		return;
	}
	++totalTriggerCount;
	triggeredThisTurn = true;
	times.at(head) = now; // movsx eax, byte [ecx+8]: the head is below 64
	head = static_cast<uint8_t>((head + 1) & (k_RingSize - 1));
	if (used < static_cast<int8_t>(k_RingSize)) // `cmp al, 0x40; jge`
	{
		++used;
	}
}

void Accumulator::EndTurn()
{
	// `movsx edx, byte [flag]; mov [flag], 0; fild; fsub [+4]; fmul 0.005; fadd [+4]; fstp [+4]`
	const auto flag = static_cast<float>(triggeredThisTurn ? 1 : 0);
	triggeredThisTurn = false;
	rate = (flag - rate) * k_RateSmoothing + rate;
}

void Accumulator::ClampTimes(uint32_t now)
{
	for (auto& time : times)
	{
		if (time > now) // `cmp [eax], ecx; jbe`
		{
			time = now;
		}
	}
}

float Accumulator::TimeSinceLastUsed(uint32_t now)
{
	if (used == 0) // 0x448F41..0x448F4F: fld 0
	{
		return 0.0f;
	}
	auto since = static_cast<int32_t>(now - times.at((head - 1u) & (k_RingSize - 1)));
	if (since < 0) // `jns`
	{
		ClampTimes(now);
		since = 0;
	}
	return static_cast<float>(since) * game_clock::k_SecondsPerMs; // fild; fmul [0x8AA3B0]
}

float Accumulator::TriggersPerSecond(uint32_t now)
{
	if (used < 2) // `cmp al, 2; jge`
	{
		return 0.0f;
	}
	const auto oldest = static_cast<uint32_t>(head - static_cast<int32_t>(used)) & (k_RingSize - 1);
	const auto span = static_cast<int32_t>(now - times.at(oldest));
	if (span < 0)
	{
		ClampTimes(now);
		return 0.0f;
	}
	if (span == 0)
	{
		return 0.0f;
	}
	// 0x449016..0x44902F: lea x5 x5 x5, [x8 - 0x3E8] = used x 1000 - 1000; fild; fidiv span
	return static_cast<float>(static_cast<int32_t>(used) * 1000 - 1000) / static_cast<float>(span);
}

void SetQueries(Queries queries)
{
	g_State.queries = std::move(queries);
}

void Trigger(Event type)
{
	if (Blocked()) // 0x5C46E8..0x5C4706
	{
		return;
	}
	const auto value = static_cast<int32_t>(type);
	const auto now = g_State.accumulatedTime;
	At(type).Trigger(now);
	if (value >= 0x0E && value <= 0x17) // 0x5C4720..0x5C4730
	{
		At(Event::GestureTotal).Trigger(now);
	}
	if (value >= 0x01 && value <= 0x2A) // 0x5C4735..0x5C4745
	{
		At(Event::AllInterface).Trigger(now);
	}
	if (value >= 0x09 && value <= 0x0B) // 0x5C474A..0x5C475A
	{
		At(Event::CastAll).Trigger(now);
	}
	At(Event::AllEvents).Trigger(now); // 0x5C475F
}

void ProcessSpecialTriggers()
{
	// 0x5C45AA..0x5C45D9: GGame::GetCamera()'s current mode, dynamic_cast to CameraModeNew3; none -> nothing
	if (!g_State.queries.playerCamera)
	{
		return;
	}
	const auto view = g_State.queries.playerCamera();
	if (!view)
	{
		return;
	}
	if (view->screenCentreOnLand) // +0xC8
	{
		Trigger(Event::LookAtLand);
		// `fcomp [15]; test ah, 1; je`: below 15; `fcomp [0.55]; test ah, 0x41; jne`: above 0.55
		if (view->headingDistance < k_TooCloseDistance && view->pitch > k_TooClosePitch)
		{
			Trigger(Event::LookAtLandTooClose);
		}
		return;
	}
	if (view->pitch < k_SkyPitch) // `fcomp [-0.3]; test ah, 1; je`
	{
		Trigger(Event::LookAtSky);
	}
}

void Process()
{
	// 0x5C4669..0x5C4690: paused, a script's wide screen, [0xC4CCEE]
	if (Blocked() || (g_State.queries.processBlocked && g_State.queries.processBlocked()))
	{
		return;
	}
	ProcessSpecialTriggers();
	EndTurn(g_State.profile);
	g_State.accumulatedTime += k_MsPerProcess; // 0x5C46CF
	// fn_00449240: CameraHelp's three tables, the same step
	EndTurn(g_State.exclusions);
	EndTurn(g_State.moves);
	EndTurn(g_State.inputs);
}

void CameraHelpCallback(CameraReason reason, uint32_t inputs)
{
	const auto value = static_cast<uint32_t>(reason);
	const auto entry = value & 0xFFu;
	const auto now = g_State.accumulatedTime;
	switch (value & 0xF00u)
	{
	case 0x300: // 0x449159..0x44919C
		Trigger(static_cast<Event>(entry + 0x19));
		for (size_t bit = 0; bit < k_InputCount; ++bit)
		{
			if ((inputs & (1u << bit)) != 0)
			{
				g_State.inputs.at(bit).Trigger(now); // CameraHelp's own: no pause test
			}
		}
		break;
	case 0x200: // 0x4491A2..0x4491BB
		if (entry < k_MoveCount)
		{
			g_State.moves.at(entry).Trigger(now);
		}
		break;
	case 0x100: // 0x4491C0..0x4491D9
		if (entry < k_ExclusionCount)
		{
			g_State.exclusions.at(entry).Trigger(now);
		}
		break;
	default:
		break;
	}
}

void OnPlayerCameraMove(float yaw, float pitch, float zoom, uint32_t inputs)
{
	if (zoom != 0.0f) // 0x45C38E..0x45C3A0: `fcomp 0; test ah, 0x40; jne`
	{
		CameraHelpCallback(CameraReason::Zoom, inputs);
	}
	if (yaw != 0.0f && std::fabs(static_cast<double>(yaw)) > k_RotateThreshold) // 0x45C6A8..0x45C732
	{
		CameraHelpCallback(CameraReason::Rotate, inputs);
		if (yaw > 0.0f) // 0x45C74F..0x45C763
		{
			CameraHelpCallback(CameraReason::RotateCW, inputs);
		}
		if (yaw < 0.0f) // 0x45C77F..0x45C790
		{
			CameraHelpCallback(CameraReason::RotateCCW, inputs);
		}
	}
	if (pitch != 0.0f) // 0x45C7ED..0x45C830
	{
		CameraHelpCallback(CameraReason::Pitch, inputs);
	}
}

std::optional<float> TotalEvents(int32_t type)
{
	const auto* accumulator = Valid(type);
	if (accumulator == nullptr)
	{
		return std::nullopt;
	}
	return static_cast<float>(accumulator->totalTriggerCount); // 0x70B976: fild dword [+8 + n x 0x10C]
}

std::optional<float> EventsPerSecond(int32_t type)
{
	if (Valid(type) == nullptr)
	{
		return std::nullopt;
	}
	return g_State.profile.at(static_cast<size_t>(type)).TriggersPerSecond(g_State.accumulatedTime);
}

std::optional<float> TimeSince(int32_t type)
{
	if (Valid(type) == nullptr)
	{
		return std::nullopt;
	}
	return g_State.profile.at(static_cast<size_t>(type)).TimeSinceLastUsed(g_State.accumulatedTime);
}

const Accumulator& Get(Event type)
{
	return At(type);
}

uint32_t AccumulatedTime()
{
	return g_State.accumulatedTime;
}

void SetToZero()
{
	ResetAll(g_State.profile);
	// CameraHelp::ResetStats 0x4491E0 (from SetToZero 0x5C478C): 0x1nn, 0x2nn, then the inputs
	ResetAll(g_State.exclusions);
	ResetAll(g_State.moves);
	ResetAll(g_State.inputs);
}

void Reset()
{
	g_State = {};
}

} // namespace openblack::help_profile
