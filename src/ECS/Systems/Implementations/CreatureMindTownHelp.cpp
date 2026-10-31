/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The world's side of the actions with which a creature helps a town or plays with its people

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureMindTownHelp.h"

#include "Common/GUtilsDistance.h"
#include "Creature/CreatureTownHelp.h"
#include "ECS/Abodes.h"
#include "ECS/Archetypes/PuzzleTotemArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/PuzzleTotem.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/PuzzleTotem.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "Worship/Citadel.h"
#include "Worship/InterfaceStatus.h"
#include "Worship/WorshipPercentage.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using creature_plan_actions::Build;
using creature_plan_actions::Target;

namespace
{
/// Where a thing stands on the ground
std::optional<glm::vec2> GroundPointOf(const ecs::Registry& registry, entt::entity thing)
{
	const auto* at = registry.Valid(thing) ? registry.TryGet<const Transform>(thing) : nullptr;
	return at != nullptr ? std::optional(glm::vec2(at->position.x, at->position.z)) : std::nullopt;
}

/// The town of a totem: the town of the town centre that it stands on
entt::entity TownOfTotem(const ecs::Registry& registry, entt::entity totem)
{
	const auto* standing = registry.Valid(totem) ? registry.TryGet<const VillageTotem>(totem) : nullptr;
	const auto* centre = standing != nullptr && registry.Valid(standing->townCentre)
	                         ? registry.TryGet<const Abode>(standing->townCentre)
	                         : nullptr;
	if (centre == nullptr)
	{
		return entt::null;
	}
	const auto town = ecs::town_queries::FindTownWithID(centre->townId);
	return town != entt::null && registry.Valid(town) && registry.AllOf<Town>(town) ? town : entt::null;
}

/// The worship site of a town, when it has one
entt::entity WorshipSiteOf(const ecs::Registry& registry, entt::entity town)
{
	const auto* magic = registry.TryGet<const TownMagic>(town);
	return magic != nullptr && registry.Valid(magic->worshipSite) ? magic->worshipSite : entt::null;
}

/// Whether a creature may move a totem: the totem has a town, and the hand of the creature's player watches the
/// creature and could take hold of the totem. Only the player at this machine has a hand here
bool MayMoveTotem(const ecs::Registry& registry, entt::entity totem, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	const bool watched = body != nullptr && worship::interface::HasInterface(body->owner);
	return creature_town_help::MayMoveTotem({
	    .hasTown = TownOfTotem(registry, totem) != entt::null,
	    .watched = watched,
	    .mayGrip = watched && Locator::villageTotemSystem::has_value() &&
	               Locator::villageTotemSystem::value().ValidForLockedSelect(totem, body->owner),
	});
}

/// The breeders that a town has now
uint32_t BreedersOf(const ecs::Registry& registry, entt::entity town)
{
	uint32_t breeders = 0;
	registry.Each<const Villager>([town, &breeders](entt::entity /*unused*/, const Villager& villager) {
		if (villager.town == town && (villager.flags & Villager::k_FlagDisciple) != 0 &&
		    villager.discipleType == static_cast<uint8_t>(VillagerDisciple::Breeder))
		{
			++breeders;
		}
	});
	return breeders;
}

bool CanPickUp(entt::entity self, entt::entity thing)
{
	return Locator::creatureObjectActionSystem::has_value() &&
	       Locator::creatureObjectActionSystem::value().CanPickUp(self, thing);
}

/// Whether a build is one of the town help actions'
bool IsTownHelp(Build build)
{
	switch (build)
	{
	case Build::GiveFieldFoodToStore:
	case Build::GiveFruitToStore:
	case Build::GiveMagicFoodToStore:
	case Build::GiveMagicWoodToStore:
	case Build::PutFieldFoodBySite:
	case Build::DropCowInStore:
	case Build::MakeBreeder:
	case Build::TakeVillagerHome:
	case Build::BringVillagerToSite:
	case Build::RaiseTotem:
	case Build::LowerTotem:
	case Build::FrightenVillagers:
	case Build::PlayWithVillager:
	case Build::KissVillager:
		return true;
	default:
		return false;
	}
}
} // namespace

