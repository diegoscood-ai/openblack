/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GamePhysicsHooksWorld.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "3D/CreatureBody.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureCatch.h"
#include "Creature/CreatureRig.h"
#include "Creature/LocalPlayer.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreaturePhysics.h"
#include "ECS/CreaturePose.h"
#include "ECS/CreatureStruck.h"
#include "ECS/ObjectFlags.h"
#include "ECS/PlayerAlliances.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicWorldInterface.h"
#include "Resources/ResourcesInterface.h"
#include "Worship/Citadel.h"

using namespace openblack;
using namespace openblack::ecs;

Registry& GamePhysicsHooksWorld::Entities()
{
	return Locator::entitiesRegistry::value();
}

systems::HandSystemInterface& GamePhysicsHooksWorld::HandPart()
{
	return Locator::handSystem::value();
}

systems::BuildingDamageSystemInterface& GamePhysicsHooksWorld::BuildingDamage()
{
	return Locator::buildingDamageSystem::value();
}

PlayerNames GamePhysicsHooksWorld::LocalPlayer() const
{
	return creature::LocalPlayer();
}

bool GamePhysicsHooksWorld::IsToy(entt::entity object) const
{
	// The football, or a static of one of the toys' models
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	std::optional<MeshId> staticModel;
	if (const auto* still = registry.TryGet<const components::MobileStatic>(object);
	    still != nullptr && Locator::infoConstants::has_value())
	{
		const auto& statics = Locator::infoConstants::value().mobileStatic;
		if (const auto row = static_cast<size_t>(still->type); row < statics.size())
		{
			staticModel = statics.at(row).meshId;
		}
	}
	return creature_struck::IsToy(registry.AllOf<components::Ball>(object), staticModel);
}

std::optional<effects::EffectValues> GamePhysicsHooksWorld::Crush() const
{
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& rows = Locator::infoConstants::value().effect;
	return effects::EffectValues::FromEffectInfo(rows.at(creature_struck::k_CrushEffect));
}

void GamePhysicsHooksWorld::ApplyEffect(entt::entity object, const effects::EffectValues& values,
                                        const magic::EffectSource& source)
{
	// Through the miracles' service, as any blow; without it, as the effect alone
	if (Locator::magicSystem::has_value())
	{
		static_cast<void>(Locator::magicSystem::value().ApplyEffectToObject(object, values, source));
		return;
	}
	auto taken = values;
	taken.blow = source.blow;
	effects::ApplyEffect(object, taken);
}

void GamePhysicsHooksWorld::UpdateAttitudeFromFeedback(entt::entity creature, float feedback)
{
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().UpdateAttitudeFromFeedback(creature, feedback);
	}
}

void GamePhysicsHooksWorld::ChangeDesireSource(entt::entity creature, uint32_t source, float amount)
{
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().ChangeDesireSource(creature, source, amount);
	}
}

void GamePhysicsHooksWorld::KickSway(entt::entity creature, glm::vec3 force, glm::vec3 point)
{
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().KickSway(creature, force, point);
	}
}

entt::entity GamePhysicsHooksWorld::HeartPassesBlowTo(entt::entity heart, bool breaksBuildings)
{
	return Locator::templeDestructionSystem::has_value()
	           ? Locator::templeDestructionSystem::value().ActualObjectToEffect(heart, breaksBuildings)
	           : heart;
}

entt::entity GamePhysicsHooksWorld::CitadelPartHeart(entt::entity part)
{
	return openblack::worship::citadel::PartHeart(part);
}

bool GamePhysicsHooksWorld::StrikeBuilding(entt::entity building, PhysicsEntry& entry)
{
	return !Locator::buildingDamageSystem::has_value() || Locator::buildingDamageSystem::value().ReactToImpact(building, entry);
}

namespace
{
/// The game's table row of every action a creature plans that the catch asks about
constexpr std::string_view k_FightAction = "Fight";
constexpr std::string_view k_CatchAction = "Catch";
/// The reaction type the catch asks a creature to be free for
constexpr uint8_t k_CatchReaction = 0;
} // namespace

std::vector<entt::entity> GamePhysicsHooksWorld::Creatures()
{
	std::vector<entt::entity> creatures;
	std::as_const(Locator::entitiesRegistry::value())
	    .Each<const components::Creature, const components::Transform>(
	        [&creatures](entt::entity creature, const auto&...) { creatures.push_back(creature); });
	// (openblack) the game's creature list is in the order the creatures were added to it; here they are asked by
	// their entities' numbers
	std::ranges::sort(creatures, {}, [](entt::entity creature) { return entt::to_integral(creature); });
	return creatures;
}

