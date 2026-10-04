/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InterfaceInteraction.h"

#include <spdlog/spdlog.h>

#include "Camera/CameraHelp.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"

namespace openblack::help::interface_interaction
{
namespace
{
int32_t s_level = 0;          // GScript+0xA4
bool s_controlSwitch1 = true; // ControlMap +0x652C
bool s_controlSwitch2 = true; // ControlMap +0x6530

/// The two writes of every limiting level: `or bl, 2` then `or cl, 4` on GInterface+0x28 (bit 0, inactive, is kept)
void LimitInterface()
{
	interface_active::SetFlags(static_cast<uint8_t>(interface_active::GetFlags() | 2u));
	interface_active::SetFlags(static_cast<uint8_t>(interface_active::GetFlags() | 4u));
}

void Features(int32_t features)
{
	camera_help::EnableCameraFeatures(features, -1);
}

void TutorialAutoPitch()
{
	camera_help::SetAutoPitch(k_TutorialAutoPitchParam1, k_TutorialAutoPitchParam2, true);
}

void Switches(bool first, bool second)
{
	s_controlSwitch1 = first;
	s_controlSwitch2 = second;
}

/// fn_0046BF20(R) on GInterface+0x3A0: the hand's reach, which the hand keeps (CHand +0x4838, owner Mano). Nothing
/// without a hand system (the unit tests)
void HandReach(int32_t level)
{
	const auto reach = LevelHandReach(level);
	if (!reach.has_value() || !Locator::handSystem::has_value())
	{
		return;
	}
	Locator::handSystem::value().SetHandReach(*reach);
}
} // namespace

void Set(int32_t level)
{
	s_level = level; // 0x70B239, before the range check
	switch (level)   // jump table 0x70B7A8
	{
	case 0: // NORMAL 0x70B24C: the whole byte = 0, so the interface is active again (reach 1800: HandReach below)
		interface_active::SetFlags(0);
		Features(camera_help::k_NormalFeatures);
		Switches(true, true);
		break;
	case 1: // JUST_GRAB 0x70B2B9 (tail 0x70B6C1, the reach 75)
		LimitInterface();
		Features(0x08);
		TutorialAutoPitch();
		Switches(true, false);
		break;
	case 2: // JUST_GRAB_FAR_TO_CITADEL 0x70B317 (tail 0x70B737)
		LimitInterface();
		Features(0x08);
		TutorialAutoPitch();
		Switches(true, false);
		break;
	case 3: // JUST_ROTATE 0x70B365 (tail 0x70B622)
		LimitInterface();
		Features(0x02);
		Switches(true, false);
		break;
	case 4: // JUST_DOUBLE_CLICK_AND_DRAG 0x70B39A (tail 0x70B6A9)
		LimitInterface();
		Features(0x18);
		Switches(true, false);
		break;
	case 5: // JUST_ZOOM 0x70B3CF (tail 0x70B72F)
		LimitInterface();
		Features(0x24);
		Switches(true, false);
		break;
	case 6: // JUST_ROTATE_INTERACT 0x70B404 (tail 0x70B555)
		interface_active::SetFlags(0);
		Features(0x02);
		Switches(true, false);
		break;
	case 7: // JUST_ROTATE_INTERACT_AND_ZOOM 0x70B456
		interface_active::SetFlags(0);
		Features(0x26);
		Switches(true, false);
		break;
	case 8: // JUST_HAND_MOVE 0x70B4BC: the reach is not touched
		LimitInterface();
		Features(0);
		Switches(false, false);
		break;
	case 10: // JUST_ROTATE_AND_DRAG 0x70B56F (tail 0x70B6B1)
		LimitInterface();
		Features(0x0A);
		TutorialAutoPitch();
		Switches(true, false);
		break;
	case 11: // JUST_PITCH 0x70B5BD (tail 0x70B72F)
		LimitInterface();
		Features(0x01);
		Switches(true, false);
		break;
	case 12: // JUST_HAND_INTERACTION 0x70B520: only bit 2, the reach is not touched
		interface_active::SetFlags(static_cast<uint8_t>(interface_active::GetFlags() | 4u));
		Features(0);
		Switches(false, false);
		break;
	case 13: // JUST_GRAB_DOUBLE_CLICK_AND_ROTATE 0x70B5F2 (tail 0x70B622)
		LimitInterface();
		Features(0x1A);
		Switches(true, false);
		break;
	case 14: // ..._AND_PITCH 0x70B679 (tail 0x70B6A9)
		LimitInterface();
		Features(0x1B);
		Switches(true, false);
		break;
	case 15: // ..._PITCH_AND_ZOOM 0x70B6FF (tail 0x70B72F)
		LimitInterface();
		Features(0x3F);
		Switches(true, false);
		break;
	default: // 9 and above 15 (unsigned): 0x70B785
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Unexpected interaction = {}", level);
		break;
	}
	// each level's fn_0046BF20 call (in the tails above). (inferred) done after the other writes: none of them reads the
	// reach, so their order does not matter
	HandReach(level);
}

std::optional<float> LevelHandReach(int32_t level)
{
	switch (level)
	{
	case 1: // JUST_GRAB: 0x42960000 (0x70B30D)
		return k_JustGrabHandReach;
	case 8:  // JUST_HAND_MOVE 0x70B4BC
	case 12: // JUST_HAND_INTERACTION 0x70B520
		return std::nullopt;
	default:
		if (level < 0 || level > 15 || level == 9)
		{
			return std::nullopt; // 0x70B785
		}
		return k_MaxHandReach; // [0x8CBEAC]
	}
}

int32_t GetLevel()
{
	return s_level;
}

bool GetControlSwitch1()
{
	return s_controlSwitch1;
}

bool GetControlSwitch2()
{
	return s_controlSwitch2;
}

bool KeyShortcutsEnabled()
{
	return s_controlSwitch1 && s_controlSwitch2; // 0x63F430 / 0x63F43E
}

bool IsActionBlocked(int32_t action)
{
	if (!s_controlSwitch1)
	{
		// 0x46F75A: action - 3 unsigned up to 0x10, byte table 0x46F7A4 (all 0 but action 5's)
		return action >= 3 && action <= 19 && action != 5;
	}
	if (!s_controlSwitch2)
	{
		return action >= 17 && action <= 19; // 0x46F783..0x46F78B
	}
	return false;
}

void Reset()
{
	s_level = 0;
	Switches(true, true);
}

} // namespace openblack::help::interface_interaction
