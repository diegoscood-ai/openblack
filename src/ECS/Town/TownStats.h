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

#include "ECS/Components/Town.h"
#include "Enums.h"

// The town's TownStats (Town +0x610 of runblack.exe W120; spec dev\tmp_dis\aldeanos\V3_spec.md §2.3): the original
// keeps them incrementally (TownStats::Add(Villager) 0x7492E0 / Remove 0x7493C0, Add(Abode) 0x7498C0 / Remove
// 0x749990 when a villager or an abode joins or leaves the town); openblack recomputes them from the entities at the
// start of Town::Process (ecs::town_process). (aproximado) the same counts; the float sums are added in another order.

namespace openblack
{
struct GAbodeInfo;
}

namespace openblack::ecs::town_stats
{
/// Add(Villager) 0x7492E0 for each villager of the town (Villager::town) and Add(Abode) 0x7498C0 for each of its
/// abodes (Abode::townId; the fields too, they are Abodes). (aproximado hasta V6) Add(Abode) runs at MakeFunctional
/// in the original: openblack's script abodes are all whole, so all count
[[nodiscard]] components::TownStats Compute(entt::entity town);
/// The abodes of the town (Town +0x754 / +0x758): Abode::townId == Town::id, newest first (the creation index from
/// high to low, as town_queries' congregation point: AddStructureToTown 0x7399C3 inserts at the head)
[[nodiscard]] std::vector<entt::entity> AbodesOf(entt::entity town);
/// The abode's GAbodeInfo (Abode +0x28): the record of its abode number and mesh, else its tribe's (as
/// influence::AbodeInfoOf; (inferido) openblack keeps no info pointer); nullptr when none
[[nodiscard]] const GAbodeInfo* AbodeInfoOf(entt::entity abode, Tribe tribe);
/// GAbodeInfo::Find 0x405B30 (TRIBE_TYPE, ABODE_NUMBER): the FIRST record whose tribe is the given one or -1 (+0x154)
/// and whose number (+0x120) matches; null when none. (GAbodeInfo::Find of InfoConstants.cpp keeps the last match and
/// throws without one: this one is the exe's)
[[nodiscard]] const GAbodeInfo* FindAbodeInfo(Tribe tribe, AbodeNumber number);
/// Abode::IsCivic 0x405FF0 (vt +0x8C0): ABODE_TYPE (+0x120) Totem 0x14, StoragePit 0x24, Creche 0x44, Workshop 0x84,
/// Wonder 0x100, Graveyard 0x204, TownCentre 0x404, FootballPitch 0x1004, SpellDispenser 0x2004 (the jump table
/// 0x406044 / 0x40604C); not Field 0x4004 nor Citadel 0x804
[[nodiscard]] bool IsCivic(AbodeType type);
} // namespace openblack::ecs::town_stats
