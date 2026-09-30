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

// MagicTree (MagicTree.cpp of the original, 0x5FCF00..0x5FD1C0): the trees of the forest miracle. A normal Tree entity
// (TreeArchetype) plus components::MagicTree. Wiki: docs/bw1-notes/magic.md ("Bosque").

namespace openblack::magic::magic_tree
{
/// fn_005FD000 (new MagicTree + CallVirtualFunctionsForCreation) -> MagicTree::MagicTree 0x5FCF50 (pos, spell, info,
/// forest, angle, scale, woodMul): Tree(pos, info, forest, maxScale 1.0, angle, scale) (0x749E00: growing, in the
/// forest: ECS/Trees' forest id, TreeArchetype::Create), the magic flag (+0x5C bit 1), +0x70 = woodMul, +0x6C = the
/// spell's player (without a spell the script player g_game +0x205A5B, the neutral one here), and
/// CreateReaction(this, REACT_TO_MAGIC_TREE, GetPlayer, 0).
/// position: x, z on the map (the tree stands on the land).
entt::entity Create(const glm::vec3& position, entt::entity spell, TreeInfo type, uint32_t forestId, float angle,
                    float scale, float woodValueMultiplier);

/// MagicTree::ToBeDeleted 0x5FD070: its reactions go, then Tree::ToBeDeleted 0x74A210 (out of the forest, the entity
/// goes). The forest going with its last tree is done by SpellForest (the forests of ECS/Trees are never deleted by a
/// tree).
void ToBeDeleted(entt::entity tree);
/// A magic tree that went without ToBeDeleted (ECS/Trees' ShrinkAllTrees destroys the entity, fire, the hand) or left
/// its forest: its reactions go as MagicTree::ToBeDeleted would have done; `gone` = the entity is no longer a tree
void Forget(entt::entity tree, bool gone);

/// MagicTree::StartOnFire 0x5FD0D0: its REACT_TO_MAGIC_TREE goes
void StartOnFire(entt::entity tree);
/// MagicTree::EndOnFire 0x5FD0E0: REACT_TO_MAGIC_TREE again, unless the game flag g_game +0x14 bit 0x8000 (not known:
/// taken as clear)
void EndOnFire(entt::entity tree);

/// GetWoodValueMultiplier (vt 0x868): MagicTree 0x5FD0C0 = +0x70, Tree 0x74B810 = 1 (Tree::GetWoodValue 0x74B7B0 = life
/// x this x woodValue x scale x [0xD1A294])
[[nodiscard]] float WoodValueMultiplier(entt::entity tree);
} // namespace openblack::magic::magic_tree
