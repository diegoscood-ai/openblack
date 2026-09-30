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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// The Forest containers (Forest.cpp of the original, 0x539960..0x53B2F0): an entity with components::ForestTrees that
// keeps two tree lists, the grown ones and the growing ones, each sorted by the distance to the forest. Only the forest
// miracle makes them yet (SpellForest fn_00725600); the scripts' forests are still the trees' forestId numbers.
// Wiki: docs/bw1-notes/magic.md ("Bosque").

namespace openblack::ecs::forests
{
/// fn_005399E0 (new Forest(pos, creator)): the Container at pos with the creator's player, id = the counter 0xBEA238++,
/// at the front of the forest list. The counter is not kept by openblack's script forests: the next id is one more than
/// any forestId or forest id in use (inf).
[[nodiscard]] entt::entity Create(const glm::vec3& position, bool hasPlayer, PlayerNames player);

/// Forest::AddTree 0x53A310: tree +0x68 = the forest; a growing tree below its maxScale goes to the growing list, the
/// rest to the grown list, each kept in DistanceToForest order (0x53A890: the tree's distance to the forest, 0 without one;
/// a new tree goes before the first one that is farther)
void AddTree(entt::entity forest, entt::entity tree);
/// fn_0053A220: the tree leaves the list it belongs in (the growing one first if it is growing below its maxScale)
void RemoveTree(entt::entity forest, entt::entity tree);

/// +0x4C + +0x54: the trees in both lists (the ones gone from the world are dropped first)
[[nodiscard]] uint32_t TreeCount(entt::entity forest);
/// fn_0053A520 (count, amount): Tree::Grow(amount, false, false) on the growing list, then the grown list; the sum of
/// the positive growths (count is not used)
float GrowTrees(entt::entity forest, float amount);
/// fn_0053A490 (count, amount): fn_0074A3A0 (shrink, deleted at 0) on every tree of both lists; the sum of the positive
/// results (count is not used)
float DecayTrees(entt::entity forest, float amount);
/// fn_0053A740: the largest Object::GetHeight (vt 0x42C) of its trees, 0 with none
[[nodiscard]] float TallestTree(entt::entity forest);

/// Forest::ToBeDeleted 0x539C60: each tree of both lists ToBeDeleted, then the forest leaves the list and goes
void ToBeDeleted(entt::entity forest);
/// The forest still exists (not ToBeDeleted: +0xA bit 0)
[[nodiscard]] bool Exists(entt::entity forest);

/// Forest::ProcessForests 0x539D70 -> Forest::Process 0x539DA0, each turn: an empty forest counts 2000 turns down and
/// goes; each growing tree's Tree::Process, and the ones done move to the grown list. The natural new trees (every 2000
/// + GameFloatRand(1000) turns x 0.05 x the grown trees, fn_00539FD0 / fn_0053A010, and the most influential player's
/// alignment) are postponed with the natural growth.
void ProcessForests();

/// A land is loaded: the containers go with the registry
void Clear();
} // namespace openblack::ecs::forests
