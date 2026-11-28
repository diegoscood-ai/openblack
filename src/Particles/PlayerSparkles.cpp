/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerSparkles.h"

#include <array>

namespace openblack::player_sparkles
{

namespace
{
/// A channel half way to a value
constexpr uint32_t Toward(uint32_t channel, uint32_t value)
{
	return (channel + value) >> 1u;
}

/// Three quarters of a channel
constexpr uint32_t Darker(uint32_t channel)
{
	return (channel * 3u) >> 2u;
}

constexpr float k_Pi = 3.1415927f;
constexpr float k_TwoPi = 6.2831855f;
/// How far above the land and to the north of its citadel a god's camera is reported
constexpr float k_OverCitadel = 20.0f;
} // namespace

bool Shows(Played played, bool mindOn, bool personInGame)
{
	switch (played)
	{
	case Played::Computer:
		return mindOn;
	case Played::Person:
		return personInGame;
	case Played::Nobody:
	case Played::Neutral:
		break;
	}
	return false;
}

uint32_t Colour(uint32_t godColour, uint32_t tint)
{
	std::array<uint32_t, 3> channels {
	    ((godColour >> 16u) & 0xFFu) >> 1u,
	    ((godColour >> 8u) & 0xFFu) >> 1u,
	    (godColour & 0xFFu) >> 1u,
	};
	auto& [red, green, blue] = channels;
	constexpr uint32_t k_Full = 0xFFu;
	constexpr uint32_t k_Pale = 0x90u;
	switch (tint)
	{
	case 0:
		red = Toward(red, k_Full);
		green = Toward(green, k_Pale);
		blue = Toward(blue, k_Pale);
		break;
	case 1:
		red = Toward(red, k_Full);
		green = Toward(green, k_Full);
		blue = Toward(blue, k_Pale);
		break;
	case 2:
		red = Toward(red, k_Pale);
		green = Toward(green, k_Full);
		blue = Toward(blue, k_Pale);
		break;
	case 3:
		red = Darker(red);
		green = Darker(green);
		blue = Darker(blue);
		break;
	default:
		break;
	}
	return (k_Alpha << 24u) | (red << 16u) | (green << 8u) | blue;
}

physics::turn::PuffKind KindOf(uint32_t roll)
{
	return roll == 0 ? physics::turn::PuffKind::MoteReversed : physics::turn::PuffKind::Mote;
}

float TurnSymbol(float angle, uint32_t milliseconds)
{
	// each step rounds to a float, as the game's 24-bit FPU does
	const float turned = static_cast<float>(milliseconds) * k_SymbolTurnPerMs + angle;
	if (k_Pi < turned)
	{
		return turned - k_TwoPi;
	}
	if (turned < -k_Pi)
	{
		return turned + k_TwoPi;
	}
	return turned;
}

uint8_t SymbolCell(uint32_t playerNumber)
{
	return static_cast<uint8_t>(playerNumber & 0x3Fu);
}

glm::vec3 CameraOverCitadel(glm::vec3 focus)
{
	return {focus.x, focus.y + k_OverCitadel, focus.z + k_OverCitadel};
}

} // namespace openblack::player_sparkles
