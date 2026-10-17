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
#include <fmt/format.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Dances.h"
#include "ECS/Influence/Influence.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/TownDesire.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/VillagerAge.h"
#include "Game.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "Locator.h"
#include "Magic/Core/Players.h"

namespace openblack::ecs::villager_speed
{
namespace
{
constexpr float k_LeastIndianPower = 0.1f;
constexpr float k_BeliefBeyondNeutral = 0.1f;
constexpr size_t k_CrawlSpeed = 4;
constexpr size_t k_DefaultSpeed = 0;
constexpr size_t k_FleeingSpeed = 1;
constexpr float k_LeastLoadMod = 0.75f;
constexpr float k_MostTownNeedsMod = 0.5f;
constexpr size_t k_StoryLands = 6;
constexpr int32_t k_CreationSpread = 31;
constexpr int32_t k_CreationStep = 47;
constexpr int32_t k_CreationMiddle = 16;
constexpr float k_CreationShare = 0.01f;
constexpr float k_AgeSlowing = 0.2f;
constexpr float k_AgeShare = 0.1f;
constexpr float k_MostAgeSlowing = 0.4f;
constexpr float k_HungerSlowing = 0.1f;
constexpr float k_LifeSlowing = 0.1f;
constexpr float k_WomanSlowing = 0.2f;

float LoadMod(float fullLoadMod, float held, float most)
{
	// (ours) a capacity of 0 gives no load term, as villager::LoadFactors has it: only made-up villagers have one, the
	// game's tables never do
	const float load = most != 0.0f ? held / most : 0.0f;
	const float mod = fullLoadMod + 1.0f - load;
	return mod < k_LeastLoadMod ? k_LeastLoadMod : std::min(mod, 1.0f);
}
} // namespace

PlayerTerms PlayerTermsOf(const std::optional<TownPlayer>& town)
{
	if (!town.has_value())
	{
		return {};
	}
	PlayerTerms terms {.indianPower = town->indianPower};
	if (!town->neutralPlayer)
	{
		terms.belief = Inputs::Belief {.player = town->beliefInPlayer, .neutral = town->beliefInNeutral};
	}
	return terms;
}

float Scale(const Inputs& inputs)
{
	const float power = inputs.indianPower.has_value()
	                        ? (*inputs.indianPower > k_LeastIndianPower ? *inputs.indianPower : k_LeastIndianPower)
	                        : 1.0f;
	float scale = inputs.landSpeedBalance * power;
	if (inputs.belief.has_value())
	{
		const float beyond = inputs.belief->player - inputs.belief->neutral;
		const float believed = beyond > 0.0f ? beyond * k_BeliefBeyondNeutral : inputs.belief->player;
		float beliefScale = inputs.landBeliefSpeedScale;
		if (beliefScale == 1.0f)
		{
			if (inputs.multiplayer)
			{
				beliefScale = inputs.beliefSpeedScaleMultiPlayer;
			}
			else if (inputs.landNumber < k_StoryLands)
			{
				beliefScale = inputs.beliefSpeedScaleStory.at(inputs.landNumber);
			}
		}
		scale = scale * (beliefScale * believed + 1.0f);
	}
	return scale;
}

int32_t StateSpeed(const Inputs& inputs, const FloatRandom& random)
{
	const float scale = Scale(inputs);
	const auto speedOf = [&inputs](size_t index) { return static_cast<float>(inputs.speeds.at(index)); };
	float speed = 0.0f;
	if (inputs.life <= inputs.lifeWhenCrawlsWounded)
	{
		return static_cast<int32_t>((random(0.2f) + 0.4f) * speedOf(k_CrawlSpeed) * scale);
	}
	if (inputs.life <= inputs.lifeWhenWalksWounded)
	{
		return static_cast<int32_t>((random(0.25f) + 0.5f) * speedOf(k_DefaultSpeed) * scale);
	}
	if (inputs.townInEmergency)
	{
		speed = (random(0.5f) + 0.75f) * speedOf(k_FleeingSpeed) * scale;
	}
	else
	{
		float townMod = 1.0f;
		if (inputs.townNeeds.has_value())
		{
			// (ours) a divisor of 0 gives no needs term, as villager::TownNeedsFactor has it: only made-up villagers have
			// one, the game's tables never do
			const float share =
			    inputs.divisorForTownNeedsSpeedMod != 0.0f ? *inputs.townNeeds / inputs.divisorForTownNeedsSpeedMod : 0.0f;
			townMod = (share < 0.0f ? 0.0f : std::min(share, k_MostTownNeedsMod)) + inputs.baseForTownNeedsSpeedMod;
		}
		const float woodMod = LoadMod(inputs.speedModWhenFullLoadOfWood, inputs.woodHeld, inputs.maxWood);
		const float foodMod = LoadMod(inputs.speedModWhenFullLoadOfFood, inputs.foodHeld, inputs.maxFood);
		speed = speedOf(inputs.speedIndex) * foodMod * woodMod * townMod * scale;
	}
	if (inputs.foodSpeedUp)
	{
		speed *= inputs.foodPowerupIncrease;
	}
	return static_cast<int32_t>(speed);
}

float PersonalFactor(const Person& person)
{
	// The multiplication wraps as the game's whole numbers do
	const auto stepped =
	    static_cast<int32_t>(static_cast<uint32_t>(person.creationIndex) * static_cast<uint32_t>(k_CreationStep));
	float factor = static_cast<float>(stepped % k_CreationSpread - k_CreationMiddle) * k_CreationShare + 1.0f;
	if (person.age < person.grownUpAge)
	{
		factor -= std::min(static_cast<float>(person.grownUpAge - person.age) * k_AgeSlowing * k_AgeShare, k_MostAgeSlowing);
	}
	else if (person.age > person.oldAge)
	{
		factor -= std::min(static_cast<float>(person.age - person.oldAge) * k_AgeSlowing * k_AgeShare, k_MostAgeSlowing);
	}
	else
	{
		factor = factor - person.desireForFood * k_HungerSlowing;
		factor -= person.life * k_LifeSlowing;
		if (person.female)
		{
			factor -= k_WomanSlowing;
		}
	}
	return factor;
}

uint16_t FinalSpeed(int32_t speed, float factor)
{
	const auto scaled = static_cast<int32_t>(static_cast<float>(speed) * factor);
	return static_cast<uint16_t>(std::clamp(scaled, 0, 0xFFFF));
}

} // namespace openblack::ecs::villager_speed

