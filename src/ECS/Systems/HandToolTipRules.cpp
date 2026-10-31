/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandToolTipRules.h"

#include <cmath>

#include "3D/MapCoords.h"
#include "Camera/CameraDrag.h"
#include "ECS/TownDesireFlags.h"

using namespace openblack::ecs::systems;

hand_tooltips::ToolTip hand_tooltips::LandToolTip(const LandFacts& facts)
{
	namespace tricon = camera_drag::tricon;
	namespace feature = camera_drag::feature;
	// the tilt is tried before the turn
	if ((facts.tricons & tricon::k_Pitch) != 0 && (facts.features & feature::k_Pitch) != 0)
	{
		return {.text = 0xE76, .action = k_ActionLeft, .align = k_ArrowsUpDown};
	}
	if ((facts.tricons & tricon::k_Rotate) != 0 && (facts.features & feature::k_Rotate) != 0)
	{
		return {.text = 0xE77, .action = k_ActionLeft, .align = k_ArrowsAll};
	}
	if (facts.fishInReach)
	{
		return {.text = 0xE73, .action = k_ActionRight};
	}
	// a distance that is no number counts as near, as the original's test lets it through
	if (facts.centreOnLand && !(facts.centreDistance >= k_ZoomOutDistance) && facts.pitch > k_ZoomOutPitch &&
	    (facts.features & feature::k_Zoom) != 0)
	{
		return {.text = 0xE8B, .action = k_ActionZoomIn};
	}
	return {.text = 0xE7E, .action = k_ActionLeft, .align = k_ArrowsAll};
}

std::optional<std::array<hand_tooltips::ToolTip, 2>> hand_tooltips::BuildingToolTips(const BuildingFacts& facts)
{
	if ((facts.built && facts.repaired) || !facts.ownOrNoPlayer || !facts.pitchAllowed)
	{
		return std::nullopt;
	}
	// how far it is built, or once built how far it is repaired, as a float times 100 in double precision
	const double completed = static_cast<double>(facts.built ? facts.percentRepaired : facts.percentBuilt) * 100.0;
	ToolTip wood {.text = 0xEFB, .action = k_ActionNone};
	if (facts.siteWood.has_value())
	{
		// the wood wanted, 0 when it is not above 0
		wood.numbers.values[0] = static_cast<double>(*facts.siteWood > 0.0f ? *facts.siteWood : 0.0f);
	}
	else
	{
		// with no site the wood's text is written as "Building Completed"
		wood.numbers.values[0] = completed;
		wood.numbers.text = 0xEF4;
	}
	wood.numbers.count = 1;
	ToolTip built {.text = 0xEF4, .action = k_ActionNone};
	built.numbers.values[0] = completed;
	built.numbers.count = 1;
	return std::array<ToolTip, 2> {wood, built};
}

std::optional<bool> hand_tooltips::SpellIconOwnOrNoPlayer(bool hasWorshipIcon, bool townCentreIcon,
                                                          std::optional<bool> townIsLocal)
{
	if (hasWorshipIcon)
	{
		return std::nullopt;
	}
	if (townCentreIcon)
	{
		return townIsLocal.value_or(true);
	}
	return false;
}

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

hand_tooltips::ToolTip hand_tooltips::PrayerEnergyToolTip(float stored)
{
	ToolTip tip {.text = 0xEE9, .action = k_ActionNone};
	tip.numbers.values[0] = static_cast<double>(stored);
	tip.numbers.count = 1;
	return tip;
}

