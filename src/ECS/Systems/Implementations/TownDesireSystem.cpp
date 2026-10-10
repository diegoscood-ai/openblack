/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TownDesireSystem.h"

#include <cstddef>

#include "ECS/Town/TownDesire.h"

using namespace openblack;
using namespace openblack::ecs::systems;
namespace town_desire = openblack::ecs::town_desire;

uint32_t TownDesireSystem::OfferVillager(entt::entity town, bool child, float trigger,
                                         const std::function<uint32_t(TownDesireInfo)>& satisfy)
{
	return town_desire::OfferVillager(town, child, trigger,
	                                  [&satisfy](size_t d) { return satisfy(static_cast<TownDesireInfo>(d)); });
}

float TownDesireSystem::GetDesire(entt::entity town, TownDesireInfo desire) const
{
	return town_desire::GetDesire(town, desire);
}

float TownDesireSystem::GetRawDesire(entt::entity town, TownDesireInfo desire) const
{
	return town_desire::GetRawDesire(town, desire);
}

TownDesireInfo TownDesireSystem::GetMostWanted(entt::entity town) const
{
	return town_desire::GetMostWanted(town);
}

void TownDesireSystem::SetBoost(entt::entity town, TownDesireInfo desire, float boost, bool resort)
{
	town_desire::SetBoost(town, desire, boost, resort);
}
