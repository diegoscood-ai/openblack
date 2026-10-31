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
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureDeeds.h"
#include "ECS/Abodes.h"
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
#include "ECS/Footpaths.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectImpressiveness.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/RelaxationRules.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
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
namespace relaxation_rules = openblack::ecs::relaxation_rules;

namespace
{
/// The land balance a script may set for how fast artefacts gain worth
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

/// A town's adults and children, as last counted
uint32_t PopulationOf(const Town& town)
{
	return town.stats.adults + town.stats.children;
}

/// The town's football pitch, its newest when it has several; none without one
entt::entity FootballPitchOf(entt::entity town)
{
	const auto abodes = ecs::town_stats::AbodesOf(town);
	const auto found =
	    std::ranges::find_if(abodes, [](entt::entity abode) { return ecs::abodes::TypeOf(abode) == AbodeType::FootballPitch; });
	return found != abodes.end() ? *found : entt::null;
}

bool IsTown(entt::entity town)
{
	return town != entt::null && Entities().Valid(town) && Entities().AllOf<Town>(town);
}
} // namespace

ArtefactSystem::~ArtefactSystem()
{
	StopListening();
}

void ArtefactSystem::Listen()
{
	auto& registry = Entities();
	registry.OnDestroy<TownArtefact>().connect<&ArtefactSystem::OnDestroyed>(*this);
	_listening = &registry;
}

void ArtefactSystem::StopListening()
{
	// (openblack) only from the registry it was connected to, while that registry is still the game's
	if (_listening != nullptr && Locator::entitiesRegistry::has_value() && &Entities() == _listening)
	{
		_listening->OnDestroy<TownArtefact>().disconnect<&ArtefactSystem::OnDestroyed>(*this);
	}
	_listening = nullptr;
}

void ArtefactSystem::ClearMap()
{
	StopListening();
	_gone.clear();
}

void ArtefactSystem::OnDestroyed(entt::registry& registry, entt::entity object)
{
	// Only its town's turn of its artefacts looks at it again, and lets it go; one in no town's artefacts is never
	// looked at again, its dance and its glow ending by themselves once its object has gone
	if (const auto& artefact = registry.get<TownArtefact>(object); artefact.inTown)
	{
		_gone.push_back(artefact);
	}
}

