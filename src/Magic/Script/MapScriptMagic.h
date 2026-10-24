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
#include <string>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// The land script's (LHScriptX) miracle commands, called from FeatureScriptCommands.cpp.

namespace openblack::magic::script
{
/// CREATE_ONE_SHOT_SPELL (83): one_off::Create(pos, the seed of that name, -1, 1). The orb; none for a seed that is not
/// known (the command then leaves the last created object alone)
std::optional<entt::entity> CreateOneShotSpell(const glm::vec3& position, const std::string& seed);
/// CREATE_ONE_SHOT_SPELL_PU (84): the magic of that name (GetInfoFromText), its first seed and that seed's level for
/// it (GetPowerUpFromMagicType). The orb, null too; none for a magic that is not known
std::optional<entt::entity> CreateOneShotSpellPu(const glm::vec3& position, const std::string& magic);

// ---- worship and miracle supply (Worship/) ----

/// CREATE_TOWN_SPELL (10) / CREATE_TOWN_CENTRE_SPELL_ICON (12): the seed of that name, AddMagicTypesHeld(its base magic)
/// and town_centre::AddSpell(seed) when the town has a centre
void CreateTownSpell(int32_t townId, const std::string& seed);
/// CREATE_NEW_TOWN_SPELL (11): the magic of that name (0 < m < 42), AddMagicTypesHeld(m) and, if the town does not
/// hold it, its seed's base magic too
void CreateNewTownSpell(int32_t townId, const std::string& magic);
/// CREATE_PLANNED_SPELL_ICON (14): AddMagicTypesHeld(the seed's base magic) only (CREATE_SPELL_ICON, 13, does nothing)
void CreatePlannedSpellIcon(int32_t townId, const std::string& seed);
/// CREATE_WORSHIP_SITE: the player's citadel with its heart builds a worship site for the tribe (the script's position
/// and site info are not used by it). The site, null too; none without the citadel or the tribe
std::optional<entt::entity> CreateWorshipSite(PlayerNames player, Tribe tribe);
/// The town a map script's dispenser joins: the town with the script's number, else the nearest town to the position
/// (the town list's first, then a strictly smaller distance over x and z); none without towns
[[nodiscard]] entt::entity DispenserTownFor(int32_t townId, const glm::vec3& position);
/// CREATE_SPELL_DISPENSER (90: town, pos, abode, magic, y angle, scale, period): dispenser::Create, the magic, an orb
/// at once, the period in turns (0 deactivates it). The dispenser, null too
entt::entity CreateSpellDispenser(int32_t townId, const glm::vec3& position, AbodeInfo abode, const std::string& magic,
                                  float yAngle, float scale, float period);
/// FIRE_FLY_SPELL_REWARD_PROB (88): the fire flies' reward probability for that magic
void FireFlySpellRewardProb(const std::string& magic, float probability);
} // namespace openblack::magic::script
