/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The score rules of the DefeatPlayer aim: the mind picks a rival, a town of its to win or destroy, and its creature

#include <cmath>
#include <cstdint>

#include <limits>
#include <optional>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ComputerPlayerScoresDetail.h"

using openblack::map_coords::MapCoords;

namespace openblack::computer_player
{

namespace
{

/// The players the game's walks go over: the seven that are not the neutral, then the neutral
constexpr uint8_t k_NotNeutralPlayers = 7;
constexpr uint8_t k_AllPlayers = 8;
/// The player type that stops the walk of active players
constexpr uint32_t k_NeutralType = 3;
/// A town's spell icon whose miracle the player lacks counts this much, and as much again when the mind has looked for
/// it
constexpr float k_MissingMiracleWorth = 0.5f;
/// How much a rival's influence held against the mind's own counts against taking its town
constexpr double k_RivalPowerWeight = 0.85;
/// The distance over which a town's pull from the hand falls off, and how much of the pull counts
constexpr float k_TownFallOff = 250.0f;
constexpr float k_TownPullShare = 0.5f;
/// A town's will to be taken over: from the belief still needed, by a fifth; more for the mind's own town; at least
/// this much
constexpr float k_BeliefNeededShare = 0.2f;
constexpr float k_OwnTownWill = 0.75f;
constexpr float k_LeastWill = 0.1f;
/// A rival's creature within this of the hand is in full reach; beyond, its pull falls off over the second
constexpr float k_CreatureNearby = 100.0f;
constexpr float k_CreatureFallOff = 500.0f;
/// The town desire for food, by its number
constexpr uint32_t k_FoodDesire = 0;
constexpr uint32_t k_WoodDesire = 1;
/// The resource of food, by its number
constexpr uint32_t k_FoodResource = 0;
/// The distance over which the nearness of two players' citadels falls off, beyond their influence powers
constexpr float k_AttackReach = 5120.0f;
/// What is added to the influence powers and their averages, and to the harm, so that none divides by 0
constexpr double k_SmallPower = 0.0001;
constexpr double k_SmallHarm = 0.001;
/// The mind's hand counts its own buildings as near up to this distance, and each metre takes this much off
constexpr float k_BuildingsNearby = 100.0f;
constexpr float k_NearbyShare = 0.01f;
/// The alignments' gap counts in whole quarters
constexpr int32_t k_AlignmentQuarters = 4;
/// The town's reaction to a thing flying, whose boredom dulls a throw to impress it
constexpr uint32_t k_FlyingObjectReaction = 9;
/// At most this likely to be seen by the town's villagers, a throw is not worth making
constexpr float k_LeastSeen = 0.01f;
/// How far round the hand the mind looks for a thing to throw
constexpr float k_ImpressThrowReach = 250.0f;

/// What a hand may throw to impress a town: a thing it may throw that breaks no buildings, by how impressive it is
float ImpressWeight(const ScoreWorld& world, entt::entity thing)
{
	const auto facts = world.ThrowFactsOf(thing);
	if (!facts.throwable || facts.breaksBuildings)
	{
		return 0.0f;
	}
	return facts.artefactModifier + facts.impressiveValue;
}

} // namespace

std::optional<PlayerNames> NextActivePlayer(const ScoreWorld& world, std::optional<PlayerNames> after)
{
	auto slot = after.has_value() ? static_cast<uint8_t>(static_cast<uint8_t>(*after) + 1) : uint8_t {0};
	// after a player, the walk stops at a neutral one
	if (after.has_value() && slot < k_AllPlayers && world.PlayerType(static_cast<PlayerNames>(slot)) == k_NeutralType)
	{
		return std::nullopt;
	}
	for (; slot < k_NotNeutralPlayers; ++slot)
	{
		if (world.PlayerType(static_cast<PlayerNames>(slot)) != 0)
		{
			return static_cast<PlayerNames>(slot);
		}
	}
	return std::nullopt;
}

float DesireToBeTakenOver(const ScoreWorld& world, entt::entity town)
{
	const float needed = world.BeliefNeededToConvert(town) * k_BeliefNeededShare;
	const float share = scores::Below(needed, 1.0f) ? needed : 1.0f;
	float will = 1.0f - share;
	if (world.TownOwner(town) == world.Me())
	{
		will = will + k_OwnTownWill;
	}
	if (scores::AtMost(will, k_LeastWill))
	{
		return k_LeastWill;
	}
	return scores::Below(will, 1.0f) ? will : 1.0f;
}

float BestTownToTake(const ScoreWorld& world, const ecs::components::ComputerMind& mind, PlayerNames victim, entt::entity& town)
{
	const auto me = world.Me();
	uint32_t people = 0;
	for (const auto mine : world.TownsOf(me))
	{
		people = people + world.TownAdults(mine) + world.TownChildren(mine);
	}
	const auto population = static_cast<float>(people);
	const auto hand = world.HandPlace();
	float best = 0.0f;
	for (const auto candidate : world.TownsOf(victim))
	{
		// the miracles its icons make that the mind's player lacks, more for those the mind has looked for
		float missing = 0.0f;
		for (const auto type : world.TownSpellIconTypes(candidate))
		{
			if (world.IsMagicTypeEnabled(type))
			{
				continue;
			}
			missing = missing + k_MissingMiracleWorth;
			if (mind.iconsFound.at(static_cast<size_t>(type)) != 0)
			{
				missing = missing + k_MissingMiracleWorth;
			}
		}
		float size = 0.0f;
		if (scores::Same(population, 0.0f))
		{
			size = missing + 1.0f;
		}
		else
		{
			const auto theirs = world.TownAdults(candidate) + world.TownChildren(candidate);
			// the town's people whole, over the player's population, rounded once
			size = static_cast<float>(static_cast<double>(theirs) / static_cast<double>(population));
		}
		float rival = 0.0f;
		if (victim != PlayerNames::NEUTRAL && !scores::Same(world.InfluencePower(me), 0.0f))
		{
			const float ratio = world.InfluencePower(victim) / world.InfluencePower(me);
			rival = scores::MulRoundedOnce(scores::AtMost(ratio, 1.0f) ? ratio : 1.0f, k_RivalPowerWeight);
		}
		const float distance = gutils::GetDistanceInMetres(hand, world.Place(candidate));
		float pull = gutils::DistanceChangeToBelief(distance, k_TownFallOff) * k_TownPullShare;
		pull = pull + k_TownPullShare;
		const float will = DesireToBeTakenOver(world, candidate);
		float value = size + missing;
		value = will * value;
		const float left = 1.0f - rival;
		value = value * left;
		value = value * pull;
		if (value > best)
		{
			best = value;
			town = candidate;
		}
	}
	return scores::Below(best, 1.0f) ? best : 1.0f;
}

namespace scores
{

float MulRoundedOnce(float value, double by)
{
	// the product exactly, then one rounding to a float: the double product is the nearest double, and differs from
	// the float's rounding only on a tie the exact product is off
	const double product = static_cast<double>(value) * by;
	const double error = std::fma(static_cast<double>(value), by, -product);
	const auto rounded = static_cast<float>(product);
	if (error == 0.0)
	{
		return rounded;
	}
	constexpr float k_Infinity = std::numeric_limits<float>::infinity();
	const float other =
	    static_cast<double>(rounded) < product ? std::nextafter(rounded, k_Infinity) : std::nextafter(rounded, -k_Infinity);
	const double halfway = (static_cast<double>(rounded) + static_cast<double>(other)) * 0.5;
	if (product != halfway)
	{
		return rounded;
	}
	// on the tie the exact product leans one way
	const bool up = error > 0.0;
	const float higher = rounded > other ? rounded : other;
	const float lower = rounded > other ? other : rounded;
	return up ? higher : lower;
}

float AddRoundedOnce(float value, double plus)
{
	// the sum exactly (its error by the two-sum), then one rounding to a float, as the product above
	const double sum = static_cast<double>(value) + plus;
	const double back = sum - static_cast<double>(value);
	const double error = (static_cast<double>(value) - (sum - back)) + (plus - back);
	const auto rounded = static_cast<float>(sum);
	if (error == 0.0)
	{
		return rounded;
	}
	constexpr float k_Infinity = std::numeric_limits<float>::infinity();
	const float other =
	    static_cast<double>(rounded) < sum ? std::nextafter(rounded, k_Infinity) : std::nextafter(rounded, -k_Infinity);
	const double halfway = (static_cast<double>(rounded) + static_cast<double>(other)) * 0.5;
	if (sum != halfway)
	{
		return rounded;
	}
	const bool up = error > 0.0;
	const float higher = rounded > other ? rounded : other;
	const float lower = rounded > other ? other : rounded;
	return up ? higher : lower;
}

std::optional<PlayerNames> NextActivePlayerOrNeutral(const ScoreWorld& world, std::optional<PlayerNames> after)
{
	auto slot = after.has_value() ? static_cast<uint8_t>(static_cast<uint8_t>(*after) + 1) : uint8_t {0};
	if (slot >= k_AllPlayers)
	{
		return std::nullopt;
	}
	for (; slot < k_NotNeutralPlayers; ++slot)
	{
		if (world.PlayerType(static_cast<PlayerNames>(slot)) != 0)
		{
			return static_cast<PlayerNames>(slot);
		}
	}
	return PlayerNames::NEUTRAL;
}

float DesireToAttack(const ScoreWorld& world, PlayerNames other)
{
	const auto me = world.Me();
	const float myPower = world.InfluencePower(me);
	const float otherPower = world.InfluencePower(other);
	// how near their citadels are, beyond both influences
	const float apart = gutils::GetDistanceInMetres(world.PlayerFixedPlace(me), world.PlayerFixedPlace(other));
	float gap = myPower + otherPower;
	gap = apart - gap;
	gap = gap > 0.0f ? gap : 0.0f;
	const float nearness = gutils::GetDistanceModifier(gap, k_AttackReach);
	// how much faster the other's influence grows than its own
	const float otherShifted = AddRoundedOnce(otherPower, k_SmallPower);
	const float myShifted = AddRoundedOnce(myPower, k_SmallPower);
	const float otherGrowth = otherShifted / AddRoundedOnce(world.AverageInfluencePower(other), k_SmallPower);
	const float myGrowth = myShifted / AddRoundedOnce(world.AverageInfluencePower(me), k_SmallPower);
	float growth = otherGrowth - myGrowth;
	growth = growth > 0.0f ? growth : 0.0f;
	// how much stronger, by the nearness, or weaker the other is
	const float ratio = otherShifted / myShifted;
	float stronger = ratio - 1.0f;
	stronger = stronger > 0.0f ? stronger : 0.0f;
	const float strength = nearness * stronger;
	const float left = 1.0f - ratio;
	float weakness = left * left;
	weakness = weakness * left;
	weakness = scores::Below(weakness, 0.0f) ? 0.0f : weakness;
	// how much more the other harmed it than it harmed the other
	const float done = AddRoundedOnce(world.HarmFrom(other, me), k_SmallHarm);
	const float suffered = AddRoundedOnce(world.HarmFrom(me, other), k_SmallHarm);
	float revenge = done / suffered;
	revenge = 1.0f - revenge;
	revenge = revenge < 0.0f ? 0.0f : revenge;
	// how far apart their alignments are, in whole quarters of the truncated gap
	const float alignments = world.AlignmentValue(me) - world.AlignmentValue(other);
	const int32_t whole = alignments >= -2147483648.0f && alignments < 2147483648.0f ? static_cast<int32_t>(alignments)
	                                                                                 : std::numeric_limits<int32_t>::min();
	const auto magnitude = static_cast<int32_t>(whole < 0 ? 0u - static_cast<uint32_t>(whole) : static_cast<uint32_t>(whole));
	const auto alignment = static_cast<float>(magnitude / k_AlignmentQuarters);
	// how near the hand is to its own buildings
	const auto hand = world.HandPlace();
	float nearest = k_BuildingsNearby;
	for (const auto town : world.TownsOf(me))
	{
		for (const auto abode : world.TownAbodes(town))
		{
			const float distance = gutils::GetDistanceInMetres(hand, world.Place(abode));
			if (scores::Below(distance, nearest))
			{
				nearest = distance;
			}
		}
	}
	float proximity = nearest * k_NearbyShare;
	proximity = 1.0f - proximity;
	float sum = proximity + alignment;
	sum = sum + revenge;
	sum = sum + weakness;
	sum = sum + strength;
	sum = sum + growth;
	return sum * nearness;
}

float ScoreExpandInfluence(ScoreAt& at)
{
	const auto me = at.world.Me();
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (uint8_t number = 0; number < k_AllPlayers; ++number)
	{
		const auto player = static_cast<PlayerNames>(number);
		if (player == me || at.world.IsAllied(player, me))
		{
			continue;
		}
		entt::entity town = entt::null;
		const float value = BestTownToTake(at.world, at.mind, player, town);
		if (town != entt::null && value > best)
		{
			best = value;
			chosen = town;
		}
	}
	if (chosen != entt::null)
	{
		at.state().thing = chosen;
	}
	return best;
}

float ScoreTakeOverTownByFulfillingFoodDesire(ScoreAt& at)
{
	const auto above = AboveParent(at);
	if (above == entt::null || !at.world.IsTown(above))
	{
		return 0.0f;
	}
	const float food = at.world.RawDesire(above, k_FoodDesire);
	const float value = BeliefAttackImportance(at.world, above) * food;
	at.state().thing = above;
	return Below(value, 1.0f) ? value : 1.0f;
}

float ScoreTakeOverTown(ScoreAt& at)
{
	const auto above = AboveParent(at);
	const auto player = above != entt::null ? at.world.PlayerOf(above) : std::nullopt;
	if (!player.has_value())
	{
		return 0.0f;
	}
	entt::entity town = entt::null;
	const float value = BestTownToTake(at.world, at.mind, *player, town);
	if (town != entt::null)
	{
		at.state().thing = town;
	}
	return value;
}

float ScoreThrowThingsToImpress(ScoreAt& at)
{
	const auto above = AboveParent(at);
	if (above == entt::null || !at.world.IsTown(above))
	{
		return 0.0f;
	}
	const float boredom = at.world.TownBoredom(above, k_FlyingObjectReaction);
	// how likely the town's villagers are to see it, up to 1: asked again when below 1, and nothing when that is at
	// most a hundredth
	MapCoords where {};
	float seen = PosWhereVillagersAre(at.world, above, at.world.HandPlace(), where);
	if (Below(seen, 1.0f))
	{
		seen = PosWhereVillagersAre(at.world, above, at.world.HandPlace(), where);
		if (AtMost(seen, k_LeastSeen))
		{
			return 0.0f;
		}
	}
	else
	{
		seen = 1.0f;
	}
	const auto nearest = NearestPlaceInInfluence(at.world, where);
	if (!nearest.has_value())
	{
		return 0.0f;
	}
	const float factor = FactorBeyond(at.world, *nearest, where);
	const float worth = factor * (seen * boredom);
	const auto thing = BestThingNear(at.world, at.world.HandPlace(), k_ImpressThrowReach,
	                                 [&at](entt::entity candidate) { return ImpressWeight(at.world, candidate); });
	if (thing == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = thing;
	at.state().point = at.world.WorldPoint(where);
	return worth;
}

float ScoreCastSpellToFulfilTownNeeds(ScoreAt& at)
{
	const auto above = AboveParent(at);
	if (above == entt::null || !at.world.IsTown(above))
	{
		return 0.0f;
	}
	// the town's store, or its food pot, which the town makes when it has neither
	const auto store = at.world.ResourceDropObject(above, k_FoodResource);
	const auto place = store != entt::null ? std::optional<MapCoords>(at.world.Place(store)) : std::nullopt;
	const float food = at.world.RawDesire(above, k_FoodDesire);
	const float wood = at.world.RawDesire(above, k_WoodDesire);
	// the wood miracle goes by the want of food and the food miracle by the want of wood, as the original has them
	entt::entity woodIcon = entt::null;
	const float byWood = SpellWorth(at.world, at.mind, MagicType::Wood, place, woodIcon) * food;
	entt::entity foodIcon = entt::null;
	const float byFood = SpellWorth(at.world, at.mind, MagicType::Food, place, foodIcon) * wood;
	entt::entity icon = entt::null;
	float value = 0.0f;
	if (byFood > byWood)
	{
		icon = foodIcon;
		value = byFood;
	}
	else
	{
		if (Same(byWood, 0.0f))
		{
			return 0.0f;
		}
		icon = woodIcon;
		value = byWood;
	}
	if (icon == entt::null || store == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = icon;
	return value;
}

float ScorePlayerWithBetterCreature(ScoreAt& at)
{
	const auto me = at.world.Me();
	for (auto player = NextActivePlayer(at.world, std::nullopt); player.has_value();
	     player = NextActivePlayer(at.world, player))
	{
		if (*player == me)
		{
			continue;
		}
		if (!at.world.IsAllied(*player, me))
		{
			at.state().thing = at.world.PlayerThing(*player);
			return 1.0f;
		}
	}
	return 0.0f;
}

float ScoreAttackCreature(ScoreAt& at)
{
	const auto above = AboveParent(at);
	const auto player = above != entt::null ? at.world.PlayerOf(above) : std::nullopt;
	const auto me = at.world.Me();
	if (!player.has_value() || *player == me)
	{
		return 0.0f;
	}
	const auto creature = at.world.CreatureOf(*player);
	if (creature == entt::null || at.world.IsAllied(*player, me))
	{
		return 0.0f;
	}
	const float distance = gutils::GetDistanceInMetres(at.world.HandPlace(), at.world.Place(creature));
	const float fallOff = gutils::GetDistanceModifier(distance, k_CreatureFallOff);
	at.state().thing = creature;
	return Below(distance, k_CreatureNearby) ? 1.0f : fallOff;
}

/// The player most worth defeating: over the active players and the neutral but the mind's own and those who count it
/// an ally, the mind's attitude to them times its desire to attack them; the strictly best, the first taken whatever it
/// is worth; the player, up to 1
float ScoreDefeatPlayer37(ScoreAt& at)
{
	const auto me = at.world.Me();
	float best = 0.0f;
	std::optional<PlayerNames> chosen;
	for (auto other = NextActivePlayerOrNeutral(at.world, std::nullopt); other.has_value();
	     other = NextActivePlayerOrNeutral(at.world, other))
	{
		if (*other == me || at.world.IsAllied(*other, me))
		{
			continue;
		}
		const float desire = DesireToAttack(at.world, *other);
		const float value = at.mind.attitude.at(static_cast<size_t>(*other)) * desire;
		if (value > best || !chosen.has_value())
		{
			best = value;
			chosen = other;
		}
	}
	if (chosen.has_value())
	{
		at.state().thing = at.world.PlayerThing(*chosen);
	}
	return scores::Below(best, 1.0f) ? best : 1.0f;
}

} // namespace scores

} // namespace openblack::computer_player
