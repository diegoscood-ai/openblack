/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Forests.h"

#include <algorithm>
#include <limits>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "Components/Forest.h"
#include "Components/MagicTree.h"
#include "Components/Transform.h"
#include "Components/Tree.h"
#include "Effects/EffectValues.h"
#include "Effects/Reactions.h"
#include "Locator.h"
#include "Registry.h"
#include "TreeGrowth.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// g_game +0x205BB4: the forests, the newest first
std::vector<entt::entity> g_Forests;

ForestTrees* ForestOf(entt::entity forest)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (forest == entt::null || !registry.Valid(forest))
	{
		return nullptr;
	}
	return registry.TryGet<ForestTrees>(forest);
}

/// SortTreesOnDistanceFromForest::DistanceToForest 0x53A890: GetDistanceInMetres (x, z) to the tree's forest, 0 without
float DistanceToForest(entt::entity tree)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& component = registry.Get<Tree>(tree);
	const auto* forest = ForestOf(component.forest);
	if (forest == nullptr)
	{
		return 0.0f;
	}
	const auto& position = registry.Get<Transform>(tree).position;
	return glm::distance(glm::vec2(position.x, position.z), glm::vec2(forest->position.x, forest->position.z));
}

/// The LHLinkedList insert of AddTree / Forest::Process: before the first tree that is farther from the forest
void InsertSorted(std::vector<entt::entity>& list, entt::entity tree)
{
	const float distance = DistanceToForest(tree);
	auto it = list.begin();
	for (; it != list.end(); ++it)
	{
		// fld existing; fcomp new; test ah, 0x41; je: existing > new
		if (DistanceToForest(*it) > distance)
		{
			break;
		}
	}
	list.insert(it, tree);
}

bool IsGrowingBelowMax(entt::entity tree)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& component = registry.Get<Tree>(tree);
	return component.growing && trees::GetScale(tree) < component.maxSize;
}

/// The trees another system took out of the world (burnt, put in a store, a dead tree now) without Tree::ToBeDeleted:
/// dropped here as ToBeDeleted would have done (inf). Returns true when the forest was deleted with its last tree.
bool Prune(entt::entity forestEntity)
{
	auto* forest = ForestOf(forestEntity);
	if (forest == nullptr)
	{
		return true;
	}
	auto& registry = Locator::entitiesRegistry::value();
	bool removed = false;
	const auto gone = [&](entt::entity tree) {
		const bool stays = registry.Valid(tree) && registry.AllOf<Tree>(tree) && registry.Get<Tree>(tree).forest == forestEntity;
		if (!stays)
		{
			removed = true;
			// MagicTree::ToBeDeleted 0x5FD070: its reactions go (a tree turned DeadTree keeps the entity: only reaction 8)
			if (registry.Valid(tree))
			{
				effects::reactions::RemoveAllReactionsOfTypeInitiatedBy(tree, Reaction::ReactToMagicTree);
				registry.Remove<MagicTree>(tree);
			}
			else
			{
				effects::reactions::RemoveAllReactionsInitiatedByObject(tree);
			}
		}
		return !stays;
	};
	std::erase_if(forest->grown, gone);
	std::erase_if(forest->growing, gone);
	// MagicTree::ToBeDeleted: the forest goes with its last tree (only the forest miracle's forests exist: they are all
	// of magic trees)
	if (removed && forest->grown.empty() && forest->growing.empty())
	{
		forests::ToBeDeleted(forestEntity);
		return true;
	}
	return false;
}
} // namespace

entt::entity forests::Create(const glm::vec3& position, bool hasPlayer, PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	uint32_t id = 0;
	registry.Each<const Tree>([&id](entt::entity, const Tree& tree) {
		if (tree.forestId != std::numeric_limits<uint32_t>::max())
		{
			id = std::max(id, tree.forestId);
		}
	});
	registry.Each<const ForestTrees>([&id](entt::entity, const ForestTrees& forest) { id = std::max(id, forest.id); });
	const auto entity = registry.Create();
	auto& forest = registry.Assign<ForestTrees>(entity);
	forest.id = id + 1;
	forest.position = glm::vec3(position.x, 0.0f, position.z);
	forest.hasPlayer = hasPlayer;
	forest.player = player;
	g_Forests.insert(g_Forests.begin(), entity);
	return entity;
}

void forests::AddTree(entt::entity forestEntity, entt::entity tree)
{
	auto* forest = ForestOf(forestEntity);
	if (forest == nullptr)
	{
		return;
	}
	auto& component = Locator::entitiesRegistry::value().Get<Tree>(tree);
	component.forest = forestEntity; // +0x68
	component.forestId = forest->id;
	InsertSorted(IsGrowingBelowMax(tree) ? forest->growing : forest->grown, tree);
}

