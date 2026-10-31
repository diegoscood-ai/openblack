/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownBelief.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Audio/Services/Guidance.h"
#include "Debug/StateHash.h"
#include "ECS/Abodes.h"
#include "ECS/Archetypes/FloatingNumberArchetype.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Events/Publish.h"
#include "ECS/Events/TownBeliefEvents.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/TownStateSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/Wonders.h"
#include "ECS/TownDesire.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "Game/GameStats.h"
#include "GameClock.h"
#include "Help/ToolTips.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Particles/Rules/SurfRevol.h"
#include "Worship/Citadel.h"
#include "Worship/TownCentreSpellIcon.h"
#include "Worship/TownMagic.h"
#include "Worship/WorshipSite.h"

// The town's belief (TownBelief.h). The original computes on stored 32-bit floats: the port keeps its order in float
// and uses double where the original keeps an unstored extended-precision intermediate (approximate: double is not
// 80-bit, the products of two floats are exact in both)

namespace openblack::ecs::town_belief
{
using components::Town;
using components::TownBelief;

namespace
{
/// The neutral player is always slot 7
constexpr PlayerNames k_Neutral = PlayerNames::NEUTRAL;
/// The local player. (inferred) PLAYER_ONE, as TownDesire's isLocalPlayer
constexpr PlayerNames k_LocalPlayer = PlayerNames::PLAYER_ONE;
/// The take-over sound, a sample of the in-game bank
constexpr uint32_t k_TakeOverSample = 205;

/// The belief-sprite queue (Locator::townStateSystem)
std::vector<BeliefSprite>& BeliefSprites()
{
	if (!Locator::townStateSystem::has_value())
	{
		std::fputs("ecs::town_belief: no townStateSystem in the locator (Locator::townStateSystem)\n", stderr);
		std::abort();
	}
	return Locator::townStateSystem::value().BeliefSprites();
}

constexpr size_t Slot(PlayerNames player)
{
	return static_cast<size_t>(player);
}

constexpr bool InRange(PlayerNames player)
{
	return static_cast<size_t>(player) < k_Players;
}

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Town* TownOf(entt::entity town)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	return town != entt::null && registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
}

bool Valid(entt::entity object)
{
	return object != entt::null && Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Valid(object);
}

/// f x 10000 truncated toward 0 (the product of a float and 10000 is exact in a double)
int32_t Amount(float f)
{
	return static_cast<int32_t>(static_cast<double>(f) * k_AmountScale);
}

/// The player's colour from particles::surf_revol's table, at its colour index on the current land
uint32_t PlayerColour(PlayerNames player)
{
	return particles::surf_revol::PlayerColour(static_cast<int>(player));
}

/// The object's position on the ground (map_coords::ToWorld of its MapCoords)
glm::vec3 GroundPoint(entt::entity object)
{
	return map_coords::ToWorld(object::MapCoordsOf(object));
}

void Help(detail::HelpSprite kind, entt::entity town)
{
	events::Publish(events::TownBeliefHelp {.kind = kind, .town = town});
}

/// The game's handler of TownBeliefHelp: the help sprite of the town
void PlayHelp(const events::TownBeliefHelp& event)
{
	const auto kind = event.kind;
	const auto town = event.town;
	switch (kind)
	{
	case detail::HelpSprite::LosingBelief:
		// Played when the help sprite may play now and the town has adults + children -> list 10
		if (const auto* t = TownOf(town); t != nullptr)
		{
			audio::guidance::WarnLosingBelief(t->stats.adults + t->stats.children);
		}
		break;
	case detail::HelpSprite::GeneralBad:
		audio::guidance::RemarkGeneralBad(GroundPoint(town)); // at the town's position
		break;
	case detail::HelpSprite::GeneralGood:
		audio::guidance::RemarkGeneralGood(GroundPoint(town)); // at the town's position
		break;
	}
}

/// ReduceBelief on a town component already found
void Reduce(Town& t, PlayerNames player, float r)
{
	if (!InRange(player))
	{
		return;
	}
	auto& b = t.belief;
	const size_t n = Slot(player);
	b.belief.at(n) -= r; // not clamped: the belief can go below 0
	if (Valid(t.centre))
	{
		b.reduceAccumulator.at(n) += r;
		if (static_cast<double>(b.reduceAccumulator.at(n)) > k_ReduceDrawThreshold)
		{
			// DrawBelief(-acc, town centre, P); a negative amount draws nothing
			DrawBelief(-b.reduceAccumulator.at(n), t.centre, player);
			b.reduceAccumulator.at(n) = 0.0f;
		}
	}
}

} // namespace