namespace openblack::ecs
{
using namespace components;

namespace
{
/// VillagerDisciple 9 TRADER
constexpr uint8_t k_DiscipleTrader = 9;
/// The last of a villager's six speeds: a state's speed index past it takes it
constexpr uint32_t k_LastSpeed = 5;
/// The land balance's values for the speed scale and the belief speed scale
constexpr size_t k_SpeedBalance = 4;
constexpr size_t k_BeliefSpeedBalance = 7;

uint32_t Raw(SpeedState state)
{
	return static_cast<uint32_t>(state);
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

namespace
{
/// SetVillagerStateSpeed with the speed group entry of `given` when there is one, else of the final state
void SetStateSpeedOf(entt::entity entity, std::optional<VillagerStates> given)
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
	// A villager controlled by a script keeps its speed, and so does a dancing one: one in a dance's group, whatever its
	// state. (approximate) the worship sites' dancers are not in a dance yet: WorshipVillager::dancing, set where the
	// original puts one in its site's dance group and cleared when it leaves the dance, stands for it
	const auto* worship = registry.TryGet<const WorshipVillager>(entity);
	const bool dancing = dances::DanceOf(registry, entity) != entt::null || (worship != nullptr && worship->dancing);
	if (!villager_speed::StateChangeSetsSpeed(script_held::IsControlledByScript(entity), dancing))
	{
		return;
	}
	const auto destination = action->states[static_cast<size_t>(LivingAction::Index::Final)];
	auto final = top < states.size() && states[top].isFinalState != 0 ? top : destination;
	if (final >= states.size())
	{
		final = top;
	}
	if (given.has_value())
	{
		final = static_cast<uint8_t>(*given);
	}
	if (final >= states.size())
	{
		return;
	}
	// The land balance's speed scale (1.5 in Land 2, 1.25 in Land 3)
	const float landSpeedBalance = land_balance::Get(k_SpeedBalance);
	const auto& group = info->speedGroup;
	const auto* town = villager->town != entt::null ? registry.TryGet<const Town>(villager->town) : nullptr;
	// The town's player gives the Indian tribal power (the most it has had, which is its tribal power) and, for a player
	// other than the neutral one, the town's belief in it beyond the town's own neutral belief
	const auto player =
	    town != nullptr
	        ? std::optional(villager_speed::TownPlayer {
	              .neutralPlayer = town->owner == PlayerNames::NEUTRAL,
	              .indianPower = magic::players::ReadMagicOf(town->owner).tribalPower.at(static_cast<size_t>(Tribe::INDIAN)),
	              .beliefInPlayer = town_belief::GetBeliefInPlayer(town->belief, town->owner),
	              .beliefInNeutral = town->belief.beliefInNeutralPlayer,
	          })
	        : std::nullopt;
	const auto terms = villager_speed::PlayerTermsOf(player);
	const auto& townInfo = Locator::infoConstants::value().town;
	// the loads of wood and food, with the trader's capacities for disciple 9
	const bool trader = villager->discipleType == k_DiscipleTrader;
	const villager_speed::Inputs inputs {
	    .speeds = {static_cast<int32_t>(Raw(group.speedDefault)), static_cast<int32_t>(Raw(group.speedFleeing)),
	               static_cast<int32_t>(Raw(group.speed2)), static_cast<int32_t>(Raw(group.speed3)),
	               static_cast<int32_t>(Raw(group.speed4)), static_cast<int32_t>(Raw(group.speed5))},
	    .speedIndex = std::min<uint32_t>(states[final].speedIndex, k_LastSpeed),
	    .life = villager->life,
	    .lifeWhenWalksWounded = info->lifeWhenWalksWounded,
	    .lifeWhenCrawlsWounded = info->lifeWhenCrawlsWounded,
	    .landSpeedBalance = landSpeedBalance,
	    .landBeliefSpeedScale = land_balance::Get(k_BeliefSpeedBalance),
	    .indianPower = terms.indianPower,
	    .belief = terms.belief,
	    .beliefSpeedScaleMultiPlayer = townInfo.beliefSpeedScaleMultiPlayer,
	    .beliefSpeedScaleStory = townInfo.beliefSpeedScaleStory,
	    // openblack has neither multiplayer nor the front end's skirmish game
	    .multiplayer = false,
	    // unsigned, so that no land number below 0 counts as a story land
	    .landNumber = static_cast<uint32_t>(influence::LandNumber()),
	    .townNeeds = villager->town != entt::null ? std::optional(town_desire::TownNeedsSum(villager->town)) : std::nullopt,
	    .baseForTownNeedsSpeedMod = info->baseForTownNeedsSpeedMod,
	    .divisorForTownNeedsSpeedMod = info->divisorForTownNeedsSpeedMod,
	    .townInEmergency = town != nullptr && town_queries::IsInStateOfEmergency(*town),
	    .woodHeld = static_cast<float>(villager->resourceHeld.at(1)),
	    .foodHeld = static_cast<float>(villager->resourceHeld.at(0)),
	    .maxWood = static_cast<float>(trader ? info->maxTraderWoodCarried : info->maxWoodCarried),
	    .maxFood = static_cast<float>(trader ? info->maxTraderFoodCarried : info->maxFoodCarried),
	    .speedModWhenFullLoadOfWood = info->speedModWhenFullLoadOfWood,
	    .speedModWhenFullLoadOfFood = info->speedModWhenFullLoadOfFood,
	    .foodSpeedUp = villager->foodSpeedUp != 0,
	    .foodPowerupIncrease = info->foodPowerupIncrease,
	};
	const int32_t whole = villager_speed::StateSpeed(inputs, [](float most) { return game_random::GameFloatRand(most); });
	if (inputs.life > inputs.lifeWhenCrawlsWounded && inputs.life > inputs.lifeWhenWalksWounded && !inputs.townInEmergency &&
	    villager::TraceOn(entity))
	{
		// the terms of a walking villager's speed, before the food speed-up
		const auto spd = inputs.speeds.at(inputs.speedIndex);
		const auto load = villager::LoadFactors(villager->resourceHeld.at(1), villager->resourceHeld.at(0), *info, trader);
		const float townNeeds = inputs.townNeeds.has_value() ? villager::TownNeedsFactor(*inputs.townNeeds, *info) : 1.0f;
		const float speed = static_cast<float>(spd) * load.food * load.wood * townNeeds * villager_speed::Scale(inputs);
		villager::Trace(entity, fmt::format("speed: spd {} foodF {:.9f} woodF {:.9f} T {:.9f} -> {:.6f}", spd, load.food,
		                                    load.wood, townNeeds, speed));
	}
	// with the villager's own factor
	SetVillagerSpeed(entity, whole, true);
}
} // namespace

void SetVillagerStateSpeed(entt::entity entity)
{
	SetStateSpeedOf(entity, std::nullopt);
}

void SetVillagerStateSpeed(entt::entity entity, VillagerStates state)
{
	SetStateSpeedOf(entity, state);
}

void SetVillagerSpeed(entt::entity entity, int32_t whole, bool applyFactor)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* villager = registry.TryGet<const Villager>(entity);
	auto* wallHug = registry.TryGet<WallHug>(entity);
	const auto* info = VillagerInfoOf(entity);
	if (villager == nullptr || wallHug == nullptr || info == nullptr)
	{
		return;
	}
	// 1 without applyFactor
	float factor = 1.0f;
	if (applyFactor)
	{
		// The creation index is a signed whole number in the game's multiplication
		const auto index = static_cast<int32_t>(std::max<int64_t>(object_index::Of(entity), 0));
		factor = villager_speed::PersonalFactor({
		    .creationIndex = static_cast<uint32_t>(index),
		    .age = villager_age::AgeNow(*villager),
		    .grownUpAge = info->grownUpAge,
		    .oldAge = info->oldAge,
		    .desireForFood = villager::GetDesireForFood(entity),
		    .life = villager->life,
		    .female = villager->sex == Villager::Sex::FEMALE,
		});
	}
	// Stored as the distance per turn in map units. openblack keeps the speed in metres, so it converts here;
	// (inferred) the original never converts this value, it adds it to map coordinates, and ToMetres (the conversion it
	// uses everywhere else) is the nearest thing to it (it differs from / 6553.6f by at most one bit)
	wallHug->speed = map_coords::ToMetres(villager_speed::FinalSpeed(whole, factor));
}

void SetVillagerSpeedInMetres(entt::entity villager, float metres)
{
	// ConvertMetersToWholeDistance (m / 10 x 65536, truncated toward zero), then SetVillagerSpeed(whole, false)
	SetVillagerSpeed(villager, gutils::ConvertMetersToWholeDistance(metres), false);
}

uint16_t WholeSpeed(const WallHug& wallHug)
{
	return static_cast<uint16_t>(std::clamp(gutils::ConvertMetersToWholeDistance(wallHug.speed), 0, 0xFFFF));
}

float VillagerSpeedInMetres(entt::entity villager)
{
	// ConvertWholeDistanceToMeters of the u16: WallHug::speed holds ToMetres of that u16, the same single rounding of
	// whole x 10 / 65536
	const auto* wallHug = Locator::entitiesRegistry::value().TryGet<const WallHug>(villager);
	return wallHug != nullptr ? wallHug->speed : 0.0f;
}

} // namespace openblack::ecs
