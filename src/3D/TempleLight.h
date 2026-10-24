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

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace openblack
{

/// How far the temple fades what it draws with its depth from the camera: nothing nearer than `from`, the most from
/// `to` on
struct TempleFade
{
	float from {0.0f};
	float to {0.0f};
};

/// The temple's fade with depth, which the game works out each frame for the room the player is in: in 1024 steps from
/// its start to its end, each colour's red, green and blue darkened down to 100/256 of themselves
namespace temple_fade
{
/// The creature's room's, which the room sets each time it is drawn; every other room's; and the fade with no room
constexpr TempleFade k_CreatureRoom {.from = 40.0f, .to = 150.0f};
constexpr TempleFade k_Room {.from = 60.0f, .to = 200.0f};
constexpr TempleFade k_NoRoom {.from = 30.0f, .to = 80.0f};
constexpr int32_t k_Steps = 1024;
/// What the last step leaves of 256
constexpr int32_t k_Darkest = 100;

/// The fade a frame draws with: the room the player is in, as it was last set. The frame's fade is chosen before its
/// rooms are drawn, so the creature's room has a room's fade until it has been drawn once
[[nodiscard]] TempleFade Of(bool inARoom, bool inCreatureRoom, bool creatureRoomDrawnBefore);
/// The depth of a point along the camera's axis, the view looking down +z, summed z, y, x and then the offset, each a
/// float
[[nodiscard]] float Depth(const glm::mat4& view, const glm::vec3& point);
/// The share of a colour the fade leaves at a depth, 0 to 255 over 256: the full 255 nearer than its start (or at no
/// depth that is a number), 100 past its end
[[nodiscard]] uint32_t Share(float depth, const TempleFade& fade);
/// A colour, 0xAARRGGBB, faded at a depth: its red, green and blue each times the share over 256, its alpha kept
[[nodiscard]] uint32_t Apply(uint32_t argb, float depth, const TempleFade& fade);

/// The step of the fade a depth is in, 0 to 1023: the first nearer than its start (or at no depth that is a number), the
/// last past its end, else the way from the start times the steps per unit, each a float, truncated
[[nodiscard]] int32_t Step(float depth, const TempleFade& fade);
/// The steps of the fade in one unit of depth, a float, as the shaders take it
[[nodiscard]] float StepsPerUnit(const TempleFade& fade);
/// The share of a colour a step leaves, 0 to 255 over 256
[[nodiscard]] uint32_t ShareOfStep(int32_t step);
/// The colour the fade adds after the texture at its end, of 255 per channel: each byte of the temple's light times
/// two fifths, as floats
[[nodiscard]] glm::vec3 EndColour(glm::vec3 lightMultiply);
/// The colour the fade adds at a step, of 255 per channel: the step's share of the end's colour, each rounded to the
/// nearest whole number, halves to even
[[nodiscard]] glm::vec3 AddedAt(int32_t step, glm::vec3 endColour);

/// A colour and the colour added after the texture, each of 255 per channel, as the fade leaves them
struct Fogged
{
	glm::vec3 colour {0.0f};
	glm::vec3 added {0.0f};
};
/// A colour and its added colour faded once at a depth, as a belt or medal is where it stands and a room's submesh
/// without a lightmap is at its first vertex: the colour times the share over 256, truncated, and the step's colour
/// added to the added colour, each channel at most 255
[[nodiscard]] Fogged Once(glm::vec3 colour, glm::vec3 added, float depth, const TempleFade& fade, glm::vec3 endColour);
/// A lightmapped wall's vertex of a colour and an added colour, faded at its depth: nearer than the start (or at no
/// depth that is a number) it keeps both, from the end on it takes the last step's grey and colour alone, and between
/// them each byte moves towards the step's by a quarter of the step over 256, rounded down. The GPU side is
/// assets/shaders/temple_fog.sh
[[nodiscard]] Fogged AtVertex(glm::vec3 colour, glm::vec3 added, float depth, const TempleFade& fade, glm::vec3 endColour);
} // namespace temple_fade

/// The temple's light, which the game works out each frame from the alignment of the realm the player came in from:
/// a colour the rooms and what is in them are multiplied by, and one added after. An evil temple is lit red, with a red
/// pulse, and a good one has a slow rainbow pulse, each the stronger the further the alignment leans. A neutral temple
/// is lit as it is.
struct TempleLight
{
	glm::vec3 multiply {1.0f};
	glm::vec3 add {0.0f};
	/// How the temple fades with depth this frame
	TempleFade fade {temple_fade::k_Room};

	/// The light at an alignment, from -1, evil, to 1, good, at a time in milliseconds the caller gives, the pulse going
	/// round every 4.096 seconds. It reads no clock of its own.
	[[nodiscard]] static TempleLight At(float alignment, uint32_t milliseconds);
	/// An object's colour, in the temple's light
	[[nodiscard]] glm::vec3 Colour(glm::vec3 colour) const;
};

} // namespace openblack
