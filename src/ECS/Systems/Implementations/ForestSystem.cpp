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

#include "ECS/Components/Spell.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "ECS/Trees.h"
#include "GameClock.h"
#include "Locator.h"
#include "Magic/Spells/SpellForest.h"

using namespace openblack;
using namespace openblack::ecs::systems;

uint32_t ForestSystem::Create(uint32_t id, glm::vec3 centre)
{
	if (id == 0)
	{
		while (_forests.contains(_nextId))
		{
			++_nextId;
		}
		id = _nextId++;
	}
	else
	{
		_nextId = std::max(_nextId, id + 1);
	}
	_forests.insert_or_assign(id, ForestData {centre, 0, 0, ++_created});
	return id;
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

void ForestSystem::Reset()
{
	_townLists.clear();
	_forests.clear();
	_nextId = 1;
	_lastTreeCreatedTurn = 0;
}
