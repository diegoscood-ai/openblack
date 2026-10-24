/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Which things a creature has come across: each turn its eyes sweep a few of the map cells about it and it comes across
// every thing in the cells it can see, and the thing it holds; a thing it had not come across before leads it on to
// what the thing belongs to (an abode's or villager's town, an animal's flock). Things gone from the world are forgotten
// each turn. Only whether a thing is new to it is kept: what it thinks of a thing is read from the world when needed.

#define LOCATOR_IMPLEMENTATIONS

#include <optional>

#include "3D/MapCoords.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/PerceivedDesires.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreaturePose.h"
#include "ECS/CreatureStruck.h"
#include "ECS/Fields.h"
#include "ECS/Map.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Placed.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerTeleport.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace beliefs = openblack::creature_object_beliefs;
namespace kinds = openblack::creature_tree::belief_types;

namespace
{
/// Whether a thing is a toy to a creature, as the game asks each thing: a ball is, and a static of one of the toys'
/// models
bool IsToy(const ecs::Registry& registry, entt::entity thing)
{
	if (registry.AllOf<Ball>(thing))
	{
		return true;
	}
	const auto* still = registry.TryGet<const MobileStatic>(thing);
	if (still == nullptr || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& statics = Locator::infoConstants::value().mobileStatic;
	const auto row = static_cast<size_t>(still->type);
	return row < statics.size() && ecs::creature_struck::IsToyModel(statics.at(row).meshId);
}

/// The kind its row of the game's tables gives a loose thing of the mobile objects' class: a ball's and a reward
/// chest's rows are of their own tables, a one-shot globe's and a whale's are rows of the mobile objects' table; none
/// without the tables
std::optional<int32_t> MobileObjectRowKind(const ecs::Registry& registry, entt::entity thing)
{
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	const auto rowKind = [&info](MobileObjectInfo row) -> std::optional<int32_t> {
		const auto at = static_cast<size_t>(row);
		return at < info.mobileObject.size() ? std::optional(static_cast<int32_t>(info.mobileObject.at(at).mobileType))
		                                     : std::nullopt;
	};
	if (registry.AllOf<Ball>(thing))
	{
		return static_cast<int32_t>(info.ball.mobileType);
	}
	if (const auto* chest = registry.TryGet<const Reward>(thing))
	{
		const auto at = static_cast<size_t>(chest->type);
		return at < info.reward.size() ? std::optional(static_cast<int32_t>(info.reward.at(at).mobileType)) : std::nullopt;
	}
	if (registry.AllOf<OneOffSpellSeed>(thing))
	{
		return rowKind(MobileObjectInfo::OneOffSpellSeed);
	}
	if (registry.AllOf<Shark>(thing))
	{
		return rowKind(MobileObjectInfo::Whale);
	}
	if (const auto* loose = registry.TryGet<const MobileObject>(thing))
	{
		return rowKind(loose->type);
	}
	return std::nullopt;
}

/// Whether a thing moves, as the game asks each thing: a creature while it is on its way or turning; a villager, an
/// animal or the whale when it is not where its last turn started; a town, a flock, a forest, a spell or a puff of mist
/// never; any other thing when it is not where it was last put (made, put by a script or come to rest in the physics),
/// so that a thing moved any other way goes on moving
bool MovedInItsLastTurn(const ecs::Registry& registry, entt::entity thing)
{
	if (registry.AllOf<Creature>(thing))
	{
		return Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(thing);
	}
	if (registry.AnyOf<Villager, Animal>(thing))
	{
		return ecs::villager_teleport::IsMoving(thing);
	}
	if (const auto* whale = registry.TryGet<const Shark>(thing))
	{
		const auto* transform = registry.TryGet<const Transform>(thing);
		return transform != nullptr && ecs::placed::MovedFrom(whale->turnStart, transform->position);
	}
	if (registry.AnyOf<Town, Flock, Forest, ecs::components::Spell, Mist>(thing))
	{
		return false;
	}
	return ecs::MovedSincePlaced(registry, thing);
}
} // namespace

std::optional<uint32_t> mind_detail::BeliefKindOf(const ecs::Registry& registry, entt::entity thing)
{
	if (registry.AllOf<Temple>(thing))
	{
		// The temple here is one thing: the game's temple and the building that stands for it
		return kinds::k_Citadel;
	}
	if (registry.AllOf<Forest>(thing))
	{
		return kinds::k_Forest;
	}
	if (registry.AllOf<Flock>(thing))
	{
		return kinds::k_Flock;
	}
	if (registry.AllOf<Town>(thing))
	{
		return kinds::k_Town;
	}
	if (registry.AllOf<Creature>(thing))
	{
		return kinds::k_Creature;
	}
	if (registry.AllOf<Villager>(thing))
	{
		return kinds::k_Villager;
	}
	if (registry.AllOf<Animal>(thing))
	{
		return kinds::k_Animal;
	}
	if (registry.AllOf<Tree>(thing))
	{
		return kinds::k_Tree;
	}
	// A worship site is a part of its player's temple
	if (registry.AllOf<WorshipSite>(thing))
	{
		return kinds::k_CitadelPart;
	}
	if (const auto* abode = registry.TryGet<const Abode>(thing))
	{
		// A football pitch is a feature
		return abode->type == AbodeNumber::FootballPitch ? kinds::k_Feature : kinds::k_Abode;
	}
	if (registry.AnyOf<Field, FishFarm>(thing))
	{
		return kinds::k_Field;
	}
	if (registry.AllOf<VillageTotem>(thing))
	{
		return kinds::k_TotemStatue;
	}
	if (registry.AnyOf<Feature, AnimatedStatic, Flowers>(thing))
	{
		return kinds::k_Feature;
	}
	if (registry.AllOf<Pot>(thing))
	{
		return kinds::k_Pot;
	}
	if (registry.AllOf<SpellIcon>(thing))
	{
		return kinds::k_SpellIcon;
	}
	// The things of no kind of their own: a big forest, a street lantern or light, a shield, a script's highlight, and
	// the plain base the game makes of the singing stone base's row
	if (registry.AnyOf<BigForest, StreetLantern, MagicShield, ScriptHighlight>(thing))
	{
		return kinds::k_Other;
	}
	if (const auto* still = registry.TryGet<const MobileStatic>(thing);
	    still != nullptr && still->type == MobileStaticInfo::SingingStoneBase)
	{
		return kinds::k_Other;
	}
	// The statics that can be moved: a rock as a rock, the others as loose things
	if (registry.AnyOf<MobileStatic, DeadTree, Fragment, TeleportStone, LandscapeVortex>(thing))
	{
		return beliefs::MobileStaticBeliefKind(ecs::object_physics::IsRock(thing));
	}
	// The loose things, by the kind their row of the game's tables gives them
	if (registry.AnyOf<MobileObject, Ball, OneOffSpellSeed, Reward, Shark>(thing))
	{
		return beliefs::MobileObjectBeliefKind(MobileObjectRowKind(registry, thing));
	}
	// A miracle's seed and a puff of mist are of one kind
	if (registry.AnyOf<SpellSeed, Mist>(thing))
	{
		return kinds::k_SpellSeed;
	}
	if (registry.AllOf<ecs::components::Spell>(thing))
	{
		return kinds::k_Spell;
	}
	// A town desire flag keeps the game's default kind for a thing on the map
	if (registry.AllOf<ecs::components::TownDesireFlag>(thing))
	{
		return kinds::k_Other;
	}
	return std::nullopt;
}

std::optional<entt::entity> CreatureMindSystem::BelongsTo(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Creature>(thing))
	{
		return thing;
	}
	if (const auto* villager = registry.TryGet<const Villager>(thing); villager != nullptr && villager->town != entt::null)
	{
		return villager->town;
	}
	if (const auto* abode = registry.TryGet<const Abode>(thing))
	{
		const auto& towns = registry.Context().towns;
		if (const auto town = towns.find(abode->townId); town != towns.end())
		{
			return town->second;
		}
		return std::nullopt;
	}
	if (registry.AllOf<Field>(thing))
	{
		const auto town = ecs::fields::TownOf(thing);
		return town != entt::null ? std::optional(town) : std::nullopt;
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing); animal != nullptr && animal->flock != entt::null)
	{
		return animal->flock;
	}
	return std::nullopt;
}