bool mind_town_help::Accepts(const ecs::Registry& registry, Target target, entt::entity thing, entt::entity self,
                             bool heldByOther)
{
	switch (target)
	{
	case Target::MagicWoodPile:
	{
		// A pile of the wood miracle counts wherever it is
		const auto* pot = registry.TryGet<const Pot>(thing);
		return !heldByOther && pot != nullptr && pot->type == PotInfo::MagicWood;
	}
	case Target::Cow:
	{
		const auto* animal = registry.TryGet<const Animal>(thing);
		return !heldByOther && animal != nullptr && creature_town_help::IsCowKind(animal->type) && CanPickUp(self, thing);
	}
	case Target::VillagerNotWorshipping:
		return registry.AllOf<Villager>(thing) && !registry.AllOf<Dancer>(thing);
	case Target::BreederVillager:
	{
		const auto* villager = registry.TryGet<const Villager>(thing);
		if (villager == nullptr)
		{
			return false;
		}
		const bool hasTown = registry.Valid(villager->town) && registry.AllOf<Town>(villager->town);
		// What the creature thinks of making a breeder, as it has learnt it, and whether it copies its player now
		const auto* mind = registry.TryGet<const CreatureMindState>(self);
		const auto* learnt = mind != nullptr && mind->learnt.has_value() ? &*mind->learnt : nullptr;
		const float opinion = learnt != nullptr && creature_town_help::k_MakeBreederAction < learnt->opinions.size()
		                          ? learnt->opinions[creature_town_help::k_MakeBreederAction]
		                          : 0.0f;
		return creature_town_help::CanMakeBreeder({
		    .available = villager->lifeStage == Villager::LifeStage::Adult && !ecs::villager::IsDead(thing) &&
		                 !registry.AllOf<ScriptControlled>(thing),
		    .hasTown = hasTown,
		    .disciple = (villager->flags & Villager::k_FlagDisciple) != 0,
		    .football = (villager->flags & Villager::k_FlagFootball) != 0,
		    .townBreeders = hasTown ? BreedersOf(registry, villager->town) : 0,
		    .skeleton = ecs::villager::IsSkeleton(thing),
		    .opinion = opinion,
		    .mimicking = learnt != nullptr && learnt->mimicry.has_value(),
		});
	}
	case Target::VillagerFarFromHome:
	{
		const auto* villager = registry.TryGet<const Villager>(thing);
		const auto at = GroundPointOf(registry, thing);
		const auto home = villager != nullptr ? GroundPointOf(registry, villager->abode) : std::nullopt;
		return at.has_value() && home.has_value() && creature_town_help::FarFromHome(gutils::GetDistanceInMetres(*home, *at));
	}
	case Target::PlayerTotem:
		return registry.AllOf<VillageTotem>(thing) && MayMoveTotem(registry, thing, self);
	default:
		return false;
	}
}

void mind_town_help::Gather(const ecs::Registry& registry, Target target,
                            const std::function<void(entt::entity, const Transform&)>& consider)
{
	switch (target)
	{
	case Target::MagicWoodPile:
		registry.Each<const Pot, const Transform>(
		    [&](entt::entity entity, const Pot& /*unused*/, const Transform& at) { consider(entity, at); });
		break;
	case Target::PlayerTotem:
		registry.Each<const VillageTotem, const Transform>(
		    [&](entt::entity entity, const VillageTotem& /*unused*/, const Transform& at) { consider(entity, at); });
		break;
	default:
		// The villagers and the animals are always looked at
		break;
	}
}

