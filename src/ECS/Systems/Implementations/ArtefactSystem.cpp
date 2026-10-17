/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ArtefactSystem.h"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureDeeds.h"
#include "ECS/ArtefactRules.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Dances.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownStores.h"
#include "ECS/TownDesire.h"
#include "ECS/Villager/VillagerCore.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "Locator.h"
#include "Worship/WorshipSite.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace rules = openblack::ecs::artefact_rules;

namespace
{
/// The land balances a script may set: how impressive things are, and how fast artefacts gain worth
constexpr size_t k_ImpressivenessBalance = 1;
constexpr size_t k_ArtefactSpeedBalance = 6;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

uint32_t Turn()
{
	return ecs::villager::CurrentTurn();
}

/// The row of the dances' table of a dance, none for one out of it
const GDanceInfo* DanceInfoOf(const Dance& dance)
{
	const auto& table = Locator::infoConstants::value().dance;
	const auto row = static_cast<size_t>(dance.type);
	return dance.type != DanceInfo::None && row < table.size() ? &table.at(row) : nullptr;
}

Dance* DanceOf(const TownArtefact& artefact)
{
	return ecs::dances::IsDance(Entities(), artefact.dance) ? &Entities().Get<Dance>(artefact.dance) : nullptr;
}

glm::vec3 PositionOf(entt::entity thing)
{
	return Entities().Get<const Transform>(thing).position;
}

/// How impressive an object's kind is, by the land's balance of impressiveness
float ImpressivenessOf(entt::entity object)
{
	const auto* info = ecs::fire::traits::InfoOf(object);
	return info != nullptr ? land_balance::Get(k_ImpressivenessBalance) * info->impressiveValue : 0.0f;
}

/// A town's adults and children, as last counted
uint32_t PopulationOf(const Town& town)
{
	return town.stats.adults + town.stats.children;
}

bool IsTown(entt::entity town)
{
	return town != entt::null && Entities().Valid(town) && Entities().AllOf<Town>(town);
}

/// The sexes a dance's group must take for a villager to join it: its own
uint32_t DanceSex(entt::entity villager)
{
	const auto* person = Entities().TryGet<const Villager>(villager);
	if (person == nullptr)
	{
		return DanceGroup::k_AnySex;
	}
	return person->sex == Villager::Sex::FEMALE ? DanceGroup::k_Women : DanceGroup::k_Men;
}
} // namespace

TownArtefact& ArtefactSystem::Record(entt::entity object, PlayerNames player)
{
	auto& registry = Entities();
	if (auto* artefact = registry.TryGet<TownArtefact>(object))
	{
		return *artefact;
	}
	// A new artefact is worth what its kind impresses villagers
	const auto* info = ecs::fire::traits::InfoOf(object);
	auto& artefact = registry.AssignState<TownArtefact>(
	    object, TownArtefact {.player = player, .value = info != nullptr ? info->villagerImpressiveValue : 0.0f});
	CreateEffectIfNecessary(object, artefact);
	return artefact;
}

void ArtefactSystem::AddToTown(entt::entity object, entt::entity town, PlayerNames player)
{
	auto& artefact = Record(object, player);
	artefact.town = town;
	if (!artefact.inTown)
	{
		artefact.inTown = true;
		artefact.joined = ++_joined;
	}
	ecs::creature_mimic::Consider(player, creature_watching::Deed::MakeArtefact, object);
}

void ArtefactSystem::AddToSite(entt::entity object, entt::entity site, PlayerNames player)
{
	auto& artefact = Record(object, player);
	artefact.site = site;
	artefact.inSite = true;
	ecs::creature_mimic::Consider(player, creature_watching::Deed::MakeArtefact, object);
}

void ArtefactSystem::Taken(entt::entity object, PlayerNames player)
{
	auto* artefact = Entities().TryGet<TownArtefact>(object);
	if (artefact == nullptr)
	{
		return;
	}
	Remove(*artefact);
	artefact->player = player;
}

void ArtefactSystem::Remove(TownArtefact& artefact)
{
	// Out of its town and its site: a dance under way sends its dancers back to their lives
	const auto leave = [this, &artefact](bool& member) {
		if (!member)
		{
			return;
		}
		member = false;
		if (const auto* dance = DanceOf(artefact);
		    dance != nullptr && dance->state == Dance::State::Dancing && artefact.dancing)
		{
			SetDancersState(artefact.dance, VillagerStates::DecideWhatToDo);
			artefact.dancing = false;
		}
	};
	leave(artefact.inTown);
	leave(artefact.inSite);
}

