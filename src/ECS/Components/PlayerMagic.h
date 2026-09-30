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

/// The magic fields of GPlayer (research sources.md §1.1), on the player's entity. The players openblack does not
/// create as entities (the neutral / script player) keep theirs in magic::players (Magic/Core/Players.h).
struct PlayerMagic
{
	static constexpr size_t k_MagicTypes = 42;
	static constexpr size_t k_Tribes = 9;

	/// +0x970 MagicRemainder: how many holders enable the magic type (towns, the script); SetMagicTypeEnabled 0x64C300
	std::array<int32_t, k_MagicTypes> remainder {};
	/// +0xA18 MagicEnabled: ever been enabled (HAS_PLAYER_MAGIC; SetMagicTypeEverBeenEnabled 0x64C250)
	std::array<bool, k_MagicTypes> everEnabled {};
	/// +0x68 TribalPower[tribe]: 1.0 (ctor 0x649396, OnEndOfClearMap 0x64CD00); nothing in vanilla writes it
	std::array<float, k_Tribes> tribalPower {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	/// +0x92C the "all magic" cheat (IsMagicTypeEnabled 0x64C220 returns 1)
	bool allMagicCheat {false};
	/// +0x8E0 player type: 1 = the human interface player
	int32_t playerType {0};

	/// +0xDC, written by Spell::InitWithPos (step 7): GET_LAST_SPELL_CAST_POS / PLAYER_SPELL_LAST_CAST /
	/// PLAYER_SPELL_CAST_TIME
	struct LastCast
	{
		glm::vec3 position {0.0f}; ///< MapCoords as metres (y above the land)
		MagicType magicType {MagicType::None};
		uint32_t turn {0};
	};
	LastCast lastCast {};

	/// the spells cast of each magic type (statistics +0xA44, fn_0056A4D0)
	std::array<uint32_t, k_MagicTypes> castCount {};
	/// statistics +0x1120: chants used (WorshipSite::UseChants)
	float chantsUsed {0.0f};
	/// +0xA58 head / +0xA5C count: the player's teleport stones (MagicTeleport, newest first; Magic/Objects/MagicTeleport)
	std::vector<entt::entity> teleportStones;
};

} // namespace openblack::ecs::components