creature_catch::Eligibility GamePhysicsHooksWorld::CatchEligibility(entt::entity creature)
{
	auto* mind = Locator::creatureMindSystem::has_value() ? &Locator::creatureMindSystem::value() : nullptr;
	return {
	    .heldByHand =
	        Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().GetCreature() == creature,
	    .plansToFight = mind != nullptr && mind->PlansAction(creature, k_FightAction),
	    .fighting = Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().IsFighting(creature),
	    .scriptControlled = ecs::script_held::IsControlledByScript(creature),
	    .availableForReaction = mind != nullptr && mind->IsAvailableForReaction(creature, k_CatchReaction),
	    // No script gives a creature a desire to follow alone in openblack (SET_CREATURE_ONLY_DESIRE is not done)
	    .onlyDesire = false,
	    .plansToCatch = mind != nullptr && mind->PlansAction(creature, k_CatchAction),
	};
}

float GamePhysicsHooksWorld::CreatureWeight(entt::entity creature)
{
	return ecs::creature_physics::Weight(std::as_const(Locator::entitiesRegistry::value()), creature);
}

float GamePhysicsHooksWorld::ThingWeight(entt::entity object)
{
	// As the physics weighs its body: the game's weight of a thing, kept from nothing
	return Locator::dynamicsSystem::has_value() ? Locator::dynamicsSystem::value().WeightOf(object) : 0.0f;
}

bool GamePhysicsHooksWorld::MayBeCaught(entt::entity object)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(object))
	{
		return false;
	}
	const bool allows = !ecs::object_flags::IsCannotBePickedUp(object);
	// A villager not while it is inside its home
	if (registry.AllOf<components::Villager>(object))
	{
		return allows && !registry.AllOf<components::AtHome>(object);
	}
	// The kinds of thing that can be picked up: living things, trees, statics and rocks, their pieces and the mobile
	// objects, the football aside
	if (registry.AnyOf<components::Animal, components::Tree, components::DeadTree, components::Pot, components::Scaffold,
	                   components::Fragment, components::MobileStatic, components::Reward, components::OneOffSpellSeed,
	                   components::LandscapeVortex>(object) ||
	    (registry.AllOf<components::MobileObject>(object) && !registry.AllOf<components::Ball>(object)))
	{
		return allows;
	}
	return false;
}

PlayerNames GamePhysicsHooksWorld::OwnerOf(entt::entity creature)
{
	return std::as_const(Locator::entitiesRegistry::value()).Get<const components::Creature>(creature).owner;
}

bool GamePhysicsHooksWorld::IsAllied(PlayerNames player, PlayerNames other)
{
	return alliances::IsAllied(player, other);
}

uint32_t GamePhysicsHooksWorld::GameRand(uint32_t range)
{
	return game_random::GameRand(range);
}

std::optional<creature_catch::Approach> GamePhysicsHooksWorld::CatchApproach(entt::entity creature)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!Locator::creatureAnimationSystem::has_value() || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& body = registry.Get<const components::Creature>(creature);
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(body.species);
	const auto* rig = rigs.Contains(rigId) ? &*rigs.Handle(rigId) : nullptr;
	auto& animations = Locator::creatureAnimationSystem::value();
	const auto stepMs = animations.AnimationDuration(creature, creature_catch::k_CatchStep);
	const auto travel = animations.AnimationTravel(creature, creature_catch::k_CatchStep);
	if (rig == nullptr || !rig->actionPoints.has_value() || !stepMs.has_value() || !travel.has_value())
	{
		return std::nullopt;
	}
	return creature_catch::Approach {.creature = registry.Get<const components::Transform>(creature).position,
	                                 .size = ecs::creature_pose::DrawnSize(registry, creature),
	                                 .modelScale = ecs::creature_pose::DrawnScale(registry, creature).x,
	                                 .catchMs = rig->actionPoints->catchMs,
	                                 .stepMs = *stepMs,
	                                 .stepTravel = travel->x};
}

void GamePhysicsHooksWorld::SetAboutCatching(entt::entity creature, entt::entity object)
{
	// A walk brakes to a stop; the catch starts once the body stands
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().BrakeToStop(creature);
	}
	if (Locator::creatureMindSystem::has_value())
	{
		// A plan of play to catch the thing, as pressing as a plan the creature chose for itself can be
		static_cast<void>(Locator::creatureMindSystem::value().ForceActivity(
		    creature,
		    {.desire = creature_desires::Desire::Play, .action = k_CatchAction, .actionObject = object, .score = 1.0f}));
	}
}
