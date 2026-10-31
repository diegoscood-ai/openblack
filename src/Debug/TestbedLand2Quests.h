/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

// The commands Land 2's quest runs share: TestbedScenarioLand2Quests.cpp and each quest's TestbedQuest*.cpp
namespace openblack::testbed_scenarios::land2_quests
{
using Kind = Command::Kind;
using testbed_land_timeline::Compare;
using testbed_land_timeline::TaskWait;
using testbed_land_timeline::ThingNear;

/// The Action (right) button
inline constexpr size_t k_ActionButton = 3;
/// Where the camera is put for a scroll or a building: this far above the land and back from it, within the 50 the
/// workshop's quest needs the camera to be near
inline constexpr float k_EyeAbove = 25.0f;
inline constexpr float k_EyeBack = 30.0f;
/// How near the hand's press looks for the scroll it presses on
inline constexpr float k_ScrollPickRadius = 15.0f;

/// The wait for a task of a script: running, by default, or none running after one was
[[nodiscard]] inline Command WaitTask(std::string_view script, TaskWait wait = TaskWait::Running)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = static_cast<size_t>(wait), .script = script};
}

/// A script started as a new task, as the land's control script starts it
[[nodiscard]] inline Command StartScript(std::string_view script)
{
	return {.kind = Kind::StartScript, .script = script};
}

[[nodiscard]] inline Command WaitText(uint32_t text)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = true};
}

[[nodiscard]] inline Command WaitGlobal(std::string_view global, float value)
{
	return {.kind = Kind::WaitGlobal, .waitUntilFree = true, .amount = value, .global = global, .compare = Compare::Equal};
}

/// Logs the globals and scripts named, some seconds after the command before it
[[nodiscard]] inline Command LogGlobals(std::string_view globals, std::string_view scripts, float delay = 0.0f)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .global = globals, .script = scripts};
}

[[nodiscard]] inline Command ClickTexts()
{
	return {.kind = Kind::ClickTexts, .value = 1};
}

/// The camera put back from a point and above it, looking at it; both heights are above the land there, as the
/// timeline's camera takes them
[[nodiscard]] inline Command CameraOn(glm::vec2 point, float eyeHeight, float back, float lookHeight)
{
	return {.kind = Kind::CameraTo,
	        .delaySeconds = 1.0f,
	        .eye = {point.x, eyeHeight, point.y - back},
	        .look = {point.x, lookHeight, point.y}};
}

/// A gold scroll pressed on with the hand: the pointer on the drawn scroll, then the Action button pressed once the
/// hand is over it and let go, as a player clicks it
inline void PressScroll(std::vector<Command>& commands, glm::vec2 scroll)
{
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = scroll, .value = 1});
	commands.push_back(
	    {.kind = Kind::PointerPress,
	     .delaySeconds = 0.5f,
	     .waitUntilFree = true,
	     .value = k_ActionButton,
	     .thing = ThingNear {.type = script::ObjectType::Highlight, .point = scroll, .radius = k_ScrollPickRadius}});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
}

/// Destroy it!, the land's last gold scroll (TestbedQuestDestroyIt.cpp)
void AddDestroyItScenarios(std::vector<Scenario>& all);
/// Leave through the vortex, Land 2's exit (TestbedQuestLeaveVortexL2.cpp)
void AddLeaveVortexL2Scenarios(std::vector<Scenario>& all);

} // namespace openblack::testbed_scenarios::land2_quests
