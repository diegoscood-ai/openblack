/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandToolTipRules.h"

#include "ECS/TownDesireFlags.h"

using namespace openblack::ecs::systems;

std::optional<hand_tooltips::ToolTip> hand_tooltips::StoragePitToolTip(bool built, uint32_t food, uint32_t wood)
{
	if (!built)
	{
		return std::nullopt;
	}
	ToolTip tip {.text = 0xEF9, .action = k_ActionNone};
	// the food goes through a float, the wood is taken as a signed whole number
	tip.numbers.values[0] = static_cast<double>(static_cast<float>(food));
	tip.numbers.values[1] = static_cast<double>(static_cast<int32_t>(wood));
	tip.numbers.count = 2;
	return tip;
}

std::optional<hand_tooltips::ToolTip> hand_tooltips::TownDesireFlagToolTip(TownDesireInfo desire, float rawDesire)
{
	namespace flags = ecs::town_desire_flags;
	const auto words = flags::ToolTipWords(desire);
	if (!words.has_value())
	{
		return std::nullopt;
	}
	ToolTip tip {.text = help::tooltips::k_First + flags::k_NeedToolTip, .action = k_ActionNone};
	tip.numbers.values[0] = static_cast<double>(flags::NeedPercent(rawDesire));
	tip.numbers.count = 1;
	tip.numbers.text = help::tooltips::k_First + *words;
	return tip;
}

std::optional<entt::entity> hand_tooltips::TownCentreForToolTips(entt::entity object, bool isTownCentre, bool isTotem,
                                                                 entt::entity totemsTownCentre)
{
	if (isTownCentre)
	{
		return object;
	}
	if (isTotem)
	{
		return totemsTownCentre;
	}
	return std::nullopt;
}

std::array<hand_tooltips::ToolTip, 2> hand_tooltips::TownToolTips(float lead, uint32_t population, uint32_t places)
{
	// the lead below 0: its size, and the text 0xEE0 "%3.0f Believers Needed"
	ToolTip believers {.text = 0xECC, .action = k_ActionNone};
	believers.numbers.values[0] = static_cast<double>(lead < 0.0f ? lead * -1.0f : lead);
	believers.numbers.count = 1;
	if (lead < 0.0f)
	{
		believers.numbers.text = 0xEE0;
	}
	// "Population: %3.0f", then "%s/%d" with the places
	ToolTip people {.text = 0xEFC, .action = k_ActionNone};
	people.numbers.values[0] = static_cast<double>(population);
	people.numbers.count = 1;
	people.numbers.outOf = static_cast<int32_t>(places);
	return {believers, people};
}

hand_tooltips::Choice hand_tooltips::SeedToolTip(bool inInfluence, bool hasSeed, bool hasTarget, bool validToObject,
                                                 bool validToMap, MagicType magic)
{
	if (!inInfluence)
	{
		return {.shows = Shows::Land};
	}
	if (!hasSeed)
	{
		return {};
	}
	if (hasTarget)
	{
		if (!validToObject)
		{
			return {.shows = Shows::Land};
		}
		return {.shows = Shows::Own, .tip = {.text = 0xE81, .action = k_ActionRight}};
	}
	if (!validToMap)
	{
		return {.shows = Shows::Land};
	}
	const bool sized = magic >= MagicType::StormWindRain && magic <= MagicType::PhysicalShield;
	return {.shows = Shows::Own, .tip = {.text = sized ? 0xEF1u : 0xE81u, .action = k_ActionRight}};
}

hand_tooltips::ToolTip hand_tooltips::OrbPickUpToolTip(uint32_t text)
{
	return {.text = text, .action = k_ActionNone, .force = true};
}

hand_tooltips::ToolTip hand_tooltips::SpellIconToolTip(uint32_t seedToolTip)
{
	return {.text = seedToolTip, .action = k_ActionNone, .force = true};
}

std::optional<hand_tooltips::ToolTip> hand_tooltips::WorkshopToolTip(bool built)
{
	if (!built)
	{
		return std::nullopt;
	}
	return ToolTip {.text = 0xEF3, .action = k_ActionNone};
}

hand_tooltips::ToolTip hand_tooltips::CreatureInteractToolTip()
{
	return {.text = 0xE85, .action = k_ActionNone};
}

int32_t hand_tooltips::ObjectHandState(const ObjectFacts& facts)
{
	if (!facts.interactable)
	{
		return 3;
	}
	if (!facts.inInfluence && facts.mustBeInInfluence)
	{
		return 18;
	}
	if (facts.validForLockedSelect)
	{
		return 13;
	}
	// (not ported) valid for a locked apply: 16 or 15, which openblack has no test for
	if (facts.validForPlaceInHand && !facts.cannotBePickedUp)
	{
		return 9;
	}
	return 18;
}
