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
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// SpellForest (0xF8 bytes, vtable 0x8F4FE4, GMagicForestInfo::AllocSpell 0x5FAD90): the forest miracle (MAGIC_TYPE 13,
// NATURE). Its PSys (SF_Forest) drops a seed from 9.4 m; when it lands (SpellEvent 3) the whole forest appears at once,
// up to finalNoTrees (18) saplings in a spiral from 2 to 11 m around the cast point, and every turn the spell grows them
// (growSpeed) towards their target scale, or shrinks them all (decaySpeed) while it wants fewer trees than it has (no
// strength left: none). The trees are MagicTrees in a Forest container (ECS/Forests, ECS/TreeGrowth,
// Magic/Objects/MagicTree). Wiki: docs/bw1-notes/magic.md ("Bosque").

namespace openblack::magic
{
/// SpellForest +0xEC..+0xF4
struct SpellForestData
{
	uint32_t forestId {0};      ///< +0xEC the Forest it made (fn_00725600): an ECS/Trees forest id, 0 none
	bool forestCreated {false}; ///< +0xF0 (fn_007254F0 clears it at allocation)
	int maxTrees {-1};          ///< +0xF4 SetMaxObjectsToCreate 0x7256C0 (-1 -> finalNoTrees)
	/// the forest went with its last magic tree (its +0xA bit 0, ToBeDeleted), seen by the next Process
	bool forestDeleted {false};
};

namespace spell_forest
{
/// SpellForest::GetForestRadius 0x725560 (the float at 0x9819F4) and fn_00725570 (0x9819F8)
constexpr float k_ForestRadius = 11.0f;
constexpr float k_InnerRadius = 2.0f;
/// SpellForest::SpellEvent 0x725830: the spiral's turns per tree (the float at 0x9819FC, 0x3FA76276 = 17 / 13)
constexpr float k_TurnsPerTree = 17.0f / 13.0f;

/// fn_00725790: round(maxTrees (+0xF4, finalNoTrees when -1) x (strength > 0 ? 1 : 0)) (fistp)
[[nodiscard]] int TreesWanted(float strength, int maxTrees, uint32_t finalNoTrees);
/// SpellForest::SpellEvent 0x725830: tree i of n in the spiral, as the offset from the cast point: f = i x (n > 1 ?
/// 1 / (n - 1) : 1), r = 2 + (11 - 2) x sqrt(1 - (1 - f)^2), angle = f x n x 1.30769 x 2 pi, (r cos, r sin)
[[nodiscard]] glm::vec2 SpiralOffset(int i, int n);
/// The MapCoords the tree goes to: (ftol(x x 6553.6), ftol(z x 6553.6), 0) back in metres
[[nodiscard]] glm::vec2 ToMapCoords(glm::vec2 point);
/// fn_007255C0: the tree's target scale (+0x64), 1 - 0.5 x distance to the cast point / 11
[[nodiscard]] float TargetScale(float distance);
/// SpellForest::GetMaxObjectsToCreate 0x7256F0: min(+0xF4, forest ? its trees : (created ? 0 : finalNoTrees))
[[nodiscard]] int MaxObjectsToCreate(int maxTrees, uint32_t finalNoTrees, bool hasForest, uint32_t trees, bool created);
/// SpellForest::CalculateCostToMaintain 0x7259E0: costPerGameTurn + trees x costPerEvent
[[nodiscard]] float CostToMaintain(float costPerGameTurn, float costPerEvent, uint32_t trees);
/// SpellForest::AdjustSpellSeedPos 0x725750: the seed's altitude (MapCoords y) is at least the tallest tree's height
/// (fn_0053A740), or -5 without a forest (+0xEC == 0: until SpellEvent 3 made it). Called through vt 0x540 by the seed's
/// draw 0x729020 (call at 0x72906E), every frame: the seed sits under the land while the PSys seed falls, then on the
/// ground in the middle of the new forest, then on top of its tallest tree as the trees grow.
[[nodiscard]] float AdjustSpellSeedAltitude(bool hasForest, float tallestTree, float altitude);
/// vt 0x540 of a forest spell (SpellForest::AdjustSpellSeedPos 0x725750) on the seed's altitude
[[nodiscard]] float AdjustSpellSeedPos(entt::entity spell, float altitude);

/// GMagicForestInfo::CanCast 0x5FAE80 (vt 0x30, the check at a position): in bounds, land, no Abode of the cell there
/// whose Get2DRadius reaches the point (fn_005FADF0), and ValidPlaceForTree
[[nodiscard]] bool CanCastAt(const glm::vec3& position);
/// SpellForest::ValidPlaceForTree 0x725C50: in bounds, land and not MapCoords::IsFixed 0x603790 (the cell's first fixed
/// object, the last one put in it, is a MultiMapFixed: a building, a field, a feature, a static... not a tree)
[[nodiscard]] bool ValidPlaceForTree(const glm::vec3& position);
/// SpellForest::GetRandomTreeInfo 0x725580: the magicTreeTypes[GameRand(4)] of Terrain::GetMaterialInfo 0x735330 at the
/// cell of the position (the snow material 27 when GClimate::GetSnow >= 27 there)
[[nodiscard]] TreeInfo RandomTreeType(const glm::vec3& position);

/// The spell's state (for the trace and the tests); nullptr if it is not a forest spell
[[nodiscard]] const SpellForestData* DataOf(entt::entity spell);
/// SpellForest::GetMaxObjectsToCreate 0x7256F0 of a spell (vt 0x550)
[[nodiscard]] int GetMaxObjectsToCreate(entt::entity spell);
} // namespace spell_forest
} // namespace openblack::magic
