/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The score rules of the CreatureInteract aim: the mind looks after its creature, teaches it and plays with it

#include <cstdint>

#include <optional>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ComputerPlayerScoresDetail.h"
#include "Creature/CreatureBodyActivity.h"

using openblack::map_coords::MapCoords;

namespace openblack::computer_player::scores
{

namespace
{

/// The least a field's food counts for before a field is chosen
constexpr float k_LeastFieldWorth = 0.1f;
/// A little food, so that an empty field divides by more than 0
constexpr float k_LittleFood = 0.001f;
/// The distance over which a field or fish farm's pull falls off
constexpr float k_FoodFallOff = 250.0f;
/// The creature counts as hurt at or below this life
constexpr float k_HurtLife = 0.2f;
/// What an ordinary action still to be taught is worth
constexpr float k_NormalActionWorth = 0.3f;
/// What each help, destruction or impression of a creature still learning adds to the will to teach it
constexpr float k_LessonShare = 0.7f;
/// The desire a miracle makes a watcher feel that is compassion
constexpr uint32_t k_CompassionDesire = 1;
/// What a miracle cast on the creature for a laugh is worth
constexpr uint32_t k_LaughWorthBits = 0x3D1332AEu;
/// The amount of food a starving creature's hunger asks for, per unit of hunger
constexpr float k_FoodPerHunger = 1000.0f;
/// Each part of the parent's worth asks for this much of the resource, and below this much nothing is asked
constexpr float k_ResourcePerWorth = 1000.0f;
constexpr float k_LeastResourceAsked = 50.0f;
/// The cells around the hand looked at for a free villager
constexpr uint32_t k_VillagerCells = 36;
/// The creature's desire for food, by its number
constexpr uint32_t k_HungerDesire = 4;

/// The food a field has, against the most it can hold
float FieldShare(const ScoreWorld& world, entt::entity field)
{
	const float food = world.FieldFood(field) + k_LittleFood;
	const float capacity = world.FieldFoodCapacity(field) + k_LittleFood;
	return food / capacity;
}

/// The best of the player's fields or fish farms, by its share of food (a fish farm's is 1), the distance from the
/// hand and the influence at it, strictly above `least`
template <typename List, typename Share>
float BestFoodPlace(ScoreAt& at, float least, List list, Share share)
{
	float best = least;
	entt::entity chosen = entt::null;
	const auto hand = at.world.HandPlace();
	for (const auto town : at.world.TownsOf(at.world.Me()))
	{
		for (const auto place : list(town))
		{
			const float part = share(place);
			const auto where = at.world.Place(place);
			float value = gutils::GetDistanceModifier(gutils::GetDistanceInMetres(hand, where), k_FoodFallOff);
			value = value * part;
			value = InfluenceFactor(at.world, where) * value;
			if (value > best)
			{
				best = value;
				chosen = place;
			}
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = chosen;
	return best;
}

} // namespace

float ScoreCreatureInteract(ScoreAt& at)
{
	const auto creature = PlayersCreature(at.world);
	if (creature == entt::null || creature_body::IsPerformingAction(at.world.CreatureBodyActivity(creature)))
	{
		return 0.0f;
	}
	return CreatureFreeTime(at.world);
}

float ScoreCreatureDesire(ScoreAt& at, uint32_t desire, float share)
{
	const auto creature = PlayersCreature(at.world);
	if (creature == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = creature;
	const float level = at.world.CreatureDesire(creature, desire);
	return share == 1.0f ? level : level * share;
}

float ScoreGetFoodFromField(ScoreAt& at)
{
	return BestFoodPlace(
	    at, k_LeastFieldWorth, [&](entt::entity town) { return at.world.TownFields(town); },
	    [&](entt::entity field) { return FieldShare(at.world, field); });
}

float ScoreGetFoodFromFishFarm(ScoreAt& at)
{
	return BestFoodPlace(
	    at, 0.0f, [&](entt::entity town) { return at.world.TownFishFarms(town); }, [](entt::entity /*farm*/) { return 1.0f; });
}

float ScoreCastFoodSpellOn(ScoreAt& at)
{
	const auto above = AboveParent(at);
	std::optional<MapCoords> place;
	if (above != entt::null && at.world.IsTown(above))
	{
		if (const auto drop = at.world.ResourceDropObject(above, 0); drop != entt::null)
		{
			place = at.world.Place(drop);
		}
	}
	else if (above != entt::null && at.world.HasPlace(above))
	{
		place = at.world.Place(above);
	}
	return SpellForNode(at, MagicType::Food, place);
}

float ScoreRestoreCreatureHealth(ScoreAt& at)
{
	const auto creature = PlayersCreature(at.world);
	if (creature == entt::null || at.world.Life(creature) > k_HurtLife)
	{
		return 0.0f;
	}
	at.state().thing = creature;
	return 1.0f - at.world.Life(creature);
}

float ScoreRestoreMyCreatureHealth(ScoreAt& at)
{
	return SpellForNode(at, MagicType::Heal, std::nullopt);
}

float ScoreTeachCreatureNormalAction(ScoreAt& at)
{
	const auto creature = PlayersCreature(at.world);
	if (creature == entt::null || at.world.UnknownNormalActions(creature) == 0)
	{
		return 0.0f;
	}
	at.state().thing = creature;
	return k_NormalActionWorth;
}

float ScoreTeachCreatureMagic(ScoreAt& at, uint32_t lessons)
{
	const auto creature = PlayersCreature(at.world);
	if (creature == entt::null)
	{
		return 0.0f;
	}
	const float value = static_cast<float>(lessons) * k_LessonShare;
	at.state().thing = creature;
	return AtMost(value, 1.0f) ? value : 1.0f;
}

float ScoreTeachCreatureCompassionateMagic166(ScoreAt& at)
{
	entt::entity icon = entt::null;
	const float value = BestSpell(at, std::nullopt, icon, [&](MagicType type) {
		return at.world.PerceivedDesire(type) == k_CompassionDesire ? 1.0f : 0.0f;
	});
	if (icon != entt::null)
	{
		at.state().thing = icon;
	}
	return value;
}

float ScoreTeachCreatureImpressiveMagic168(ScoreAt& at)
{
	const auto above = AboveParent(at);
	if (above == entt::null || !at.world.IsTown(above))
	{
		return 0.0f;
	}
	entt::entity icon = entt::null;
	const float value =
	    BestSpell(at, at.world.Place(above), icon, [&](MagicType type) { return at.world.MagicImpressiveBalance(type); });
	// how likely the town's villagers are to see it, up to 1: asked again when below 1, as the original does
	MapCoords where {};
	float seen = PosWhereVillagersAre(at.world, above, at.world.HandPlace(), where);
	seen = Below(seen, 1.0f) ? PosWhereVillagersAre(at.world, above, at.world.HandPlace(), where) : 1.0f;
	const float worth = seen * value;
	if (icon != entt::null)
	{
		at.state().thing = icon;
		at.state().point = at.world.WorldPoint(where);
	}
	return worth;
}

float ScorePlayWithCreature(ScoreAt& at, float most, float least, bool offTheLeash)
{
	const auto creature = PlayersCreature(at.world);
	if (creature == entt::null || (offTheLeash && at.world.LeashOn().has_value()))
	{
		return 0.0f;
	}
	at.state().thing = creature;
	const float draw = at.world.GameFloatRand(most);
	return least == 0.0f ? draw : draw + least;
}

bool FreeVillager(ScoreAt& at, entt::entity villager, float& best)
{
	const auto facts = at.world.VillagerFactsOf(villager);
	if (!facts.available || facts.child || !facts.reachable || !facts.idle || facts.atWorshipSite || facts.disciple ||
	    facts.building || at.world.ScriptControlled(villager))
	{
		return false;
	}
	const float reach = at.world.InfluencePower(at.world.Me());
	const auto place = at.world.Place(villager);
	const float fallOff = gutils::GetDistanceModifier(gutils::GetDistanceInMetres(at.world.HandPlace(), place), reach);
	float value = at.world.Life(villager) * fallOff;
	value = InfluenceFactor(at.world, place) * value;
	if (AtMost(value, best))
	{
		return false;
	}
	best = fallOff;
	return true;
}

float ScoreFindFreeVillagerInArea(ScoreAt& at)
{
	auto place = at.world.HandPlace();
	map_coords::Spiral spiral;
	float best = 0.0f;
	entt::entity found = entt::null;
	for (uint32_t cell = 0; cell < k_VillagerCells; ++cell)
	{
		for (const auto villager : at.world.VillagersAt(place))
		{
			if (FreeVillager(at, villager, best))
			{
				found = villager;
			}
		}
		if (found != entt::null)
		{
			at.state().thing = found;
			return best;
		}
		map_coords::AddCells(place, spiral.Next());
	}
	return 0.0f;
}

float ScoreCastSpellOnMyCreatureForALaugh(ScoreAt& at)
{
	const auto creature = PlayersCreature(at.world);
	if (creature == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = creature;
	return at.world.AnySpellOnCreature(creature) ? 0.0f : FromBits(k_LaughWorthBits);
}

float ScoreCastSpellOnMyCreatureForALaugh170(ScoreAt& at)
{
	const auto above = AboveParent(at);
	// at the town's building most worth attacking from the hand, else the town's middle; at a thing's place; or at the
	// map's corner, the original's place when there is nothing above
	MapCoords place {};
	if (above != entt::null && at.world.IsTown(above))
	{
		const auto target = BestAttackObject(at.world, above, at.world.HandPlace());
		place = target != entt::null ? at.world.Place(target) : at.world.TownAreaCentre(above);
	}
	else if (above != entt::null && at.world.HasPlace(above))
	{
		place = at.world.Place(above);
	}
	entt::entity icon = entt::null;
	const float value = BestSpell(at, place, icon, [&](MagicType type) { return at.world.MagicAggressiveAttackValue(type); });
	if (icon != entt::null)
	{
		at.state().thing = icon;
	}
	return value;
}

float ScoreReacttoCreatureStarvingEmergency(ScoreAt& at)
{
	const auto creature = PlayersCreature(at.world);
	if (creature == entt::null || at.world.CreatureEnergy(creature) > at.world.CreatureLowEnergyThreshold(creature))
	{
		return 0.0f;
	}
	at.state().thing = creature;
	const float hunger = at.world.CreatureDesire(creature, k_HungerDesire);
	const float value = Below(hunger, 1.0f) ? hunger : 1.0f;
	// the food it asks for, truncated
	at.state().intValue = static_cast<int32_t>(value * k_FoodPerHunger);
	return value;
}

float ScoreTakeFromOtherTown(ScoreAt& at, uint32_t resource)
{
	const auto above = AboveParent(at);
	if (above == entt::null)
	{
		return 0.0f;
	}
	const auto parent = *at.tree.nodes[at.node].parent;
	const float parentWorth = at.mind.nodes[parent].score;
	const float asked = parentWorth * k_ResourcePerWorth;
	if (AtMost(asked, k_LeastResourceAsked))
	{
		return 0.0f;
	}
	const uint32_t kind = resource != 0 ? 1 : 0;
	const auto wanted = static_cast<uint32_t>(static_cast<int32_t>(asked));
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto town : at.world.TownsOf(at.world.Me()))
	{
		if (town == above)
		{
			continue;
		}
		const auto pit = at.world.StoragePit(town);
		if (pit == entt::null || at.world.AbodeResource(pit, kind) <= wanted)
		{
			continue;
		}
		const float desire = at.world.RawDesire(town, resource);
		const float want = Below(desire, 1.0f) ? at.world.RawDesire(town, resource) : 1.0f;
		const float value = InfluenceFactor(at.world, at.world.Place(pit)) * want;
		if (Below(value, parentWorth))
		{
			const float left = 1.0f - value;
			if (left > best)
			{
				best = left;
				chosen = town;
			}
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	const auto pit = at.world.StoragePit(chosen);
	// the wood piles from the last; food has one
	for (int32_t index = kind != 0 ? 4 : 0; index >= 0; --index)
	{
		const auto pile = at.world.StorePile(pit, kind, static_cast<uint32_t>(index));
		if (pile != entt::null && at.world.PileAmount(pile) != 0)
		{
			at.state().floatValue = asked;
			at.state().thing = pile;
			return best;
		}
	}
	return 0.0f;
}

} // namespace openblack::computer_player::scores
