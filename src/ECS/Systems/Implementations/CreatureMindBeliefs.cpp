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
// each turn. Only whether a thing is new to it is kept: what it thinks of a thing is read from the world when needed. A
// thing seen again stirs some of its desires, and a villager seen again may show it a skill to learn.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <limits>
#include <optional>
#include <tuple>
#include <utility>

#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Common/GameDistance.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureExplorationMap.h"
#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/CreatureSteal.h"
#include "Creature/CreatureWatching.h"
#include "Creature/PerceivedDesires.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFriends.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/LivingAction.h"
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
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/CreaturePose.h"
#include "ECS/CreatureStruck.h"
#include "ECS/Fields.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Placed.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/TownDesire.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerStateInfo.h"
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
/// How far from a tree in no forest the forest it is taken to belong to may be
constexpr float k_ForestSearchRadius = 1000.0f;

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
	if (registry.AnyOf<Town, Flock, Forest, BigForest, ecs::components::Spell, Mist>(thing))
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
	// A worship site and the altar at its head are parts of their player's temple
	if (registry.AnyOf<WorshipSite, WorshipTotem>(thing))
	{
		return kinds::k_CitadelPart;
	}
	// A field is a field, though it is an abode too
	if (registry.AnyOf<Field, FishFarm>(thing))
	{
		return kinds::k_Field;
	}
	if (const auto* abode = registry.TryGet<const Abode>(thing))
	{
		// A football pitch is a feature
		return abode->type == AbodeNumber::FootballPitch ? kinds::k_Feature : kinds::k_Abode;
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

std::optional<entt::entity> mind_detail::TempleOfPart(const ecs::Registry& registry, entt::entity part)
{
	auto site = part;
	if (const auto* altar = registry.TryGet<const WorshipTotem>(part))
	{
		site = altar->site;
	}
	const auto* worship = registry.Valid(site) ? registry.TryGet<const WorshipSite>(site) : nullptr;
	if (worship == nullptr || !registry.Valid(worship->citadel) || !registry.AllOf<Temple>(worship->citadel))
	{
		return std::nullopt;
	}
	return worship->citadel;
}

std::optional<entt::entity> CreatureMindSystem::BelongsTo(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	// A town or a flock that is about to be deleted is no whole thing a new part is taken to belong to
	const auto available = [](entt::entity whole) { return ecs::IsAvailable(whole) ? std::optional(whole) : std::nullopt; };
	if (registry.AllOf<Creature>(thing))
	{
		return thing;
	}
	// A worship site is a part of its temple, and so is the altar at its head, whether the temple is about to be
	// deleted or not
	if (registry.AnyOf<WorshipSite, WorshipTotem>(thing))
	{
		return mind_detail::TempleOfPart(registry, thing);
	}
	if (const auto* villager = registry.TryGet<const Villager>(thing); villager != nullptr && villager->town != entt::null)
	{
		return available(villager->town);
	}
	// A field is a part of its town, whether the town is about to be deleted or not, though the field is an abode too
	if (registry.AllOf<Field>(thing))
	{
		const auto town = ecs::fields::TownOf(thing);
		return town != entt::null ? std::optional(town) : std::nullopt;
	}
	// and so is a fish farm, which is believed in as a field
	if (const auto* farm = registry.TryGet<const FishFarm>(thing))
	{
		return registry.Valid(farm->town) ? std::optional(farm->town) : std::nullopt;
	}
	const auto& towns = registry.Context().towns;
	// A football pitch is believed in as a feature, which belongs to nothing
	if (const auto* abode = registry.TryGet<const Abode>(thing); abode != nullptr && abode->type != AbodeNumber::FootballPitch)
	{
		const auto town = towns.find(abode->townId);
		return town != towns.end() ? available(town->second) : std::nullopt;
	}
	// A town's totem statue is a part of the town of the town centre it stands on
	if (const auto* totem = registry.TryGet<const VillageTotem>(thing))
	{
		const auto* centre = registry.Valid(totem->townCentre) ? registry.TryGet<const Abode>(totem->townCentre) : nullptr;
		const auto town = centre != nullptr ? towns.find(centre->townId) : towns.end();
		return town != towns.end() ? available(town->second) : std::nullopt;
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing); animal != nullptr && animal->flock != entt::null)
	{
		return available(animal->flock);
	}
	// A tree is a part of its forest, or else of the closest forest with trees within the belief's search radius,
	// while that forest is available
	if (registry.AllOf<Tree>(thing) && Locator::forestSystem::has_value())
	{
		auto& forests = Locator::forestSystem::value();
		auto key = forests.ForestOf(thing);
		if (!key.has_value())
		{
			const auto* transform = registry.TryGet<const Transform>(thing);
			key = transform != nullptr ? forests.FindClosestForest(transform->position, k_ForestSearchRadius) : std::nullopt;
		}
		const auto found = key.has_value() ? forests.All().find(*key) : forests.All().end();
		if (found != forests.All().end() && registry.Valid(found->second.entity))
		{
			return available(found->second.entity);
		}
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
	// A new belief takes in all that the creature finds of the thing; the attributes that do not change stay so
	if (added.isNew && added.belief != nullptr)
	{
		added.belief->firstSight = mind_detail::BeliefNowOf(registry, thing, creature);
	}
	// It takes the thing to stand where it sees it now
	if (added.belief != nullptr && transform != nullptr && beliefs::SightingTakesInPlace(added.isNew, *kind))
	{
		added.belief->place = ecs::object::MapCoordsOf(thing);
	}
	// A thing it knows already is seen again
	if (!added.isNew)
	{
		if (added.belief != nullptr)
		{
			StirSourcesOnSighting(creature, mind, *added.belief);
		}
		if (*kind == kinds::k_Villager)
		{
			WatchVillager(creature, mind, thing);
		}
		if (*kind == kinds::k_Creature && added.belief != nullptr)
		{
			NoteAttentionOf(creature, mind, thing);
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
		// and takes the thing to belong to it, once it holds a belief about it
		auto* belief = mind.beliefs.Find(thing);
		if (belief != nullptr && mind.beliefs.Find(*owner) != nullptr)
		{
			belief->belongsTo = *owner;
		}
	}
}

void CreatureMindSystem::UpdateLongRangeVision(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr)
	{
		return;
	}
	const auto coords = ecs::object::MapCoordsOf(creature);
	const auto otherTown = ecs::map_cells::GetNearestTownCells(coords, mind->longRangeVision.town, std::nullopt).town;
	const auto citadel = ecs::map_cells::GetNearestCitadel(coords, std::numeric_limits<float>::max());
	const auto sighting = beliefs::TakeInFromAfar(mind->longRangeVision, otherTown, citadel);
	if (sighting.town != entt::null)
	{
		ComeAcross(creature, *mind, sighting.town);
		if (const auto pit = ecs::town_queries::GetStoragePit(sighting.town); pit != entt::null)
		{
			ComeAcross(creature, *mind, pit);
		}
	}
	// openblack's temple is the citadel and its heart in one, so the one belief stands for both the game takes in
	if (sighting.citadel != entt::null)
	{
		ComeAcross(creature, *mind, sighting.citadel);
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
	// A thing it could steal stirs its wish to steal, at every sighting
	if (mind_detail::CanSteal(registry, creature, belief.thing))
	{
		creature_desires::ChangeSource(desires, creature_steal::k_StealSource, creature_steal::k_StealableSightingAmount);
	}
	// A town that wants something stirs its compassion
	if (belief.kind == kinds::k_Town &&
	    beliefs::TownSightingStirs(ecs::script_held::IsControlledByScript(creature),
	                               ecs::town_desire::GetMostSignificantRawDesire(belief.thing, 0.0f).has_value()))
	{
		const auto msPerTurn = game_clock::MsPerTurn();
		creature_desires::ChangeSource(desires, beliefs::k_TownSightingSource,
		                               beliefs::TownSightingAmount(msPerTurn != 0 ? 1000u / msPerTurn : 0u));
	}
}

void CreatureMindSystem::NoteAttentionOf(entt::entity creature, CreatureMindState& mind, entt::entity other)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto* otherMind = registry.TryGet<const CreatureMindState>(other);
	if (tables == nullptr || otherMind == nullptr || !mind.learnt.has_value())
	{
		return;
	}
	// What the other creature believes about this one, if it knows it at all: knowing nothing of it, a part of its plan
	// or a step left that uses nothing counts as using it
	const std::optional<uint32_t> known =
	    otherMind->beliefs.Find(creature) != nullptr ? std::optional<uint32_t>(entt::to_integral(creature)) : std::nullopt;
	const std::optional<creature_planner::Plan> plan =
	    otherMind->planActive ? otherMind->planner.current : std::optional<creature_planner::Plan> {};
	const auto use = creature_mind::AgendaUseOf({
	    .plan = plan,
	    .searchesForGoal = plan.has_value() && tables->rules.at(static_cast<size_t>(plan->desire)).searchesForGoal,
	    .self = entt::to_integral(other),
	    .agenda = otherMind->idle.agenda,
	    .step = otherMind->idle.step,
	});
	if (creature_friendship::AttentionRises(ecs::script_held::IsControlledByScript(creature), other == creature,
	                                        creature_friendship::AgendaUses(use, known)))
	{
		creature_friendship::ChangeAttention(creature_learning::AttitudeTo(mind.learnt->creatures, entt::to_integral(other)),
		                                     creature_friendship::k_AttentionStep);
	}
}

void CreatureMindSystem::WatchVillager(entt::entity creature, CreatureMindState& mind, entt::entity villager)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto planDesire = mind.planner.current.has_value() ? std::optional(mind.planner.current->desire) : std::nullopt;
	if (!creature_watching::WatchesVillagers(mind.developmentPhase, ecs::script_held::IsControlledByScript(creature),
	                                         planDesire))
	{
		return;
	}
	WatchVillagerSkill(creature, mind, villager);
	// A villager of another player's town, or of a neutral one, makes it wish more to impress the villagers
	const auto* person = registry.TryGet<const Villager>(villager);
	const auto* town = person != nullptr && registry.Valid(person->town) ? registry.TryGet<const Town>(person->town) : nullptr;
	const auto* body = registry.TryGet<const Creature>(creature);
	if (town != nullptr && body != nullptr && mind.desires.has_value() &&
	    creature_watching::RaisesWishToImpress(town->owner, body->owner))
	{
		creature_desires::SeeVillagerToImpress(*mind.desires);
	}
}

