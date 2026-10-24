/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerPlayerScores.h"

#include <algorithm>
#include <bit>
#include <vector>

#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "ComputerPlayerScoresDetail.h"

using openblack::ecs::components::ComputerMind;
using openblack::map_coords::MapCoords;

namespace openblack::computer_player
{

using namespace scores;

namespace
{

/// The farthest a citadel's influence edge can be and still be the nearest
constexpr float k_FarthestEdge = 9999999.0f;
/// The share of the influence power the virtual influence reaches
constexpr float k_PowerShareOfReach = 0.2f;
/// Below this fall-off the place is out of reach
constexpr double k_LeastFallOff = 0.01;
/// Turns of prayer over which an icon's keenness falls off
constexpr float k_TurnsToCreateFallOff = 2000.0f;
/// A prayer of nothing, so that no division is by 0
constexpr float k_SmallPrayer = 0.001f;
/// The largest gap, in map units on each side, at which a cast miracle counts as at the place
constexpr int32_t k_CastNearGap = 0x2000;
/// The turns a leash must have been tied before the creature counts as free on it, and the share each turn adds after
constexpr float k_TiedTurnsBeforeFree = 450.0f;
constexpr uint32_t k_TiedTurnsShareBits = 0x3B11A2B4u;
/// The desire to heal, whose plan keeps the creature from any job
constexpr uint32_t k_RestoreHealthDesire = 15;
/// A creature's size from its scale
constexpr float k_SizePerScale = 15.0f;
/// The reach of a creature's job: so much per size, and so much more
constexpr float k_JobReachPerSize = 12.0f;
constexpr float k_JobReach = 1000.0f;
/// How near a diversion is: twice the size and so much more
constexpr float k_DiversionReach = 200.0f;
/// The least distance two creatures fight at, by their sizes
constexpr float k_LeastFightDistance = 30.0f;
/// The town's area counts this much larger as near a creature
constexpr float k_TownNearShare = 1.2f;
/// The turns over which a town's harm by a player fades
constexpr uint32_t k_HarmFadeShareBits = 0x3A5A740Eu;
/// The players whose last attacks on a town count for its harm lately, and the share of the fade each turn since adds
constexpr uint8_t k_HarmPlayers = 7;
constexpr uint32_t k_RecentHarmShareBits = 0x3ADA740Eu;
/// The turns after a blow to the citadel's heart before it is reacted to, and the share each turn since takes off
constexpr uint32_t k_ThrowingStuffQuietTurns = 20;
constexpr uint32_t k_ThrowingStuffShareBits = 0x3ADA740Eu;
/// Below this a damaged building is in need of a repair at once
constexpr float k_BadlyDamaged = 0.9f;
/// What a physical shield is worth against a belief attack, beside a shield
constexpr float k_PhysicalShieldAgainstBelief = 0.9f;
/// How strongly a rival's belief in a town counts against the town's importance
constexpr float k_RivalBeliefImportance = 10.0f;
/// A desire to fight, as a miracle makes a creature watching it feel
constexpr uint32_t k_AngerDesire = 2;
/// The creature's desires the mind looks after, by their numbers
constexpr uint32_t k_HungerDesire = 4;
constexpr uint32_t k_TirednessDesire = 8;
constexpr uint32_t k_ThirstDesire = 14;
constexpr uint32_t k_WarmerDesire = 19;
constexpr uint32_t k_ColderDesire = 20;
constexpr uint32_t k_StayNearHomeDesire = 28;
/// The town's desire for sleep, above which its villagers are not looked for
constexpr uint32_t k_SleepDesire = 16;
constexpr float k_MostSleep = 0.5f;
/// The cells round a place whose villagers count, and the distance over which a place's worth falls off
constexpr uint32_t k_VillagersNearCells = 9;
constexpr float k_VillagersFallOff = 1000.0f;
/// The distance over which a building's worth to an attack falls off
constexpr float k_AttackFallOff = 1000.0f;
/// The fraction of a map cell at its middle
constexpr uint32_t k_HalfCell = 0x8000;
/// How far round the hand the mind looks for a thing to throw
constexpr float k_ThrowReach = 250.0f;

/// The cell at an edge of a reach round a map coordinate, worked out as the original does in floats: the coordinate
/// in metres, with the reach taken off or added, back to map units cut toward zero; its whole cells
int16_t ReachEdge(int32_t coordinate, float reach, bool high)
{
	const float metres = map_coords::ToMetres(coordinate);
	const float edge = high ? metres + reach : metres - reach;
	return map_coords::SignedCellOf(map_coords::ToFixedGUtils(edge));
}

/// What a hand may throw at a creature: a thing it may throw that breaks buildings or is a tree, by its weight
float RockWeight(const ScoreWorld& world, entt::entity thing)
{
	const auto facts = world.ThrowFactsOf(thing);
	if (!facts.throwable || (!facts.breaksBuildings && !facts.tree))
	{
		return 0.0f;
	}
	return facts.weight;
}

/// What a town has to be defended from first: its worship site, else its store, else its newest building
entt::entity BeliefAttackDefendObject(const ScoreWorld& world, entt::entity town)
{
	if (const auto site = world.WorshipSiteOf(town); site != entt::null)
	{
		return site;
	}
	if (const auto pit = world.StoragePit(town); pit != entt::null)
	{
		return pit;
	}
	const auto abodes = world.TownAbodes(town);
	return abodes.empty() ? entt::entity {entt::null} : abodes.front();
}

// ---- PlayerInteract ------------------------------------------------------------------------------------------------

/// Talking to the player, giving it something or throwing something at it: never. (An ill creature too: what it is
/// tested on is never set in the game; raising a town's totem; and the player with more worshippers, which the original
/// compares with itself)
float ScoreNothing(ScoreAt&)
{
	return 0.0f;
}

// ---- ReactToPlayer and the creature's fights ----------------------------------------------------------------------

/// A miracle to make a creature fight, against the creature above unless it is too near the mind's own creature
float ScoreTeachCreatureAggressiveMagic164(ScoreAt& at)
{
	const auto above = AboveParent(at);
	const auto target = above != entt::null && at.world.IsCreature(above) ? above : entt::entity {entt::null};
	const auto mine = at.world.MyCreature();
	if (target == entt::null)
	{
		// the original goes on with a place that belongs to no thing, which only a force with nothing above can reach;
		// openblack scores nothing then
		return 0.0f;
	}
	if (mine != entt::null)
	{
		const float distance = gutils::GetDistanceInMetres(at.world.Place(target), at.world.Place(mine));
		if (Below(distance, FightDistance(at.world, mine, target)))
		{
			return 0.0f;
		}
	}
	entt::entity icon = entt::null;
	const float value = BestSpell(at, at.world.Place(target), icon, [&](MagicType type) {
		return at.world.PerceivedDesire(type) == k_AngerDesire ? 1.0f : 0.0f;
	});
	if (icon != entt::null)
	{
		at.state().thing = icon;
	}
	return value;
}

/// A thing to throw at the creature above, unless it is too near the mind's own creature: worth 1 with no creature
/// above, else the influence factor at a point twice the player's speed from the creature towards the nearest place
/// inside the influence, whatever is thrown; the thing is the best near the hand by its weight
float ScoreThrowrockatCreature(ScoreAt& at)
{
	const auto above = AboveParent(at);
	const auto target = above != entt::null && at.world.IsCreature(above) ? above : entt::entity {entt::null};
	float factor = 1.0f;
	if (target != entt::null)
	{
		if (const auto mine = at.world.MyCreature(); mine != entt::null)
		{
			const float distance = gutils::GetDistanceInMetres(at.world.Place(target), at.world.Place(mine));
			if (Below(distance, FightDistance(at.world, mine, target)))
			{
				return 0.0f;
			}
		}
		const auto nearest = NearestPlaceInInfluence(at.world, at.world.Place(target));
		if (!nearest.has_value())
		{
			return 0.0f;
		}
		factor = FactorBeyond(at.world, *nearest, at.world.Place(target));
	}
	const auto thing = BestThingNear(at.world, at.world.HandPlace(), k_ThrowReach,
	                                 [&at](entt::entity candidate) { return RockWeight(at.world, candidate); });
	if (thing == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = thing;
	return factor;
}

/// The mind's creature to impress the town above, from its store
float ScoreUseCreatureToImpressTown(ScoreAt& at)
{
	const auto above = AboveParent(at);
	const auto town = above != entt::null && at.world.IsTown(above) ? above : entt::entity {entt::null};
	const auto creature = PlayersCreature(at.world);
	if (town == entt::null || at.world.StoragePit(town) == entt::null || creature == entt::null)
	{
		return 0.0f;
	}
	if (const auto tied = at.world.LeaderLeashTiedTo(); tied != entt::null && at.world.TownOf(tied) == town)
	{
		return 0.0f;
	}
	const float value = CreatureJobWorth(at.world, at.world.StoragePit(town), true);
	at.state().thing = creature;
	return value;
}

/// The mind's creature to fight the creature above, unless its leash is tied to it
float ScoreAttackCreatureWithCreature(ScoreAt& at)
{
	const auto above = AboveParent(at);
	if (above == entt::null)
	{
		return 0.0f;
	}
	const auto target = at.world.IsCreature(above) ? above : entt::entity {entt::null};
	const auto creature = PlayersCreature(at.world);
	if (creature == entt::null || target == entt::null)
	{
		return 0.0f;
	}
	if (at.world.LeaderLeashTiedTo() == target)
	{
		return 0.0f;
	}
	at.state().thing = creature;
	return 1.0f;
}

/// The creature most threatening the mind's citadel and towns, in the mind's player's influence and not an ally's
float ScoreReactToAggressiveCreature(ScoreAt& at)
{
	const auto me = at.world.Me();
	const float reach = at.world.InfluencePower(me) + 1.0f;
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto creature : at.world.Creatures())
	{
		if (creature == PlayersCreature(at.world))
		{
			continue;
		}
		const auto place = at.world.Place(creature);
		if (at.world.MostInfluentialPlayerAt(place) != std::optional<PlayerNames>(me))
		{
			continue;
		}
		const auto citadel = at.world.MyCitadel();
		if (!citadel.has_value())
		{
			continue;
		}
		if (const auto owner = at.world.OwnerOf(creature); owner.has_value() && at.world.IsAllied(*owner, me))
		{
			continue;
		}
		float threat = gutils::GetDistanceModifier(gutils::GetDistanceInMetres(place, citadel->place), reach);
		for (const auto town : at.world.TownsOf(me))
		{
			const float distance = gutils::GetDistanceInMetres(place, at.world.TownAreaCentre(town));
			const float near = TownRadius(at.world.TownArea(town)) * k_TownNearShare;
			if (near > distance)
			{
				threat = threat + 1.0f;
			}
			threat = gutils::GetDistanceModifier(distance, at.world.TownInfluenceRadius(town)) + threat;
		}
		if (threat > best)
		{
			best = threat;
			chosen = creature;
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = chosen;
	return Below(best, 1.0f) ? best : 1.0f;
}

/// The mind's creature to draw a player away from the thing above
float ScoreUseCreatureAsDiverseionAgainstPlayer(ScoreAt& at)
{
	const auto above = AboveParent(at);
	const auto target = above != entt::null && at.world.HasPlace(above) ? above : entt::entity {entt::null};
	const auto creature = PlayersCreature(at.world);
	if (creature == entt::null || target == entt::null)
	{
		return 0.0f;
	}
	const float distance = gutils::GetDistanceInMetres(at.world.Place(creature), at.world.Place(target));
	const float fallOff = gutils::GetDistanceModifier(distance, DiversionReach(at.world));
	at.state().thing = creature;
	return CreatureFreeTime(at.world) * fallOff;
}

/// A physical shield over the object above, unless the mind cast one there
float ScoreCastPhysicalShieldToProtect(ScoreAt& at)
{
	const auto above = AboveParent(at);
	if (above == entt::null || !at.world.IsObject(above))
	{
		return 0.0f;
	}
	const auto place = at.world.Place(above);
	if (CastNear(at.world, at.mind, place, MagicType::PhysicalShield, 0.0f))
	{
		return 0.0f;
	}
	return TwiceSpellForNode(at, MagicType::PhysicalShield, place);
}

/// The town of the mind's player that a player harmed the most lately, against that player
float ScoreDefendTown(ScoreAt& at)
{
	const auto above = AboveParent(at);
	auto attacker = above != entt::null ? at.world.PlayerOf(above) : std::nullopt;
	const auto me = at.world.Me();
	float best = 0.0f;
	entt::entity town = entt::null;
	const auto turn = static_cast<double>(at.world.Turn());
	for (const auto candidate : at.world.TownsOf(me))
	{
		for (uint8_t number = 0; number < static_cast<uint8_t>(PlayerNames::_COUNT); ++number)
		{
			const auto player = static_cast<PlayerNames>(number);
			const auto type = at.world.PlayerType(player);
			if ((type != 1 && type != 2) || player == me)
			{
				continue;
			}
			const auto harm = at.world.TownAggression(candidate, player);
			if (AtMost(harm.aggression, 0.0f))
			{
				continue;
			}
			const auto lastTurn = static_cast<float>(harm.lastTurn);
			float strength = harm.aggression + harm.aggression;
			strength = Below(strength, 1.0f) ? strength : 1.0f;
			// the turn is exact and the difference rounds once, as the original's
			float faded = static_cast<float>(turn - static_cast<double>(lastTurn));
			faded = faded * FromBits(k_HarmFadeShareBits);
			faded = Below(faded, 1.0f) ? faded : 1.0f;
			const float left = 1.0f - faded;
			float value = left * left;
			value = value * left;
			value = strength * value;
			if (value > best || town == entt::null)
			{
				town = candidate;
				best = value;
				attacker = player;
			}
		}
	}
	if (town == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = town;
	// with nothing chosen above, the parent takes the attacker
	if (const auto parent = at.tree.nodes[at.node].parent;
	    parent.has_value() && ThingAbove(at.tree, at.mind, *parent) == entt::null)
	{
		at.mind.nodes[*parent].thing = attacker.has_value() ? at.world.PlayerThing(*attacker) : entt::entity {entt::null};
	}
	return best;
}

/// The worst damaged building of the mind's player's towns, as far as it can be reached
float ScoreCheckTownForDamage(ScoreAt& at)
{
	float lowest = 1.0f;
	entt::entity worst = entt::null;
	for (const auto town : at.world.TownsOf(at.world.Me()))
	{
		for (const auto abode : at.world.TownAbodes(town))
		{
			const float life = at.world.Life(abode);
			const float value = InfluenceFactor(at.world, at.world.Place(abode)) * life;
			if (Below(value, lowest))
			{
				lowest = value;
				worst = abode;
			}
		}
	}
	if (worst == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = worst;
	float value = lowest * lowest;
	value = 1.0f - value;
	return Below(lowest, k_BadlyDamaged) ? 1.0f : value;
}

/// The building above, when it burns
float ScoreFindBurningAbode(ScoreAt& at)
{
	const auto above = AboveParent(at);
	if (above == entt::null || !at.world.IsAbode(above) || !at.world.OnFire(above) ||
	    !at.world.FireAboveReactionTemperature(above))
	{
		return 0.0f;
	}
	at.state().thing = above;
	return 1.0f;
}

/// The building above, when nothing burns on it
float ScoreFindDamagedAbode(ScoreAt& at)
{
	const auto above = AboveParent(at);
	if (above == entt::null || !at.world.IsAbode(above) || at.world.OnFire(above))
	{
		return 0.0f;
	}
	at.state().thing = above;
	return 1.0f;
}

/// The building above, unless the mind cast its shield there: a shield on a burning one, else a physical shield
float ScoreFindUnprotectedAbode(ScoreAt& at)
{
	const auto above = AboveParent(at);
	if (above == entt::null || !at.world.IsAbode(above))
	{
		return 0.0f;
	}
	const auto type = at.world.OnFire(above) ? MagicType::Shield : MagicType::PhysicalShield;
	if (CastNear(at.world, at.mind, at.world.Place(above), type, 0.0f))
	{
		return 0.0f;
	}
	at.state().thing = above;
	return 1.0f;
}

/// The water miracle, anywhere
float ScoreCastWaterSpell(ScoreAt& at)
{
	return TwiceSpellForNode(at, MagicType::Water, std::nullopt);
}

/// The mind's player's town that a rival's belief threatens the most, while it still believes in the mind's player
/// more or less than in the others
float ScoreCheckTownsForBeliefAttack(ScoreAt& at)
{
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto town : at.world.TownsOf(at.world.Me()))
	{
		const float threat = at.world.RivalRecentRatio(town);
		const float left = at.world.BeliefNeededOrLeft(town);
		if (AtMost(threat, 0.0f) || Same(left, 0.0f))
		{
			continue;
		}
		if (threat > best)
		{
			best = threat;
			chosen = town;
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = chosen;
	return best;
}

/// A shield against the belief attack on the town above, by its importance
float ScoreCastSpellToDefendAgainstBelief(ScoreAt& at)
{
	const auto above = ThingAbove(at.tree, at.mind, at.node);
	if (above == entt::null || !at.world.IsTown(above) || BeliefAttackDefendObject(at.world, above) == entt::null)
	{
		return 0.0f;
	}
	const auto centre = at.world.TownAreaCentre(above);
	entt::entity icon = entt::null;
	const float value = BestSpell(at, centre, icon, [&](MagicType type) {
		if (type == MagicType::Shield)
		{
			return CastNear(at.world, at.mind, centre, type, 0.0f) ? 0.0f : 1.0f;
		}
		if (type == MagicType::PhysicalShield)
		{
			return CastNear(at.world, at.mind, centre, type, 0.0f) ? 0.0f : k_PhysicalShieldAgainstBelief;
		}
		return 0.0f;
	});
	const float worth = BeliefAttackImportance(at.world, above) * value;
	if (icon == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = icon;
	return worth;
}

/// A shield over the town above against its harm lately: a shield when the attack burnt more than it crushed and
/// hit, else a physical shield; nothing when the mind cast it there; twice the miracle's worth times the harm's fade
/// (not up to 1), the icon
float ScoreDefendtownWithSpells(ScoreAt& at)
{
	const auto above = AboveParent(at);
	if (above == entt::null || !at.world.IsTown(above))
	{
		return 0.0f;
	}
	const auto harm = HighestRecentHarm(at.world, above);
	if (!harm.has_value())
	{
		return 0.0f;
	}
	const auto type = harm->burn > harm->physical ? MagicType::Shield : MagicType::PhysicalShield;
	const auto centre = at.world.TownAreaCentre(above);
	if (CastNear(at.world, at.mind, centre, type, 0.0f))
	{
		return 0.0f;
	}
	entt::entity icon = entt::null;
	float value = SpellWorth(at.world, at.mind, type, centre, icon);
	value = value * harm->fade;
	value = value + value;
	// (the original then gives the node a word it never set; openblack's node keeps nothing, see the wiki)
	if (icon == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = icon;
	return value;
}

/// The mind's player's town harmed the most lately whose shield the mind has not cast there: a physical shield when the
/// attack crushed and hit more than it burnt, else a shield; the strictly highest fade, the town
float ScoreCheckTownForProtection(ScoreAt& at)
{
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto town : at.world.TownsOf(at.world.Me()))
	{
		const auto harm = HighestRecentHarm(at.world, town);
		if (!harm.has_value() || !(harm->fade > best))
		{
			continue;
		}
		const auto type = harm->physical > harm->burn ? MagicType::PhysicalShield : MagicType::Shield;
		if (CastNear(at.world, at.mind, at.world.TownAreaCentre(town), type, 0.0f))
		{
			continue;
		}
		best = harm->fade;
		chosen = town;
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = chosen;
	return best;
}

/// The mind's citadel heart struck by a thing that breaks buildings: nothing for 20 turns after the blow, then the
/// most, falling off over 600 turns since it; the heart as the thing once those 20 turns are over
float ScoreReactToPlayerThrowingStuff(ScoreAt& at)
{
	const auto heart = at.world.MyCitadelHeart();
	if (heart == entt::null)
	{
		return 0.0f;
	}
	const auto struck = at.world.HeartBreakingBlowTurn(heart);
	if (struck == 0)
	{
		return 0.0f;
	}
	// the turns since as a whole number that wraps round, so a blow at a later turn counts as long ago
	const uint32_t since = at.world.Turn() - struck;
	if (since <= k_ThrowingStuffQuietTurns)
	{
		return 0.0f;
	}
	float faded = MulRoundedOnce(FromBits(k_ThrowingStuffShareBits), static_cast<double>(since));
	faded = Below(faded, 1.0f) ? faded : 1.0f;
	at.state().thing = heart;
	return 1.0f - faded;
}

} // namespace

namespace scores
{

ecs::components::ComputerMindNode& ScoreAt::state()
{
	return mind.nodes[node];
}

entt::entity AboveParent(const ScoreAt& at)
{
	const auto parent = at.tree.nodes[at.node].parent;
	return parent.has_value() ? ThingAbove(at.tree, at.mind, *parent) : entt::entity {entt::null};
}

bool Below(float value, float than)
{
	return !(value >= than);
}

bool AtMost(float value, float than)
{
	return !(value > than);
}

bool Same(float value, float than)
{
	return !(value < than) && !(value > than);
}

int32_t Magnitude(int32_t value)
{
	return value < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(value)) : value;
}

float FromBits(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

std::optional<RecentHarm> HighestRecentHarm(const ScoreWorld& world, entt::entity town)
{
	RecentHarm harm;
	float best = 0.0f;
	for (uint8_t number = 0; number < k_HarmPlayers; ++number)
	{
		const auto player = static_cast<PlayerNames>(number);
		// the turns since as a whole number that wraps round, rounded once with the share
		const uint32_t since = world.Turn() - world.TownAggression(town, player).lastTurn;
		float faded = MulRoundedOnce(FromBits(k_RecentHarmShareBits), static_cast<double>(since));
		faded = Below(faded, 1.0f) ? faded : 1.0f;
		float cube = faded * faded;
		cube = cube * faded;
		const float fade = 1.0f - cube;
		if (fade > best)
		{
			best = fade;
			const auto effect = world.TownLastAttackEffect(town, player);
			harm = RecentHarm {.fade = fade, .burn = effect.burn, .physical = effect.crush + effect.hit};
		}
	}
	if (harm.burn > 0.0f || harm.physical > 0.0f)
	{
		return harm;
	}
	return std::nullopt;
}

uint32_t VillagersNear(const ScoreWorld& world, MapCoords place, uint32_t cells)
{
	uint32_t count = 0;
	map_coords::Spiral spiral;
	for (uint32_t cell = 0; cell < cells; ++cell)
	{
		count += static_cast<uint32_t>(world.VillagersAt(place).size());
		map_coords::AddCells(place, spiral.Next());
	}
	return count;
}

float PosWhereVillagersAre(ScoreWorld& world, entt::entity town, const MapCoords& from, MapCoords& where)
{
	if (world.RawDesire(town, k_SleepDesire) > k_MostSleep)
	{
		return 0.0f;
	}
	struct Location
	{
		MapCoords place;
		float score;
	};
	std::vector<Location> locations;
	const auto add = [&](const MapCoords& place) {
		const float fallOff = gutils::GetDistanceModifier(gutils::GetDistanceInMetres(from, place), k_VillagersFallOff);
		const auto count = VillagersNear(world, place, k_VillagersNearCells);
		const Location location {.place = place, .score = static_cast<float>(count) * fallOff};
		// it goes before the first one it beats, so that of equals the first stays first
		const auto before =
		    std::ranges::find_if(locations, [&location](const Location& other) { return location.score > other.score; });
		locations.insert(before, location);
	};
	if (const auto pit = world.StoragePit(town); pit != entt::null)
	{
		add(world.DoorPlace(pit));
	}
	add(world.CongregationPlace(town));
	if (const auto centre = world.TownCentre(town); centre != entt::null)
	{
		add(world.Place(centre));
	}
	for (const auto field : world.TownFields(town))
	{
		add(world.Place(field));
	}
	where = locations.front().place;
	return locations.front().score;
}

entt::entity BestAttackObject(const ScoreWorld& world, entt::entity town, const MapCoords& from)
{
	float best = 0.0f;
	entt::entity chosen = entt::null;
	const auto weigh = [&](entt::entity thing, float worth) {
		const float fallOff =
		    gutils::GetDistanceModifier(gutils::GetDistanceInMetres(from, world.Place(thing)), k_AttackFallOff);
		const float value = fallOff * worth;
		if (value > best)
		{
			best = value;
			chosen = thing;
		}
	};
	for (const auto abode : world.TownAbodes(town))
	{
		float worth = world.Life(abode) * world.AbodePercentBuilt(abode);
		worth = worth * world.AbodeAttackDesire(abode);
		weigh(abode, worth);
	}
	for (const auto field : world.TownFields(town))
	{
		const float full = world.FieldFood(field) / world.FieldFoodCapacity(field);
		weigh(field, full * world.FieldAttackDesire(field));
	}
	return chosen;
}

entt::entity BestThingNear(const ScoreWorld& world, const MapCoords& place, float reach,
                           const std::function<float(entt::entity)>& weight)
{
	const int16_t lowX = ReachEdge(place.x, reach, false);
	const int16_t lowZ = ReachEdge(place.z, reach, false);
	const int16_t highX = ReachEdge(place.x, reach, true);
	const int16_t highZ = ReachEdge(place.z, reach, true);
	const auto cells = world.MapCells();
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (int32_t x = lowX; x <= highX; ++x)
	{
		if (lowZ > highZ)
		{
			continue;
		}
		// the count is the high edge as an unsigned word less the low one, as the original counts it
		const int32_t count = static_cast<int32_t>(static_cast<uint16_t>(highZ)) - lowZ + 1;
		int32_t z = lowZ;
		for (int32_t step = 0; step < count; ++step, ++z)
		{
			const auto cellX = static_cast<uint16_t>(x);
			const auto cellZ = static_cast<uint16_t>(z);
			if (static_cast<uint32_t>(cellX) >= static_cast<uint32_t>(cells.x) ||
			    static_cast<uint32_t>(cellZ) >= static_cast<uint32_t>(cells.z))
			{
				continue;
			}
			const MapCoords middle {static_cast<int32_t>((static_cast<uint32_t>(cellX) << 16u) | k_HalfCell),
			                        static_cast<int32_t>((static_cast<uint32_t>(cellZ) << 16u) | k_HalfCell), 0.0f};
			if (!world.InMyInfluence(middle))
			{
				continue;
			}
			for (const auto thing : world.CellObjects(cellX, cellZ))
			{
				const float share = weight(thing);
				if (Below(share, best))
				{
					continue;
				}
				const float fallOff =
				    gutils::GetDistanceModifier(gutils::GetDistanceInMetres(place, world.Place(thing)), reach);
				const float value = fallOff * share;
				if (value > best)
				{
					best = value;
					chosen = thing;
				}
			}
		}
	}
	return chosen;
}

float FactorBeyond(const ScoreWorld& world, const MapCoords& nearest, const MapCoords& from)
{
	const float angle = gutils::Get3DAngleFromXZ(from, nearest);
	float distance = world.PlayerSpeed();
	distance = distance + distance;
	return InfluenceFactor(world, from + gutils::GetPosFromAngle(angle, distance));
}

float CreatureSize(const ScoreWorld& world, entt::entity creature)
{
	return world.CreatureScale(creature) * k_SizePerScale;
}

float DiversionReach(const ScoreWorld& world)
{
	float reach = CreatureSize(world, PlayersCreature(world));
	reach = reach + reach;
	return reach + k_DiversionReach;
}

float FightDistance(const ScoreWorld& world, entt::entity mine, entt::entity other)
{
	const float mineSize = CreatureSize(world, mine);
	float limit = CreatureSize(world, other) + mineSize;
	limit = limit + limit;
	return AtMost(limit, k_LeastFightDistance) ? k_LeastFightDistance : limit;
}

float SpellForNode(ScoreAt& at, MagicType type, const std::optional<MapCoords>& place)
{
	entt::entity icon = entt::null;
	const float value = SpellWorth(at.world, at.mind, type, place, icon);
	if (icon != entt::null)
	{
		at.state().thing = icon;
	}
	return value;
}

float BeliefAttackImportance(const ScoreWorld& world, entt::entity town)
{
	if (world.TownOwner(town) != world.Me())
	{
		return 1.0f;
	}
	const float value = world.RivalBeliefRatio(town) * k_RivalBeliefImportance;
	return Below(value, 1.0f) ? value : 1.0f;
}

float TwiceSpellForNode(ScoreAt& at, MagicType type, const std::optional<MapCoords>& place)
{
	float value = SpellForNode(at, type, place);
	value = value + value;
	if (!Below(value, 1.0f))
	{
		return 1.0f;
	}
	value = SpellForNode(at, type, place);
	return value + value;
}

} // namespace scores

entt::entity PlayersCreature(const ScoreWorld& world)
{
	const auto creature = world.MyCreature();
	return creature != entt::null && !world.ScriptControlled(creature) ? creature : entt::entity {entt::null};
}

float VirtualInfluenceReach(const ScoreWorld& world)
{
	const float share = world.InfluencePower(world.Me()) * k_PowerShareOfReach;
	return share + world.VirtualInfluenceMaxDistance();
}

std::optional<MapCoords> NearestPlaceInInfluence(const ScoreWorld& world, const MapCoords& place)
{
	if (world.InMyInfluence(place))
	{
		return place;
	}
	std::optional<MapCoords> nearest;
	float best = k_FarthestEdge;
	if (const auto citadel = world.MyCitadel(); citadel.has_value())
	{
		const float gap = gutils::GetDistanceInMetres(citadel->place, place) - citadel->influence;
		if (Below(gap, k_FarthestEdge))
		{
			const float angle = gutils::Get3DAngleFromXZ(citadel->place, place);
			nearest = citadel->place + gutils::GetPosFromAngle(angle, citadel->influence);
			best = gap;
		}
	}
	for (const auto town : world.TownsOf(world.Me()))
	{
		const auto centre = world.Place(town);
		const float radius = world.TownInfluenceRadius(town);
		const float gap = gutils::GetDistanceInMetres(centre, place) - radius;
		if (Below(gap, best))
		{
			const float angle = gutils::Get3DAngleFromXZ(centre, place);
			nearest = centre + gutils::GetPosFromAngle(angle, radius);
			best = gap;
		}
	}
	return nearest;
}

float InfluenceFactor(const ScoreWorld& world, const MapCoords& place)
{
	if (world.InMyInfluence(place))
	{
		return 1.0f;
	}
	const auto nearest = NearestPlaceInInfluence(world, place);
	if (!nearest.has_value())
	{
		return 0.0f;
	}
	const float distance = gutils::GetDistanceInMetres(place, *nearest);
	const float fallOff = gutils::GetDistanceModifier(distance, VirtualInfluenceReach(world));
	// the original compares with a double
	if (!(static_cast<double>(fallOff) >= k_LeastFallOff))
	{
		return 0.0f;
	}
	return fallOff * fallOff;
}

float ChargeUpSpeedDesire(const ScoreWorld& world, entt::entity icon)
{
	const auto site = world.IconSite(icon);
	const float cost = world.IconChantsToCreate(icon);
	float stored = world.IconChantStore(icon) + k_SmallPrayer;
	if (site != entt::null)
	{
		const float battery = world.SiteBattery(site);
		const float available = world.SiteChantsAvailable(site) + battery;
		stored = available + stored;
		if (Below(stored, cost) && Same(world.SiteChantsVillagersCanSupply(site), 0.0f))
		{
			return 0.0f;
		}
	}
	return stored / (cost + k_SmallPrayer);
}

float TurnsToCreate(const ScoreWorld& world, entt::entity icon)
{
	const auto site = world.IconSite(icon);
	if (site == entt::null)
	{
		return -1.0f;
	}
	float needed = world.IconChantsToCreate(icon) - world.IconChantStore(icon);
	needed = needed - world.SiteTotalChantsAvailable(site);
	if (AtMost(needed, 0.0f))
	{
		return 0.0f;
	}
	const float supply = world.SiteChantsVillagersCanSupply(site);
	if (Same(supply, 0.0f))
	{
		return -1.0f;
	}
	return needed / supply;
}

float IconWant(const ScoreWorld& world, ComputerMind& mind, MagicType type, entt::entity& icon)
{
	icon = entt::null;
	if (!world.MyCitadel().has_value())
	{
		return 0.0f;
	}
	const auto found = world.CitadelSpellIcon(type);
	if (found == entt::null)
	{
		return 0.0f;
	}
	const auto site = world.IconSite(found);
	const float desire = ChargeUpSpeedDesire(world, found);
	++mind.iconsFound.at(static_cast<size_t>(type));
	if (Below(desire, 1.0f) &&
	    (site == entt::null || !world.SiteBuilt(site) || !world.SiteRepaired(site) || world.SiteWorshippers(site) == 0))
	{
		return 0.0f;
	}
	icon = found;
	return desire;
}

float SpellWorth(const ScoreWorld& world, ComputerMind& mind, MagicType type, const std::optional<MapCoords>& place,
                 entt::entity& icon)
{
	if (!world.IsMagicTypeEnabled(type))
	{
		return 0.0f;
	}
	entt::entity found = entt::null;
	float keenness = IconWant(world, mind, type, found);
	// a keenness below 1 is asked again (and counted again)
	keenness = Below(keenness, 1.0f) ? IconWant(world, mind, type, found) : 1.0f;
	if (found == entt::null)
	{
		return 0.0f;
	}
	const float turns = TurnsToCreate(world, found);
	if (Below(turns, 0.0f))
	{
		return 0.0f;
	}
	const float fallOff = gutils::GetDistanceModifier(turns, k_TurnsToCreateFallOff);
	const float reach = place.has_value() ? InfluenceFactor(world, *place) : 1.0f;
	icon = found;
	float value = reach * fallOff;
	value = value * keenness;
	return Below(value, 1.0f) ? value : 1.0f;
}

bool CastNear(const ScoreWorld& world, const ComputerMind& mind, const MapCoords& place, MagicType type, float minRadius)
{
	for (const auto spell : mind.spellsCast)
	{
		const auto cast = world.SpellCast(spell);
		const auto gapX = static_cast<int32_t>(static_cast<uint32_t>(cast.place.x) - static_cast<uint32_t>(place.x));
		const auto gapZ = static_cast<int32_t>(static_cast<uint32_t>(cast.place.z) - static_cast<uint32_t>(place.z));
		if (Magnitude(gapX) >= k_CastNearGap || Magnitude(gapZ) >= k_CastNearGap || cast.type != type)
		{
			continue;
		}
		if (!Below(cast.radius, minRadius) || Same(minRadius, 0.0f))
		{
			return true;
		}
	}
	return false;
}

float CreatureFreeTime(const ScoreWorld& world)
{
	const auto creature = PlayersCreature(world);
	if (creature == entt::null)
	{
		return 0.0f;
	}
	const auto leash = world.LeashOn();
	if (!leash.has_value())
	{
		return 1.0f;
	}
	const auto tied = static_cast<float>(world.Turn() - leash->tiedTurn);
	if (AtMost(tied, k_TiedTurnsBeforeFree) || world.IsFighting(creature))
	{
		return 0.0f;
	}
	float time = tied - k_TiedTurnsBeforeFree;
	time = time * FromBits(k_TiedTurnsShareBits);
	const float life = world.Life(creature);
	const float lifeShare = AtMost(life, 0.5f) ? world.Life(creature) : 1.0f;
	const float exhaustion = world.CreatureExhaustion(creature);
	const float restShare = Below(exhaustion, 0.5f) ? 1.0f : 1.0f - exhaustion;
	const float energy = world.CreatureEnergy(creature);
	const float energyShare = AtMost(energy, 0.5f) ? energy : 1.0f;
	float free = energyShare * restShare;
	free = lifeShare * free;
	if (world.PlanDesire(creature) == std::optional<uint32_t>(k_RestoreHealthDesire))
	{
		free = 0.0f;
	}
	free = free * time;
	return Below(free, 1.0f) ? free : 1.0f;
}

float CreatureJobWorth(const ScoreWorld& world, entt::entity target, bool sameTown)
{
	const auto creature = PlayersCreature(world);
	if (creature == entt::null)
	{
		return 0.0f;
	}
	if (const auto leash = world.LeashOn(); leash.has_value() && leash->tiedTo != entt::null)
	{
		if (leash->tiedTo == target)
		{
			return 0.0f;
		}
		if (sameTown)
		{
			const auto town = world.TownOf(leash->tiedTo);
			if (town != entt::null && town == world.TownOf(target))
			{
				return 0.0f;
			}
		}
	}
	const float distance = gutils::GetDistanceInMetres(world.Place(creature), world.Place(target));
	float reach = CreatureSize(world, creature) * k_JobReachPerSize;
	reach = reach + k_JobReach;
	const float fallOff = gutils::GetDistanceModifier(distance, reach);
	const float exhaustion = world.CreatureExhaustion(creature);
	float value = 1.0f - exhaustion;
	value = CreatureFreeTime(world) * value;
	return value * fallOff;
}

float TownRadius(const ScoreWorld::Area& area)
{
	// each side in metres: the map units times the metres per cell (rounded once), over 65536
	constexpr float k_UnitsPerCell = 65536.0f;
	const auto metres = [](int32_t units) {
		const auto scaled = static_cast<float>(static_cast<double>(units) * gutils::k_MetresPerCell);
		return scaled / k_UnitsPerCell;
	};
	const auto across = Magnitude(static_cast<int32_t>(metres(area.max.x) - metres(area.min.x)));
	const auto deep = Magnitude(static_cast<int32_t>(metres(area.max.z) - metres(area.min.z)));
	return static_cast<float>(across > deep ? across : deep) * 0.5f;
}

bool IsScorePorted(ScoreKind kind)
{
	switch (kind)
	{
	case ScoreKind::TalktoPlayer:
	case ScoreKind::GiveToPlayer:
	case ScoreKind::ThrowThingAtPlayer:
	case ScoreKind::ReactToPlayerCastingSpells:
	case ScoreKind::TeachCreatureAggressiveMagic164:
	case ScoreKind::ThrowrockatCreature:
	case ScoreKind::GetCreaturetoHelp:
	case ScoreKind::UseCreatureToImpressTown:
	case ScoreKind::AttackCreatureWithCreature:
	case ScoreKind::ReactToAggressiveCreature:
	case ScoreKind::UseCreatureAsDiverseionAgainstPlayer:
	case ScoreKind::ReactToPlayerThrowingStuff:
	case ScoreKind::CastPhysicalShieldToProtect:
	case ScoreKind::DefendTown:
	case ScoreKind::CheckTownForDamage:
	case ScoreKind::FindBurningAbode:
	case ScoreKind::FindDamagedAbode:
	case ScoreKind::FindUnprotectedAbode:
	case ScoreKind::CastWaterSpell:
	case ScoreKind::CheckTownsForBeliefAttack:
	case ScoreKind::CastSpellToDefendAgainstBelief:
	case ScoreKind::DefendtownWithSpells:
	case ScoreKind::CheckTownForProtection:
	case ScoreKind::CreatureInteract:
	case ScoreKind::GiveCreatureFood:
	case ScoreKind::GetFoodFromField:
	case ScoreKind::GetFoodFromFishFarm:
	case ScoreKind::CastFoodSpellOn:
	case ScoreKind::MakeCreatureDrink:
	case ScoreKind::RestoreCreatureHealth:
	case ScoreKind::RestoreMyCreatureHealth:
	case ScoreKind::MakeCreatureSleep:
	case ScoreKind::SendCreatureHome:
	case ScoreKind::MoveCreaturetoWarmerArea:
	case ScoreKind::MoveCreaturetoCoollerArea:
	case ScoreKind::TeachCreatureNormalAction:
	case ScoreKind::TeachCreatureAggressiveMagic:
	case ScoreKind::TeachCreatureCompassionateMagic:
	case ScoreKind::TeachCreatureCompassionateMagic166:
	case ScoreKind::TeachCreatureImpressiveMagic168:
	case ScoreKind::TeachCreatureImpressiveMagic:
	case ScoreKind::ThrowFoodatCreature:
	case ScoreKind::SlapCreature:
	case ScoreKind::StrokeCreature:
	case ScoreKind::AttachLeashtoMovingObject:
	case ScoreKind::FindFreeVillagerInArea:
	case ScoreKind::CastSpellOnMyCreatureForALaugh:
	case ScoreKind::CastSpellOnMyCreatureForALaugh170:
	case ScoreKind::ReacttoCreatureStarvingEmergency:
	case ScoreKind::GetFoodFromOtherTownForObject:
	case ScoreKind::ReacttoCreatureIllEmergency:
	case ScoreKind::TownWithHighestFoodDesire:
	case ScoreKind::CreateDiscipleToFulfillTownDesire:
	case ScoreKind::TownWithHighestWoodDesire:
	case ScoreKind::ShallCastWoodSpell:
	case ScoreKind::GetWoodFromForest:
	case ScoreKind::GetWoodFromOtherTown:
	case ScoreKind::GiveBuildingSiteWood:
	case ScoreKind::GiveBuildingSiteWorkers:
	case ScoreKind::FindFreeVillagerBuilderInTown:
	case ScoreKind::CombineScaffolds:
	case ScoreKind::FindScaffoldToCombineWith:
	case ScoreKind::FindPlaceToUseScaffoldForBuilding:
	case ScoreKind::FindLocationToUseScaffold:
	case ScoreKind::FillWorkshop:
	case ScoreKind::GiveWorshippersFood:
	case ScoreKind::WorshipSiteThatNeedsVillagers:
	case ScoreKind::RaiseTotemInTown:
	case ScoreKind::TakeVillagerFromTownToWorshipSite:
	case ScoreKind::ChargeUpSpell:
	case ScoreKind::MakeDiscipleBreader:
	case ScoreKind::DefeatPlayer:
	case ScoreKind::DefeatPlayer37:
	case ScoreKind::ExpandInfluence:
	case ScoreKind::TakeOverTownByFulfillingFoodDesire:
	case ScoreKind::TakeOverTown:
	case ScoreKind::ThrowThingsToImpress:
	case ScoreKind::CastSpellToFulfilTownNeeds:
	case ScoreKind::DestroyTown:
	case ScoreKind::DestroytownWithCreature:
	case ScoreKind::PlayerWithMoreSpells:
	case ScoreKind::PlayerWithBetterCreature:
	case ScoreKind::AttackCreature:
	case ScoreKind::PlayerWithMoreWorshippers:
		return true;
	default:
		return false;
	}
}

float Score(ScoreKind kind, const MindTree& tree, ComputerMind& mind, uint16_t node, ScoreWorld& world)
{
	ScoreAt at {.world = world, .mind = mind, .tree = tree, .node = node};
	switch (kind)
	{
	case ScoreKind::TalktoPlayer:
	case ScoreKind::GiveToPlayer:
	case ScoreKind::ThrowThingAtPlayer:
	case ScoreKind::ReactToPlayerCastingSpells:
	case ScoreKind::ReacttoCreatureIllEmergency:
	case ScoreKind::RaiseTotemInTown:
	case ScoreKind::PlayerWithMoreWorshippers:
		return ScoreNothing(at);
	case ScoreKind::DefeatPlayer:
		return 1.0f;
	case ScoreKind::TeachCreatureAggressiveMagic164:
		return ScoreTeachCreatureAggressiveMagic164(at);
	case ScoreKind::ThrowrockatCreature:
		return ScoreThrowrockatCreature(at);
	case ScoreKind::GetCreaturetoHelp:
	case ScoreKind::UseCreatureToImpressTown:
	case ScoreKind::DestroytownWithCreature:
		return ScoreUseCreatureToImpressTown(at);
	case ScoreKind::AttackCreatureWithCreature:
		return ScoreAttackCreatureWithCreature(at);
	case ScoreKind::ReactToAggressiveCreature:
		return ScoreReactToAggressiveCreature(at);
	case ScoreKind::UseCreatureAsDiverseionAgainstPlayer:
		return ScoreUseCreatureAsDiverseionAgainstPlayer(at);
	case ScoreKind::ReactToPlayerThrowingStuff:
		return ScoreReactToPlayerThrowingStuff(at);
	case ScoreKind::CastPhysicalShieldToProtect:
		return ScoreCastPhysicalShieldToProtect(at);
	case ScoreKind::DefendTown:
		return ScoreDefendTown(at);
	case ScoreKind::CheckTownForDamage:
		return ScoreCheckTownForDamage(at);
	case ScoreKind::FindBurningAbode:
		return ScoreFindBurningAbode(at);
	case ScoreKind::FindDamagedAbode:
		return ScoreFindDamagedAbode(at);
	case ScoreKind::FindUnprotectedAbode:
		return ScoreFindUnprotectedAbode(at);
	case ScoreKind::CastWaterSpell:
		return ScoreCastWaterSpell(at);
	case ScoreKind::CheckTownsForBeliefAttack:
		return ScoreCheckTownsForBeliefAttack(at);
	case ScoreKind::CastSpellToDefendAgainstBelief:
		return ScoreCastSpellToDefendAgainstBelief(at);
	case ScoreKind::DefendtownWithSpells:
		return ScoreDefendtownWithSpells(at);
	case ScoreKind::CheckTownForProtection:
		return ScoreCheckTownForProtection(at);
	case ScoreKind::CreatureInteract:
		return ScoreCreatureInteract(at);
	case ScoreKind::GiveCreatureFood:
		return ScoreCreatureDesire(at, k_HungerDesire, 1.0f);
	case ScoreKind::GetFoodFromField:
		return ScoreGetFoodFromField(at);
	case ScoreKind::GetFoodFromFishFarm:
		return ScoreGetFoodFromFishFarm(at);
	case ScoreKind::CastFoodSpellOn:
		return ScoreCastFoodSpellOn(at);
	case ScoreKind::MakeCreatureDrink:
		return ScoreCreatureDesire(at, k_ThirstDesire, 1.0f);
	case ScoreKind::RestoreCreatureHealth:
		return ScoreRestoreCreatureHealth(at);
	case ScoreKind::RestoreMyCreatureHealth:
		return ScoreRestoreMyCreatureHealth(at);
	case ScoreKind::MakeCreatureSleep:
		return ScoreCreatureDesire(at, k_TirednessDesire, 1.0f);
	case ScoreKind::SendCreatureHome:
		return ScoreCreatureDesire(at, k_StayNearHomeDesire, 1.0f);
	case ScoreKind::MoveCreaturetoWarmerArea:
		return ScoreCreatureDesire(at, k_WarmerDesire, 0.5f);
	case ScoreKind::MoveCreaturetoCoollerArea:
		return ScoreCreatureDesire(at, k_ColderDesire, 0.1f);
	case ScoreKind::TeachCreatureNormalAction:
		return ScoreTeachCreatureNormalAction(at);
	case ScoreKind::TeachCreatureAggressiveMagic:
		return ScoreTeachCreatureMagic(at, mind.destroyLessons);
	case ScoreKind::TeachCreatureCompassionateMagic:
		return ScoreTeachCreatureMagic(at, mind.helpLessons);
	case ScoreKind::TeachCreatureCompassionateMagic166:
		return ScoreTeachCreatureCompassionateMagic166(at);
	case ScoreKind::TeachCreatureImpressiveMagic168:
		return ScoreTeachCreatureImpressiveMagic168(at);
	case ScoreKind::TeachCreatureImpressiveMagic:
		return ScoreTeachCreatureMagic(at, mind.impressLessons);
	case ScoreKind::ThrowFoodatCreature:
		return ScorePlayWithCreature(at, 0.3f, 0.2f, false);
	case ScoreKind::SlapCreature:
		return ScorePlayWithCreature(at, 0.1f, 0.0f, false);
	case ScoreKind::StrokeCreature:
		return ScorePlayWithCreature(at, 0.1f, 0.1f, false);
	case ScoreKind::AttachLeashtoMovingObject:
		return ScorePlayWithCreature(at, 0.3f, 0.2f, true);
	case ScoreKind::FindFreeVillagerInArea:
		return ScoreFindFreeVillagerInArea(at);
	case ScoreKind::CastSpellOnMyCreatureForALaugh:
		return ScoreCastSpellOnMyCreatureForALaugh(at);
	case ScoreKind::CastSpellOnMyCreatureForALaugh170:
		return ScoreCastSpellOnMyCreatureForALaugh170(at);
	case ScoreKind::ReacttoCreatureStarvingEmergency:
		return ScoreReacttoCreatureStarvingEmergency(at);
	case ScoreKind::GetFoodFromOtherTownForObject:
		return ScoreTakeFromOtherTown(at, 0);
	case ScoreKind::TownWithHighestFoodDesire:
		return ScoreTownWithHighestFoodDesire(at);
	case ScoreKind::CreateDiscipleToFulfillTownDesire:
		return ScoreCreateDiscipleToFulfillTownDesire(at);
	case ScoreKind::TownWithHighestWoodDesire:
		return ScoreTownWithHighestWoodDesire(at);
	case ScoreKind::ShallCastWoodSpell:
		return ScoreShallCastWoodSpell(at);
	case ScoreKind::GetWoodFromForest:
		return ScoreGetWoodFromForest(at);
	case ScoreKind::GetWoodFromOtherTown:
		return ScoreTakeFromOtherTown(at, 1);
	case ScoreKind::GiveBuildingSiteWood:
		return ScoreGiveBuildingSiteWood(at);
	case ScoreKind::GiveBuildingSiteWorkers:
		return ScoreGiveBuildingSiteWorkers(at);
	case ScoreKind::FindFreeVillagerBuilderInTown:
		return ScoreFindFreeVillagerBuilderInTown(at);
	case ScoreKind::CombineScaffolds:
		return ScoreCombineScaffolds(at);
	case ScoreKind::FindScaffoldToCombineWith:
		return ScoreFindScaffoldToCombineWith(at);
	case ScoreKind::FindPlaceToUseScaffoldForBuilding:
		return ScoreFindPlaceToUseScaffoldForBuilding(at);
	case ScoreKind::FindLocationToUseScaffold:
		return ScoreFindLocationToUseScaffold(at);
	case ScoreKind::FillWorkshop:
		return ScoreFillWorkshop(at);
	case ScoreKind::GiveWorshippersFood:
		return ScoreGiveWorshippersFood(at);
	case ScoreKind::WorshipSiteThatNeedsVillagers:
		return ScoreWorshipSiteThatNeedsVillagers(at);
	case ScoreKind::TakeVillagerFromTownToWorshipSite:
		return ScoreTakeVillagerFromTownToWorshipSite(at);
	case ScoreKind::ChargeUpSpell:
		return ScoreChargeUpSpell(at);
	case ScoreKind::MakeDiscipleBreader:
		return ScoreMakeDiscipleBreader(at);
	case ScoreKind::ExpandInfluence:
		return ScoreExpandInfluence(at);
	case ScoreKind::TakeOverTownByFulfillingFoodDesire:
		return ScoreTakeOverTownByFulfillingFoodDesire(at);
	case ScoreKind::TakeOverTown:
	case ScoreKind::DestroyTown:
		return ScoreTakeOverTown(at);
	case ScoreKind::ThrowThingsToImpress:
		return ScoreThrowThingsToImpress(at);
	case ScoreKind::CastSpellToFulfilTownNeeds:
		return ScoreCastSpellToFulfilTownNeeds(at);
	case ScoreKind::PlayerWithMoreSpells:
	case ScoreKind::PlayerWithBetterCreature:
		return ScorePlayerWithBetterCreature(at);
	case ScoreKind::AttackCreature:
		return ScoreAttackCreature(at);
	case ScoreKind::DefeatPlayer37:
		return ScoreDefeatPlayer37(at);
	default:
		// a way of scoring openblack lacks scores nothing (the wiki lists what each still needs)
		return 0.0f;
	}
}

} // namespace openblack::computer_player
