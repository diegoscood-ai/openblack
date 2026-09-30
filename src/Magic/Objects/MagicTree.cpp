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
#include "ECS/Forests.h"
#include "ECS/Registry.h"
#include "ECS/TreeGrowth.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

entt::entity magic_tree::Create(const glm::vec3& position, entt::entity spell, TreeInfo type, entt::entity forest,
                                float angle, float scale, float woodValueMultiplier)
{
	auto& registry = Locator::entitiesRegistry::value();
	const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	// Tree(pos, info, forest, 1.0, angle, scale): the growing flag and countdown (TreeArchetype -> trees::InitGrowth),
	// then Forest::AddTree
	const auto tree =
	    ecs::archetypes::TreeArchetype::Create(0, glm::vec3(position.x, ground, position.z), type, true, angle, 1.0f, scale);
	if (tree == entt::null)
	{
		return entt::null;
	}
	ecs::forests::AddTree(forest, tree);
	// the spell's GetPlayer (vt 0x1C, +0xA4); none: the player at g_game +0x205A5B
	PlayerNames player = PlayerNames::NEUTRAL;
	if (spell != entt::null && registry.Valid(spell))
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
	const auto forest = registry.AllOf<Tree>(tree) ? registry.Get<Tree>(tree).forest : entt::null;
	ecs::effects::reactions::RemoveAllReactionsInitiatedByObject(tree);
	ecs::trees::BaseToBeDeleted(tree);
	// the forest is still available and has no tree left (+0x4C + +0x54 == 0): it goes too
	if (forest != entt::null && ecs::forests::Exists(forest) && ecs::forests::TreeCount(forest) == 0)
	{
		ecs::forests::ToBeDeleted(forest);
	}
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