void CreatureMindSystem::ComeAcross(entt::entity creature, CreatureMindState& mind, entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing))
	{
		return;
	}
	// A thing that moves, not itself, is kept as the last moving thing it saw while the one kept counts no more than a
	// half: where it is, how high it stands and when
	const auto* transform = registry.TryGet<const Transform>(thing);
	if (transform != nullptr &&
	    beliefs::KeepsMovingThing(MovedInItsLastTurn(registry, thing), thing == creature, mind.movingSeen.weight))
	{
		const auto& position = transform->position;
		mind.movingSeen = {
		    .at = {map_coords::ToMetres(map_coords::ToFixed(position.x)),
		           map_coords::ToMetres(map_coords::ToFixed(position.z))},
		    .height = ecs::object::GetHeight(thing),
		    .turn = game_clock::Turn(),
		    .weight = beliefs::k_MovingSeenWeight,
		};
	}
	const auto kind = mind_detail::BeliefKindOf(registry, thing);
	if (!kind.has_value())
	{
		return;
	}
	const auto added = mind.beliefs.Add(thing, *kind);
	// A thing it knows already is seen again
	if (!added.isNew)
	{
		if (added.belief != nullptr)
		{
			StirSourcesOnSighting(creature, mind, *added.belief);
		}
		return;
	}
	// A new belief leads it on to what the thing belongs to, but for another creature, which is its own
	if (*kind == kinds::k_Creature)
	{
		return;
	}
	if (const auto owner = BelongsTo(thing); owner.has_value() && *owner != thing)
	{
		ComeAcross(creature, mind, *owner);
	}
}

