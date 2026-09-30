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

#include <cstddef>
#include <optional>
#include <vector>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs
{

/// Tree::Process 0x74A290, once per game turn: a tree that belongs to a forest and has not reached its maximum size
/// grows every growTurns turns. Only forests process their trees in the original (Forest::Process 0x539DA0), so the
/// script's forest-less trees (every tree of Land 1 and Land 2) never grow.
void ProcessTreesTurn(uint32_t turn);

/// Per frame (Tree::PreDraw 0x74A7C0 and the tail of Tree::Draw 0x74B111): the brightness of every tree this frame and
/// the leaf rustle of the tall ones next to the camera.
void UpdateTrees(float seconds);

/// Tree::PreDraw 0x74A883, the global 0xC22FA0 / 256: every RGB channel of a tree's colour is multiplied by this,
/// 200/256 (looking against the light) to 255/256. Only trees use it.
[[nodiscard]] uint8_t TreeBrightness();

/// Tree::Grow 0x74A3F0: grows by `amount` up to maxSize (raising maxSize first when `raiseMax`), and re-registers the
/// obstacle circle like the original's SetScale. Returns how much it actually grew.
float GrowTree(entt::entity tree, float amount, bool raiseMax);

/// Forest ctor 0x539BD0: a forest at `centre` with that id (0 takes the next free id, a given id raises the counter
/// past it; CREATE_FOREST and Tree::EndPhysics's `new Forest(pos, 0)`). Returns its id.
uint32_t CreateForest(uint32_t id, glm::vec3 centre);

/// Whether a forest with that id exists (CREATE_TREE / CREATE_NEW_TREE look the script's id up in the forest list and
/// pass no forest when there is none, 0x7162BE).
[[nodiscard]] bool IsInForest(uint32_t forestId);

/// The id to store in a tree for the script's forest id: the id itself if that forest exists, else 0 (no forest).
[[nodiscard]] uint32_t ResolveForestId(int32_t scriptForestId);

/// The forest a town's replanted trees join (the original keeps a list of forests per town, Town +0x608, and
/// Tree::EndPhysics joins the last of them). Creates one at `at` the first time the town needs it.
uint32_t TownForestId(uint32_t townId, glm::vec3 at);

/// The nearest forest whose centre is within `radius` of `at` (for the forest miracle), if any.
[[nodiscard]] std::optional<uint32_t> NearestForest(glm::vec3 at, float radius);

/// Moves a tree into a forest (Forest::RemoveTree of the old one, Forest::AddTree 0x53A310 of the new one; 0 = none).
void SetTreeForest(entt::entity tree, uint32_t forestId);

/// fn_0053A010: plants a sapling of `parent`'s type next to it: 32 angles (from a random one, 2pi/32 apart) x 5 radii
/// (a random whole 5-9 m, then (r + 2) mod 10), the first free spot on land; the new tree is size 0.1, grows to
/// 0.8 + rand(0.4), at a random angle, in the forest. Returns the tree or entt::null when nothing fits.
entt::entity PlantTreeNear(uint32_t forestId, entt::entity parent);

/// Tree::ApplyWaterSpell 0x74C390 (the tree's side of the water miracle): a growing tree grows by waterMultiplier x
/// growAmount; with `raiseMaximum` (the spell subtype 0x17) a full grown one also grows, by half that times
/// GetDistanceModifier(size, 3), past its maximum. A full grown tree of a forest watered without it, more than 40 turns
/// after the last new tree of the world, plants a sapling next to itself (returned; the caller gives the player the good
/// alignment and the 0xE statistic).
entt::entity ApplyWaterSpell(entt::entity tree, bool raiseMaximum);

/// fn_0053A520 (SpellForest::ProcessTrees 0x725A30): every tree of the forest gets Tree::Grow(amount, false, false);
/// returns the sum of what they grew.
float GrowAllTrees(uint32_t forestId, float amount);

/// fn_0053A490: every tree of the forest shrinks by `amount` (fn_0074A3A0: a tree that would reach 0 is deleted and adds
/// nothing); returns the sum of what they shrank.
float ShrinkAllTrees(uint32_t forestId, float amount);

/// fn_0053A740: the height (Object::GetHeight, mesh height x scale) of the forest's tallest tree, 0 if it has none.
[[nodiscard]] float TallestTreeHeight(uint32_t forestId);

/// The forests in the order of the original's list (g_game +0x205BB4, head insertion in the ctor 0x539BD0): the newest
/// first (Tiger/Wolf::CalculeLairPos walk it)
[[nodiscard]] std::vector<uint32_t> ForestsNewestFirst();

/// Forest +0x14: its centre (the position it was created at)
[[nodiscard]] glm::vec3 ForestCentre(uint32_t forestId);

/// Forest +0x4C + +0x54: how many trees it has, grown and growing (fn_0053AD00 counts both)
[[nodiscard]] size_t ForestTreeCount(uint32_t forestId);

/// Forest +0x48: its full grown trees, nearest its centre first (Forest::AddTree 0x53A310 keeps the list sorted by
/// SortTreesOnDistanceFromForest::DistanceToForest 0x53A890; equal distances keep the order they joined in)
[[nodiscard]] std::vector<entt::entity> GrownTreesByDistance(uint32_t forestId);

/// On map load: the forests go with the map.
void ClearForests();

} // namespace openblack::ecs
