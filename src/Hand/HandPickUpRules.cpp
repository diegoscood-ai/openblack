/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandPickUpRules.h"

#include <algorithm>

using namespace openblack;

bool hand_pick_up::GrabTakes(bool inInfluence, bool cannotBePickedUp) noexcept
{
	return inInfluence && !cannotBePickedUp;
}

PotInfo hand_pick_up::HandPileFor(ResourceType resource) noexcept
{
	return resource == ResourceType::Wood ? PotInfo::HandWood : PotInfo::HandFood;
}

uint32_t hand_pick_up::FirstHandful(uint32_t sourceHas, uint32_t takenAtFirst) noexcept
{
	return std::min<uint32_t>(sourceHas, takenAtFirst);
}

bool hand_pick_up::FieldValidForLockedSelect(float growth, float food) noexcept
{
	return growth > 0.0f && food > 1.0f;
}

int32_t hand_pick_up::FieldFirstHandful(float takenWithHand, float food, bool ripe) noexcept
{
	auto n = static_cast<int>(std::min(takenWithHand, food));
	if (ripe)
	{
		n /= 2;
	}
	return n;
}

int32_t hand_pick_up::FieldStraightHandful(float takenWithHand) noexcept
{
	return static_cast<int32_t>(takenWithHand);
}

std::optional<int32_t> hand_pick_up::FishFarmStraightHandful(float handFoodValue, float farmFoodValue) noexcept
{
	if (!(farmFoodValue > 0.0f))
	{
		return std::nullopt;
	}
	return static_cast<int32_t>(handFoodValue);
}

hand_pick_up::FieldScoop hand_pick_up::FieldTurnScoop(uint32_t turns, float food, uint32_t held, bool ripe) noexcept
{
	const float t = std::min(static_cast<float>(turns) / 60.0f, 1.0f);
	auto n = static_cast<int>(std::min(8.0f + 62.0f * t * t, food));
	n = std::min(n, 20000 - static_cast<int>(held));
	if (ripe)
	{
		n /= 2;
	}
	return {.t = t, .amount = n};
}

uint32_t hand_pick_up::PileScoopTaken(uint32_t amount, uint32_t sourceHas, uint32_t mostHeld, uint32_t held) noexcept
{
	const uint32_t room = mostHeld > held ? mostHeld - held : 0u;
	return std::min({amount, sourceHas, room});
}

int32_t hand_pick_up::PickUpSoundSample(bool wood) noexcept
{
	return wood ? k_PickUpWoodSample : k_PickUpFoodSample;
}

int32_t hand_pick_up::PickUpSoundPitch(float rampSquared) noexcept
{
	return static_cast<int>(60.0f + 180.0f * rampSquared);
}

hand_pick_up::HoldParameters hand_pick_up::HoldParametersOf(HoldClass kind, MobileStaticInfo staticType, float height,
                                                            float radius2D) noexcept
{
	HoldParameters hold {.type = HoldType::Above, .radius = 0.75f * height, .lowering = 0.0f};
	switch (kind)
	{
	case HoldClass::Tree:
		hold.type = HoldType::Tree;
		hold.radius = 0.2f * radius2D;
		hold.lowering = 0.1f;
		break;
	case HoldClass::Poo:
		// on the palm, its radius the default one
		hold.lowering = -0.3f;
		break;
	case HoldClass::SideObject:
		// a one-shot orb is a mobile object: SIDE, with its lowering and the 2D radius
		hold.type = HoldType::Side;
		hold.radius = radius2D;
		hold.lowering = 0.7f;
		break;
	case HoldClass::Living:
		// the Living hold class: villagers and animals alike
		hold.type = HoldType::Villager;
		hold.radius = radius2D;
		hold.lowering = 0.65f;
		break;
	case HoldClass::MobileStatic:
		switch (staticType)
		{
		case MobileStaticInfo::GateTotemApe:
		case MobileStaticInfo::GateTotemBlank:
		case MobileStaticInfo::GateTotemCow:
		case MobileStaticInfo::GateTotemTiger:
		case MobileStaticInfo::WeepingStone:
		case MobileStaticInfo::WeepingStoneReward:
			hold.type = HoldType::Side;
			hold.radius = radius2D;
			hold.lowering = 0.7f;
			break;
		case MobileStaticInfo::SingingStone_1:
			hold.type = HoldType::Side;
			hold.radius = radius2D;
			hold.lowering = 0.4f;
			break;
		case MobileStaticInfo::ToyCuddly:
		case MobileStaticInfo::ToySkittle:
			// a toy whose info's mesh is ObjectToyCuddly or ObjectToySkittle is held from the side, with the default
			// lowering 0
			hold.type = HoldType::Side;
			hold.radius = radius2D;
			hold.lowering = 0.0f;
			break;
		default:
			break;
		}
		break;
	case HoldClass::Object:
		break;
	}
	return hold;
}
