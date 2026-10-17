/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleChallengeRoom.h"

#include <cctype>

#include <algorithm>
#include <array>
#include <string>

#include <fmt/format.h>

#include "3D/TempleChallengePick.h"

using namespace openblack;
using namespace openblack::temple_challenge_room;

namespace
{
/// The controls' names, as the game's table gives them (the mesh has them in other cases), each followed by the slot
/// from 01 to 21 but the scroll's
struct ControlName
{
	std::string_view prefix;
	Control control;
};
constexpr std::array<ControlName, 5> k_SlotNames {{
    {"LH_Picture_Challenge_", Control::Picture},
    {"LH_FRAME_CHALLENGE_", Control::Frame},
    {"LH_FRAME_CHALLENGE_SILVER_", Control::SilverFrame},
    {"LH_FRAME_CHALLENGE_GOLD_", Control::GoldFrame},
    {"LH_Button_Challenge_", Control::Button},
}};
constexpr std::string_view k_ScrollName = "LH_Scroll_Challenge";

bool SameName(std::string_view a, std::string_view b)
{
	return std::ranges::equal(a, b, [](char x, char y) {
		return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
	});
}
} // namespace

std::optional<ControlSlot> temple_challenge_room::ControlOf(std::string_view subMeshName)
{
	if (SameName(subMeshName, k_ScrollName))
	{
		return ControlSlot {.control = Control::Scroll, .slot = 0};
	}
	for (const auto& name : k_SlotNames)
	{
		for (uint32_t slot = 0; slot < k_Slots; ++slot)
		{
			if (SameName(subMeshName, fmt::format("{}{:02}", name.prefix, slot + 1)))
			{
				return ControlSlot {.control = name.control, .slot = slot};
			}
		}
	}
	return std::nullopt;
}

float temple_challenge_room::BlendStep(float blend, bool looking, float seconds)
{
	constexpr float k_Rise = 0.75f;
	constexpr float k_Fall = 0.7f;
	if (!looking)
	{
		// from at most 1 (an unordered blend is kept), and not under 0 (an unordered one becomes 0)
		if (blend > 1.0f)
		{
			blend = 1.0f;
		}
		blend = blend - seconds * k_Fall;
		if (!(blend > 0.0f))
		{
			blend = 0.0f;
		}
		return blend;
	}
	blend = blend + seconds * k_Rise;
	if (blend > 1.0f)
	{
		blend = 1.0f;
	}
	return blend;
}

bool temple_challenge_room::Visible(Control control, const std::optional<Challenge>& challenge)
{
	switch (control)
	{
	case Control::Picture:
		return challenge.has_value();
	case Control::Frame:
		return !challenge.has_value();
	case Control::SilverFrame:
		return challenge.has_value() && !challenge->gold;
	case Control::GoldFrame:
		return challenge.has_value() && challenge->gold;
	case Control::Button:
		return challenge.has_value() && temple_challenge_pick::CanChoose(challenge->success);
	case Control::Scroll:
		return true;
	}
	return false;
}
