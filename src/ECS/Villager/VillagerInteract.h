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

#include <entt/entity/fwd.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

// The disciple's inspection states 171-175 CHECK_INTERACT_WITH_*
// (docs/wiki/beings/villager-hand-and-death.md#soft-drop-and-landing). Their state function reads the inspected object
// (components::Villager::targetThing) and sets the villager to work at it. They are reached from the landing's disciple branch
// (the close objects to interact with -> 233 INSPECT_OBJECT -> the object's state), which is pending: until then nothing enters
// them. No entry / exit function; they always react to a town emergency.
//
// The disciples' checks of what they inspect (171 to 175: the worship site, the abode, the field, the fish farm and the
// tree) are never entered in the original: their one maker of those states is called from nowhere
// (docs/wiki/project/not-in-the-original.md#creaturelearning_by_observation). 172 CHECK_INTERACT_WITH_ABODE's reading,
// kept for the record (disciples): it needs the move-house check (and the move into another town), the
// town of another player's abode and the player of a town-less abode. Its reading: the target as a MultiMapFixed,
// available (else 0); built and not in need of repair, or another player's -> (A); same player, another town and not a
// town centre -> (A); another town's centre -> ForceMoveVillagerToAbode; then villager::SetupBuildingObjectForBuilding
// == 1 -> the creature empathises with the player's town desire (5, 0.5, Pos), 1; else DecideWhatToDo.
// (A): the move-house check == 1 -> 1; another player's -> the villager's town = its town, SetTopState(163), 1; a
// workshop -> SetTopState(163), 1; its own abode -> GO_HOME; else 0.

namespace openblack::ecs::villager
{

// ---- two villagers meeting (docs/wiki/beings/villager-life.md#love-outside-the-home-and-the-breeder) ---------------

/// The villager turns to face the object: the angle from it to the object, made a game angle
void MakeVillagerFaceObject(entt::entity villager, entt::entity object);
/// Two villagers meet to go into `state` together. Touching already (the gap between their edges at most `distance`):
/// they face each other and the first one goes into the state at once; true. Else each walks to its own point a gap
/// short of halfway (game_math::GetMidPoint), into the state as it arrives, and each takes the other as its target; false
bool MakeVillagesMeet(entt::entity villager, entt::entity other, VillagerStates state, float distance);

} // namespace openblack::ecs::villager
