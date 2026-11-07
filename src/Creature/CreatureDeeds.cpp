/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDeeds.h"

using namespace openblack;
using namespace openblack::creature_watching;

std::optional<Deed> creature_watching::GiftDeed(const Gift& gift)
{
	if (gift.goingUp && gift.resource == ResourceType::Wood)
	{
		return Deed::PutWoodInBuildingSite;
	}
	switch (gift.receiver)
	{
	case Receiver::Workshop:
		return gift.resource == ResourceType::Wood ? std::optional(Deed::PutWoodInWorkshop) : std::nullopt;
	case Receiver::WorshipSite:
		return gift.resource == ResourceType::Food ? std::optional(Deed::PutFoodInWorshipSite) : std::nullopt;
	case Receiver::Other:
		break;
	}
	return std::nullopt;
}

std::optional<Deed> creature_watching::LandingDeed(const Landing& landing)
{
	const bool othersThing = landing.thingsTown.has_value() && *landing.thingsTown != landing.player;
	if (landing.nearOwnTemple)
	{
		return othersThing ? Deed::StealObjectAndPutByCitadel : Deed::TakeObjectHome;
	}
	if (othersThing && landing.nearestTown == landing.player)
	{
		return Deed::StealObjectAndPutInTown;
	}
	return std::nullopt;
}

std::optional<Deed> creature_watching::DiscipleDeed(VillagerDisciple type)
{
	const auto deed = static_cast<size_t>(Deed::ThrowInTheSea) + static_cast<size_t>(type);
	return deed < k_DeedCount ? std::optional(static_cast<Deed>(deed)) : std::nullopt;
}