std::vector<entt::entity> ArtefactSystem::ArtefactsOf(entt::entity town) const
{
	std::vector<std::pair<uint64_t, entt::entity>> found;
	Entities().Each<const TownArtefact>([&found, town](entt::entity object, const TownArtefact& artefact) {
		if (artefact.inTown && artefact.town == town)
		{
			found.emplace_back(artefact.joined, object);
		}
	});
	std::ranges::sort(found, std::greater {}, &std::pair<uint64_t, entt::entity>::first);
	std::vector<entt::entity> objects;
	objects.reserve(found.size());
	std::ranges::transform(found, std::back_inserter(objects), &std::pair<uint64_t, entt::entity>::second);
	return objects;
}

void ArtefactSystem::ProcessTown(entt::entity town)
{
	auto& registry = Entities();
	if (!Locator::infoConstants::has_value() || !IsTown(town))
	{
		return;
	}
	for (const auto object : ArtefactsOf(town))
	{
		if (registry.Valid(object) && registry.AllOf<TownArtefact>(object))
		{
			Process(object, registry.Get<TownArtefact>(object));
		}
	}
}

void ArtefactSystem::Process(entt::entity object, TownArtefact& artefact)
{
	// A glow that has gone out is forgotten
	if (artefact.effect != entt::null && !Entities().Valid(artefact.effect))
	{
		artefact.effect = entt::null;
	}
	if (auto* dance = DanceOf(artefact))
	{
		if (dance->state == Dance::State::Dancing)
		{
			if (dance->dancers == 0)
			{
				// A dance left with no dancers ends
				dance->state = Dance::State::Stopped;
				artefact.dancing = false;
			}
			else
			{
				GiveBeliefIfNecessary(object, artefact);
				if (!artefact.dancing)
				{
					// The dance has started: every dancer dances
					artefact.dancing = true;
					artefact.danceStarted = Turn();
					SetDancersState(artefact.dance, VillagerStates::ArtifactDance);
				}
				else
				{
					StopDanceIfNecessary(object, artefact, false);
				}
			}
		}
		else if (artefact.dancing)
		{
			// The dance stopped by itself: it ends now
			StopDanceIfNecessary(object, artefact, true);
		}
	}
	else
	{
		artefact.dance = entt::null;
	}
	if (artefact.town != entt::null && !IsTown(artefact.town))
	{
		artefact.town = entt::null;
	}
}

void ArtefactSystem::StopDanceIfNecessary(entt::entity object, TownArtefact& artefact, bool force)
{
	auto* dance = DanceOf(artefact);
	if (dance == nullptr)
	{
		return;
	}
	const bool over = rules::DanceTimeLeft(dance->duration, Turn(), artefact.danceStarted) == 0;
	if (!over && !force)
	{
		return;
	}
	const bool townRelaxes =
	    IsTown(artefact.town) && ecs::town_desire::GetMostWanted(artefact.town) == TownDesireInfo::ForRelaxation;
	const bool backToLife = !townRelaxes || force;
	// A dance in its owner's own town raises its worth
	if (IsTown(artefact.town) && artefact.player == Entities().Get<const Town>(artefact.town).owner)
	{
		const auto& town = Entities().Get<const Town>(artefact.town);
		std::optional<float> belief;
		if (town.owner != PlayerNames::NEUTRAL)
		{
			belief = ecs::town_belief::GetBeliefInPlayer(town.belief, town.owner);
		}
		const auto* info = ecs::fire::traits::InfoOf(object);
		const auto* danceInfo = DanceInfoOf(*dance);
		const float gain = rules::DanceGain({
		    .townModifier = rules::TownModifier(belief),
		    .artefactMultiplier = info != nullptr ? info->artifactMultiplier : 0.0f,
		    .dancers = dance->dancers,
		    .maximumDancers = danceInfo != nullptr ? danceInfo->maximumDancers : 0,
		    .danced = Turn() - artefact.danceStarted,
		    .duration = dance->duration,
		    .artefactSpeed = land_balance::Get(k_ArtefactSpeedBalance),
		});
		SetValue(object, artefact, rules::AddToWorth(artefact.value, gain));
		if (DanceOf(artefact) == nullptr)
		{
			return;
		}
	}
	// While the town most wants to relax, its dancers wait for the next dance; otherwise they go back to their lives
	SetDancersState(artefact.dance, backToLife ? VillagerStates::DecideWhatToDo : VillagerStates::WaitForArtifactDance);
	if (auto* stopped = DanceOf(artefact))
	{
		stopped->state = Dance::State::Stopped;
	}
	artefact.dancing = false;
}