void forests::RemoveTree(entt::entity forestEntity, entt::entity tree)
{
	auto* forest = ForestOf(forestEntity);
	if (forest == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const bool growingFirst = registry.Valid(tree) && registry.AllOf<Tree>(tree) && IsGrowingBelowMax(tree);
	auto& first = growingFirst ? forest->growing : forest->grown;
	auto& second = growingFirst ? forest->grown : forest->growing;
	if (auto it = std::find(first.begin(), first.end(), tree); it != first.end())
	{
		first.erase(it);
		return;
	}
	if (auto it = std::find(second.begin(), second.end(), tree); it != second.end())
	{
		second.erase(it);
	}
}

uint32_t forests::TreeCount(entt::entity forestEntity)
{
	if (Prune(forestEntity))
	{
		return 0;
	}
	const auto* forest = ForestOf(forestEntity);
	return static_cast<uint32_t>(forest->grown.size() + forest->growing.size());
}

float forests::GrowTrees(entt::entity forestEntity, float amount)
{
	if (Prune(forestEntity))
	{
		return 0.0f;
	}
	const auto* forest = ForestOf(forestEntity);
	float total = 0.0f;
	// the growing list (+0x50) first, then the grown one (+0x48); Grow(amount, 0, 0)
	for (const auto& list : {forest->growing, forest->grown})
	{
		for (const auto tree : list)
		{
			const float grown = trees::Grow(tree, amount, false, false);
			if (grown > 0.0f)
			{
				total += grown;
			}
		}
	}
	return total;
}

float forests::DecayTrees(entt::entity forestEntity, float amount)
{
	if (Prune(forestEntity))
	{
		return 0.0f;
	}
	const auto* forest = ForestOf(forestEntity);
	// copies: a tree that reaches 0 is deleted and leaves the list (the original reads the next link first); the last
	// one deletes the forest too
	const auto growing = forest->growing;
	const auto grown = forest->grown;
	float total = 0.0f;
	for (const auto& list : {growing, grown})
	{
		for (const auto tree : list)
		{
			if (!Locator::entitiesRegistry::value().Valid(tree))
			{
				continue;
			}
			const float shrunk = trees::Shrink(tree, amount);
			if (shrunk > 0.0f)
			{
				total += shrunk;
			}
		}
	}
	return total;
}

float forests::TallestTree(entt::entity forestEntity)
{
	if (Prune(forestEntity))
	{
		return 0.0f;
	}
	const auto* forest = ForestOf(forestEntity);
	float tallest = 0.0f;
	for (const auto& list : {forest->growing, forest->grown})
	{
		for (const auto tree : list)
		{
			tallest = std::max(tallest, effects::ObjectHeight(tree)); // vt 0x42C
		}
	}
	return tallest;
}

bool forests::Exists(entt::entity forest)
{
	return ForestOf(forest) != nullptr;
}

void forests::ToBeDeleted(entt::entity forestEntity)
{
	auto* forest = ForestOf(forestEntity);
	if (forest == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// the grown list, then the growing one: each tree's ToBeDeleted (vt 0xC) takes it out of the list
	while (!forest->grown.empty() || !forest->growing.empty())
	{
		auto& list = !forest->grown.empty() ? forest->grown : forest->growing;
		const auto tree = list.front();
		list.erase(list.begin());
		if (registry.Valid(tree) && registry.AllOf<Tree>(tree))
		{
			registry.Get<Tree>(tree).forest = entt::null;
			trees::ToBeDeleted(tree);
		}
		forest = ForestOf(forestEntity);
		if (forest == nullptr)
		{
			return;
		}
	}
	std::erase(g_Forests, forestEntity);
	registry.Destroy(forestEntity);
}

void forests::ProcessForests()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto forestEntity : std::vector<entt::entity>(g_Forests))
	{
		if (Prune(forestEntity))
		{
			continue;
		}
		auto* forest = ForestOf(forestEntity);
		// Forest::Process 0x539DA0 (+0x38 and +0x3C, 0 from the ctor, are not known: taken as 0)
		if (forest->grown.empty() && forest->growing.empty())
		{
			if (forest->emptyCountdown == 0)
			{
				forest->emptyCountdown = 2000;
				continue;
			}
			--forest->emptyCountdown;
			if (forest->emptyCountdown < 2)
			{
				ToBeDeleted(forestEntity);
			}
			continue;
		}
		// each growing tree's Tree::Process (vt 0x5FC); 0 -> it moves to the grown list, sorted
		for (const auto tree : std::vector<entt::entity>(forest->growing))
		{
			if (!registry.Valid(tree) || !registry.AllOf<Tree>(tree) || trees::Process(tree))
			{
				continue;
			}
			std::erase(forest->growing, tree);
			InsertSorted(forest->grown, tree);
		}
		// TODO(natural growth, postponed): ++newTreeTimer against (2000 + GameFloatRand(1000)) x 0.05 x grown (at most
		// 1) x (turn - start) x 0.00333333, then a new tree next to a random grown one (fn_00539FD0 / fn_0053A010) and
		// FUN_0064da80(14, 1) for the most influential non-neutral player there
	}
}

void forests::Clear()
{
	g_Forests.clear();
}
