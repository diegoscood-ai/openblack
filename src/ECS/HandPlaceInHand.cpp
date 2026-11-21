/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandPlaceInHand.h"

#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/HandActionPoint.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/ParticleCarriedObjects.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/ToBeDeleted.h"
#include "Hand/HandGrabRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/Objects/MagicFireBall.h"
#include "Magic/Objects/MagicTeleport.h"

using namespace openblack;
using namespace openblack::ecs::components;

bool ecs::hand_place_in_hand::Interactable(entt::entity object)
{
	// Available; an abode also needs to be partly built
	auto& registry = Locator::entitiesRegistry::value();
	const bool available = object != entt::null && ecs::IsAvailable(object);
	const bool abode = available && registry.AllOf<Abode>(object);
	return ecs::hand_action_point::Interactable(available, abode, abode ? abodes::GetPercentBuilt(object) : 1.0f);
}

entt::entity ecs::hand_place_in_hand::SeedToPlaceInHand(entt::entity object, PlayerNames handPlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(object))
	{
		return entt::null;
	}
	if (registry.AllOf<SpellSeed>(object))
	{
		// the seed's own check, with the hand's player
		return magic::seed::ValidForPlaceInHand(object, handPlayer) ? object : entt::null;
	}
	if (registry.AllOf<TeleportStone>(object))
	{
		// the stone's spell's seed, then the seed's own check; none without one
		const auto seed = magic::teleport::SeedOf(object);
		return seed != entt::null && magic::seed::ValidForPlaceInHand(seed, handPlayer) ? seed : entt::null;
	}
	return entt::null;
}

bool ecs::hand_place_in_hand::ValidForPlaceInHand(entt::entity object, PlayerNames handPlayer)
{
	// An allowlist, as the original: an object never goes into the hand unless its class says it may. Buildings, totems,
	// features, animated statics, creatures, temple parts, icons, leash posts and highlights keep that default
	auto& registry = Locator::entitiesRegistry::value();
	const auto* potInfo = ecs::object::PotInfoOf(object);
	const auto rule = hand_grab::PlaceInHandRuleOf({
	    .creature = registry.AllOf<Creature>(object),
	    .villager = registry.AllOf<Villager>(object),
	    .animal = registry.AllOf<Animal>(object),
	    .scaffold = registry.AllOf<Scaffold>(object),
	    .seedOrTeleport = registry.AnyOf<SpellSeed, TeleportStone>(object),
	    .fireBall = registry.AllOf<MagicFireBall>(object),
	    .field = registry.AllOf<Field>(object),
	    .fishFarm = registry.AllOf<FishFarm>(object),
	    .pot = registry.AllOf<Pot>(object),
	    .pile = potInfo != nullptr && potInfo->potType != PotType::Pot,
	    .deadTree = registry.AllOf<DeadTree>(object),
	    .mobileStatic = registry.AllOf<MobileStatic>(object),
	    .rock = object_physics::IsRockObject(object),
	    .bonfire = object_physics::IsBonfire(object),
	    .fragment = registry.AllOf<Fragment>(object),
	    .tree = registry.AllOf<Tree>(object),
	    .bigForest = registry.AllOf<BigForest>(object),
	    .oneOffSpellSeed = registry.AllOf<OneOffSpellSeed>(object),
	    .mobile = registry.AllOf<Mobile>(object),
	});
	switch (rule)
	{
	case hand_grab::PlaceInHandRule::Always:
		return true;
	case hand_grab::PlaceInHandRule::Rock:
		// no wider than 3.6 across the ground
		return object_physics::RockValidForPlaceInHand(object);
	case hand_grab::PlaceInHandRule::Villager:
		return ecs::villager::IsReachable(object);
	case hand_grab::PlaceInHandRule::Animal:
		return ecs::animal_ai::ValidForPlaceInHand(object);
	case hand_grab::PlaceInHandRule::Field:
	{
		const auto& field = registry.Get<const Field>(object);
		return hand_grab::FieldValidForPlaceInHand(field.growth, field.food);
	}
	case hand_grab::PlaceInHandRule::FishFarm:
		// the farm's food value as food, which is its info's full stock rather than what is left in it
		return FishFarm::k_FoodValue > 0.0f;
	case hand_grab::PlaceInHandRule::Pile:
		// only a pile of the hand's own row with something in it: a stored pile, or a magic one, never
		return hand_grab::PileValidForPlaceInHand(
		    potInfo->potType, registry.Get<const Pot>(object).type,
		    ecs::object_resources::GetResource(object, ecs::object_resources::GetResourceType(object)));
	case hand_grab::PlaceInHandRule::Scaffold:
		return ecs::scaffolds::ValidForPlaceInHand(object);
	case hand_grab::PlaceInHandRule::Seed:
		// a spell seed and a magic teleport by their seed
		return SeedToPlaceInHand(object, handPlayer) != entt::null;
	case hand_grab::PlaceInHandRule::FireBall:
		// never the hand's own player's ball
		return magic::fireball::ValidForPlaceInHand(object, handPlayer);
	case hand_grab::PlaceInHandRule::Never:
		break;
	}
	return false;
}

bool ecs::hand_place_in_hand::Passes(entt::entity object, hand_pick_up::PlaceInHandCheck check, bool handEmpty,
                                     PlayerNames handPlayer)
{
	switch (check)
	{
	case hand_pick_up::PlaceInHandCheck::Interactable:
		return Interactable(object);
	case hand_pick_up::PlaceInHandCheck::HandEmpty:
		return handEmpty;
	case hand_pick_up::PlaceInHandCheck::Valid:
		return ValidForPlaceInHand(object, handPlayer);
	case hand_pick_up::PlaceInHandCheck::MayBePickedUp:
		return !ecs::object_flags::IsCannotBePickedUp(object);
	case hand_pick_up::PlaceInHandCheck::NotCarried:
		return !ecs::physics::particle_carried_objects::IsCarried(object);
	}
	return false;
}
