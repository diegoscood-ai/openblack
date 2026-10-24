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

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A frightened villager taking cover: it runs along its town's paths to the nearest building it can hide in, stays
/// inside out of sight for a while, then comes out and looks about before going back to what it was doing. One with
/// nowhere to hide panics in the open: having run off it turns to look back at what it ran from, then decides afresh.
/// The state functions are the living action system's rows 236, 237 and 240.
namespace openblack::ecs::villager_cover
{

/// The villager sets off for the nearest building of its town it can hide in, if there is one near enough; whether it
/// did. What frightened it, if known, keeps it inside while that stays within the keep-away distance of a kind of
/// reaction.
bool SetupGoAndHideInNearbyBuilding(entt::entity villager, entt::entity threat = entt::null,
                                    Reaction threatKind = Reaction::None);
/// The point a panicking villager ran from, which it looks back at once it stops
void SetPanicPoint(entt::entity villager, glm::vec2 point);
/// The building the villager last hid in, while it still stands and can hide a villager; entt::null otherwise
[[nodiscard]] entt::entity BuildingHiddenIn(entt::entity villager);
/// The villager goes back into the building it last hid in, if that still stands and can hide it, its turns inside
/// counted afresh when `countAfresh`; whether it went
bool HideAgain(entt::entity villager, bool countAfresh);

/// The villager's take-up of REACT_TO_MAGIC_SHIELD_DESTROYED (the slot type's setup): it hides first; with nowhere to
/// hide it runs from where the shield stood, a little to one side, and then looks back there in panic. The shield it
/// was amazed by before, if any, is what it hides from or goes on watching
void SetupReactToMagicShieldDestroyed(entt::entity villager, entt::entity spell, uint32_t reaction);

uint32_t GoAndHideInNearbyBuilding(components::LivingAction& action);
/// Looking about outside the building it hid in: a predator still about (an animal within the reaction's reach, awake
/// and out of its lair) sends it back inside, or with its building gone makes it run from the predator if that is
/// coming at it; else it counts the turns until it is safe
uint32_t LookToSeeIfItIsSafe(components::LivingAction& action);
uint32_t PanicReaction(components::LivingAction& action);

/// A land is loaded: no villager hides or panics
void Clear();

} // namespace openblack::ecs::villager_cover
