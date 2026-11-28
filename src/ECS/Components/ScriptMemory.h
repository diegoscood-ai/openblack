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

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/ScriptAnimation.h"
#include "ECS/Components/ScriptStateArguments.h"
#include "Enums.h"

namespace openblack::ecs::components
{

/// What a villager taken out of a script state keeps of it, to take it up again when it comes back to that state: the
/// walk it was on, what the script had given it, and the clip it last played (ECS/Villager/VillagerScriptMemory.h).
/// Made the first time it is in a script state or a script's dance, and kept until it goes
struct ScriptMemory
{
	/// The thing its walk followed (openblack's villagers have no walk to a thing yet: always none)
	entt::entity target {entt::null};
	/// Where its walk was going, its height above the land with it
	map_coords::MapCoords destination {};
	/// The script's place and number, and its pair of whole numbers (the clip and its plays), as it had them
	std::optional<ScriptStateArguments> arguments;
	std::optional<ScriptAnimation> clipAndPlays;
	/// The clip its state last put on it; -2 for none yet
	int32_t stateClip {-2};
	/// The state it was taken out of, which coming back to resumes what it kept; none once taken up
	VillagerStates state {VillagerStates::InvalidState};
};

} // namespace openblack::ecs::components
