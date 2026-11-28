/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ForestSystem.h"

#include <algorithm>
#include <map>

#include "ECS/Components/Forest.h"
#include "ECS/Components/ForestMember.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Trees.h"
#include "GameClock.h"
#include "Locator.h"
#include "Magic/Spells/SpellForest.h"

using namespace openblack;
using namespace openblack::ecs::systems;

uint32_t ForestSystem::Create(uint32_t id, glm::vec3 centre)
{
	// The forest's own id: one made without an id takes the counter's and the counter goes up by one; one made with an
	// id raises the counter past it only when the counter is below it. Two forests may so share an id (a script's
	// forest of the counter's id, then one made without an id), and each is kept under its own key
	if (id == 0)
	{
		id = _nextId++;
	}
	else if (_nextId < id)
	{
		_nextId = id + 1;
	}
	const auto key = _nextKey++;
	auto& forest = _forests.emplace(key, ForestData {.centre = centre, .created = ++_created, .id = id}).first->second;
	// its own entity, which a creature can believe in, standing at its centre as a town's does; it takes no creation
	// number
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		forest.entity = registry.Create();
		registry.Assign<ecs::components::Forest>(forest.entity, ecs::components::Forest {.key = key});
		registry.Assign<ecs::components::Transform>(forest.entity, centre, glm::mat3(1.0f), glm::vec3(1.0f));
	}
	return key;
}

namespace
{
void DestroyForestEntity(const openblack::ecs::ForestData& forest)
{
	if (forest.entity == entt::null || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(forest.entity))
	{
		registry.Destroy(forest.entity);
	}
}
} // namespace

void ForestSystem::Remove(uint32_t forestKey)
{
	if (const auto found = _forests.find(forestKey); found != _forests.end())
	{
		DestroyForestEntity(found->second);
		_forests.erase(found);
	}
}

ForestSystem::Forests& ForestSystem::All()
{
	return _forests;
}

ForestSystem::TownForestLists& ForestSystem::TownLists()
{
	return _townLists;
}

uint32_t ForestSystem::LastTreeCreatedTurn() const
{
	return _lastTreeCreatedTurn;
}

void ForestSystem::SetLastTreeCreatedTurn(uint32_t turn)
{
	_lastTreeCreatedTurn = turn;
}

uint32_t ForestSystem::NextId() const
{
	return _nextId;
}

void ForestSystem::SetNextId(uint32_t id)
{
	_nextId = id;
}

void ForestSystem::NoteForestLostAMagicTree(uint32_t forestId)
{
	_forestsThatLostAMagicTree.push_back(forestId);
}

bool ForestSystem::TakeForestLostAMagicTree(uint32_t forestId)
{
	if (std::ranges::find(_forestsThatLostAMagicTree, forestId) == _forestsThatLostAMagicTree.end())
	{
		return false;
	}
	std::erase(_forestsThatLostAMagicTree, forestId);
	return true;
}

void ForestSystem::ClearForestsThatLostAMagicTree()
{
	_forestsThatLostAMagicTree.clear();
}

uint32_t ForestSystem::Plant(entt::entity spell, components::Spell& miracle, float tribalPower)
{
	return magic::spell_forest::Plant(spell, miracle, tribalPower);
}

bool ForestSystem::CanGrowAt(glm::vec3 point) const
{
	return magic::spell_forest::CanCastAt(point);
}

bool ForestSystem::HasTrees(entt::entity spell) const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* data = registry.Valid(spell) ? registry.TryGet<const magic::SpellForestData>(spell) : nullptr;
	// The spell's forest, while it stands: made, not gone with its last tree, still kept
	if (data == nullptr || data->forestId == 0 || data->forestDeleted || !_forests.contains(data->forestId))
	{
		return false;
	}
	bool found = false;
	registry.Each<const components::Tree>(
	    [&found, data](const components::Tree& tree) { found = found || tree.forestId == data->forestId; });
	return found;
}

std::optional<entt::entity> ForestSystem::AddTreeNear(entt::entity tree)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.Valid(tree) ? registry.TryGet<const components::Tree>(tree) : nullptr;
	if (component == nullptr || component->forestId == 0 || !_forests.contains(component->forestId) ||
	    !(game_clock::Turn() - _lastTreeCreatedTurn > k_TreeAddingGapTurns))
	{
		return std::nullopt;
	}
	const auto added = ecs::PlantTreeNear(component->forestId, tree);
	return added != entt::null ? std::optional<entt::entity>(added) : std::nullopt;
}

std::optional<uint32_t> ForestSystem::ForestOf(entt::entity tree) const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.Valid(tree) ? registry.TryGet<const components::Tree>(tree) : nullptr;
	if (component == nullptr || component->forestId == 0 || !_forests.contains(component->forestId))
	{
		return std::nullopt;
	}
	return component->forestId;
}

std::optional<uint32_t> ForestSystem::FindClosestForest(glm::vec3 pos, float maxDistance) const
{
	std::map<uint32_t, uint32_t> trees; // forest key -> its trees
	if (Locator::entitiesRegistry::has_value())
	{
		Locator::entitiesRegistry::value().Each<const components::Tree>([&trees](const components::Tree& tree) {
			if (tree.forestId != 0)
			{
				++trees[tree.forestId];
			}
		});
	}
	std::vector<forest_queries::ForestCandidate> candidates;
	candidates.reserve(_forests.size());
	for (const auto& [key, forest] : _forests)
	{
		const auto found = trees.find(key);
		candidates.push_back({.key = key,
		                      .created = forest.created,
		                      .centre = forest.centre,
		                      .trees = found != trees.end() ? found->second : 0});
	}
	return forest_queries::ClosestForestWithTrees(candidates, pos, maxDistance);
}

std::vector<ecs::forest_queries::ForestTree> ForestSystem::GatherTrees(uint32_t forestKey) const
{
	std::vector<forest_queries::ForestTree> trees;
	const auto forest = _forests.find(forestKey);
	if (forestKey == 0 || forest == _forests.end() || !Locator::entitiesRegistry::has_value())
	{
		return trees;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto centre = forest->second.centre;
	registry.Each<const components::Tree, const components::Transform>(
	    [&](entt::entity entity, const components::Tree& tree, const components::Transform& transform) {
		    if (tree.forestId != forestKey)
		    {
			    return;
		    }
		    // The list it joined and when (ECS/Trees.h JoinForestList); a tree that never joined one counts as joining
		    // the growing list while it grows below its full size, in the order the trees were made
		    const auto* member = registry.TryGet<const components::ForestMember>(entity);
		    const auto* index = registry.TryGet<const components::ObjectCreationIndex>(entity);
		    trees.push_back({
		        .entity = entity,
		        .growing = member != nullptr ? member->growing : tree.growing && transform.scale.x < tree.maxSize,
		        .made = member != nullptr  ? member->listed
		                : index != nullptr ? index->value
		                                   : 0,
		        .distance = game_math::GetDistanceInMetres(transform.position, centre),
		    });
	    });
	return trees;
}

ecs::forest_queries::TreeCounts ForestSystem::CountTrees(uint32_t forestKey) const
{
	return forest_queries::CountTrees(GatherTrees(forestKey));
}

std::vector<entt::entity> ForestSystem::Trees(uint32_t forestKey) const
{
	return forest_queries::OrderedTrees(GatherTrees(forestKey));
}

void ForestSystem::Reset()
{
	_townLists.clear();
	for (const auto& [key, forest] : _forests)
	{
		DestroyForestEntity(forest);
	}
	_forests.clear();
	_nextId = 1;
	_nextKey = 1;
	_lastTreeCreatedTurn = 0;
}
