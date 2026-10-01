/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <entt/entity/entity.hpp>

#include "Enums.h"

// The miracles a town holds (Town.cpp 0x73D1C0..0x73D630) and whether it may have a worship site (0x740BB0 / 0x740BF0).
// A player owns a magic type while one of its towns holds it (GPlayer::MagicRemainder, Magic/Core/Players.h).

namespace openblack::worship::town
{
/// Town::AddMagicTypesHeld 0x73D380: not held yet -> held, the owner's SetMagicTypeEnabled(1), and the town centre's
/// icon (TownCentre::AddSpell of the seed for a base magic, AddPowerUp of its level for a power-up). 1 if newly added.
bool AddMagicTypesHeld(entt::entity town, MagicType type);
/// Town::RemoveMagicTypesHeld 0x73D450: the mirror (fn_007442C0 removes the spell's icon, fn_00744030 clears the level)
void RemoveMagicTypesHeld(entt::entity town, MagicType type);
/// Town::IsMagicTypeHeld 0x73D630
[[nodiscard]] bool IsMagicTypeHeld(entt::entity town, MagicType type);

/// fn_0073D500 (creature theft): if the town holds the seed's base magic, it and its held power-ups go; `taken` records
/// which ([0] the base, [1..3] the levels). 1 when something was taken.
bool TakeSeedMagic(entt::entity town, SpellSeedType seed, std::array<bool, 4>& taken);
/// fn_0073D5A0: gives them back / to another town
void GiveSeedMagic(entt::entity town, SpellSeedType seed, const std::array<bool, 4>& taken);

/// Town::IsAllowedToCreateWorshipSite 0x740BB0: not on land 1, not forbidden by the script (+0x5F0), and a population
/// (Town +0x618 + +0x61C: the TownStats adults and children, TownStats::Add 0x7492E0)
[[nodiscard]] bool IsAllowedToCreateWorshipSite(entt::entity town);
/// Town::CheckAddWorshipSite 0x740BF0: allowed, a player that is not type 3 with a citadel ->
/// Citadel::FindOrCreateWorshipSite and WorshipSite::AddTown when the town is not in it yet
void CheckAddWorshipSite(entt::entity town);

/// The town's centre (Town +0x9A4): its Abode of type TOWN_CENTRE, or entt::null
[[nodiscard]] entt::entity TownCentreOf(entt::entity town);
/// The town's population for worship (+0x618 + +0x61C): its villagers
[[nodiscard]] int Population(entt::entity town);
/// Town vt 0x1C GetPlayer
[[nodiscard]] PlayerNames OwnerOf(entt::entity town);
/// The town entity of an id (components::Town::id), or entt::null
[[nodiscard]] entt::entity FromId(uint32_t id);
} // namespace openblack::worship::town
