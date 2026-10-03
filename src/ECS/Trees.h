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
#include <functional>
#include <optional>
#include <vector>

#include <entt/fwd.hpp>

#include "Enums.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs
{

/// Tree::Process 0x74A290, once per game turn: a tree that belongs to a forest and has not reached its maximum size
/// grows every growTurns turns. Only forests process their trees in the original (Forest::Process 0x539DA0), so the
/// script's forest-less trees (every tree of Land 1 and Land 2) never grow.
void ProcessTreesTurn(uint32_t turn);

/// How much a tree grows on its grow turn (the argument of Tree::Grow 0x74A3F0, built in 0x74A2D7..0x74A33E):
/// growthAmount (info +0x11C) x (1 + 0.01 [0x8C5840] x rainMultiplier (info +0x130) x GClimate::GetMaxRainingOrSnowing
/// 0x771600) x (1 + 0.5 [0x8AA3B4] x MapCoords::GetAlignment 0x6057B0), rain and alignment both taken at the tree. Rain
/// is the 0..127 byte and the land alignment is -1..1, so the worst land halves the growth and the best adds half.
[[nodiscard]] float TreeGrowthAmount(float growthAmount, float rainMultiplier, float rain, float landAlignment);

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

/// How a tree goes: Removed, the entity is destroyed (DeleteTree); BecameDeadTree, the Tree object is deleted but the
/// entity stays as the DeadTree that took over its 3D object, fire included (FellTree: FelledTree::Create then
/// ForesterChopsTree's ToBeDeleted; a tree that falls: Tree::EndPhysics's DeadTree, then ToBeDeleted). The DeadTree ctor
/// 0x510880 hands the fire over (fn_00730960), so a listener keeps it for BecameDeadTree.
enum class TreeDeletion
{
	Removed,
	BecameDeadTree,
};

/// Called with each tree (or dead tree) just before it goes: fire, reactions, the hand and the like clean up after it
/// (the rest of Object::ToBeDeleted).
using TreeDeletedListener = std::function<void(entt::entity, TreeDeletion)>;

/// Tells the listeners (for code that turns a Tree into a DeadTree itself, like the hand's MakeDeadTree)
void NotifyTreeDeleted(entt::entity tree, TreeDeletion how);
void AddTreeDeletedListener(TreeDeletedListener listener);

/// Tree::ToBeDeleted 0x74A210 / DeadTree::ToBeDeleted 0x510C90: out of its forest (Forest::RemoveTree), out of the
/// physics, the listeners told, and gone.
void DeleteTree(entt::entity tree);

/// Forest::ToBeDeleted 0x539C60: every tree of both its lists deleted (Tree::ToBeDeleted), then the forest itself
/// leaves the forest list.
void DeleteForest(uint32_t forestId);

/// Tree::GetWoodValue 0x74B7B0 = life x GetWoodValueMultiplier (1) x woodValue x scale x GLandBalance::Values[5];
/// DeadTree::GetWoodValue 0x511AD0 = life x woodValue x scale^3 (the original cubes the scale there).
[[nodiscard]] float TreeWoodValue(entt::entity tree);

/// GetDefaultResource(WOOD): Tree 0x74B7A0 = (int)GetWoodValue; DeadTree 0x511330 = (int)(woodValue x its wood
/// multiplier (1, copied from the tree) x scale): no life, no land balance. What a store gets for it.
[[nodiscard]] uint32_t TreeWood(entt::entity tree);

/// DeadTree::RemoveResource 0x511370: n wood taken from a dead tree; with no more than n left it is deleted and gives
/// what it had, otherwise it SHRINKS: SetScale((wood - n) / (woodValue x multiplier)). Returns what was taken.
uint32_t RemoveWood(entt::entity deadTree, uint32_t amount);

/// GetCarriedTreeType (Tree 0x55D900: info carriedType; DeadTree 0x511A20: 0-3 when its mesh is one of the four
/// CarriedObject::Init 0x462600 logs, MeshPack 406 / 347 / 348 / 349, else the tree info's carriedType): the log a
/// villager carries it as.
[[nodiscard]] CarriedTreeType TreeCarriedType(entt::entity tree);

/// FelledTree::Create 0x5116A0 (called only by Villager::ForesterChopsTree 0x75FAC0): the tree becomes a felled
/// DeadTree with its mesh, thrown into the physics by the forester: velocity 0.2 x its height along the direction from
/// the forester to the tree, spin 0.4 rad/s about the tree's own axis (cos a, 0, sin a) (body space: the fall depends on
/// its yaw),
/// adjusted to the ground. Returns the felled tree (the same entity) or entt::null.
entt::entity FellTree(entt::entity tree, entt::entity chopper);

/// Get2DRadius vt +0x64 (Object 0x638180: GetScale x max(LH3DMesh +0x24, +0x2C), the half extents x and z that
/// LH3DMesh::ComputeBoundingBox 0x8081B0 stores): ecs::object::Get2DRadius, with the class overrides
[[nodiscard]] float Object2DRadius(entt::entity object);

/// Tree::GetWorkingPos 0x74C040: where `who` stands to work on the tree: the tree's position plus, towards `who`,
/// who->Get2DRadius() + 0.9 (0x8C5844)
[[nodiscard]] glm::vec3 TreeWorkingPos(entt::entity tree, entt::entity who);

/// Villager::FindTreeNearVillager 0x75FD00: the 9 cells around `who` in the order of GUtils::Spiral (0x74D7E0, table
/// 0xDA59FC, dir 1 steps 1: (0,0) (-1,0) (-1,-1) (0,-1) (1,-1) (1,0) (1,1) (0,1) (-1,1)), in each only the FIRST tree of the cell (FindType(6) 0x6045C0) that
/// is not INDESTRUCTIBLE (Object +0x24 bit 0x4000; set only by puzzle objects: no tree has it in a normal game); the
/// nearest by Dist2D(who, its working position), from 99999 (0x47C34F80). No other rule (no distance limit, no scenic
/// bit, no size, no forest). entt::null when none; the caller tells "touching" (10) from "found" (1) with IsTouching.
[[nodiscard]] entt::entity FindTreeNearVillager(entt::entity who);

/// Forest +0x38 and +0x3C: the BigForest it belongs to (entt::null if none) and whether it is its town's scenic forest
[[nodiscard]] entt::entity ForestBigForest(uint32_t forestId);
void SetForestBigForest(uint32_t forestId, entt::entity bigForest);
[[nodiscard]] bool IsScenicForest(uint32_t forestId);

/// fn_0053B280: the forest's wood: its BigForest's GetWoodValue (life x wood) plus every tree's GetWoodValue
[[nodiscard]] float ForestWood(uint32_t forestId);

/// Forest::GetForestCentreTree 0x53ABF0: of the heads of its two lists (grown +0x48, growing +0x50, each sorted by
/// distance to the centre) the one nearer the centre; the grown one on a tie. entt::null when it has none.
[[nodiscard]] entt::entity ForestCentreTree(uint32_t forestId);

/// fn_0053A1A0(pos, max, onlyEmpty): over the forest list, the forest whose CENTRE is nearest `at` within `max` (max
/// shrinks to the best so far); onlyEmpty: no trees and no BigForest; otherwise at least one tree (BigForest and scenic
/// not looked at)
[[nodiscard]] std::optional<uint32_t> FindForest(glm::vec3 at, float max, bool onlyEmpty);

/// Town::MakeScenicForest 0x741B40: the town's scenic forest (Forest +0x3C = 1; made at `townCentre` if the town has
/// none) takes every tree within 250 + 10 m of the town centre (a spiral over the cells that stops at the first cell
/// farther than that) that has no forest, or whose forest is scenic and whose tree is nearer the town centre than that
/// forest's centre. It does not add the forest to the town list (AssignForestsToTown does).
void MakeScenicForest(uint32_t townId, glm::vec3 townCentre);

/// Town::AssignForestsToTown 0x73EB00: the town's forest list (Town +0x608) cleared and filled again with every forest
/// whose nearest point (a BigForest's nearest edge, otherwise its centre; fn_0053ADB0) is within
/// GTownInfo::maxDistanceForTownForest (250, +0x164) of `reference` (the town's storage pit, or its temporary store
/// point) and that has wood (fn_0053B280 > 0). Called by Town::AsssignTownFeature 0x73EAC0 (after MakeScenicForest, for
/// every town) and Scaffold::BuildBuilding 0x6E932C; not when trees are made or planted.
void AssignForestsToTown(uint32_t townId, glm::vec3 reference);

/// Town +0x608: the town's forests (head first: fn_00741AF0 inserts at the head)
[[nodiscard]] std::vector<uint32_t> TownForests(uint32_t townId);

/// Town::FindNearestForestToPos 0x73EC10: over the town's list, the forest nearest `at` (a BigForest's nearest edge,
/// 0 when `at` is inside its 2D radius, else its centre) within 250 (GTownInfo +0x164); a non-scenic one wins, the
/// scenic forest only when there is no other. No wood check here.
[[nodiscard]] std::optional<uint32_t> FindNearestForestToPos(uint32_t townId, glm::vec3 at);

/// BigForest::GetArrivePos 0x439360: its position plus, towards `who`, 0.5 x its Get2DRadius
[[nodiscard]] glm::vec3 BigForestArrivePos(entt::entity bigForest, entt::entity who);

/// BigForest::RemoveResource 0x4390D0 (WOOD only): n / life wood taken; when it has no more than that, it gives what
/// it had (ftol of GetWoodValue) and is deleted; otherwise its wood (+0x84) goes down, and once its GetWoodValue is more
/// than 250.0 (0x8C6210) away from life x scale x woodValue it is rescaled to GetWoodValue / (life x woodValue) and a
/// sapling is planted at its edge (AddTreeAround 0x439220: up to 10 random angles at its radius, on land with no object
/// nearer than 4; a Pine (GTreeInfo 0xDA49D8) of its forest, size 0.05, maximum 0.75 + rand(0.5), random angle).
/// Returns what was taken (n, or what it had).
uint32_t BigForestRemoveWood(entt::entity bigForest, uint32_t amount);

/// On map load: the forests go with the map.
void ClearForests();

} // namespace openblack::ecs
