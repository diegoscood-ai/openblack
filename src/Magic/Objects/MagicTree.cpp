/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MagicTree.h"

#include <algorithm>
#include <vector>

#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Tree.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Trees.h"
#include "Locator.h"
#include "Magic/Script/ScriptPlayer.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

entt::entity magic_tree::Create(const glm::vec3& position, entt::entity spell, TreeInfo type, uint32_t forestId,
                                float angle, float scale, float woodValueMultiplier)
{
	auto& registry = Locator::entitiesRegistry::value();
	const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	// Tree(pos, info, forest, 1.0, angle, scale) 0x749E00: in the forest (Forest::AddTree), growing since maxScale 1.0 is
	// not the scale, with its random growth counter (ECS/Trees: TreeArchetype::Create)
	const auto tree = ecs::archetypes::TreeArchetype::Create(forestId, glm::vec3(position.x, ground, position.z), type, true,
	                                                         angle, 1.0f, scale);
	if (tree == entt::null)
	{
		return entt::null;
	}
	// 0x5FCF91..0x5FCFC8: +0x6C = GetPlayerNumber 0x64A790 of the spell's GetPlayer (vt 0x1C, +0xA4); no spell: the
	// player at g_game +0x205A5B, the neutral one (Magic/Script/ScriptPlayer.h). A spell with a NULL +0xA4 would make the
	// original read +0xB5 of NULL; here it stays neutral (inferido: no caller does that)
	PlayerNames player = k_NeutralPlayerSlot;
	if (spell != entt::null && registry.Valid(spell) && registry.Get<const Spell>(spell).hasPlayer)
	{
		player = registry.Get<const Spell>(spell).player;
	}
	registry.Assign<MagicTree>(tree, player, woodValueMultiplier);
	// +0x70: MagicTree::GetWoodValueMultiplier 0x5FD0C0 (Tree::GetWoodValue 0x74B7B0 reads it through vt 0x868; ECS/Trees
	// keeps it in Tree::woodValueMultiplier, 1 for a plain tree)
	registry.Get<Tree>(tree).woodValueMultiplier = woodValueMultiplier;
	ecs::effects::reactions::CreateReaction(tree, Reaction::ReactToMagicTree, player, false);
	return tree;
}



void magic_tree::StartOnFire(entt::entity tree)
{
	ecs::effects::reactions::RemoveAllReactionsOfTypeInitiatedBy(tree, Reaction::ReactToMagicTree);
}

void magic_tree::EndOnFire(entt::entity tree)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(tree) || !registry.AllOf<MagicTree>(tree))
	{
		return;
	}
	ecs::effects::reactions::CreateReaction(tree, Reaction::ReactToMagicTree, registry.Get<const MagicTree>(tree).player,
	                                        false);
}

float magic_tree::WoodValueMultiplier(entt::entity tree)
{
	auto& registry = Locator::entitiesRegistry::value();
	// a dead tree made from it (the same entity here) has its own GetWoodValue (DeadTree 0x511AD0)
	if (const auto* magic = registry.TryGet<const MagicTree>(tree); magic != nullptr && registry.AllOf<Tree>(tree))
	{
		return magic->woodValueMultiplier;
	}
	return 1.0f;
}

void magic_tree::ToBeDeleted(entt::entity tree)
{
	// MagicTree::ToBeDeleted 0x5FD070 -> Tree::ToBeDeleted 0x74A210: ECS/Trees' DeleteTree (out of its forest and the
	// physics, gone); its reactions and the rest go in the tree-deleted listener below
	ecs::DeleteTree(tree);
}

namespace
{
/// the forests that lost a magic tree since the spells last looked (MagicTree::ToBeDeleted's "the forest goes with its
/// last tree", done by SpellForest::Process; ECS/Trees' listener runs while the tree is still in its forest)
std::vector<uint32_t> g_ForestsThatLostAMagicTree;

/// The rest of Tree::ToBeDeleted 0x74A210 / Object::ToBeDeleted 0x636670 (MagicTree::ToBeDeleted 0x5FD070 for a magic
/// tree) for a tree ECS/Trees takes away (DeleteTree, ShrinkAllTrees, FellTree, the hand's MakeDeadTree): its reactions
/// go (RemoveAllReactionsInitiatedByObject 0x6E4750; a MagicTree's REACT_TO_MAGIC_TREE with them) and a magic tree's
/// forest is noted ("the forest goes with its last tree", SpellForest). Only when the entity is destroyed (Removed):
/// its fire goes (FireEffect::ToBeDeleted 0x72EBE0) and the hand lets go of it (inf). BecameDeadTree: the entity stays
/// as the DeadTree that took over the tree's 3D object and fire (ctor 0x510880, fn_00730960), which is not a MagicTree.
void OnTreeDeleted(entt::entity tree, ecs::TreeDeletion how)
{
	auto& registry = Locator::entitiesRegistry::value();
	const bool magic = registry.AllOf<MagicTree>(tree);
	if (const auto* component = registry.TryGet<const Tree>(tree); component != nullptr && component->forestId != 0 && magic)
	{
		g_ForestsThatLostAMagicTree.push_back(component->forestId);
	}
	ecs::effects::reactions::RemoveAllReactionsInitiatedByObject(tree);
	if (how == ecs::TreeDeletion::BecameDeadTree)
	{
		if (magic)
		{
			registry.Remove<MagicTree>(tree); // the DeadTree is another object (inf: the same entity in openblack)
		}
		return;
	}
	if (auto* fire = ecs::fire::Find(tree); fire != nullptr)
	{
		ecs::fire::ToBeDeleted(*fire);
	}
	if (Locator::handSystem::has_value())
	{
		const auto held = Locator::handSystem::value().GetHeldObject();
		if (held.has_value() && *held == tree)
		{
			Locator::handSystem::value().ForceDropHeld();
		}
	}
}

const bool k_TreeListenerRegistered = [] {
	ecs::AddTreeDeletedListener(&OnTreeDeleted);
	return true;
}();
} // namespace

bool magic_tree::ForestLostAMagicTree(uint32_t forestId)
{
	const auto it = std::find(g_ForestsThatLostAMagicTree.begin(), g_ForestsThatLostAMagicTree.end(), forestId);
	if (it == g_ForestsThatLostAMagicTree.end())
	{
		return false;
	}
	std::erase(g_ForestsThatLostAMagicTree, forestId);
	return true;
}

void magic_tree::Clear()
{
	g_ForestsThatLostAMagicTree.clear();
}
