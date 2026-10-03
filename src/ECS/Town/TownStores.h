/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "ECS/MapCoords.h"
#include "Enums.h"

// The town's resource stores outside its storage pit: the temporary pots Town +0x600 / +0x604 (Town.cpp of runblack.exe
// W120). Villagers drop their resources there while the town has no working storage pit.

namespace openblack::ecs::town_stores
{
/// What Town::GetTemporaryResourceStorePotOrPos returns: the pot (its return value) and the point to go to (its out
/// MapCoords)
struct TemporaryStore
{
	entt::entity pot {entt::null};
	map_coords::MapCoords pos {};
};

/// 5.0 (the immediate 0x40A00000 at 0x73E96E): how far from the congregation point a new wood pot goes
constexpr float k_WoodPotOffset = 5.0f;
/// Town::FindClearArea 0x7412F0's arguments at 0x73E9A6..0x73E9C2: 45.0 (0x42340000), 1.5 (0x3FC00000), 2.0 (0x40000000)
constexpr float k_ClearAreaA = 45.0f;
constexpr float k_ClearAreaB = 1.5f;
constexpr float k_ClearAreaRadius = 2.0f;

/// Town::GetTemporaryResourceStorePotOrPos 0x73E900 (from, &out, type): the pot of +0x600[type] (FOOD 0, WOOD 1) when
/// it is available (vt +0x2C). Otherwise a new one: the congregation point (GetCongregationPos 0x7408B0) +
/// GetPosFromAngle(0, WOOD ? 5 : 0), moved by FindClearArea(45, 1.5, 2) away from the MultiMapFixed objects (the filter
/// 0x73EA50, Flags +0x24 bit 1; its result is not tested) and Pot::Create 0x66CF10 with GPotInfo FOOD ? 10 MagicFood
/// : 9 MagicWood and amount 0; kept in +0x600[type]. Either way out = pot.GetNearestEdgeToPos(from) (vt +0x83C). There is
/// always a pot: the original never returns NULL (Pot::Create fails only when its allocation does). Without a town (or
/// another type) {entt::null, from}
[[nodiscard]] TemporaryStore GetTemporaryResourceStorePotOrPos(entt::entity town, const map_coords::MapCoords& from,
                                                               ResourceType type);
} // namespace openblack::ecs::town_stores