void CreatureMindSystem::WatchVillagerSkill(entt::entity creature, const CreatureMindState& mind, entt::entity villager)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* at = registry.TryGet<const Transform>(villager);
	if (at == nullptr || !mind.learnt.has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	// How far the villager is from where its walk goes, only asked of one on its way to the storage pit for food
	const auto* walk = registry.TryGet<const WallHug>(villager);
	const float toGoal = walk != nullptr
	                         ? game_math::GetDistanceInMetres(map_coords::FromMetres(glm::vec2(at->position.x, at->position.z)),
	                                                          map_coords::FromMetres(walk->goal))
	                         : 0.0f;
	using Index = LivingAction::Index;
	const auto state = creature_watching::StateShown(ecs::villager::GetState(villager, Index::Top),
	                                                 ecs::villager::GetState(villager, Index::Final), toGoal);
	const auto skill = ecs::villager::state_info::StateInfo(state).field0xe0;
	if (skill == static_cast<uint32_t>(CreatureActionKnownAbout::None) ||
	    creature_watching::Knows(mind.learnt->knowledge, static_cast<uint32_t>(CreatureActionLearningType::Normal), skill))
	{
		return;
	}
	// A readout for the log (the game channel at debug): where the creature and the villager are, the state the villager
	// shows and that state's skill
	if (const auto logger = spdlog::get("game"))
	{
		const auto* from = registry.TryGet<const Transform>(creature);
		const auto here = from != nullptr ? from->position : at->position;
		logger->debug("Creature {} at ({:.1f}, {:.1f}) watches villager {} at ({:.1f}, {:.1f}), {:.1f} m away, in state {}: "
		              "skill {}",
		              entt::to_integral(creature), here.x, here.z, entt::to_integral(villager), at->position.x, at->position.z,
		              std::hypot(here.x - at->position.x, here.z - at->position.z), static_cast<int>(state), skill);
	}
	// It watches the skill, even with its mind stilled (which learns nothing), and goes to follow the villager to look
	WatchSkill(creature, skill);
	std::ignore = ForceActivity(
	    creature, {.desire = creature_desires::Desire::Curiosity, .action = "ExamineByFollowing", .actionObject = villager});
}

