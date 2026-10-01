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
#include "ECS/Components/WorshipSite.h"
#include "ECS/MapCoords.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerCore.h"
#include "Game.h"
#include "ECS/ObjectCreationIndex.h"
#include "InfoConstants.h"
#include "LandBalance.h"
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

/// g_game +0x205A40, the game turn (0 without a Game, in the tests)
uint32_t CurrentGameTurn()
{
	const auto* game = Game::Instance();
	return game != nullptr ? game->GetTurn() : 0;
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
	// 0x753766: a villager controlled by a script keeps its speed (GameThingWithPos +0x25 & 4)
	if (script_held::IsControlledByScript(entity))
	{
		return;
	}
	// 0x753772: so does a dancing one (Living::IsDancing 0x5ECC10: the DanceGroup at Living +0xD8 is not null).
	// (aproximado: openblack has no Living +0xD8 yet; WorshipVillager::dancing, set where the original's
	// GroupBehaviour::FindDanceGroup puts it in a dance group and cleared at RemoveFromDance, stands for it, and TOP ==
	// IN_DANCE for the other dances, as openblack had it)
	const auto* worship = registry.TryGet<const WorshipVillager>(entity);
	if (top == static_cast<uint8_t>(VillagerStates::InDance) || (worship != nullptr && worship->dancing))
	{
		return;
	}
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
	// m: the land balance speed scale (GLandBalance::Values[4]: 1.5 in Land2, 1.25 in Land3) * the player's wonder bonus
	// (1) * the town's belief term (openblack has no belief in the player yet: 1)
	const float m = land_balance::Get(4);
	const float life = villager->life;
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
	// ObjectCreationIndex (+0x3C), a signed int in the original's multiplication
	const auto index = static_cast<int32_t>(std::max<int64_t>(object_index::Of(entity), 0));
	float f = static_cast<float>((index * 47) % 31 - 16) * 0.01f + 1.0f;
	// Living::GetAge 0x5ECAF0 (vt +0x8D0, an unsigned div), compared unsigned with GLivingInfo +0x138 grownUpAge
	// (0x750F26, jae) and +0x13C oldAge (0x750F87, jbe); the differences are loaded as unsigned qwords (fild, high
	// dword 0: 0x750F47 / 0x750FA6), times 0.2 and 0.1 (0x8AA3AC, 0x8AC404), at most 0.4 (0x8C7A44)
	const uint32_t age = villager::AgeFromBirthTurn(villager->birthTurn, CurrentGameTurn());
	const uint32_t grownUp = info->grownUpAge;
	const uint32_t old = info->oldAge;
	if (age < grownUp)
	{
		f -= std::min(static_cast<float>(grownUp - age) * 0.2f * 0.1f, 0.4f);
	}
	else if (age > old)
	{
		f -= std::min(static_cast<float>(age - old) * 0.2f * 0.1f, 0.4f);
	}
	else
	{
		// 0x750FDB..0x750FEE: GetDesireForFood 0x75BB50 (POWER(food) = 1 - min(food, 1)^3) * 0.1
		f -= villager::GetDesireForFood(entity) * 0.1f;
		// 0x750FF2..0x751008: life (vt +0x11C) * 0.1, and 0.2 more for a woman (GVillagerInfo +0x1F8 == 1, which
		// Villager::sex mirrors)
		f -= life * 0.1f;
		if (villager->sex == Villager::Sex::FEMALE)
		{
			f -= 0.2f;
		}
	}
	const auto raw = std::clamp(static_cast<int32_t>(static_cast<float>(static_cast<int32_t>(speed)) * f), 0, 0xFFFF);
	// MobileWallHug::SetSpeed 0x60FC50 clamps to 0..0xFFFF and stores the u16 at +0x5A as it is: the distance per turn in
	// MapCoords units. openblack keeps the speed in metres, so it converts here; (inferido) the original never converts
	// this value, it adds it to a MapCoords, and ToMetres ([0x8AA3A4], the conversion it uses everywhere else) is the
	// nearest thing to it (it differs from / 6553.6f by at most one bit)
	wallHug->speed = map_coords::ToMetres(raw);
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