void ArtefactSystem::GiveBeliefIfNecessary(entt::entity object, TownArtefact& artefact)
{
	auto& registry = Entities();
	if (!IsTown(artefact.town) || !artefact.player.has_value() ||
	    *artefact.player == registry.Get<const Town>(artefact.town).owner)
	{
		return;
	}
	const auto& townInfo = Locator::infoConstants::value().town;
	const uint32_t gap = townInfo.numGameTurnsBetweenGettingBeliefForArtifacts;
	auto* gifts = registry.TryGet<TownArtefactGifts>(artefact.town);
	if (gifts == nullptr)
	{
		gifts = &registry.AssignState<TownArtefactGifts>(artefact.town);
	}
	auto& last = gifts->lastGift.at(static_cast<size_t>(*artefact.player));
	const uint32_t random = game_random::GameRand(rules::GiftRandomLimit(gap));
	if (!rules::BeliefGiftDue(Turn() - last, random, gap))
	{
		return;
	}
	last = Turn();
	const auto* dance = DanceOf(artefact);
	const float belief = rules::DancingBelief({
	    .worth = artefact.value,
	    .impressiveness = ImpressivenessOf(object),
	    .dancers = dance != nullptr ? dance->dancers : 0,
	    .population = PopulationOf(registry.Get<const Town>(artefact.town)),
	    .beliefMultiplier = townInfo.multiplierForBeliefFromVillagersDancingAroundArtifact,
	});
	// Its symbol rises from the artefact, and the town's awe may be heard there
	ecs::town_stores::AddToBelief(artefact.town, *artefact.player, belief, object, true,
	                              static_cast<int>(GuidanceAlignment::Good));
}

bool ArtefactSystem::OfferRelaxation(entt::entity town, entt::entity villager)
{
	if (!IsTown(town) || !Locator::infoConstants::has_value())
	{
		return false;
	}
	// TODO(football): the town's football pitch is offered first, and the town's player's creature before the
	// artefacts; openblack has neither yet
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto object : ArtefactsOf(town))
	{
		const float desire = JoiningDesire(object, Entities().Get<const TownArtefact>(object), villager);
		if (desire > best)
		{
			best = desire;
			chosen = object;
		}
	}
	if (best == 0.0f || chosen == entt::null)
	{
		return false;
	}
	return JoinDance(chosen, Entities().Get<TownArtefact>(chosen), villager);
}

float ArtefactSystem::JoiningDesire(entt::entity object, const TownArtefact& artefact, entt::entity villager) const
{
	const float distance = glm::distance(glm::xz(PositionOf(object)), glm::xz(PositionOf(villager)));
	rules::JoiningInputs in {.dancing = artefact.dancing, .distance = distance};
	const auto type = rules::DanceForRadius(ecs::object::Get2DRadius(object));
	const auto& table = Locator::infoConstants::value().dance;
	in.maximumDancers = table.at(static_cast<size_t>(type)).maximumDancers;
	if (const auto* dance = DanceOf(artefact))
	{
		in.dancers = dance->dancers;
		in.running = dance->state != Dance::State::Stopped;
		in.duration = dance->duration;
		in.sinceStart = Turn() - dance->startTurn;
		if (const auto* info = DanceInfoOf(*dance))
		{
			in.maximumDancers = info->maximumDancers;
		}
	}
	return rules::JoiningDesire(in, gutils::GetDistanceModifier(distance, rules::k_JoiningReach));
}

bool ArtefactSystem::JoinDance(entt::entity object, TownArtefact& artefact, entt::entity villager)
{
	auto& registry = Entities();
	const auto position = PositionOf(object);
	const auto place = map_coords::FromMetres(glm::xz(position));
	if (DanceOf(artefact) == nullptr && Locator::danceSystem::has_value())
	{
		// Its dance is chosen by how wide it is, made where it stands, lasting the dance's length
		const auto type = rules::DanceForRadius(ecs::object::Get2DRadius(object));
		const auto& info = Locator::infoConstants::value().dance.at(static_cast<size_t>(type));
		artefact.dance = Locator::danceSystem::value().Create(type, place, object, info.duration, false);
	}
	auto* dance = DanceOf(artefact);
	if (dance == nullptr)
	{
		return false;
	}
	// An artefact moved since its dance was set out takes its dance with it
	if (dance->place.x != place.x || dance->place.z != place.z || dance->place.altitude != place.altitude)
	{
		dance->place = place;
		// TODO(footpaths): footpaths are laid to it from the storage pit of the nearest town; openblack lays none yet
	}
	if (!ecs::dances::AddDancer(registry, artefact.dance, villager, DanceSex(villager)))
	{
		return false;
	}
	static_cast<void>(ecs::villager::SetTopState(villager, VillagerStates::WaitForArtifactDance));
	return true;
}

