/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DiscipleRules.h"

#include <algorithm>

#include "Common/GUtilsDistance.h"

using namespace openblack;
using namespace openblack::disciple;

namespace
{
using enum VillagerDisciple;

/// Any building of a town: preaching to another player's town, building up one's own
VillagerDisciple OfferedByBuilding(const NearFacts& near, const HeldFacts& held)
{
	if (near.hasTown && !near.townHabitable)
	{
		return None;
	}
	if (held.hand != held.villager)
	{
		return None;
	}
	if (near.owner != held.hand)
	{
		return near.hasTown ? Missionary : None;
	}
	return !near.built || !near.repaired ? Builder : None;
}

/// A building of the abodes: as any building, else one of the player's own houses with room takes in a villager of
/// another home
VillagerDisciple OfferedByAbode(const NearFacts& near, const HeldFacts& held)
{
	const auto offered = OfferedByBuilding(near, held);
	if (offered == None && near.kind == NearKind::House && near.functional && near.owner == held.hand && near.roomForAdults &&
	    !near.heldVillagersHome && near.hasTown && near.townHabitable)
	{
		return ChangeHouse;
	}
	return offered;
}
} // namespace

float disciple::Interest(float edgeGap, float kindDesire)
{
	return gutils::GetDistanceModifier(edgeGap > 0.0f ? edgeGap : 0.0f, k_InterestReach) * kindDesire;
}

std::vector<size_t> disciple::OrderByInterest(std::span<const float> interests)
{
	std::vector<size_t> order;
	order.reserve(interests.size());
	for (size_t i = 0; i < interests.size(); ++i)
	{
		if (interests[i] > k_LeastInterest)
		{
			order.push_back(i);
		}
	}
	std::ranges::stable_sort(order, [interests](size_t a, size_t b) { return interests[a] > interests[b]; });
	return order;
}

VillagerDisciple disciple::Offered(const NearFacts& near, const HeldFacts& held)
{
	const bool ownVillager = held.hand == held.villager;
	switch (near.kind)
	{
	case NearKind::Building:
	case NearKind::House:
		return OfferedByAbode(near, held);
	case NearKind::StoragePit:
	{
		// Another player's storage pit is traded with rather than preached to
		const auto offered = OfferedByAbode(near, held);
		return offered == Missionary ? Trader : offered;
	}
	case NearKind::TownCentre:
	{
		// The centre of a town other than its own takes it in, when the town has room
		const auto offered = OfferedByAbode(near, held);
		if (offered != Missionary && offered != Builder && !near.heldVillagersTown)
		{
			return near.townHasRoom ? ChangeHouse : None;
		}
		return offered;
	}
	case NearKind::Workshop:
	{
		const auto offered = OfferedByAbode(near, held);
		if (offered == None && ownVillager && near.hasTown && near.townHabitable)
		{
			return Craftsman;
		}
		return offered;
	}
	case NearKind::WorshipSite:
	{
		const auto offered = OfferedByBuilding(near, held);
		return offered != Builder && near.heldVillagersWorshipSite ? Worship : offered;
	}
	case NearKind::Field:
		// Any field, whoever's, though one not yet built up is built
		if (!ownVillager)
		{
			return None;
		}
		return near.built ? Farmer : Builder;
	case NearKind::Tree:
	case NearKind::Forest:
		return ownVillager ? Forester : None;
	case NearKind::FishFarm:
		return ownVillager ? Fisherman : None;
	case NearKind::Villager:
		// One of the player's own of an age to make love, by one of the other sex
		if (near.villagerFunctional && near.owner == held.hand && held.villager == near.owner && held.sexuallyActive &&
		    near.villagerSexuallyActive && held.male != near.villagerMale)
		{
			return Breeder;
		}
		return None;
	}
	return None;
}

Choice disciple::ChooseWhileHeld(std::span<const NearFacts> ordered, const HeldFacts& held, VillagerDisciple atPickUp)
{
	const bool refused = held.child || held.otherwiseRefused;
	for (size_t i = 0; i < ordered.size(); ++i)
	{
		const auto offered = Offered(ordered[i], held);
		if (offered == None)
		{
			continue;
		}
		if (refused && !(offered == Worship && held.child))
		{
			continue;
		}
		return {.type = offered, .from = i};
	}
	return {.type = atPickUp, .from = std::nullopt};
}

VillagerDisciple disciple::AtPickUp(VillagerDisciple current)
{
	return current == Missionary ? None : current;
}

std::optional<uint32_t> disciple::SymbolCell(VillagerDisciple type)
{
	// The symbols are the fifth row of the sheet and the start of the sixth
	constexpr uint32_t k_FirstCell = 32;
	switch (type)
	{
	case Missionary:
		return k_FirstCell + 0;
	case Farmer:
		return k_FirstCell + 1;
	case Breeder:
		return k_FirstCell + 2;
	case Forester:
		return k_FirstCell + 3;
	case Builder:
		return k_FirstCell + 4;
	case Protection:
	case Trader:
		return k_FirstCell + 5;
	case Fisherman:
		return k_FirstCell + 6;
	case Craftsman:
		return k_FirstCell + 7;
	case ChangeHouse:
		return k_FirstCell + 8;
	default:
		return std::nullopt;
	}
}

glm::vec4 disciple::TintOf(uint32_t argb)
{
	const auto channel = [argb](uint32_t shift) { return static_cast<float>((argb >> shift) & 0xFFu) / 255.0f; };
	return {channel(16), channel(8), channel(0), channel(24)};
}

glm::vec3 disciple::GlowPoint(const glm::vec3& drawn, float size)
{
	return {drawn.x, drawn.y + size, drawn.z};
}

glm::vec3 disciple::SymbolPoint(const glm::vec3& glow, float size)
{
	// The rise is added to the glow's height as it stands, not to the villager's
	return {glow.x, glow.y + (size * k_SymbolRise), glow.z};
}

float disciple::AlignmentForMaking(std::optional<float> townDesire, float evilBelow, float goodAbove, float change)
{
	if (!townDesire.has_value())
	{
		return 0.0f;
	}
	if (*townDesire > goodAbove)
	{
		return change;
	}
	return *townDesire < evilBelow ? -change : 0.0f;
}
