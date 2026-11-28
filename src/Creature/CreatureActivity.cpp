/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureActivity.h"

#include "Creature/CreatureDecisionTree.h"

using namespace openblack;
using namespace openblack::creature_activity;
using creature_desires::Desire;

std::optional<Whole> creature_activity::WholeOf(uint32_t beliefKind)
{
	switch (beliefKind)
	{
	case creature_tree::belief_types::k_Town:
		return Whole::Town;
	case creature_tree::belief_types::k_Forest:
		return Whole::Forest;
	case creature_tree::belief_types::k_Citadel:
		return Whole::Citadel;
	case creature_tree::belief_types::k_Creature:
		return Whole::Creature;
	case creature_tree::belief_types::k_Flock:
		return Whole::Flock;
	default:
		return std::nullopt;
	}
}

namespace
{
/// Whether anger can be about a whole thing, by the kind of the thing
bool AngerCanBeAbout(const WholeFacts& whole)
{
	switch (whole.kind)
	{
	case Whole::Town:
		// A town of no player always counts
		return !(whole.hasPlayer && whole.samePlayer) || whole.lookedAt;
	case Whole::Creature:
		return whole.attackable;
	case Whole::Citadel:
		return !whole.samePlayer;
	case Whole::Forest:
	case Whole::Flock:
		return true;
	}
	return false;
}
} // namespace

bool creature_activity::CanBeAbout(Desire desire, const WholeFacts& whole, bool mayAttackOwnPlayer)
{
	switch (desire)
	{
	case Desire::Impress:
		return whole.kind == Whole::Town && !(whole.believesInPlayer && whole.samePlayer);
	case Desire::Compassion:
		return whole.kind == Whole::Town || whole.kind == Whole::Creature;
	case Desire::Anger:
		return (mayAttackOwnPlayer || !whole.samePlayer) && AngerCanBeAbout(whole);
	case Desire::BeFriends:
	case Desire::ObeyCreature:
	case Desire::TellCreature:
	case Desire::EducateFriend:
	case Desire::MissFriend:
		return whole.kind == Whole::Creature;
	default:
		return false;
	}
}

bool creature_activity::CompassionHasActions(Whole kind)
{
	return kind == Whole::Town;
}

std::optional<size_t> creature_activity::MostUseful(std::span<const float> usefulness)
{
	std::optional<size_t> best;
	float most = 0.0f;
	for (size_t i = 0; i < usefulness.size(); ++i)
	{
		// Strictly more than the best so far, which starts at nothing: the first of equals stays
		if (usefulness[i] > most)
		{
			best = i;
			most = usefulness[i];
		}
	}
	return best;
}

std::optional<size_t> creature_activity::ChooseWhole(Desire desire, std::span<const WholeCandidate> wholes,
                                                     bool mayAttackOwnPlayer)
{
	std::optional<size_t> best;
	float most = 0.0f;
	for (size_t i = 0; i < wholes.size(); ++i)
	{
		const auto& whole = wholes[i];
		if (whole.itself || !CanBeAbout(desire, whole.facts, mayAttackOwnPlayer))
		{
			continue;
		}
		if (whole.worth > most)
		{
			best = i;
			most = whole.worth;
		}
	}
	return best;
}

bool creature_activity::ThingSuits(Desire desire, const ThingFacts& thing)
{
	if (!thing.canActOn)
	{
		return false;
	}
	const bool lives = thing.kind == Thing::Villager || thing.kind == Thing::Animal || thing.kind == Thing::Creature;
	const bool object = thing.kind != Thing::Spell && thing.kind != Thing::Other;
	switch (desire)
	{
	case Desire::Impress:
		return (thing.kind == Thing::Villager && !thing.dancing) || thing.kind == Thing::TotemStatue ||
		       thing.kind == Thing::Totem;
	case Desire::Compassion:
		return thing.kind == Thing::Villager ? thing.pickable : object;
	case Desire::Anger:
		if (thing.kind == Thing::Tree || thing.kind == Thing::Rock)
		{
			return true;
		}
		if (!object || (lives && thing.dead) || thing.friendOfCreature)
		{
			return false;
		}
		return thing.attackable && !thing.itself;
	case Desire::Play:
		switch (thing.kind)
		{
		case Thing::MobileObject:
		case Thing::MobileStatic:
		case Thing::Rock:
		case Thing::Tree:
		case Thing::Creature:
		case Thing::TotemStatue:
		case Thing::Totem:
			return true;
		case Thing::Villager:
		case Thing::Animal:
			return !thing.dead && thing.playable;
		case Thing::Building:
		case Thing::Field:
		case Thing::FishFarm:
		case Thing::PlainObject:
			return thing.playable;
		default:
			return false;
		}
	case Desire::Fear:
		return thing.kind == Thing::Creature || thing.kind == Thing::Spell ||
		       (thing.kind == Thing::Animal && thing.frightening);
	case Desire::Curiosity:
		return object && !(lives && thing.dead) && thing.inspectable;
	case Desire::Poo:
		return object && !lives && thing.height > k_TallestThingNotToPooOn;
	case Desire::Tiredness:
	case Desire::Rest:
		switch (thing.kind)
		{
		case Thing::Creature:
		case Thing::Tree:
		case Thing::Rock:
		case Thing::MobileStatic:
		case Thing::Building:
		case Thing::TotemStatue:
		case Thing::Totem:
			return true;
		default:
			return false;
		}
	case Desire::BringStuffHome:
		return thing.creatureCanPickUp;
	case Desire::BeFriends:
	case Desire::TellCreature:
	case Desire::EducateFriend:
	case Desire::MissFriend:
		return thing.kind == Thing::Creature;
	case Desire::GetHigh:
		return thing.mushroom;
	default:
		return true;
	}
}
