/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SavedScriptObjects.h"

#include <algorithm>

#include <LHVM.h>

#include "ECS/Components/ScriptHeld.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "Locator.h"

namespace openblack::ecs::saved_script_objects
{

std::optional<std::vector<uint32_t>> PlacesOf(std::span<const uint32_t> named, std::span<const uint32_t> held)
{
	std::vector<uint32_t> places;
	places.reserve(named.size() + held.size());
	places.insert(places.end(), named.begin(), named.end());
	places.insert(places.end(), held.begin(), held.end());
	std::ranges::sort(places);
	const auto [first, last] = std::ranges::unique(places);
	places.erase(first, last);
	// the null takes no place
	std::erase(places, 0U);
	if (places.size() > k_Places)
	{
		return std::nullopt;
	}
	return places;
}

uint32_t PlaceOf(std::span<const uint32_t> places, uint32_t value)
{
	if (value == 0)
	{
		return 0;
	}
	const auto found = std::ranges::lower_bound(places, value);
	return found != places.end() && *found == value ? static_cast<uint32_t>(found - places.begin()) + 1 : 0;
}

uint32_t ValueOf(std::span<const uint32_t> loaded, uint32_t place)
{
	return place == 0 || place > loaded.size() ? 0 : loaded[place - 1];
}

bool HasMachine()
{
	return Locator::vm::has_value();
}

std::optional<std::vector<uint32_t>> Current()
{
	std::vector<uint32_t> named;
	if (Locator::vm::has_value())
	{
		named = Locator::vm::value().ObjectValues();
	}
	std::vector<uint32_t> held;
	if (Locator::entitiesRegistry::has_value())
	{
		Locator::entitiesRegistry::value().Each<const components::ScriptHeld>(
		    [&held](entt::entity entity, const components::ScriptHeld& slot) {
			    if (slot.hasSlot)
			    {
				    held.push_back(static_cast<uint32_t>(entity));
			    }
		    });
	}
	return PlacesOf(named, held);
}

std::vector<uint32_t> PutBack(std::span<const LoadedPlace> places)
{
	std::vector<uint32_t> values;
	values.reserve(places.size());
	const bool hasRegistry = Locator::entitiesRegistry::has_value();
	for (const auto& place : places)
	{
		if (place.object == entt::null || !hasRegistry || !Locator::entitiesRegistry::value().Valid(place.object))
		{
			values.push_back(0);
			continue;
		}
		script_held::AddScriptThing(place.object, place.createdByScript);
		for (uint8_t i = 0; i < place.references; ++i)
		{
			script_held::IncrementReference(place.object);
		}
		values.push_back(static_cast<uint32_t>(place.object));
	}
	return values;
}

} // namespace openblack::ecs::saved_script_objects
