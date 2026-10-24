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
#include <glm/vec3.hpp>

#include "Enums.h"

/// A held thing given to a worship site's altar (its worship totem) by a player's hand (see altar_sacrifice for the
/// rules)
namespace openblack::ecs::altar_offering
{

/// What the altar does with the thing it is given
enum class Answer : uint8_t
{
	/// The target is not an altar, or the thing is not one an altar takes
	NotAnAltar,
	/// The altar refuses it: it stays where the hand is, to be put down
	Refused,
	/// The altar took it, and it is gone from the world
	Sacrificed,
};

/// Whether an object is a worship site's altar
[[nodiscard]] bool IsAltar(entt::entity object);
/// Whether an altar takes a held thing of its kind: a villager, an animal, a tree or a dead tree. The altar can still
/// refuse it when it is given.
[[nodiscard]] bool Takes(entt::entity altar, entt::entity held);

/// The held thing, already out of the hand, is given to the altar by a player. Taken, its prayer power goes into the
/// altar's site, the player's creature may see the deed, a villager dies as a sacrifice and an animal moves the player's
/// alignment; given by a hand (`handPoint`, the hand's point this turn), the sacrifice sounds there; it is gone from the
/// world.
Answer Offer(entt::entity altar, entt::entity held, PlayerNames player, std::optional<glm::vec3> handPoint);

} // namespace openblack::ecs::altar_offering
