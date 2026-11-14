/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerShepherd.h"

#include <array>
#include <optional>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "Common/GameAngle.h"
#include "Common/GameDistance.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/ShepherdWork.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Flocks.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerAnimate.h"
#include "ECS/Systems/Implementations/VillagerMoveToObject.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/VillagerSpeed.h"
#include "ECS/WaterQueries.h"
#include "ECS/Weather/Calendar.h"
#include "InfoConstants.h"
#include "Locator.h"

// The shepherds (VillagerShepherd.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}

entt::entity TownOfVillager(entt::entity villager)
{
	auto& registry = Entities();
	const auto* v = registry.TryGet<const Villager>(villager);
	return v != nullptr && v->town != entt::null && registry.Valid(v->town) && registry.AllOf<Town>(v->town) ? v->town
	                                                                                                         : entt::null;
}

Flock* FlockData(entt::entity flock)
{
	auto& registry = Entities();
	return flock != entt::null && registry.Valid(flock) ? registry.TryGet<Flock>(flock) : nullptr;
}

ShepherdWork& WorkOf(entt::entity villager)
{
	auto& registry = Entities();
	if (auto* work = registry.TryGet<ShepherdWork>(villager); work != nullptr)
	{
		return *work;
	}
	return registry.Assign<ShepherdWork>(villager);
}

/// The jobs table's shepherd row is at work in this season
bool ShepherdsWork()
{
	const auto& job = Locator::infoConstants::value().job.at(static_cast<size_t>(k_ShepherdJob));
	const std::array activities {job.spring, job.summer, job.autumn, job.winter};
	const auto season = weather::calendar::GetSeason(CurrentTurn());
	return activities.at(season < activities.size() ? season : activities.size() - 1) != JobActivity::None;
}

/// A member's speed in whole map units a turn: an animal's, a villager's
int32_t SpeedOf(entt::entity member)
{
	auto& registry = Entities();
	if (member == entt::null || !registry.Valid(member))
	{
		return 0;
	}
	if (const auto* brain = registry.TryGet<const AnimalBrain>(member); brain != nullptr)
	{
		return brain->speed;
	}
	if (const auto* walker = registry.TryGet<const WallHug>(member); walker != nullptr)
	{
		return WholeSpeed(*walker);
	}
	return 0;
}

/// The flock's newest member (the head of its list)
entt::entity HeadOf(const Flock& flock)
{
	return flock.members.empty() ? entt::null : flock.members.back();
}

/// The villager walks at the pace of the flock's newest member, and plays its state's clip
void WalkAtTheFlocksPace(entt::entity villager, const Flock& flock)
{
	SetVillagerSpeed(villager, SpeedOf(HeadOf(flock)), false);
	villager_animate::SetStateAnim(villager);
}

/// The home's drinking water: the one it keeps, else looked for within 400 m of the home now
std::optional<glm::vec3> WaterFor(entt::entity abode)
{
	auto& registry = Entities();
	auto& water = registry.AllOf<water_queries::DrinkingWater>(abode) ? registry.Get<water_queries::DrinkingWater>(abode)
	                                                                  : registry.Assign<water_queries::DrinkingWater>(abode);
	if (const auto found = water_queries::GetNearestWaterPos(water); found.has_value())
	{
		return found;
	}
	const auto* transform = registry.TryGet<const Transform>(abode);
	if (transform == nullptr || !Locator::terrainSystem::has_value())
	{
		return std::nullopt;
	}
	water_queries::FindNearestDrinkingWater(Locator::terrainSystem::value(), water,
	                                        glm::vec3(transform->position.x, 0.0f, transform->position.z),
	                                        k_ShepherdWaterRadius);
	return water_queries::GetNearestWaterPos(water);
}

/// Where a thing stands, as a map position
map_coords::MapCoords PlaceOf(entt::entity thing)
{
	const auto* transform = Entities().TryGet<const Transform>(thing);
	return transform != nullptr ? map_coords::FromMetres(glm::xz(transform->position)) : map_coords::MapCoords {};
}

