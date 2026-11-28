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

#include "Creature/CreatureDeeds.h"
#include "Enums.h"

namespace openblack::ecs::events
{
/// A player did one of the deeds the player's creature watches, and may copy if it saw it
struct PlayerDeedForMimic
{
	PlayerNames player {PlayerNames::NEUTRAL};
	creature_watching::Deed deed {};
	entt::entity object {entt::null}; ///< what it was done to
	glm::vec3 point {0.0f};           ///< where (the object's position)
	std::optional<MagicType> magic;   ///< the miracle it was done with, if any
};

/// A player's villager saw to one of the town's needs where the player's creature may see it, and may come to share
/// that need
struct CreatureEmpathyWithTownDesire
{
	PlayerNames player {PlayerNames::NEUTRAL};
	TownDesireInfo desire {TownDesireInfo::None};
	float weight {0.0f};
	glm::vec3 point {0.0f}; ///< where the villager is
};

/// A player showed one of its own desires (kind, cruel, playful...) where the player's creature may see it, and the
/// creature may come to think its god wants it
struct CreatureEmpathyWithPlayerDesire
{
	PlayerNames player {PlayerNames::NEUTRAL};
	CreatureDesires desire {CreatureDesires::ToImpress};
	float weight {0.0f};
	glm::vec3 point {0.0f}; ///< where it was shown
};
} // namespace openblack::ecs::events