namespace
{
/// How much things, mountains and the sea catch the creature's eye: its personality's three scales
creature_look::Weights LookWeightsOf(const CreatureMindState& mind)
{
	return {.things = mind.personality[6], .mountains = mind.personality[7], .water = mind.personality[8]};
}

/// How far the creature's weighing of what it looks at reaches, for the size it is shown at
float LookRangeOf(const ecs::Registry& registry, entt::entity creature)
{
	return creature_look::LookRange(creature_morph::ClampScale(ecs::creature_pose::DrawnSize(registry, creature)));
}

/// The game's whole turns a second
uint32_t TurnsPerSecond()
{
	return 1000 / std::max<uint32_t>(game_clock::MsPerTurn(), 1);
}

/// The place on the map of what the creature watches, if any
std::optional<map_coords::MapCoords> WatchedPlace(const ecs::Registry& registry, const creature_look::Target& look)
{
	if (look.place.has_value())
	{
		return map_coords::FromMetres(*look.place);
	}
	if (!look.id.has_value() || !registry.Valid(static_cast<entt::entity>(*look.id)))
	{
		return std::nullopt;
	}
	return ecs::object::MapCoordsOf(static_cast<entt::entity>(*look.id));
}

/// How far what it watches is from where it stands (nothing when it watches nothing)
float WatchedDistance(const ecs::Registry& registry, const creature_look::Target& look, const map_coords::MapCoords& from)
{
	const auto place = WatchedPlace(registry, look);
	return place.has_value() ? game_math::GetDistanceInMetres(from, *place) : 0.0f;
}

/// The game angle the creature's body heads at
uint16_t HeadingOf(const ecs::Registry& registry, entt::entity creature, const Transform& transform)
{
	if (const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature); locomotion != nullptr)
	{
		return creature_player_attention::GameAngleOf(creature_locomotion::DirectionOf(locomotion->heading));
	}
	const auto ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	return creature_player_attention::GameAngleOf({ahead.x, ahead.z});
}
} // namespace

