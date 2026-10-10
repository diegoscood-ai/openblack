/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <glm/gtc/type_precision.hpp>
#include <glm/vec3.hpp>

/// How the sky's dome is drawn: the alignments' domes mixed by the sky's alignment, in the colours of the frame's
/// weather, and how much the sun and the moon show through an overcast. The dome's blend for the time of day is
/// sky_type::DomeBlend's.
namespace openblack::sky_dome
{

/// Two of the three alignments' domes and how much of the second there is, of 255
struct Pair
{
	uint8_t lower;
	uint8_t upper;
	uint8_t weight;

	bool operator==(const Pair&) const = default;
};

/// The alignments' domes the sky's alignment mixes, as the game keeps it: 0 good, 1 neutral, 2 evil, and the domes
/// numbered the same way. The first is drawn whole and the second over it.
[[nodiscard]] Pair AlignmentPair(float alignment);

/// The colours each alignment's dome is drawn in: its picture times the first, with the second added, 0 to 255
struct Tint
{
	glm::u8vec3 modulate {255};
	glm::u8vec3 add {0};

	bool operator==(const Tint&) const = default;
};

/// What tints the dome in a frame
struct TintInputs
{
	/// The colour of the distance haze, 0 to 255, which is a third of the land's
	glm::vec3 hazeColour;
	/// The overcast at the camera, 0 for a clear sky and 1 for a full one
	float overcast;
	/// A flash of lightning, 0 to 255
	uint8_t flash;
	/// How much the sky's alignment darkens the dome, 0 to 90
	uint8_t darkness;
	/// The fog setting: an overcast turns the dome towards the haze's colour
	bool fog;
	/// The weather setting: the darkness darkens the dome
	bool weather;
};

/// How much the sky's alignment darkens the dome, with the alignment as the game keeps it, 0 good to 2 evil: nothing
/// from a little on the good side of neutral, then more towards good, 90 at most
[[nodiscard]] uint8_t Darkness(float alignment);
/// The dome's colours: white with nothing added on a clear day, towards the haze's colour as an overcast comes in,
/// darker by the darkness and towards white in a flash of lightning, all in whole steps of the 8-bit channels
[[nodiscard]] Tint TintOf(const TintInputs& inputs);
/// How strongly the sun or the moon shows through an overcast: with the fog setting, a full overcast leaves a ninth
[[nodiscard]] float ThroughOvercast(float alpha, float overcast, bool fog);

} // namespace openblack::sky_dome
