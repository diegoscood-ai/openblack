/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerSpeed.h"

#include <algorithm>
#include <array>

#include <entt/entity/entity.hpp>

#include "Common/RandomNumberManager.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs
{
using namespace components;

namespace
{
/// FloatRand (0x6DE530): [0, x)
float FloatRand(float x)
{
	return x > 0.0f ? Locator::rng::value().NextValue(0.0f, x) : 0.0f;
}

uint32_t Raw(SpeedState state)
{
	return static_cast<uint32_t>(state);
}

uint32_t SpeedGroupEntry(const SpeedGroup& group, uint32_t index)
{
	const std::array<SpeedState, 6> entries = {group.speedDefault, group.speedFleeing, group.speed2,
	                                           group.speed3,       group.speed4,       group.speed5};
	return Raw(entries.at(std::min<uint32_t>(index, 5)));
}
} // namespace

const GVillagerInfo* VillagerInfoOf(entt::entity entity)
{
	const auto* villager = Locator::entitiesRegistry::value().TryGet<const Villager>(entity);
	if (villager == nullptr)
	{
		return nullptr;
	}
	for (const auto& info : Locator::infoConstants::value().villager)
	{
		if (info.tribeType == villager->tribe && info.villagerNumber == villager->number)
		{
			return &info;
		}
	}
	return nullptr;
}

void SetVillagerStateSpeed(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* villager = registry.TryGet<const Villager>(entity);
	const auto* action = registry.TryGet<const LivingAction>(entity);
	auto* wallHug = registry.TryGet<WallHug>(entity);
	const auto* info = VillagerInfoOf(entity);
	if (villager == nullptr || action == nullptr || wallHug == nullptr || info == nullptr)
	{
		return;
	}
	const auto& states = Locator::infoConstants::value().villagerStateTable;
	// GetFinalState: the current state if it is a final one, else the destination
	const auto top = action->states[static_cast<size_t>(LivingAction::Index::Top)];
	const auto destination = action->states[static_cast<size_t>(LivingAction::Index::Final)];
	auto final = top < states.size() && states[top].isFinalState != 0 ? top : destination;
	if (final >= states.size())
	{
		final = top;
	}
	if (final >= states.size())
	{
		return;
	}
	// m: the land balance speed scale (1 unless the land script changes it) * the player's wonder bonus (1) * the town's
	// belief term (openblack has no belief in the player yet: 1)
	const float m = 1.0f;
	const float life = static_cast<float>(villager->health) / 100.0f;
	const auto& group = info->speedGroup;
	float speed = 0.0f;
	if (life <= info->lifeWhenCrawlsWounded)
	{
		speed = (FloatRand(0.2f) + 0.4f) * static_cast<float>(Raw(group.speed4)) * m;
	}
	else if (life <= info->lifeWhenWalksWounded)
	{
		speed = (FloatRand(0.25f) + 0.5f) * static_cast<float>(Raw(group.speedDefault)) * m;
	}
	else
	{
		// town needs: base + clamp(sum of the town's desires / divisor, 0, 0.5) (openblack's towns have no desires yet);
		// the loads of wood and food (villagers carry none yet)
		const float townNeeds = villager->town != entt::null ? info->baseForTownNeedsSpeedMod : 1.0f;
		const float wood = std::clamp(1.0f + info->speedModWhenFullLoadOfWood - 0.0f, 0.75f, 1.0f);
		const float food = std::clamp(1.0f + info->speedModWhenFullLoadOfFood - 0.0f, 0.75f, 1.0f);
		speed = static_cast<float>(SpeedGroupEntry(group, states[final].field0x24)) * food * wood * townNeeds * m;
	}
	// Villager::SetSpeed: the factor of the villager (its creation index), age, and for adults food, life and sex
	const auto index = static_cast<int32_t>(entt::to_entity(static_cast<entt::entity>(entity)));
	float f = static_cast<float>((index * 47) % 31 - 16) * 0.01f + 1.0f;
	const auto age = static_cast<int32_t>(villager->age);
	const auto grownUp = static_cast<int32_t>(info->grownUpAge);
	const auto old = static_cast<int32_t>(info->oldAge);
	if (age < grownUp)
	{
		f -= std::min(static_cast<float>(grownUp - age) * 0.2f * 0.1f, 0.4f);
	}
	else if (age > old)
	{
		f -= std::min(static_cast<float>(age - old) * 0.02f, 0.4f);
	}
	else
	{
		// (1 - min(food, 1))^3 * 0.1: villagers don't get hungry yet (food 1)
		f -= life * 0.1f;
		if (villager->sex == Villager::Sex::FEMALE)
		{
			f -= 0.2f;
		}
	}
	const auto raw = std::clamp(static_cast<int32_t>(static_cast<float>(static_cast<int32_t>(speed)) * f), 0, 0xFFFF);
	// the u16 is the distance per turn in MapCoords (6553.6 per metre)
	wallHug->speed = static_cast<float>(raw) / 6553.6f;
}

float VillagerScaleForAge(const GVillagerInfo& info, uint32_t age)
{
	const auto& table = info.ageToScale.values;
	if (age < info.grownUpAge)
	{
		// InitialiseScale: ageToScale[age - 1] (at age 0 the original reads the float before the table: about 0)
		float scale = age >= 1 && age - 1 < table.size() ? table[age - 1] : 0.0f;
		const float next = age + 1 < table.size() ? table[age + 1] : table.back();
		scale += FloatRand((next - scale) * 0.75f);
		return scale;
	}
	// adult: 0.9, then 1.05 - rand(0.1) twice if the first is above the current scale
	float scale = 0.9f;
	const float a = 1.05f - FloatRand(0.1f);
	if (scale < a)
	{
		scale = 1.05f - FloatRand(0.1f);
	}
	return scale;
}

} // namespace openblack::ecs
