/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LoadWorkshops.h"

#include <cstdint>

#include <algorithm>
#include <bit>

#include "3D/MapCoords.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Registry.h"
#include "ECS/ShowNeeds.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The row of the first show needs info in the game's tables; the others follow it downwards
constexpr uint32_t k_FirstShowNeedsRow = 1855;

components::Workshop* WorkshopOf(entt::entity workshop)
{
	auto& registry = Locator::entitiesRegistry::value();
	return workshop != entt::null && registry.Valid(workshop) ? registry.TryGet<components::Workshop>(workshop) : nullptr;
}
} // namespace

void load_workshops::SetValues(entt::entity workshop, const gamesave::WorkshopRecord& record)
{
	auto* made = WorkshopOf(workshop);
	if (made == nullptr)
	{
		return;
	}
	made->countdown = static_cast<int32_t>(record.firstWord);
	made->owned = static_cast<int32_t>(record.secondWord);
	made->slots = {};
	std::copy_n(record.bytes.values.begin(), std::min(record.bytes.values.size(), made->slots.size()), made->slots.begin());
}

entt::entity load_workshops::MakeSign(const gamesave::ShowNeedsVisualsRecord& record, entt::entity workshop)
{
	const auto info = record.object.info;
	if (info > k_FirstShowNeedsRow)
	{
		return entt::null;
	}
	const auto& at = record.position.position;
	const auto sign =
	    show_needs::CreateLoaded(map_coords::ToWorld(map_coords::MapCoords {.x = at.x, .z = at.z, .altitude = at.altitude}),
	                             workshop, static_cast<uint8_t>(k_FirstShowNeedsRow - info), record.object.creationIndex);
	if (sign == entt::null)
	{
		return sign;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<components::ShowNeedsVisuals>(sign).desire = std::bit_cast<float>(record.lastWord);
	registry.AssignOrReplace<components::LoadedRecord<gamesave::ShowNeedsVisualsRecord>>(sign, record);
	return sign;
}

void load_workshops::SetPileAndSign(entt::entity workshop, const gamesave::WorkshopRecord& record,
                                    const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	if (auto* made = WorkshopOf(workshop); made != nullptr)
	{
		made->woodPile = record.first.number != 0 ? objectOf(record.first.number) : entt::entity {entt::null};
		made->showNeeds = record.second.number != 0 ? objectOf(record.second.number) : entt::entity {entt::null};
	}
}

void load_workshops::SetScaffolds(entt::entity workshop, const gamesave::WorkshopRecord& record,
                                  const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	auto* made = WorkshopOf(workshop);
	if (made == nullptr)
	{
		return;
	}
	made->scaffolds.clear();
	for (const auto number : record.objects.numbers)
	{
		if (const auto scaffold = number != 0 ? objectOf(number) : entt::entity {entt::null}; scaffold != entt::null)
		{
			made->scaffolds.push_back(scaffold);
		}
	}
}
