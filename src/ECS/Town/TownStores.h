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

/// Town::GetGameTurnResourceLastRemovedModifier 0x740030 (player, type): player >= 8 or type not FOOD / WOOD -> 0
/// (0x740037..0x740049); never taken -> 1 (0x740061); else r = min((turn - last) / maxGameturnsForBeliefAfterRemoving
/// FromStoragePit (GTownInfo +0x100, 1000), 1) (0x74006F..0x7400B4) and r^3 (0x7400BA..0x7400C8). How much a player
/// giving back what it has just taken counts
[[nodiscard]] float GetGameTurnResourceLastRemovedModifier(entt::entity town, PlayerNames player, ResourceType type);
/// Town::SetGameTurnResourceLastRemoved 0x7400D0 (player, type): the same bounds, then +0xEC8[player][type] = the turn
/// (0x7400E6..0x7400F9). Only Abode::DoResourceRemoving 0x404FD7 calls it
void SetGameTurnResourceLastRemoved(entt::entity town, PlayerNames player, ResourceType type);
/// GBelief::AddToBelief 0x437EB0 (P, f, thing, draw, GUIDANCE_ALIGNMENT) of the town's +0x798: +0xC8[n] += f (pending,
/// folded each turn by ecs::town_belief::Fold), +0x28[n] += f (recent, decays), f != 0 -> +0x48[n] = the turn
/// (0x437EC0..0x437EFC). With a thing: draw -> GBelief::DrawBelief 0x438800 (town_belief::DrawBelief, the belief-sprite
/// queue) and GGuidance::BeliefSFX 0x437F40 (pending: it needs the interface's position IS +0x14 for the distance; it
/// plays only for a player below the strongest belief +0x8)
void AddToBelief(entt::entity town, PlayerNames player, float f, entt::entity thing, bool draw, int guidanceAlignment);
/// Town::SetStoragePit 0x73EA60 (StoragePit::MakeFunctional 0x732F30): +0x30 = the pit; each temporary pot +0x600 /
/// +0x604: available and holding its resource -> Pot::SetupReaction 0x66D660 (animal_ai::SetupPotReaction), else
/// ToBeDeleted; the slot = 0
void SetStoragePit(entt::entity town, entt::entity pit);
} // namespace openblack::ecs::town_stores