void mind_town_help::Situate(const ecs::Registry& registry, entt::entity creature,
                             const creature_plan_actions::Executor& executor, std::optional<uint32_t> object,
                             creature_plan_actions::Situation& situation)
{
	if (!IsTownHelp(executor.build))
	{
		return;
	}
	situation.height = object_measures::Height(registry, creature);
	situation.radius = object_measures::TwoDRadius(registry, creature);
	if (Locator::creatureObjectActionSystem::has_value())
	{
		const auto& hands = Locator::creatureObjectActionSystem::value();
		if (const auto held = hands.GetHeld(creature))
		{
			const auto* animal = registry.TryGet<const Animal>(*held);
			situation.holdingCow = animal != nullptr && creature_town_help::IsCowKind(animal->type);
			situation.holdingVillager = registry.AllOf<Villager>(*held);
		}
	}
	const auto thing = object.has_value() ? static_cast<entt::entity>(*object) : entt::null;
	if (!registry.Valid(thing))
	{
		return;
	}
	if (executor.build == Build::PutFieldFoodBySite)
	{
		// Where the site keeps its food: its food pot; none without one
		if (const auto* site = registry.TryGet<const WorshipSite>(thing))
		{
			situation.siteFood = GroundPointOf(registry, site->foodPot);
		}
	}
	if (executor.build == Build::TakeVillagerHome)
	{
		if (const auto* villager = registry.TryGet<const Villager>(thing);
		    villager != nullptr && registry.Valid(villager->abode))
		{
			situation.villagerHome = entt::to_integral(villager->abode);
		}
	}
}

std::optional<float> mind_town_help::TotemShareOf(const ecs::Registry& registry, const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size())
	{
		return std::nullopt;
	}
	const auto& step = idle.agenda[idle.step];
	if ((step.kind != creature_mind::Step::Kind::RaiseTotem && step.kind != creature_mind::Step::Kind::LowerTotem) ||
	    !step.object.has_value())
	{
		return std::nullopt;
	}
	// a puzzle totem: the share of its highest it stands at, its own
	const auto thing = static_cast<entt::entity>(*step.object);
	if (const auto* puzzle = registry.Valid(thing) ? registry.TryGet<const PuzzleTotem>(thing) : nullptr; puzzle != nullptr)
	{
		return ecs::puzzle_totem::ShareAsRead(*puzzle);
	}
	const auto town = TownOfTotem(registry, thing);
	if (town == entt::null || WorshipSiteOf(registry, town) == entt::null)
	{
		return std::nullopt;
	}
	return worship::percentage::GetWorshipPercentage(town);
}

void mind_town_help::Apply(const ecs::Registry& registry, const creature_mind::Commands& commands)
{
	// a puzzle totem takes the share as its height, in whole steps, and stands that high; a village totem's share is its
	// town's
	auto* puzzle =
	    commands.totem.has_value() && registry.Valid(static_cast<entt::entity>(commands.totem->totem))
	        ? Locator::entitiesRegistry::value().TryGet<PuzzleTotem>(static_cast<entt::entity>(commands.totem->totem))
	        : nullptr;
	if (puzzle != nullptr)
	{
		ecs::puzzle_totem::SetShare(*puzzle, commands.totem->share);
		ecs::archetypes::PuzzleTotemArchetype::Place(static_cast<entt::entity>(commands.totem->totem));
	}
	else if (commands.totem.has_value() && Locator::villageTotemSystem::has_value())
	{
		if (const auto town = TownOfTotem(registry, static_cast<entt::entity>(commands.totem->totem)); town != entt::null)
		{
			Locator::villageTotemSystem::value().SetTownShare(town, commands.totem->share);
		}
	}
	if (commands.makeDisciple.has_value())
	{
		const auto villager = static_cast<entt::entity>(*commands.makeDisciple);
		if (registry.Valid(villager) && registry.AllOf<Villager>(villager))
		{
			static_cast<void>(ecs::villager::SetVillagerDisciple(villager, entt::null, VillagerDisciple::Breeder, 0));
		}
	}
}