hand_tooltips::ToolTip hand_tooltips::NeedsSignToolTip(uint8_t infoIndex, float shown, uint32_t woodWanted)
{
	// the rows: 0 hunger, 1 rest, 2 people hiding (an altar's), 3 the workshop's
	constexpr uint8_t k_Hunger = 0;
	constexpr uint8_t k_Rest = 1;
	constexpr uint8_t k_Workshop = 3;
	ToolTip tip {.text = 0xE73, .action = k_ActionNone};
	if (infoIndex == k_Workshop)
	{
		tip.text = 0xEFB;
		tip.numbers.values[0] = static_cast<double>(woodWanted);
		tip.numbers.count = 1;
	}
	else if (infoIndex == k_Hunger || infoIndex == k_Rest)
	{
		tip.text = infoIndex == k_Hunger ? 0xECE : 0xEFF;
		// how full it shows, times 100 as a float
		const float percent = shown * 100.0f;
		tip.numbers.values[0] = static_cast<double>(percent);
		tip.numbers.count = 1;
	}
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

hand_tooltips::ToolTip hand_tooltips::PickUpToolTip(std::optional<uint32_t> scaffoldValue)
{
	if (!scaffoldValue)
	{
		return {.text = 0xE73, .action = k_ActionRight};
	}
	return {.text = 0xEE7, .action = k_ActionRight, .numbers = {.values = {static_cast<double>(*scaffoldValue)}, .count = 1}};
}

hand_tooltips::ToolTip hand_tooltips::TapToolTip(bool rock, bool rewardChest)
{
	return {.text = rock ? 0xEF7u : rewardChest ? 0xEF8u : 0xE7Au, .action = k_ActionRight};
}

hand_tooltips::ToolTip hand_tooltips::PileToolTip(bool food, uint32_t amount)
{
	return {.text = food ? 0xEFDu : 0xEFEu,
	        .action = k_ActionRight,
	        .numbers = {.values = {static_cast<double>(amount)}, .count = 1}};
}

hand_tooltips::ToolTip hand_tooltips::FieldToolTip()
{
	return {.text = 0xE73, .action = k_ActionRight};
}

std::optional<hand_tooltips::Choice> hand_tooltips::CanPickUpFirst(uint8_t interfaceFlags, bool inPhysics, float altitude)
{
	constexpr uint8_t k_InteractionLimits = 2;
	if ((interfaceFlags & k_InteractionLimits) != 0)
	{
		return Choice {.shows = Shows::Land};
	}
	// Not a number is no altitude of at least nothing
	if (inPhysics && altitude >= 0.0f)
	{
		return Choice {.shows = Shows::Own, .tip = {.text = 0xE80, .action = k_ActionRight}};
	}
	return std::nullopt;
}

hand_tooltips::ToolTip hand_tooltips::OrbPickUpToolTip(uint32_t text)
{
	return {.text = text, .action = k_ActionNone, .force = true};
}

std::optional<double> hand_tooltips::SpellIconChargePercent(float store, float required)
{
	const float percent = (store / required) * 100.0f;
	// zero and not-a-number alike compare as equal to zero on the original's FPU
	if (percent == 0.0f || std::isnan(percent))
	{
		return std::nullopt;
	}
	return static_cast<double>(percent);
}

hand_tooltips::ToolTip hand_tooltips::SpellIconToolTip(uint32_t seedToolTip, std::optional<double> chargePercent)
{
	return {.text = seedToolTip, .action = k_ActionNone, .force = true, .numbers = {.percentAfter = chargePercent}};
}

int32_t hand_tooltips::WorkshopProductionPercent(int32_t turnsLeft, float turnsToProduce)
{
	// the product in 32 bits, unsigned, then divided and taken from 100 in floats
	const uint32_t scaled = static_cast<uint32_t>(turnsLeft) * 100u;
	const auto left = static_cast<float>(static_cast<double>(scaled) / static_cast<double>(turnsToProduce));
	return map_coords::FtoL(100.0f - left) % 100;
}

std::optional<hand_tooltips::ToolTip> hand_tooltips::WorkshopToolTip(bool built, bool repaired, int32_t turnsLeft,
                                                                     float turnsToProduce)
{
	if (!built)
	{
		return std::nullopt;
	}
	ToolTip tip {.text = 0xEF3, .action = k_ActionNone};
	if (repaired)
	{
		tip.numbers = {.values = {static_cast<double>(WorkshopProductionPercent(turnsLeft, turnsToProduce))}, .count = 1};
	}
	return tip;
}

hand_tooltips::ToolTip hand_tooltips::TempleHealthToolTip(float life)
{
	return {.text = 0xEE8, .action = k_ActionNone, .numbers = {.values = {static_cast<double>(life * 100.0f)}, .count = 1}};
}

hand_tooltips::ToolTip hand_tooltips::CreatureInteractToolTip()
{
	return {.text = 0xE85, .action = k_ActionNone};
}

bool hand_tooltips::OverObjectShowsLand(bool inInfluence, bool mustBeInInfluence)
{
	return !inInfluence && mustBeInInfluence;
}

hand_tooltips::ToolTip hand_tooltips::ScriptHighlightToolTip(bool active)
{
	return {.text = active ? 0xEA0u : 0xE83u, .action = k_ActionRight};
}

hand_tooltips::ToolTip hand_tooltips::GiveToolTip()
{
	return {.text = 0xE87, .action = k_ActionRight};
}

hand_tooltips::ToolTip hand_tooltips::CombineScaffoldsToolTip()
{
	return {.text = 0xEA5, .action = k_ActionRight};
}

uint32_t hand_tooltips::HeldDropText(bool tree, std::optional<uint32_t> scaffoldDrop)
{
	if (tree)
	{
		return 0xEEF;
	}
	return scaffoldDrop.value_or(0);
}

std::array<hand_tooltips::ToolTip, 2> hand_tooltips::DropToolTips(uint32_t ownDrop)
{
	return {{
	    {.text = ownDrop != 0 ? ownDrop : 0xEEEu, .action = k_ActionRight},
	    {.text = 0xE74, .action = k_ActionRight, .align = k_ArrowsAll},
	}};
}

hand_tooltips::HeldChoice hand_tooltips::HeldToolTips(const HeldFacts& facts)
{
	// a held query icon shows its own text first in the original; nothing the hand holds is one
	if (!facts.inInfluence)
	{
		return {.shows = Shows::Land};
	}
	const auto one = [](const ToolTip& tip) { return HeldChoice {.shows = Shows::Own, .tips = {tip}, .count = 1}; };
	if (facts.overTarget)
	{
		// two scaffolds: they join, or only the drop texts, with no gift or apply test
		if (facts.bothScaffolds)
		{
			if (facts.scaffoldsJoin)
			{
				return one(CombineScaffoldsToolTip());
			}
		}
		else if (facts.takesGift)
		{
			return one(GiveToolTip());
		}
		else if (facts.validToApply)
		{
			// "Sacrifice" over a worship site's altar, else "Supply", on the action button
			return one({.text = facts.overSacrificeAltar ? 0xE8Du : 0xE8Eu, .action = k_ActionRight});
		}
	}
	return {.shows = Shows::Own, .tips = DropToolTips(facts.ownDrop), .count = 2};
}

hand_tooltips::LeashToolTipKind hand_tooltips::LeashToolTipOf(const LeashFacts& facts)
{
	// the creature has to have grown past its first stages for the leash to offer a tie
	constexpr uint32_t k_TiesFromPhase = 4;
	if (!facts.leashOn)
	{
		return LeashToolTipKind::None;
	}
	if (facts.tied)
	{
		return facts.hasObject && facts.overTiedObject ? LeashToolTipKind::Detach : LeashToolTipKind::None;
	}
	if (!facts.hasObject)
	{
		return LeashToolTipKind::BringCreature;
	}
	if (facts.hasCreature && facts.creaturePhase >= k_TiesFromPhase && !facts.objectIsCreature &&
	    facts.validAsInterfaceTarget && !facts.orbOrSpellIcon)
	{
		return LeashToolTipKind::Attach;
	}
	return LeashToolTipKind::None;
}

std::optional<hand_tooltips::ToolTip> hand_tooltips::LeashToolTip(LeashToolTipKind kind)
{
	switch (kind)
	{
	case LeashToolTipKind::Detach:
		return ToolTip {.text = 0xEA3, .action = k_ActionRight};
	case LeashToolTipKind::Attach:
		return ToolTip {.text = 0xEA4, .action = k_ActionRight};
	case LeashToolTipKind::BringCreature:
		return ToolTip {.text = 0xEA2, .action = k_ActionRight};
	case LeashToolTipKind::None:
		break;
	}
	return std::nullopt;
}

int32_t hand_tooltips::LandGripHandState(bool actionHeld, uint32_t features)
{
	constexpr uint32_t k_FeatureZoom = 4;
	return actionHeld && (features & k_FeatureZoom) != 0 ? 22 : 20;
}

hand_tooltips::ToolTip hand_tooltips::ZoomLandscapeToolTip()
{
	// BINDABLE_ACTION 6, the zoom control's
	constexpr int32_t k_ActionZoomOn = 6;
	return {.text = 0xEFA, .action = k_ActionZoomOn, .align = 0x300};
}

hand_tooltips::ToolTip hand_tooltips::BubbleToolTip()
{
	// the up and down arrows: a bubble is dragged up and down
	return {.text = 0xE84, .action = k_ActionRight, .align = 0x300};
}

bool hand_tooltips::ShowsCameraIcons(int32_t handState, bool scooping)
{
	constexpr int32_t k_Normal = 1;
	constexpr int32_t k_Camera = 2;
	constexpr int32_t k_Holding = 4;
	constexpr int32_t k_Grain = 8;
	if (handState == k_Holding || handState == k_Grain)
	{
		return !scooping;
	}
	return handState == k_Normal || handState == k_Camera;
}

bool hand_tooltips::HoldsCrossBack(bool interactable, bool inInfluence, bool mustBeInInfluence)
{
	return interactable && (inInfluence || !mustBeInInfluence);
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

bool hand_tooltips::IsInterfaceObject(const CursorObjectFacts& facts)
{
	return facts.totemPart || facts.tappable || facts.creature || facts.needsSign || facts.desireFlag || facts.altar ||
	       facts.templeHeart || facts.worshipSite;
}
