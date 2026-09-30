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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

struct BigForest
{
	int type;
	/// +0x84 (Tree::GetWoodValue): the wood left; the forest is scaled to wood / its info's woodValue
	float wood {0.0f};
	float woodValue {1.0f}; ///< the info's woodValue
};

struct Forest
{
	int type;
};

/// Forest (0x58 bytes, a Container; ctor 0x539BD0, fn_005399E0; list g_game +0x205BB4): a group of trees. Its entity has
/// only this. ECS/Forests.
struct ForestTrees
{
	uint32_t id {0};              ///< +0x40 (the counter 0xBEA238)
	glm::vec3 position {0.0f};    ///< +0x14 (x, z; the MapCoords y is 0)
	bool hasPlayer {false};       ///< the Container's player (fn_0046B8A0 argument)
	PlayerNames player {PlayerNames::NEUTRAL};
	std::vector<entt::entity> grown;   ///< +0x48 / +0x4C Trees0: grown (or never growing), nearest the forest first
	std::vector<entt::entity> growing; ///< +0x50 / +0x54 Trees1: still growing, nearest first
	uint16_t emptyCountdown {0};  ///< +0x34 (Forest::Process: an empty forest goes after 2000 turns)
	uint16_t newTreeTimer {0};    ///< +0x36 (Forest::Process: the natural new-tree timer, postponed)
};

} // namespace openblack::ecs::components
