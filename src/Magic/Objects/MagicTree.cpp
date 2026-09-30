/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MagicTree.h"

#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Tree.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Physics/PhysicsObjects.h"
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
	ecs::effects::reactions::CreateReaction(tree, Reaction::ReactToMagicTree, player, false);
	return tree;
}

void magic_tree::ToBeDeleted(entt::entity tree)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(tree))
	{
		return;
	}
	// MagicTree::ToBeDeleted 0x5FD070: its reactions go, then Tree::ToBeDeleted. (The forest going with its last tree is
	// SpellForest's side: ECS/Trees has no forest deletion, see Magic/Spells/SpellForest.cpp.)
	if (registry.AllOf<MagicTree>(tree))
	{
		ecs::effects::reactions::RemoveAllReactionsInitiatedByObject(tree);
	}
	// Tree::ToBeDeleted 0x74A210: out of its forest (fn_0053A220), then Object::ToBeDeleted 0x636670 (inf: it also
	// leaves the hand and the physics; the global tree list g_game +0x205CDC is the registry here)
	ecs::SetTreeForest(tree, 0);
	if (Locator::handSystem::has_value())
	{
		const auto held = Locator::handSystem::value().GetHeldObject();
		if (held.has_value() && *held == tree)
		{
			Locator::handSystem::value().ForceDropHeld();
		}
	}
	ecs::physics::PhysicsObjects::RemoveObject(tree);
	registry.Destroy(tree);
	registry.SetDirty();
}

void magic_tree::Forget(entt::entity tree, bool gone)
{
	// what MagicTree::ToBeDeleted 0x5FD070 does for a tree another system took out of the world (burnt, put in a
	// store, a dead tree now) or out of its forest: gone, all its reactions; a tree that stays (a DeadTree keeps the
	// entity) loses only REACT_TO_MAGIC_TREE and its MagicTree part (inf)
	auto& registry = Locator::entitiesRegistry::value();
	if (gone || !registry.Valid(tree))
	{
		ecs::effects::reactions::RemoveAllReactionsInitiatedByObject(tree);
		return;
	}
	ecs::effects::reactions::RemoveAllReactionsOfTypeInitiatedBy(tree, Reaction::ReactToMagicTree);
	registry.Remove<MagicTree>(tree);
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
