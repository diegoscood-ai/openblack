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

#include <functional>
#include <span>
#include <vector>

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanner.h"

/// The creature loses a thing that it uses when the thing goes before the creature completes the work. Then the
/// creature stops and decides again.
namespace openblack::creature_lost_thing
{

/// The things that the creature uses now
struct InUse
{
	/// The things of its plan (what it acts on, what the plan is done for, what it uses), when it has a plan with a step
	/// that is not done
	std::vector<uint32_t> planThings;
	/// The things of its steps, from the current step to the last
	std::vector<uint32_t> stepThings;
	/// The things that the creature itself ate or used up. Such a thing goes as the work ends, so the creature does not
	/// lose it.
	std::vector<uint32_t> usedByItself;
};

/// The things of the plan
[[nodiscard]] std::vector<uint32_t> PlanThings(const creature_planner::Plan& plan);
/// The things of the steps of the agenda, from the step given to the last
[[nodiscard]] std::vector<uint32_t> StepThings(std::span<const creature_mind::Step> agenda, size_t from);

/// Whether the creature lost a thing that it uses: a thing of its plan or of its steps is gone
[[nodiscard]] bool Lost(const InUse& inUse, const std::function<bool(uint32_t)>& gone);

} // namespace openblack::creature_lost_thing