/// A town leaving its owner leaves the owner's temple heart too, when the heart has a building site: the town's
/// builders there stop and decide what to do next, and when a site of the town's list is the heart's, the heart's site
/// leaves that list (it is not deleted)
void LeaveHeartSite(PlayerNames owner, entt::entity town)
{
	const auto heart = worship::citadel::HeartOf(worship::citadel::Of(owner));
	if (heart == entt::null)
	{
		return;
	}
	const auto* part = Entities().TryGet<const components::CitadelPartBuild>(heart);
	if (part == nullptr || part->buildingSite == entt::null || !Entities().Valid(part->buildingSite))
	{
		return;
	}
	const auto site = part->buildingSite;
	building_sites::RemoveAllVillagersFromTown(site, town);
	if (building_sites::GetBuildingSiteInList(town, heart) != entt::null)
	{
		building_sites::RemoveBuildingSiteFromList(town, site);
	}
}

void RemoveTownFromPlayer(entt::entity town)
{
	auto* t = TownOf(town);
	if (t == nullptr)
	{
		return;
	}
	t->emptyCountdown = 0;
	const auto owner = t->owner;
	// Every magic type it holds is no longer enabled for its owner
	for (size_t i = 0; i < components::TownMagic::k_MagicTypes; ++i)
	{
		if (const auto type = static_cast<MagicType>(i); worship::town::IsMagicTypeHeld(town, type))
		{
			magic::players::SetMagicTypeEnabled(owner, type, false);
		}
	}
	// It leaves its worship site; the site's building site, when it is in the town's list, is deleted, and the site's
	// builders from the town decide what to do next
	if (const auto* magic = Entities().TryGet<const components::TownMagic>(town);
	    magic != nullptr && magic->worshipSite != entt::null && Entities().Valid(magic->worshipSite))
	{
		const auto site = magic->worshipSite;
		worship::site::RemoveTown(site, town);
		if (building_sites::GetBuildingSiteInList(town, site) != entt::null)
		{
			static_cast<void>(building_sites::RemoveBuildingSite(town, site));
		}
		if (const auto building = abodes::GetBuildingSite(site); building != entt::null && Entities().Valid(building))
		{
			building_sites::RemoveAllVillagersFromTown(building, town);
		}
	}
	// It leaves its owner's temple heart, when that is being built or repaired
	LeaveHeartSite(owner, town);
	// Each of its buildings leaves the owner: a built wonder takes its power off that player's tribe
	for (const auto abode : town_stats::AbodesOf(town))
	{
		wonders::RemoveFromPlayer(abode);
	}
}

// ---- Small methods --------------------------------------------------------------------------------------------------

void ResetDesireThresholds(TownBelief& belief)
{
	// Each desire's desireAffectsBeliefAfter from the town desire info
	if (!Locator::infoConstants::has_value())
	{
		belief.desireThreshold.fill(0.0f); // (openblack) no info: the zeroed allocation
		return;
	}
	const auto& info = Locator::infoConstants::value().townDesire;
	for (size_t d = 0; d < k_Desires; ++d)
	{
		belief.desireThreshold.at(d) = info.at(d).desireAffectsBeliefAfter;
	}
}

void Init(Town& town)
{
	auto& b = town.belief;
	// The 8 slots
	b.belief.fill(0.0f);
	b.cap.fill(k_InitialCap);
	b.pending.fill(0.0f);
	b.reduceAccumulator.fill(0.0f);
	b.lastAddedTurn.fill(0);
	// belief[neutral] = beliefInNeutralPlayer (written raw, no SetBelief)
	b.belief.at(Slot(k_Neutral)) = b.beliefInNeutralPlayer;
	b.boredom.fill(k_InitialBoredom);
	ResetDesireThresholds(b);
}

void SetBelief(TownBelief& belief, PlayerNames player, float value)
{
	if (!InRange(player)) // (openblack) a guard of the array
	{
		return;
	}
	const size_t n = Slot(player);
	// cap < v -> cap
	belief.belief.at(n) = belief.cap.at(n) < value ? belief.cap.at(n) : value;
}

