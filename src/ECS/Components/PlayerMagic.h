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

#include <array>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// The magic fields of a player, on the player's entity. The players openblack does not
/// create as entities (the neutral / script player) keep theirs in the player system (Locator::playerSystem).
struct PlayerMagic
{
	static constexpr size_t k_MagicTypes = 42;
	static constexpr size_t k_Tribes = 9; ///< Tribe::_COUNT

	/// how many holders enable the magic type (towns, the script)
	std::array<int32_t, k_MagicTypes> remainder {};
	/// ever been enabled (HAS_PLAYER_MAGIC)
	std::array<bool, k_MagicTypes> everEnabled {};
	/// the tribal power of each tribe, read by the miracles, the villagers and the worship: 1.0 when made and when the
	/// map is cleared, then every player's turn copies the most tribal power into it (magic::players)
	std::array<float, k_Tribes> tribalPower {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	/// the most tribal power of each tribe: 1.0 when made and when the map is cleared; a built wonder adds its power
	/// to its tribe's and takes it off again when it goes (ecs::wonders); every player's turn keeps it at least 1
	std::array<float, k_Tribes> maxTribalPower {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	/// the "all magic" cheat (every magic type is enabled)
	bool allMagicCheat {false};
	/// The player type of a player the computer plays
	static constexpr int32_t k_ComputerPlayerType = 2;

	/// player type: 1 = the human interface player, 2 = a player the computer plays
	int32_t playerType {0};

	/// written when a spell starts at its position: GET_LAST_SPELL_CAST_POS / PLAYER_SPELL_LAST_CAST /
	/// PLAYER_SPELL_CAST_TIME
	struct LastCast
	{
		glm::vec3 position {0.0f}; ///< MapCoords as metres (y above the land)
		MagicType magicType {MagicType::None};
		uint32_t turn {0};
	};
	LastCast lastCast {};

	/// the spells cast of each magic type (statistics)
	std::array<uint32_t, k_MagicTypes> castCount {};
	/// statistics: chants used (by the worship site)
	float chantsUsed {0.0f};
};

} // namespace openblack::ecs::components