/// The member of the flock nearest a place, from the head, strictly nearer than `reach` metres, that is neither
/// `exclude` nor a villager
entt::entity NearestAnimal(const Flock& flock, const map_coords::MapCoords& from, float reach, entt::entity exclude)
{
	auto& registry = Entities();
	entt::entity nearest = entt::null;
	for (auto it = flock.members.rbegin(); it != flock.members.rend(); ++it)
	{
		const auto member = *it;
		if (member == exclude || !registry.Valid(member) || registry.AllOf<Villager>(member))
		{
			continue;
		}
		const float d = game_math::GetDistanceInMetres(from, PlaceOf(member));
		if (d < reach)
		{
			reach = d;
			nearest = member;
		}
	}
	return nearest;
}

/// The villager's store (its town's, else its home) when it works
entt::entity WorkingStoreOf(entt::entity villager)
{
	const auto pit = GetStoragePit(villager);
	return pit != entt::null && abode_queries::IsFunctional(pit) ? pit : entt::entity(entt::null);
}

/// The villager points at a flock, shepherds it, and the flock knows it
void Shepherd(entt::entity villager, entt::entity flock)
{
	flocks::SetFlock(villager, flock);
	if (auto* data = FlockData(flock); data != nullptr)
	{
		data->shepherd = villager;
	}
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

float FlockScore(float distance)
{
	return game_math::GetDistanceModifier(distance, k_FlockDistanceLimit);
}

StoreLeg StoreLegFor(float distanceToDoor)
{
	if (distanceToDoor > k_FarFromStore)
	{
		return StoreLeg::Quarter;
	}
	return distanceToDoor > k_NearStore ? StoreLeg::Step : StoreLeg::Door;
}

int16_t SlaughterFood(float foodValue, FoodType foodType)
{
	// Object::GetFoodValue(MEAT | VEGETABLE), truncated
	const bool food = (static_cast<uint32_t>(foodType) & static_cast<uint32_t>(FoodType::MeatVegetable)) != 0;
	return food ? static_cast<int16_t>(static_cast<int32_t>(foodValue)) : int16_t {0};
}

FlockDecision DecideWithFlock(std::size_t members, float foodDesire, float trigger)
{
	if (members < 2)
	{
		return FlockDecision::Release;
	}
	if (foodDesire >= trigger && (members > 2 || foodDesire > k_HungryTown))
	{
		return FlockDecision::Slaughter;
	}
	return FlockDecision::MoveToWater;
}

// ---- the finder and the job --------------------------------------------------------------------------------------

entt::entity FindFlockWithoutShepherd(entt::entity town, entt::entity villager, float& score)
{
	score = 0.0f;
	auto& registry = Entities();
	const auto* t = town != entt::null && registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	if (t == nullptr)
	{
		return entt::null;
	}
	float best = 0.0f;
	entt::entity found = entt::null;
	const auto me = tq::PosOf(villager);
	// The town's flocks from the head of its list, the newest (openblack keeps the newest at the back)
	for (auto it = t->flocks.rbegin(); it != t->flocks.rend(); ++it)
	{
		const auto* flock = FlockData(*it);
		if (flock == nullptr || flock->shepherd != entt::null)
		{
			continue;
		}
		const float s =
		    FlockScore(tq::GetDistanceInMetres(tq::ToMapCoords(glm::vec2(flock->domainCentre.x, flock->domainCentre.z)), me));
		// Only strictly above the best
		if (s > best)
		{
			best = s;
			found = *it;
		}
	}
	score = best;
	return found;
}

entt::entity FirstFreeFlock(entt::entity town)
{
	auto& registry = Entities();
	const auto* t = town != entt::null && registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	if (t == nullptr)
	{
		return entt::null;
	}
	for (auto it = t->flocks.rbegin(); it != t->flocks.rend(); ++it)
	{
		const auto* flock = FlockData(*it);
		// no shepherd, and a head and a tail of any kind of living
		if (flock != nullptr && flock->shepherd == entt::null && !flock->members.empty() &&
		    registry.Valid(flock->members.front()) && registry.Valid(flock->members.back()))
		{
			return *it;
		}
	}
	return entt::null;
}

uint32_t VillagerBecomesShepherd(entt::entity villager, entt::entity flock)
{
	if (FlockData(flock) == nullptr)
	{
		const auto town = TownOfVillager(villager);
		flock = town != entt::null ? FirstFreeFlock(town) : entt::entity(entt::null);
		if (flock == entt::null)
		{
			return 0;
		}
	}
	// someone else's flock
	if (const auto shepherd = FlockData(flock)->shepherd; shepherd != entt::null && shepherd != villager)
	{
		return 0;
	}
	Shepherd(villager, flock);
	return ShepherdGotoFlock(villager);
}

uint32_t ShepherdLookForFlock(entt::entity villager)
{
	const auto town = TownOfVillager(villager);
	const auto flock = town != entt::null ? FirstFreeFlock(town) : entt::entity(entt::null);
	if (flock == entt::null)
	{
		return 0;
	}
	// out of the flock it is in, which goes when left empty
	if (const auto old = flocks::FlockOf(villager); old != entt::null)
	{
		flocks::RemoveLiving(old, villager, true);
	}
	Shepherd(villager, flock);
	return 1;
}

uint32_t ShepherdGotoFlock(entt::entity villager)
{
	const auto flock = flocks::FlockOf(villager);
	const auto* data = FlockData(flock);
	if (data == nullptr || data->members.empty() || !Entities().Valid(data->members.front()))
	{
		ShepherdLookForFlock(villager);
		return 0;
	}
	if (!ShepherdsWork())
	{
		return 0;
	}
	const auto place = flocks::GetFlockPos(flock);
	SetupMoveToWithHug(villager, glm::vec2(place.x, place.z), VillagerStates::ShepherdTakesControlOfFlock);
	return 1;
}

// ---- the states --------------------------------------------------------------------------------------------------

uint32_t ShepherdLookForFlockState(LivingAction& action)
{
	return ShepherdLookForFlock(EntityOf(action));
}

uint32_t ShepherdMoveFlockToWater(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto flock = flocks::FlockOf(villager);
	auto* data = FlockData(flock);
	const auto abode = Entities().Get<const Villager>(villager).abode;
	const auto water = data != nullptr && abode != entt::null && Entities().Valid(abode) ? WaterFor(abode) : std::nullopt;
	if (!water.has_value())
	{
		SetTopState(villager, VillagerStates::ShepherdReleasesControlOfFlock);
		return 0;
	}
	// the flock's place is kept to come back to, and moved to the water
	data->savedDomainCentre = data->domainCentre;
	flocks::SetDomainCentrePos(flock, *water);
	auto& work = WorkOf(villager);
	work.waitTurns = k_TurnsAtWater;
	work.pending = VillagerStates::ShepherdMoveFlockBack;
	SetupMoveToWithHug(villager, glm::vec2(water->x, water->z), VillagerStates::ShepherdWaitForFlock);
	if (const auto* stillThere = FlockData(flock); stillThere != nullptr)
	{
		WalkAtTheFlocksPace(villager, *stillThere);
	}
	return 1;
}

uint32_t ShepherdMoveFlockToFood(LivingAction& /*action*/)
{
	return 1;
}

uint32_t ShepherdMoveFlockBack(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto flock = flocks::FlockOf(villager);
	const auto* data = FlockData(flock);
	if (data == nullptr)
	{
		return 0;
	}
	const auto back = data->savedDomainCentre;
	flocks::SetDomainCentrePos(flock, back);
	SetupMoveToWithHug(villager, glm::vec2(back.x, back.z), VillagerStates::ShepherdReleasesControlOfFlock);
	if (const auto* stillThere = FlockData(flock); stillThere != nullptr)
	{
		WalkAtTheFlocksPace(villager, *stillThere);
	}
	return 1;
}

uint32_t ShepherdDecideWhatToDoWithFlock(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& work = WorkOf(villager);
	--work.waitTurns;
	if (work.waitTurns != 0)
	{
		return 1;
	}
	const auto* data = FlockData(flocks::FlockOf(villager));
	const auto members = data != nullptr ? data->members.size() : 0;
	// the town's food desire against its trigger. (openblack, guard) a villager without a town wants none
	float desire = 0.0f;
	if (const auto town = TownOfVillager(villager); town != entt::null && Locator::townDesireSystem::has_value())
	{
		desire = Locator::townDesireSystem::value().GetDesire(town, TownDesireInfo::ForFood);
	}
	const float trigger = Locator::infoConstants::value()
	                          .townDesire.at(static_cast<size_t>(TownDesireInfo::ForFood))
	                          .desireTriggersVillagerAction;
	switch (DecideWithFlock(members, desire, trigger))
	{
	case FlockDecision::Release:
		SetTopState(villager, VillagerStates::ShepherdReleasesControlOfFlock);
		return 0;
	case FlockDecision::Slaughter:
		SetTopState(villager, VillagerStates::ShepherdTakeAnimalForSlaughter);
		return 1;
	case FlockDecision::MoveToWater:
		SetTopState(villager, VillagerStates::ShepherdMoveFlockToWater);
		return 1;
	}
	return 1;
}

uint32_t ShepherdWaitForFlock(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& work = WorkOf(villager);
	--work.waitTurns;
	if (work.waitTurns != 0)
	{
		return 1;
	}
	if (work.pending != VillagerStates::InvalidState)
	{
		const auto next = work.pending;
		SetTopState(villager, next);
		WorkOf(villager).pending = VillagerStates::InvalidState;
		return 1;
	}
	work.waitTurns = k_ShortWait;
	SetTopState(villager, VillagerStates::ShepherdDecideWhatToDoWithFlock);
	return 1;
}

uint32_t ShepherdSlaughterAnimal(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& work = WorkOf(villager);
	--work.waitTurns;
	const auto flock = flocks::FlockOf(villager);
	const auto* data = FlockData(flock);
	if (work.waitTurns > 0 || data == nullptr)
	{
		return 1;
	}
	if (const auto animal = NearestAnimal(*data, PlaceOf(villager), k_TakeReach, villager); animal != entt::null)
	{
		// out of the flock, its food to the shepherd, then it walks to the shepherd to die there at once
		flocks::RemoveLiving(flock, animal, true);
		const auto& info =
		    Locator::infoConstants::value().animal.at(static_cast<size_t>(Entities().Get<const Animal>(animal).type));
		PickupFood(villager, SlaughterFood(info.foodValue, info.foodType));
		const auto here = map_coords::ToMetres(PlaceOf(villager));
		animal_ai::WalkToSlaughter(animal, here);
		WorkOf(villager).pending = VillagerStates::ArrivesAtStoragePitForDropOff;
		SetTopState(villager, VillagerStates::ShepherdWaitForFlock);
	}
	WorkOf(villager).waitTurns = k_ShortWait;
	return 1;
}

uint32_t ShepherdFetchStray(LivingAction& /*action*/)
{
	return 1;
}

uint32_t ShepherdGotoFlockState(LivingAction& action)
{
	return ShepherdGotoFlock(EntityOf(action));
}

uint32_t ShepherdTakesControlOfFlock(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto flock = flocks::FlockOf(villager);
	if (!ShepherdsWork() || FlockData(flock) == nullptr)
	{
		return 0;
	}
	// the leader it had before the shepherd joins
	const auto& members = FlockData(flock)->members;
	const auto leader = members.empty() ? entt::entity(entt::null) : members.front();
	flocks::AddMember(flock, villager);
	WorkOf(villager).waitTurns = k_LookOverTurns;
	SetupMoveToObject(villager, leader, VillagerStates::ShepherdDecideWhatToDoWithFlock);
	return 1;
}

uint32_t ShepherdTakeAnimalForSlaughter(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto flock = flocks::FlockOf(villager);
	if (const auto* data = FlockData(flock); data != nullptr)
	{
		const auto animal = NearestAnimal(*data, PlaceOf(villager), k_PickAnimalReach, villager);
		if (animal != entt::null && WorkingStoreOf(villager) != entt::null)
		{
			// it leaves the flock, which no longer has a shepherd, and leads the animal away in a flock of its own
			flocks::RemoveLiving(flock, villager, true);
			if (auto* left = FlockData(flock); left != nullptr)
			{
				left->shepherd = entt::null;
			}
			const auto led = flocks::SeparateIntoNewFlock(flock, animal, true);
			flocks::AddMember(led, villager);
			if (auto* ledData = FlockData(led); ledData != nullptr)
			{
				ledData->shepherd = villager;
				ledData->flockDistance = k_LedFlockDistance;
				ledData->followShepherd = true;
			}
			WorkOf(villager).waitTurns = k_ShortWait;
			SetTopState(villager, VillagerStates::ShepherdCheckAnimalForSlaughter);
			return 1;
		}
	}
	WorkOf(villager).waitTurns = k_ShortWait;
	SetTopState(villager, VillagerStates::ShepherdDecideWhatToDoWithFlock);
	return 0;
}

uint32_t ShepherdCheckAnimalForSlaughter(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& work = WorkOf(villager);
	--work.waitTurns;
	if (work.waitTurns > 0)
	{
		return 0;
	}
	const auto flock = flocks::FlockOf(villager);
	const auto* data = FlockData(flock);
	if (data == nullptr)
	{
		SetTopState(villager, VillagerStates::ShepherdDecideWhatToDoWithFlock);
		WorkOf(villager).waitTurns = k_ShortWait;
		return 0;
	}
	if (data->members.size() < 2)
	{
		work.waitTurns = k_ShortWait;
		SetTopState(villager, VillagerStates::ShepherdDecideWhatToDoWithFlock);
		return 0;
	}
	const auto here = PlaceOf(villager);
	const auto animal = NearestAnimal(*data, here, k_LeadReach, villager);
	if (animal == entt::null)
	{
		// it turns to its flock's newest member and waits for the animal
		SetGameAngle(villager, game_math::GetAngleFromXZ(here, PlaceOf(HeadOf(*data))));
		work.waitTurns = k_ShortWait;
		return 0;
	}
	const auto store = WorkingStoreOf(villager);
	if (store == entt::null)
	{
		return 0;
	}
	const auto doorFixed = abode_queries::GetDoorPos(store);
	const map_coords::MapCoords door {doorFixed.x, doorFixed.y, 0.0f};
	const float distance = game_math::GetDistanceInMetres(here, door);
	auto goal = here;
	auto final = VillagerStates::ShepherdCheckAnimalForSlaughter;
	switch (StoreLegFor(distance))
	{
	case StoreLeg::Quarter:
		if (distance != 0.0f)
		{
			goal += game_math::GetPosFromAngle(game_math::Get3DAngleFromXZ(here, door), (1.0f * distance) / 4.0f);
		}
		break;
	case StoreLeg::Step:
		if (distance != 0.0f)
		{
			goal += game_math::GetPosFromAngle(game_math::Get3DAngleFromXZ(here, door), k_StepNearStore);
		}
		break;
	case StoreLeg::Door:
		work.waitTurns = k_WaitAtDoor;
		goal = door;
		final = VillagerStates::ShepherdSlaughterAnimal;
		break;
	}
	SetupMoveToWithHug(villager, map_coords::ToMetres(goal), final);
	// at the pace of the animal it leads
	SetVillagerSpeed(villager, SpeedOf(animal), false);
	villager_animate::SetStateAnim(villager);
	return 1;
}

uint32_t ShepherdReleasesControlOfFlock(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (FlockData(flocks::FlockOf(villager)) == nullptr)
	{
		return 0;
	}
	auto& work = WorkOf(villager);
	work.waitTurns = k_ShortWait;
	work.pending = VillagerStates::GoHome;
	SetTopState(villager, VillagerStates::ShepherdWaitForFlock);
	return 1;
}

uint32_t ExitShepherding(LivingAction& action, VillagerStates next)
{
	const auto villager = EntityOf(action);
	if (IsStateExitFunctionSameAs(villager, next))
	{
		return 1;
	}
	const auto flock = flocks::FlockOf(villager);
	if (auto* data = FlockData(flock); data != nullptr)
	{
		// its place back to where it was kept, the shepherd out of it (which may empty it) and no one shepherds it
		flocks::SetDomainCentrePos(flock, data->savedDomainCentre);
		flocks::RemoveLiving(flock, villager, true);
		if (auto* still = FlockData(flock); still != nullptr)
		{
			still->shepherd = entt::null;
		}
	}
	if (auto* v = Entities().TryGet<Villager>(villager); v != nullptr)
	{
		v->flock = entt::null;
	}
	return 1;
}

} // namespace openblack::ecs::villager
