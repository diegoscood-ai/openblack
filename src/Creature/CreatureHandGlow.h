/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The glows that a creature can have on its body, as on the hands of a creature that has a creed power. A script gives
/// them (SET_CREATURE_CREED_PROPERTIES), and the creature hums while any of them has power. Each glow is drawn as the
/// creed model's glowing shell (see particles::volume_glow), the model itself unseen: white, its texture sliding with
/// the real time of each frame, reaching further out the more power the glow has.
namespace openblack::creature_hand_glow
{

/// Where a glow is on the body
enum class Place : uint8_t
{
	/// The left hand: the mirror bone of the right hand
	LeftHand = 0,
	/// The belly bone
	Belly = 1,
	RightHand = 2,
	/// Above the body, where it turns about the up axis with the game's time
	Circling = 3,
};
constexpr size_t k_PlaceCount = 4;

/// One glow. A new glow has a power and a scale of 1.
struct Glow
{
	float power {1.0f};
	float scale {1.0f};
	/// How far the circling glow has turned, in radians
	float turn {0.0f};
	/// How far the shell's texture has slid across and up, 0 to 1
	glm::vec2 slide {0.0f};

	bool operator==(const Glow&) const = default;
};
using Glows = std::array<std::optional<Glow>, k_PlaceCount>;

/// The shell reaches out this far for each unit of power, in parts of the model's half diagonal
inline constexpr float k_ZoomPerPower = 2.0f;
/// How fast the shell's texture slides across and up, in sheets a second
inline constexpr glm::vec2 k_SlideSpeed {2.0f, 0.2f};
/// How much of the texture the shell spans across and up
inline constexpr glm::vec2 k_TextureFraction {0.5f, 0.2f};
/// The shell's texture, by its number among the glowing shells' textures (particles::volume_glow::k_Textures)
inline constexpr int k_Texture = 3;
/// The shell's colour: white, its alpha whole
inline constexpr uint32_t k_Argb = 0xFFFFFFFFu;
/// The glow's model is this many times the body's scale
inline constexpr float k_BodyScale = 1.0f;
/// The model's back axis is multiplied by this: the model is drawn in mirror image along it
inline constexpr float k_BackAxis = -1.0f;
/// The circling glow turns this many radians each second of the game's time
inline constexpr float k_TurnSpeed = 1.0f;
/// The circling glow is this many times the body's drawn size, times k_HeightPerSize, above where the body stands
inline constexpr float k_CirclingHeight = 1.3f;
inline constexpr float k_HeightPerSize = 15.0f;

/// Gives a place a glow of a scale and a power. A power of 0 (or not a number) removes the glow. A place without a glow
/// gets a new one.
void Set(Glows& glows, Place place, float scale, float power);

/// The place of a creed power that a script gives, if the number names a place
[[nodiscard]] std::optional<Place> PlaceOf(int32_t number);
/// Whether a glow has a power above 0
[[nodiscard]] bool AnyActive(const Glows& glows);

/// What the hum of a creed power does after a change to the glows
enum class Hum : uint8_t
{
	/// The glows had power before the change and have power after it, or had none and have none
	Unchanged,
	/// The glows had no power before the change and have power after it: the hum starts
	Start,
	/// The glows had power before the change and have no power after it: the hum stops
	Stop,
};
/// A script gives a creature a creed power: the place gets a glow of the scale and the power (see Set). The hum starts
/// or stops when the glows change from no power to some power, or back. The script also gives a time, which has no
/// effect.
[[nodiscard]] Hum SetCreedPower(Glows& glows, Place place, float scale, float power);

/// The first of the animation moments that set a creed glow (their sound actions, which make no sound). Each place has
/// five moments in turn, one for each level.
inline constexpr int32_t k_FirstCreedEvent = 95;
/// The last of the animation moments that set a creed glow
inline constexpr int32_t k_LastCreedEvent = 109;
/// The places of the creed glows that the animation moments set, five moments for each place, in the order of the
/// moments
inline constexpr std::array<Place, 3> k_CreedEventPlaces {Place::LeftHand, Place::RightHand, Place::Belly};
/// The level of each of a place's five moments. A level of 0 removes the glow.
inline constexpr std::array<float, 5> k_CreedEventLevels {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
/// The scale of a glow that an animation moment sets
inline constexpr float k_CreedEventScale = 1.0f;

/// The glow that an animation moment sets: the place, and a power of two times the moment's level
struct CreedEvent
{
	Place place;
	float power;
};
/// The creed glow that an animation moment sets, if the moment's action is one that sets a glow
[[nodiscard]] std::optional<CreedEvent> CreedEventOf(int32_t event);

/// How far a glow's shell reaches out, in parts of the model's half diagonal: by its power
[[nodiscard]] float Zoom(const Glow& glow);
/// The circling glow turns on by some milliseconds of the game's time
void Turn(Glow& glow, uint32_t gameMilliseconds);
/// The axes of the circling glow, turned about the up axis by an angle: its sine and cosine each kept as a float
[[nodiscard]] glm::mat3 CirclingAxes(float turn);
/// Where the circling glow is: above where the body stands, by its drawn size
[[nodiscard]] glm::vec3 CirclingPoint(glm::vec3 bodyOrigin, float size);
/// The matrix the glow's model is drawn with: the axes, each times the body's scale, the back axis in mirror image, then
/// each times the glow's scale; at the point
[[nodiscard]] glm::mat4 ModelMatrix(const glm::mat3& axes, glm::vec3 point, float bodyScale, float glowScale);

/// The body a glow is placed on, as it is drawn this frame
struct Body
{
	/// Its bones in the world, by their numbers in its skeleton, and each bone's parent and mirror bone
	std::span<const glm::mat4> bones;
	std::span<const uint32_t> parents;
	std::span<const uint32_t> mirror;
	/// The bones its species acts with: its right hand, whose mirror is the left, and its belly
	uint32_t rightHand {0};
	uint32_t belly {0};
	/// Where it stands, its scale and its size
	glm::vec3 origin {0.0f};
	float scale {1.0f};
	float size {1.0f};
};

/// A glow where the frame draws it: its model's matrix and scale (the body's scale times the glow's), how far its shell
/// reaches out and how far its texture has slid
struct Placed
{
	glm::mat4 model {1.0f};
	float scale {1.0f};
	float zoom {0.0f};
	glm::vec2 slide {0.0f};
};

/// Places a glow on a body for a frame, by some real and game milliseconds of it. A hand glow is at the middle of the
/// ends of the bones under its hand (the left hand's the mirror of the right), with the hand bone's axes made one long;
/// the belly glow at the belly bone, with its axes the same; the circling glow turns by the game's time about the up
/// axis, above where the body stands. The shell reaches out by the glow's power, and its texture slides on by the real
/// time. None (the glow left as it was) when the body's bones do not cover the glow's bone.
[[nodiscard]] std::optional<Placed> PlaceOn(Glow& glow, Place place, const Body& body, uint32_t realMilliseconds,
                                            uint32_t gameMilliseconds);

} // namespace openblack::creature_hand_glow