void ArtefactSystem::SetDancersState(entt::entity dance, VillagerStates state) const
{
	auto& registry = Entities();
	for (const auto dancer : ecs::dances::Dancers(registry, dance))
	{
		if (registry.Valid(dancer) && registry.AllOf<Villager>(dancer))
		{
			static_cast<void>(ecs::villager::SetTopState(dancer, state));
		}
	}
}

void ArtefactSystem::SetValue(entt::entity object, TownArtefact& artefact, float value)
{
	artefact.value = value;
	CreateEffectIfNecessary(object, artefact);
	if (artefact.effect == entt::null || !Locator::particleSystem::has_value())
	{
		return;
	}
	if (!Entities().Valid(artefact.effect))
	{
		artefact.effect = entt::null;
		return;
	}
	// The glow is as strong as it is worth
	Locator::particleSystem::value().SetSpotVisualMagnitude(artefact.effect, artefact.value);
}

void ArtefactSystem::CreateEffectIfNecessary(entt::entity object, TownArtefact& artefact)
{
	if (object == entt::null || artefact.effect != entt::null || !rules::Glows(artefact.value))
	{
		return;
	}
	if (Locator::particleSystem::has_value())
	{
		// It acts on its object (the table's target-owner flag gives it the object), in its owner's colours, for good
		auto& particles = Locator::particleSystem::value();
		artefact.effect = particles.StartSpotVisual(SpotVisualType::MagicFxOnObject, PositionOf(object), std::nullopt, object);
		if (artefact.effect != entt::null && artefact.player.has_value())
		{
			particles.SetSpotVisualPlayer(artefact.effect, static_cast<int>(*artefact.player));
		}
	}
	// Its owner's symbol is made with it, shown once it is worth more
	artefact.symbol = true;
}

void ArtefactSystem::PowerUpSite(entt::entity site, uint32_t dancers)
{
	auto& registry = Entities();
	if (Turn() % rules::k_SitePowerUpTurns != 0 || !registry.Valid(site) || !registry.AllOf<WorshipSite>(site) ||
	    !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& component = registry.Get<const WorshipSite>(site);
	// By how many of its player's people dance at it
	uint32_t population = 0;
	registry.Each<const Town>([&population, &component](const Town& town) {
		if (town.owner == component.player)
		{
			population += PopulationOf(town);
		}
	});
	const float gain = rules::SitePowerUp(land_balance::Get(k_ArtefactSpeedBalance),
	                                      worship::site::InfoOf(component).artifactPowerupMultiplier, dancers, population);
	if (gain == 0.0f)
	{
		return;
	}
	std::vector<entt::entity> artefacts;
	registry.Each<const TownArtefact>([&artefacts, site](entt::entity object, const TownArtefact& artefact) {
		if (artefact.inSite && artefact.site == site)
		{
			artefacts.push_back(object);
		}
	});
	for (const auto object : artefacts)
	{
		auto& artefact = registry.Get<TownArtefact>(object);
		SetValue(object, artefact, rules::AddToWorth(artefact.value, gain));
	}
}

void ArtefactSystem::SetWorth(entt::entity object, float worth)
{
	if (auto* artefact = Entities().TryGet<TownArtefact>(object))
	{
		SetValue(object, *artefact, worth);
	}
}

uint32_t ArtefactSystem::CountFor(PlayerNames player) const
{
	uint32_t count = 0;
	if (!Locator::entitiesRegistry::has_value())
	{
		return count;
	}
	Entities().Each<const TownArtefact>([&count, player](const TownArtefact& artefact) {
		if (artefact.inTown && artefact.player == player)
		{
			++count;
		}
	});
	return count;
}

float ArtefactSystem::ImpressiveModifier(entt::entity object) const
{
	const auto* artefact = Locator::entitiesRegistry::has_value() && Entities().Valid(object)
	                           ? Entities().TryGet<const TownArtefact>(object)
	                           : nullptr;
	return artefact != nullptr ? rules::ImpressiveModifier(artefact->value) : 1.0f;
}