float GetBeliefInPlayer(const TownBelief& belief, PlayerNames player)
{
	return InRange(player) ? belief.belief.at(Slot(player)) : 0.0f;
}

float GetBeliefInPlayer(entt::entity town, PlayerNames player)
{
	const auto* t = TownOf(town);
	return t != nullptr ? GetBeliefInPlayer(t->belief, player) : 0.0f;
}

void SetCap(TownBelief& belief, PlayerNames player, float cap)
{
	if (InRange(player))
	{
		belief.cap.at(Slot(player)) = cap;
	}
}

float GetCap(const TownBelief& belief, PlayerNames player)
{
	return InRange(player) ? belief.cap.at(Slot(player)) : 0.0f;
}

float GetAddedThisPeriod(const TownBelief& belief, PlayerNames player)
{
	return InRange(player) ? belief.addedThisPeriod.at(Slot(player)) : 0.0f;
}

float GetAddedThisPeriodRatio(const TownBelief& belief, PlayerNames player)
{
	if (!InRange(player))
	{
		return 0.0f;
	}
	const float added = belief.addedThisPeriod.at(Slot(player));
	if (added == 0.0f)
	{
		return 0.0f;
	}
	const auto mine = static_cast<double>(belief.belief.at(Slot(player)));
	return static_cast<float>(static_cast<double>(added) / (mine * 10.0));
}

void SetBeliefInPlayer(Town& town, PlayerNames player, float value)
{
	if (player == k_Neutral)
	{
		town.belief.beliefInNeutralPlayer = value;
	}
	SetBelief(town.belief, player, value);
}

void PlayerTakeTownCheat(TownBelief& belief, PlayerNames player, float amount, std::span<const PlayerNames> players)
{
	if (player == k_Neutral)
	{
		for (const auto other : players)
		{
			SetBelief(belief, other, 0.0f);
		}
		return;
	}
	// the one believed in most, strictly more than the ones before it, from 0
	float most = 0.0f;
	std::optional<PlayerNames> believedMost;
	for (const auto other : players)
	{
		if (GetBeliefInPlayer(belief, other) > most)
		{
			most = GetBeliefInPlayer(belief, other);
			believedMost = other;
		}
	}
	if (believedMost.has_value())
	{
		SetBelief(belief, *believedMost, most * 0.5f);
	}
	SetBelief(belief, player, most + amount);
}

std::vector<PlayerNames> PlayersOnLandAndNeutral(std::span<const PlayerNames> onLand)
{
	std::vector<PlayerNames> players;
	for (size_t slot = 0; slot < k_Players; ++slot)
	{
		const auto name = static_cast<PlayerNames>(slot);
		if (name == k_Neutral || std::ranges::find(onLand, name) != onLand.end())
		{
			players.push_back(name);
		}
	}
	return players;
}

void SetBeliefRelativeToOwner(TownBelief& belief, PlayerNames owner, PlayerNames player, float share)
{
	SetBelief(belief, player, GetBeliefInPlayer(belief, owner) * share);
}

void ReduceBelief(entt::entity town, PlayerNames player, float r)
{
	if (auto* t = TownOf(town); t != nullptr)
	{
		Reduce(*t, player, r);
	}
}

void AddToBoredomMultiplier(TownBelief& belief, size_t reaction, float f)
{
	if (reaction >= k_Reactions)
	{
		return;
	}
	auto& value = belief.boredom.at(reaction);
	const float b = value + f;
	value = b < 0.0f ? 0.0f : b; // below 0 -> 0
}

float GetBoredomMultiplier(entt::entity town, size_t reaction)
{
	const auto* t = TownOf(town);
	if (t == nullptr || reaction >= k_Reactions)
	{
		return 1.0f; // no town -> 1.0
	}
	return t->belief.boredom.at(reaction);
}

float GetMaxBeliefMeNotIncluded(const TownBelief& belief, PlayerNames player)
{
	float best = 0.0f;
	for (size_t i = 0; i < k_Players; ++i)
	{
		if (i != Slot(player) && belief.belief.at(i) > best)
		{
			best = belief.belief.at(i);
		}
	}
	return best;
}