void CreatureMindSystem::ConsiderPlacesToLookAt(entt::entity creature, CreatureMindState& mind)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto from = ecs::object::MapCoordsOf(creature);
	const auto range = LookRangeOf(registry, creature);
	const auto weights = LookWeightsOf(mind);
	const glm::vec2 at {map_coords::ToMetres(from.x), map_coords::ToMetres(from.z)};
	// The nearest mountains, then the nearest sea, in a region it did not go into when there is one
	constexpr std::array k_Places {std::pair {creature_exploration::Kind::Hill, creature_look::Sight::Mountains},
	                               std::pair {creature_exploration::Kind::Sea, creature_look::Sight::Water}};
	for (const auto& [region, sight] : k_Places)
	{
		const auto found = ecs::creature_exploration_land::FindPlace(creature, region, at, true);
		if (!found.has_value())
		{
			continue;
		}
		const auto distance = game_math::GetDistanceInMetres(map_coords::FromMetres(*found), from);
		static_cast<void>(creature_look::ConsiderLand(mind.look, {.sight = sight, .place = *found, .distance = distance},
		                                              WatchedDistance(registry, mind.look, from), range, weights,
		                                              TurnsPerSecond()));
	}
}

void CreatureMindSystem::ConsiderThingToLookAt(entt::entity creature, CreatureMindState& mind, entt::entity thing, float look,
                                               uint16_t heading, float range)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (thing == creature || !registry.Valid(thing) || !ecs::IsAvailable(thing))
	{
		return;
	}
	const auto from = ecs::object::MapCoordsOf(creature);
	const auto at = ecs::object::MapCoordsOf(thing);
	if (!creature_perceived_desires::CanSeePos(look, from, at) ||
	    !creature_player_attention::IsDirectionClose(from, at, heading))
	{
		return;
	}
	static_cast<void>(creature_look::Consider(mind.look,
	                                          {.id = entt::to_integral(thing),
	                                           .kind = mind_detail::LookInterestOfThing(registry, thing),
	                                           .distance = game_math::GetDistanceInMetres(at, from)},
	                                          WatchedDistance(registry, mind.look, from), range, LookWeightsOf(mind),
	                                          TurnsPerSecond()));
}

void CreatureMindSystem::ForgetGoneThings(entt::entity creature, CreatureMindState& mind)
{
	auto& registry = Locator::entitiesRegistry::value();
	// (a thing deleted counts as gone at once, waiting on the dead list or not)
	mind.beliefs.ForgetIf(
	    [](entt::entity thing) { return creature_object_beliefs::ForgetsBeliefAbout(ecs::IsAvailable(thing)); });
	const auto gone = [&registry](entt::entity thing) { return !registry.Valid(thing); };
	// A friend a script made for it that is gone is no longer its friend
	if (auto* friends = registry.TryGet<CreatureFriends>(creature); friends != nullptr)
	{
		creature_friendship::ForgetGoneFriends(friends->friends, gone);
	}
	// The partner of a joint action that is deleted (no longer available, waiting on the dead list or destroyed) is
	// no longer its partner, as the original's sweep clears the partner then
	if (const auto* link = std::as_const(registry).TryGet<const CreaturePartner>(creature);
	    link != nullptr && link->partner != entt::null && !ecs::IsAvailable(link->partner))
	{
		registry.RemoveState<CreaturePartner>(creature);
	}
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
	const auto heading = HeadingOf(registry, creature, transform);
	const auto range = LookRangeOf(registry, creature);
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
			ConsiderThingToLookAt(creature, mind, thing, look, heading, range);
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
	// What it watches is watched a turn longer while it is there and it sees it ahead, else let go of
	const auto place = WatchedPlace(registry, mind.look);
	const bool there =
	    mind.look.place.has_value() || (mind.look.id.has_value() && ecs::IsAvailable(static_cast<entt::entity>(*mind.look.id)));
	const bool seenAhead = place.has_value() && creature_perceived_desires::CanSeePos(look, from, *place) &&
	                       creature_player_attention::IsDirectionClose(from, *place, heading);
	creature_look::Watch(mind.look, there && seenAhead);
	mind.look.point = mind_detail::LookPointOf(registry, mind.look).value_or(mind.look.point);
}
