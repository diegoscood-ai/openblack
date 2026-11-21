/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::physics_hooks
{
class World;
} // namespace openblack::ecs::physics_hooks

/// A thing starting to fly is offered to every creature that may catch it (PhysicsClassHooks::OfferToCatchingCreatures):
/// each creature, in turn, that may set about catching, is heavy enough, and can step to where the thing passes before
/// its hand must close sets about catching it. A throw of another player's is gone for only now and then.
namespace openblack::ecs::creature_catch_walk
{
/// The thing as it starts to fly: where it is and how it flies, what threw it, and the player whose throw it is, if any
struct Flight
{
	glm::vec3 centre {0.0f};
	glm::vec3 velocity {0.0f};
	entt::entity thrower {entt::null};
	std::optional<PlayerNames> player;
};
/// Asks every creature of the world, in the game's order, and sets about catching for each that can
void Offer(physics_hooks::World& world, entt::entity object, const Flight& flight);
} // namespace openblack::ecs::creature_catch_walk