float RelativeBelief(const TownBelief& belief, PlayerNames player, PlayerNames thingPlayer)
{
	if (thingPlayer != player)
	{
		return GetBeliefInPlayer(belief, thingPlayer);
	}
	const float mine = GetBeliefInPlayer(belief, player);
	float bestDifference = k_RelativeStart;
	float result = 0.0f;
	for (size_t i = 0; i < k_Players; ++i)
	{
		if (i == Slot(player))
		{
			continue;
		}
		const float difference = belief.belief.at(i) - mine;
		if (difference > bestDifference)
		{
			bestDifference = difference;
			result = belief.belief.at(i);
		}
	}
	return result;
}

float RivalRecentRatio(const TownBelief& belief, PlayerNames player, const std::function<bool(PlayerNames)>& allied)
{
	// The slots 0..5
	constexpr size_t k_RivalSlots = 6;
	float best = k_RivalRecentStart;
	std::optional<size_t> rival;
	for (size_t i = 0; i < k_RivalSlots; ++i)
	{
		if (i == Slot(player) || (allied && allied(static_cast<PlayerNames>(i))))
		{
			continue;
		}
		if (belief.recent.at(i) > best)
		{
			best = belief.recent.at(i);
			rival = i;
		}
	}
	const float mine = GetBeliefInPlayer(belief, player);
	if (!rival.has_value() || mine == 0.0f)
	{
		return 0.0f;
	}
	// x = 2 belief[i] / belief[P]; x^2, at most 1
	const double x = 2.0 * static_cast<double>(belief.belief.at(*rival)) / static_cast<double>(mine);
	const double squared = x * x;
	return squared < 1.0 ? static_cast<float>(squared) : 1.0f;
}

float BeliefRatioVsRivals(const TownBelief& belief, PlayerNames player, const std::function<bool(PlayerNames)>& allied)
{
	float best = 0.0f;
	for (size_t i = 0; i < k_Players; ++i)
	{
		const auto other = static_cast<PlayerNames>(i);
		if (other == player || other == k_Neutral || (allied && allied(other)))
		{
			continue;
		}
		if (belief.belief.at(i) > best)
		{
			best = belief.belief.at(i);
		}
	}
	const float mine = GetBeliefInPlayer(belief, player);
	return mine == 0.0f ? 0.0f : static_cast<float>(static_cast<double>(best) / static_cast<double>(mine));
}

float LostTownScale()
{
	return land_balance::LostTownScale();
}

// ---- The fold -------------------------------------------------------------------------------------------------------

