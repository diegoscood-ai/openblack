/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureObjectBeliefs.h"

#include <algorithm>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureLook.h"

using namespace openblack::creature_object_beliefs;

Store::Added Store::Add(entt::entity thing, uint32_t kind)
{
	if (const auto known = _index.find(thing); known != _index.end())
	{
		return {.belief = &*_pools.at(known->second.kind).slots.at(known->second.slot), .isNew = false};
	}
	if (kind >= k_KindCount)
	{
		return {};
	}
	auto& pool = _pools.at(kind);
	const auto capacity = static_cast<size_t>(k_Capacity.at(kind));
	if (pool.slots.empty())
	{
		pool.slots.resize(capacity);
	}
	size_t slot = 0;
	if (pool.used < capacity)
	{
		const auto free = std::ranges::find_if(pool.slots, [](const auto& held) { return !held.has_value(); });
		slot = static_cast<size_t>(std::distance(pool.slots.begin(), free));
		if (!pool.oldest.has_value())
		{
			pool.oldest = slot;
		}
		++pool.used;
	}
	else
	{
		// Every place is taken: the belief at the oldest-kept marker gives way, and the marker stays where it is
		slot = pool.oldest.value_or(0);
		_index.erase(pool.slots.at(slot)->thing);
	}
	pool.slots.at(slot) = Belief {.thing = thing, .kind = kind};
	_index[thing] = Place {.kind = kind, .slot = slot};
	return {.belief = &*pool.slots.at(slot), .isNew = true};
}

const Belief* Store::Find(entt::entity thing) const
{
	const auto known = _index.find(thing);
	return known != _index.end() ? &*_pools.at(known->second.kind).slots.at(known->second.slot) : nullptr;
}

Belief* Store::Find(entt::entity thing)
{
	const auto known = _index.find(thing);
	return known != _index.end() ? &*_pools.at(known->second.kind).slots.at(known->second.slot) : nullptr;
}

bool Store::Forget(entt::entity thing)
{
	const auto known = _index.find(thing);
	if (known == _index.end())
	{
		return false;
	}
	const auto [kind, slot] = known->second;
	_index.erase(known);
	auto& pool = _pools.at(kind);
	pool.slots.at(slot).reset();
	// The marker moves on to the next place, round to the first after the last
	if (pool.oldest == slot)
	{
		pool.oldest = slot + 1 < pool.slots.size() ? slot + 1 : 0;
	}
	if (--pool.used == 0)
	{
		pool.oldest.reset();
	}
	return true;
}

size_t Store::Count(uint32_t kind) const
{
	return kind < k_KindCount ? _pools.at(kind).used : 0;
}

glm::ivec2 openblack::creature_object_beliefs::RingCell(glm::ivec2 centre, uint32_t ring, uint32_t step)
{
	if (ring < 2)
	{
		return centre;
	}
	const auto out = static_cast<int32_t>(ring - 1);
	const auto along = static_cast<int32_t>(step % ((ring * 2) - 2));
	switch ((step * 4) / ((ring - 1) * 8))
	{
	case 0:
		return {centre.x + along - out, centre.y - out};
	case 1:
		return {centre.x + out, centre.y + along - out};
	case 2:
		return {centre.x + out - along, centre.y + out};
	default:
		return {centre.x - out, centre.y + out - along};
	}
}

std::vector<glm::ivec2> openblack::creature_object_beliefs::NextCells(Sweep& sweep, glm::ivec2 centre, float shownSize)
{
	const auto cells = creature_look::SweepCells(shownSize) & 0xFFFFu;
	if (sweep.left == 0)
	{
		sweep = Sweep {.left = cells, .ring = 1, .step = 0};
	}
	constexpr uint32_t k_MostPerTurn = 9;
	const auto perTurn = std::min(cells / 9, k_MostPerTurn);
	std::vector<glm::ivec2> next;
	next.reserve(perTurn);
	for (uint32_t i = 0; i < perTurn; ++i)
	{
		next.push_back(RingCell(centre, sweep.ring, sweep.step));
		if (--sweep.left == 0)
		{
			break;
		}
		++sweep.step;
		if (sweep.ring == 1 || sweep.step >= (sweep.ring * 8) - 8)
		{
			++sweep.ring;
			sweep.step = 0;
		}
	}
	return next;
}

uint32_t openblack::creature_object_beliefs::MobileObjectBeliefKind(std::optional<int32_t> rowKind)
{
	namespace kinds = creature_tree::belief_types;
	if (rowKind == k_MagicFoodRowKind)
	{
		return kinds::k_MagicFood;
	}
	if (rowKind == k_PooRowKind)
	{
		return kinds::k_Poo;
	}
	return kinds::k_MobileObject;
}

uint32_t openblack::creature_object_beliefs::MobileStaticBeliefKind(bool rockClass)
{
	namespace kinds = creature_tree::belief_types;
	return rockClass ? kinds::k_Rock : kinds::k_MobileObject;
}

bool openblack::creature_object_beliefs::KeepsMovingThing(bool moving, bool itself, float weight)
{
	return moving && !itself && !(weight > k_MovingSeenWeight);
}

bool openblack::creature_object_beliefs::MovingSeenRecently(uint32_t turn, uint32_t seenTurn, uint32_t turnsPerSecond)
{
	return turn - seenTurn < 2 * turnsPerSecond;
}

bool openblack::creature_object_beliefs::SeeingAgainStirs(uint32_t kind)
{
	return kind != creature_tree::belief_types::k_Abode;
}

bool openblack::creature_object_beliefs::InListOfWholeThings(uint32_t kind)
{
	namespace kinds = creature_tree::belief_types;
	return kind == kinds::k_Town || kind == kinds::k_Forest || kind == kinds::k_Citadel || kind == kinds::k_Creature ||
	       kind == kinds::k_Flock;
}

bool openblack::creature_object_beliefs::SightingStirsSources(uint32_t kind, bool itself, uint32_t turn, uint32_t sourcesTurn)
{
	return InListOfWholeThings(kind) && !itself && turn - sourcesTurn > k_SightingSourceTurns;
}

bool openblack::creature_object_beliefs::UsefulEnoughToStir(float usefulness)
{
	return usefulness > k_SightingUsefulness;
}