TownArtefact& ArtefactSystem::Record(entt::entity object, PlayerNames player)
{
	auto& registry = Entities();
	// (openblack) its object's deletion is heard, as the original's artefact outlives its object until its town's turn
	Listen();
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

void ArtefactSystem::AddToNoTown(entt::entity object, PlayerNames player)
{
	// in no town's artefacts and no site's, and no deed for the creature to see
	static_cast<void>(Record(object, player));
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

void ArtefactSystem::Delete(TownArtefact& artefact)
{
	Remove(artefact);
	// Its dance goes, unless it has gone already, each of its dancers leaving it and going back to its life
	if (artefact.dance != entt::null && ecs::IsAvailable(artefact.dance) && Locator::danceSystem::has_value())
	{
		Locator::danceSystem::value().Destroy(artefact.dance);
	}
	artefact.dance = entt::null;
	// and its glow at once, not faded out
	if (artefact.effect != entt::null && Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().DeleteSpotVisual(artefact.effect);
	}
	artefact.effect = entt::null;
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
	// Those whose objects have gone since are among them, newest first, and go in their turn
	std::vector<TownArtefact> gone;
	for (auto it = _gone.begin(); it != _gone.end();)
	{
		if (it->town == town)
		{
			gone.push_back(*it);
			it = _gone.erase(it);
		}
		else
		{
			++it;
		}
	}
	std::ranges::sort(gone, std::greater {}, &TownArtefact::joined);
	auto next = gone.begin();
	for (const auto object : ArtefactsOf(town))
	{
		if (registry.Valid(object) && registry.AllOf<TownArtefact>(object))
		{
			auto& artefact = registry.Get<TownArtefact>(object);
			for (; next != gone.end() && next->joined > artefact.joined; ++next)
			{
				Delete(*next);
			}
			Process(object, artefact);
		}
	}
	for (; next != gone.end(); ++next)
	{
		Delete(*next);
	}
}

void ArtefactSystem::Process(entt::entity object, TownArtefact& artefact)
{
	// One whose object is to go goes itself, and nothing else of its turn is done
	if (!ecs::IsAvailable(object))
	{
		Delete(artefact);
		return;
	}
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
	    .impressiveness = ecs::object::ImpressiveValue(object),
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
	// The town's football pitch is weighed first. Its player's creature comes next, but a creature never draws anyone.
	std::optional<float> football;
	if (FootballPitchOf(town) != entt::null)
	{
		// TODO(football): openblack plays no football, so no pitch has had a game or has teams yet
		football = relaxation_rules::FootballDesire(ecs::villager::GetAge(villager), relaxation_rules::FootballGame {}, Turn(),
		                                            [](float range) { return game_random::GameFloatRand(range); });
	}
	// Then the artefacts, newest first, a later one taking over only when wanted strictly more
	const auto artefacts = ArtefactsOf(town);
	std::vector<float> desires;
	desires.reserve(artefacts.size());
	for (const auto object : artefacts)
	{
		desires.push_back(JoiningDesire(object, Entities().Get<const TownArtefact>(object), villager));
	}
	const auto chosen = relaxation_rules::Choose(football, desires);
	switch (chosen.choice)
	{
	case relaxation_rules::Choice::Artefact:
	{
		const auto object = artefacts.at(chosen.index);
		return JoinDance(object, Entities().Get<TownArtefact>(object), villager);
	}
	case relaxation_rules::Choice::Football:
		// TODO(football): the villager would join the pitch's game, or do nothing when the town has no ball; openblack
		// plays no football, so it does nothing
	case relaxation_rules::Choice::None:
		break;
	}
	return false;
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
		artefact.dance = Locator::danceSystem::value().Create(type, place, object, info.duration, false, artefact.town);
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
		// and a footpath is laid to it from the storage pit of the town nearest its new place, however far
		if (const auto town = ecs::map_cells::GetNearestTown(place, std::numeric_limits<float>::max()); town != entt::null)
		{
			if (const auto pit = ecs::town_queries::GetStoragePit(town); pit != entt::null)
			{
				ecs::footpaths::GenerateRoutesBetween(pit, artefact.dance);
			}
		}
	}
	if (!ecs::dances::AddDancer(registry, artefact.dance, villager, ecs::dances::DanceSex(registry, villager)))
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
	// The glow's strength is what it is worth; its magnitude stays the 1 it was made with
	Locator::particleSystem::value().SetSpotVisualStrength(artefact.effect, artefact.value);
}

void ArtefactSystem::CreateEffectIfNecessary(entt::entity object, TownArtefact& artefact)
{
	if (object == entt::null || artefact.effect != entt::null || !rules::Glows(artefact.value))
	{
		return;
	}
	if (Locator::particleSystem::has_value())
	{
		// It lasts for good and goes with its object
		auto& particles = Locator::particleSystem::value();
		artefact.effect = particles.StartSpotVisual(SpotVisualType::MagicFxOnObject, PositionOf(object), std::nullopt, object);
		if (artefact.effect != entt::null)
		{
			// It acts on its object: the table's entry doesn't give its effect its owner, so the artefact gives itself
			const auto entry = static_cast<size_t>(SpotVisualType::MagicFxOnObject);
			const bool ownerGiven = Locator::infoConstants::has_value() &&
			                        Locator::infoConstants::value().spotVisual.at(entry).targetOwnerObject == 1;
			if (!ownerGiven)
			{
				particles.AddSpotVisualTargetObject(artefact.effect, object);
			}
			// in its owner's colours
			if (artefact.player.has_value())
			{
				particles.SetSpotVisualPlayer(artefact.effect, static_cast<int>(*artefact.player));
			}
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
	// those whose objects have gone are counted until their town's turn lets them go
	count += static_cast<uint32_t>(
	    std::ranges::count_if(_gone, [player](const TownArtefact& artefact) { return artefact.player == player; }));
	return count;
}

uint32_t ArtefactSystem::CountGone() const
{
	return static_cast<uint32_t>(_gone.size());
}

float ArtefactSystem::ImpressiveModifier(entt::entity object) const
{
	const auto* artefact = Locator::entitiesRegistry::has_value() && Entities().Valid(object)
	                           ? Entities().TryGet<const TownArtefact>(object)
	                           : nullptr;
	return artefact != nullptr ? rules::ImpressiveModifier(artefact->value) : 1.0f;
}