void CreatureMindSystem::StirSourcesOnSighting(entt::entity creature, CreatureMindState& mind, beliefs::Belief& belief)
{
	// A belief about an abode takes in nothing when its thing is seen again, and stirs nothing
	if (!mind.desires.has_value() || !beliefs::SeeingAgainStirs(belief.kind))
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	auto& desires = *mind.desires;
	// A whole thing it may act on, not itself, stirs the sources whose desires it is useful for, at most once in so many
	// game turns
	const auto turn = game_clock::Turn();
	if (beliefs::SightingStirsSources(belief.kind, belief.thing == creature, turn, belief.sourcesTurn))
	{
		belief.sourcesTurn = turn;
		for (const auto source : beliefs::k_SightingSources)
		{
			const auto desire = creature_desires::DesireOfSource(desires, source);
			if (desire.has_value() && beliefs::UsefulEnoughToStir(ActivityUsefulness(creature, mind, *desire, belief.thing)))
			{
				creature_desires::ChangeSource(desires, source, beliefs::k_SightingSourceAmount);
			}
		}
	}
	// A toy stirs its wish to play, at every sighting
	if (IsToy(registry, belief.thing))
	{
		creature_desires::ChangeSource(desires, beliefs::k_ToySightingSource, beliefs::k_ToySightingAmount);
	}
}

void CreatureMindSystem::ForgetGoneThings(CreatureMindState& mind)
{
	const auto& registry = Locator::entitiesRegistry::value();
	mind.beliefs.ForgetIf([&registry](entt::entity thing) { return !registry.Valid(thing); });
}

void CreatureMindSystem::SweepEyes(entt::entity creature, CreatureMindState& mind, const CreatureAnimation& animation,
                                   const Transform& transform)
{
	if (!Locator::entitiesMap::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& map = Locator::entitiesMap::value();
	const auto from = map_coords::FromMetres(glm::vec2(transform.position.x, transform.position.z));
	const auto centre = map_coords::Cell(from);
	const auto look = creature_perceived_desires::LookYaw(animation.lookAt, transform.position,
	                                                      mind_detail::BodyYaw(registry, creature, transform));
	const auto shownSize = creature_morph::ClampScale(ecs::creature_pose::DrawnSize(registry, creature));
	for (const auto cell : beliefs::NextCells(mind.sweep, centre, shownSize))
	{
		if (!map_coords::InBounds(cell))
		{
			continue;
		}
		// Seen from where it stands towards the cell's corner at the least x and z; its own cell always
		const map_coords::MapCoords corner {
		    .x = cell.x * map_coords::k_FixedPerCell, .z = cell.y * map_coords::k_FixedPerCell, .altitude = 0.0f};
		if (!creature_perceived_desires::CanSeePos(look, from, corner))
		{
			continue;
		}
		// The things that stay put, then the things that move
		for (const auto thing : map.GetAllInCell(cell))
		{
			ComeAcross(creature, mind, thing);
		}
	}
	// Then the thing it holds
	if (Locator::creatureObjectActionSystem::has_value())
	{
		if (const auto held = Locator::creatureObjectActionSystem::value().GetHeld(creature))
		{
			ComeAcross(creature, mind, *held);
		}
	}
}
