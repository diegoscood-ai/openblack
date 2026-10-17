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

#include <optional>

#include <glm/vec3.hpp>

#include "Common/GUtilsDistance.h"
#include "Enums.h"

/// The rules of a villager under a script's control: playing the clips a script asks for, the states that may take it
/// out of the script's hands, and how scripts measure distance
namespace openblack::ecs::villager_script_rules
{

/// A script's distance below this is no distance at all
constexpr float k_NoDistance = 0.5f;

/// The distance a script measures between two points: the game's distance, along the ground only, and none at all
/// under half a metre, so a script waiting for something to reach a place sees it there once it has stepped onto it
[[nodiscard]] inline float ScriptDistance(const glm::vec3& from, const glm::vec3& to)
{
	const float distance = gutils::GetDistance(from, to);
	return distance < k_NoDistance ? 0.0f : distance;
}

/// One turn of playing a script's clip: the plays left after it, and the state to go into once the clip playing ends
struct ScriptClipStep
{
	uint32_t playsLeft;
	std::optional<VillagerStates> then;
};

/// A villager playing a script's clip counts one more play each time it starts the clip: with more to come it plays it
/// again, after the last it stands waiting for the script. With no plays left it does nothing more; a script asking for
/// endless plays gives a count that never runs out
[[nodiscard]] constexpr ScriptClipStep StepScriptClip(uint32_t playsLeft)
{
	if (playsLeft == 0)
	{
		return {.playsLeft = 0, .then = std::nullopt};
	}
	--playsLeft;
	return {.playsLeft = playsLeft, .then = playsLeft == 0 ? VillagerStates::InScript : VillagerStates::ScriptPlayAnim};
}

/// Whether a villager has played the clip a script asked for: not while a clip plays out, nor while plays are left
[[nodiscard]] constexpr bool HasPlayedScriptClip(VillagerStates top, uint32_t playsLeft)
{
	if (top == VillagerStates::WaitForAnimation)
	{
		return false;
	}
	if (top == VillagerStates::ScriptPlayAnim)
	{
		return playsLeft == 0;
	}
	return true;
}

/// What a state says about a villager a script holds
struct StateRules
{
	bool isScriptState;
	bool isScriptInterruptable;
	/// A state a villager works towards, rather than one it passes through on the way
	bool isFinalState;
};

/// Whether a villager a script holds may leave for another state: for another of the script's states, one the script
/// lets interrupt it, being picked up, a state with the same exit as the one it works towards (`sameExit`), or a state
/// it only passes through
[[nodiscard]] constexpr bool ScriptLetsGo(VillagerStates next, StateRules nextRules, bool sameExit)
{
	return nextRules.isScriptState || nextRules.isScriptInterruptable || next == VillagerStates::InHand || sameExit ||
	       !nextRules.isFinalState;
}

/// What a villager dancing in a script's dance does when asked to leave for another state
struct DanceExit
{
	/// It leaves its dance
	bool leavesDance;
	/// It may go into the state
	bool mayGo;
};

/// A villager in a script's dance asked to go into `next`: into another of the script's states it stays in its dance
/// and may go; else it leaves its dance, and may go only where the script lets it (ScriptLetsGo). Asked to decide what
/// to do, as when its dance goes, it is out of the dance but stays in the dance's state, waiting for the script
[[nodiscard]] constexpr DanceExit ExitDance(VillagerStates next, StateRules nextRules, bool sameExit)
{
	if (nextRules.isScriptState)
	{
		return {.leavesDance = false, .mayGo = true};
	}
	return {.leavesDance = true, .mayGo = ScriptLetsGo(next, nextRules, sameExit)};
}

/// The age a script gives a child it creates
constexpr uint32_t k_ScriptChildAge = 10;

/// The age a script gives a villager it creates: a child is 10, an adult one year past growing up
[[nodiscard]] constexpr uint32_t ScriptCreateAge(bool child, uint32_t grownUpAge)
{
	return child ? k_ScriptChildAge : grownUpAge + 1;
}

/// A script creating a villager or a child: it is made at its age, then given that same age a second time (which draws
/// its size for the age again), then put straight into the script's hands (IN_SCRIPT), whatever state it was born in,
/// drowning included. `create(age)` makes the villager, `setAge(villager, age)` and `setScriptState(villager, state)`
/// act on it. The result is the villager made
template <typename Create, typename SetAge, typename SetScriptState>
auto CreateScriptVillager(bool child, uint32_t grownUpAge, Create&& create, SetAge&& setAge, SetScriptState&& setScriptState)
{
	const uint32_t age = ScriptCreateAge(child, grownUpAge);
	auto villager = create(age);
	setAge(villager, age);
	setScriptState(villager, VillagerStates::InScript);
	return villager;
}

} // namespace openblack::ecs::villager_script_rules
