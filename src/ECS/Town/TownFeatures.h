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
#include <glm/vec3.hpp>

// The town features of a map load (Town.cpp of runblack.exe W120; spec
// dev\documentacion\edificios\fields_features_spec.md §1): Town::AsssignTownFeature 0x73EAC0, which only does the
// towns' forests (fields and fish farms take their town in their constructors).

namespace openblack::ecs::town_features
{
/// Town::AssignForestsToTown 0x73EB00's reference point (0x73EB06..0x73EB4D): the storage pit's position (+0x14) when
/// GetStoragePit 0x73B5B0 gives one, else GetTemporaryResourceStorePotOrPos(town +0x14, WOOD) 0x73EB4D (which makes
/// the town's MagicWood pot when it has none); metres, altitude 0
[[nodiscard]] glm::vec3 ForestReference(entt::entity town);

/// Town::AsssignTownFeature 0x73EAC0 (static): MakeScenicForest 0x741B40 for every town of g_game +0x205C84 (newest
/// first, town_queries::TownsNewestFirst) at its town rectangle's centre (fn_0073AE10), then AssignForestsToTown
/// 0x73EB00 for every town in the same order (two separate passes, 0x73EAC6..0x73EADC and 0x73EAE4..0x73EAFA). Called
/// at the end of GSetup::LoadMapFeatures 0x71813D (Game::LoadMap, after the map's script)
void AsssignTownFeature();
} // namespace openblack::ecs::town_features
