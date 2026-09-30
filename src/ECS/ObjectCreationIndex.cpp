/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectCreationIndex.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "ECS/Registry.h"
#include "Locator.h"

namespace openblack::ecs::object_index
{
namespace
{
uint32_t g_Counter = 0;
bool g_Loaded = false;

struct TownSpells
{
	std::vector<std::string> seeds;
	bool centre {false};
	uint32_t icons {0};
};
std::unordered_map<uint32_t, TownSpells> g_Towns;
constexpr uint32_t k_MaxIcons = 6;
} // namespace

void OnLoadMap()
{
	g_Counter = g_Loaded ? 0 : 2;
	g_Loaded = true;
	g_Towns.clear();
}

void Assign(entt::entity entity)
{
	Locator::entitiesRegistry::value().AssignOrReplace<components::ObjectCreationIndex>(entity, g_Counter++);
}

void Skip(uint32_t count)
{
	g_Counter += count;
}

int64_t Of(entt::entity entity)
{
	const auto* index = Locator::entitiesRegistry::value().TryGet<const components::ObjectCreationIndex>(entity);
	return index != nullptr ? static_cast<int64_t>(index->value) : -1;
}

void AddTownSpell(uint32_t town, const std::string& spell)
{
	auto& spells = g_Towns[town];
	if (std::ranges::find(spells.seeds, spell) != spells.seeds.end())
	{
		return;
	}
	spells.seeds.push_back(spell);
	if (spells.centre && spells.icons < k_MaxIcons)
	{
		++spells.icons;
		Skip(1);
	}
}

void OnTownCentre(uint32_t town)
{
	auto& spells = g_Towns[town];
	spells.centre = true;
	const auto icons = std::min<uint32_t>(static_cast<uint32_t>(spells.seeds.size()), k_MaxIcons);
	spells.icons = icons;
	Skip(icons);
}

} // namespace openblack::ecs::object_index