void Fold(entt::entity town)
{
	auto* t = TownOf(town);
	if (t == nullptr || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& info = Locator::infoConstants::value();
	auto& b = t->belief;
	const PlayerNames owner = t->owner;
	const float lost = LostTownScale();

	// 1. pending -> belief, every player
	for (size_t i = 0; i < k_Players; ++i)
	{
		const float d = b.beliefScale * b.pending.at(i);
		if (d == 0.0f || std::isnan(d)) // skipped, and pending[i] is NOT cleared
		{
			continue;
		}
		b.addedThisPeriod.at(i) += d;
		if (Valid(t->centre))
		{
			// A belief sprite at the town centre: d x 10000 (truncated) in player i's colour
			QueueBeliefSprite(GroundPoint(t->centre), Amount(d), PlayerColour(static_cast<PlayerNames>(i)));
		}
		ResetDesireThresholds(b);                                      // whoever the player
		SetBelief(b, static_cast<PlayerNames>(i), b.belief.at(i) + d); // capped
		b.pending.at(i) = 0.0f;
	}

	// 2. the boredom of every reaction: x = lost-town scale x the reaction's boredom addition + boredom[k]
	for (size_t k = 0; k < k_Reactions; ++k)
	{
		const double x = static_cast<double>(lost) * static_cast<double>(info.reaction.at(k).additionToTownBoredomMultipliers) +
		                 static_cast<double>(b.boredom.at(k));
		if (x < static_cast<double>(k_BoredomCeiling)) // below 1 -> store; otherwise unchanged
		{
			b.boredom.at(k) = static_cast<float>(x);
		}
	}

	// 3. the desires that cost the owner belief, not for the neutral player
	if (owner != k_Neutral)
	{
		for (size_t j = 0; j < k_Desires; ++j)
		{
			const auto& desireInfo = info.townDesire.at(j);
			auto& threshold = b.desireThreshold.at(j);
			// The sum of the desire's three parts. (approximate) GetDesire is rounded to float, the original compares
			// the unrounded sum
			const float s = town_desire::GetDesire(t->desire, j);
			if (s > threshold)
			{
				// (s - threshold) x desireToBeliefScale
				const auto r = static_cast<float>((static_cast<double>(s) - static_cast<double>(threshold)) *
				                                  static_cast<double>(desireInfo.desireToBeliefScale));
				Reduce(*t, owner, r);
			}
			// Above the belief info's minimumThreshold (0.25) -> decays (it can step below once)
			if (threshold > info.belief.minimumThreshold)
			{
				threshold -= desireInfo.desireToBeliefThresholdDecay;
			}
		}
	}

	// 4. the neutral belief pinned to beliefInNeutralPlayer, through SetBelief (capped)
	SetBelief(b, k_Neutral, b.beliefInNeutralPlayer);

	// 5. the strongest player and the decay of recent. The slots 0..7 in order; only < skips, so a tie goes to the
	//    higher slot (the owner included)
	size_t best = Slot(owner);
	float bestValue = b.belief.at(best);
	const float decay = info.player.computerPlayerBeliefChangeDecay; // the player info's decay (0.997)
	for (size_t n = 0; n < k_Players; ++n)
	{
		if (b.belief.at(n) >= bestValue)
		{
			bestValue = b.belief.at(n);
			best = n;
		}
		b.recent.at(n) *= decay;
	}

	// 6. the conversion when the strongest is not the owner
	if (best == Slot(t->owner))
	{
		return;
	}
	const auto newOwner = static_cast<PlayerNames>(best);
	// SetBelief(best, claimedTownBeliefMultiplier (1.5) x belief[best])
	SetBelief(b, newOwner,
	          static_cast<float>(static_cast<double>(info.belief.claimedTownBeliefMultiplier) *
	                             static_cast<double>(b.belief.at(best))));
	if (owner != k_Neutral)
	{
		// Every town of P (this one is still in the list):
		// SetBelief(P, lostATownBeliefInPlayerMultiplier (0.9) x belief[P] x lost-town scale)
		for (const auto other : map_cells::TownsOf(owner))
		{
			if (auto* o = TownOf(other); o != nullptr)
			{
				const double value = static_cast<double>(info.belief.lostATownBeliefInPlayerMultiplier) *
				                     static_cast<double>(o->belief.belief.at(Slot(owner))) * static_cast<double>(lost);
				SetBelief(o->belief, owner, static_cast<float>(value));
			}
		}
	}
	// The old owner local -> GeneralBad help sprite, else the new one local -> GeneralGood; then TakeOverTown
	if (owner == k_LocalPlayer)
	{
		Help(detail::HelpSprite::GeneralBad, town);
	}
	else if (newOwner == k_LocalPlayer)
	{
		Help(detail::HelpSprite::GeneralGood, town);
	}
	TakeOverTown(newOwner, town);
}

bool HearsTakeOver(PlayerNames previous, PlayerNames next, PlayerNames local)
{
	return next == local && previous != local;
}

void TakeOverTown(PlayerNames player, entt::entity town)
{
	auto* t = TownOf(town);
	if (t == nullptr)
	{
		return;
	}
	// Already the player's: nothing
	const auto previous = t->owner;
	if (previous == player)
	{
		return;
	}
	// The local player winning it from another hears the take-over: the in-game bank's sample 205, 2D, untracked
	if (HearsTakeOver(previous, player, k_LocalPlayer))
	{
		audio::PlayOptions options;
		options.sample = {audio::Bank(audio::SfxBank::InGame), k_TakeOverSample};
		options.owner = audio::Owner::None();
		options.is3D = false;
		options.track = false;
		audio::PlaySoundEffect(options);
	}
	// Nobody worships there any more
	if (Locator::villageTotemSystem::has_value())
	{
		Locator::villageTotemSystem::value().SetTownShare(town, 0.0f);
	}
	// Its disciples held at their jobs give them up, before the town leaves its owner
	villager::ResetAllDiscipleStates(town);
	// It leaves its old owner: its worship site and temple heart's building sites, its buildings and its magic
	RemoveTownFromPlayer(town);
	// Then the new owner; out of the old owner's list, at the tail of the new owner's
	t = TownOf(town);
	t->owner = player;
	t->ownerListStamp = town_queries::NextOwnerListStamp();
	// It joins the new player's citadel (none for a player without one, or a town that may have no worship site)
	worship::citadel::AddTown(worship::citadel::Of(player), town);
	// Each of its buildings is given to the new owner: a built wonder adds its power to that player's tribe, the town
	// centre hands it to its totem
	for (const auto abode : town_stats::AbodesOf(town))
	{
		wonders::AddToPlayer(abode);
	}
	// Every magic type it holds is enabled for the new owner
	for (size_t i = 0; i < components::TownMagic::k_MagicTypes; ++i)
	{
		if (const auto type = static_cast<MagicType>(i); worship::town::IsMagicTypeHeld(town, type))
		{
			magic::players::SetMagicTypeEnabled(player, type, true);
		}
	}
	if (Locator::villageTotemSystem::has_value())
	{
		Locator::villageTotemSystem::value().TownChangedPlayer(town);
	}
	// Its town centre's spell icons are the new owner's, with their seed graphics and those graphics' effects
	if (const auto* owned = TownOf(town); owned != nullptr && owned->centre != entt::null && Entities().Valid(owned->centre))
	{
		worship::town_centre::SetPlayer(owned->centre, player);
	}
	// The new player's GameStats: its taken-over population += adults + children
	t = TownOf(town);
	game_stats::TownTakenOver(Slot(player), t->stats.adults + t->stats.children);
	// The villagers celebrate: a REACTION 31 from the town centre (the town without one), of the new owner
	const auto centre = t->centre != entt::null && Entities().Valid(t->centre) ? t->centre : entt::null;
	static_cast<void>(effects::reactions::CreateReaction(centre != entt::null ? centre : town,
	                                                     openblack::Reaction::ReactToTownCelebration, player, false));
	// Won by a player: the fireworks and the fountain of its symbol at the centre (the town without one)
	if (player != k_Neutral && Locator::particleSystem::has_value())
	{
		const auto at = centre != entt::null ? centre : town;
		if (const auto* transform = Entities().TryGet<const components::Transform>(at); transform != nullptr)
		{
			auto& particles = Locator::particleSystem::value();
			static_cast<void>(
			    particles.StartSpotVisual(SpotVisualType::TownFireworks, transform->position, std::nullopt, entt::null));
			const auto fountain =
			    particles.StartSpotVisual(SpotVisualType::PlayerIconFountain, transform->position, std::nullopt, entt::null);
			if (fountain != entt::null)
			{
				particles.SetSpotVisualPlayer(fountain, static_cast<int>(player));
			}
		}
	}
	// The influence is drawn afresh
	influence::ForceNeedUpdateInfluence();
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_DEBUG(logger, "town_belief: town {} taken over by player {} (was {})", t->id, static_cast<int>(player),
		                    static_cast<int>(previous));
	}
}

void SetTownEmpty(entt::entity town)
{
	auto* t = TownOf(town);
	if (t == nullptr)
	{
		return;
	}
	Init(*t);
	for (size_t i = 0; i < k_Players; ++i)
	{
		SetBelief(t->belief, static_cast<PlayerNames>(i), t->belief.beliefInNeutralPlayer);
	}
	if (t->owner != k_Neutral)
	{
		TakeOverTown(k_Neutral, town);
	}
}

// ---- ProcessOncePerTurn ---------------------------------------------------------------------------------------------

void ProcessOncePerTurn()
{
	// Only on turn % 10 == 0
	if (game_clock::Turn() % k_ProcessEvery != 0 || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const size_t me = Slot(k_LocalPlayer);
	float myAdded = 0.0f;
	// The newest town first (towns are inserted at the head of the list)
	const auto towns = town_queries::TownsNewestFirst();
	for (size_t p = 0; p < k_Players; ++p)
	{
		float sum = 0.0f;
		for (const auto town : towns)
		{
			auto& b = registry.Get<Town>(town).belief;
			const float a = b.addedThisPeriod.at(p);
			if (p == me && a > 0.0f)
			{
				myAdded += a;
			}
			b.addedThisPeriod.at(p) = 0.0f;
			sum = sum + b.belief.at(p); // stored as a float on every step
		}
		// After the towns (also with none): the player's world belief max / min
		game_stats::WorldBelief(p, sum);
	}
	if (myAdded > k_ToolTipMinimum)
	{
		// v = myAdded x 1000; the "believers gained" tooltip
		const float v = myAdded * k_ToolTipScale;
		events::Publish(events::TownBeliefToolTip {.text = k_ToolTipBelieversGained, .value = v});
		// and the same count floating up from the land under the cursor, which is the map's corner when the cursor
		// finds no land. The point is held as map coordinates, so it is on the ground there
		const auto hover = Locator::handSystem::has_value() ? Locator::handSystem::value().GetHoverLandPoint() : std::nullopt;
		const auto point = hover.value_or(glm::vec3(0.0f));
		const auto at = map_coords::ToWorld(map_coords::MapCoords {map_coords::ToFixed(point.x), map_coords::ToFixed(point.z)});
		archetypes::FloatingNumberArchetype::Create(at, v, k_BelieversGainedColour, k_BelieversGainedWidth);
	}
	CheckLosingBelief(k_LocalPlayer); // inside the % 10 block
}

void CheckLosingBelief(PlayerNames player)
{
	// Nothing on land 1
	if (influence::LandNumber() == 1 || !InRange(player) || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto warn = static_cast<double>(Locator::infoConstants::value().belief.beliefLeftWhenHelpSpritesWarn);
	const size_t n = Slot(player);
	for (const auto town : map_cells::TownsOf(player))
	{
		const auto* t = TownOf(town);
		if (t == nullptr)
		{
			continue;
		}
		const double mine = static_cast<double>(t->belief.belief.at(n)) - warn;
		for (size_t i = 0; i < k_Players; ++i) // the neutral slot included
		{
			if (i == n)
			{
				continue;
			}
			if (mine < static_cast<double>(t->belief.belief.at(i)))
			{
				if (player == k_LocalPlayer) // P is the local player
				{
					Help(detail::HelpSprite::LosingBelief, town);
				}
				return; // the first town found only
			}
		}
	}
}

// ---- DrawBelief and the belief sprites ------------------------------------------------------------------------------

void DrawBelief(float f, entt::entity thing, std::optional<PlayerNames> player)
{
	const int32_t v = Amount(f);
	if (v <= 0)
	{
		return;
	}
	glm::vec3 position {0.0f}; // (openblack) 0: the original leaves it uninitialised without a thing
	if (Valid(thing))
	{
		// The ground point plus the thing's height
		position = GroundPoint(thing);
		position.y += object::GetHeight(thing);
	}
	// A debug flag would show a "B n" value spinner. (not ported) no one sets it
	const uint32_t colour = player.has_value() ? PlayerColour(*player) : k_NoPlayerColour;
	QueueBeliefSprite(position, v, colour);
}

void QueueBeliefSprite(const glm::vec3& position, int32_t amount, uint32_t colour)
{
	// Every sprite is queued: the original's two conditions are a constant flag that is always set and a count of 400
	// that it tests on a word nothing ever writes, so neither turns a sprite away
	BeliefSprites().push_back({position, amount, colour});
}

std::optional<BeliefSprite> PopBeliefSprite()
{
	auto& sprites = BeliefSprites();
	if (sprites.empty())
	{
		return std::nullopt;
	}
	const auto last = sprites.back(); // the last entry
	sprites.pop_back();
	return last;
}

size_t BeliefSpriteCount()
{
	return BeliefSprites().size();
}

void AddBeliefEventHandlers(EventManager& manager)
{
	manager.AddHandler<events::TownBeliefHelp>(PlayHelp);
	// The tooltip of the believers gained
	manager.AddHandler<events::TownBeliefToolTip>(
	    [](const events::TownBeliefToolTip& event) { help::tooltips::Force(event.text, event.value); });
}

void detail::ClearForTests()
{
	BeliefSprites().clear();
}

void HashTown(state_hash::Hasher& h, uint32_t entity, const TownBelief& belief)
{
	h.Entity(entity);
	for (const float value : belief.belief)
	{
		h.Float(value);
	}
	for (const float value : belief.pending)
	{
		h.Float(value);
	}
	for (const float value : belief.recent)
	{
		h.Float(value);
	}
	for (const float value : belief.boredom)
	{
		h.Float(value);
	}
}

void RegisterStateHash()
{
	state_hash::Register("town_belief", [](state_hash::Hasher& h) {
		if (!Locator::entitiesRegistry::has_value())
		{
			return;
		}
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		for (const auto town : town_queries::TownsNewestFirst())
		{
			HashTown(h, static_cast<uint32_t>(town), registry.Get<const Town>(town).belief);
		}
	});
}
} // namespace openblack::ecs::town_belief
