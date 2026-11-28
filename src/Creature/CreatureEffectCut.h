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

#include <functional>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The cut an effect leaves on a creature it harms out of a fight: a line aimed at the body's groin, along which the
/// first triangle of the skin it meets takes a wound. An effect that crushes more than it burns cuts from where the
/// effect came from, of a kind and a picture that go by how hard it hurts; any other marks from about the groin, and
/// only by chance, rarer the more the skin is already marked. Every sum and product is rounded to a float after each operation,
/// in the order written. Docs: docs/wiki/magic/spell-core.md#a-creature-taking-an-effect.
namespace openblack::creature_effect_cut
{
/// The wound kinds (the damage atlas's rows) the cut leaves: from where the effect came, the first below a damage of a
/// half and the second from it; about the groin the first, or one time in k_RareKindOneIn the seventh
constexpr uint8_t k_LowDamageKind = 0;
constexpr uint8_t k_HighDamageKind = 1;
constexpr uint8_t k_GroinKind = 0;
constexpr uint8_t k_RareGroinKind = 6;
/// The damage from which a cut from where the effect came takes the second kind
constexpr float k_HighDamage = 0.5f;
/// About the groin: the rare kind one time in this many, and a mark at all one time in (marked share x 140 + 10)
constexpr uint32_t k_RareKindOneIn = 3;
constexpr float k_MarkedShareWeight = 140.0f;
constexpr float k_MarkedShareBase = 10.0f;
/// The box about the groin a mark there is drawn in, across x and z and up y, in the body's size times 15 (its standing
/// height): an eighth of that each side across, a half each side up
constexpr float k_HeightPerSize = 15.0f;
constexpr float k_AcrossShare = 0.125f;
constexpr float k_UpShare = 0.5f;
/// A line shorter than this, squared, cuts nothing
constexpr float k_LeastLength2 = 0.01f;
/// The picture's column: a draw of 5, less 2, less the damage x 7 - 7 cut to a whole number, kept to 0..7
constexpr uint32_t k_ColumnDraws = 5;
constexpr float k_ColumnPerDamage = 7.0f;
constexpr uint8_t k_LastColumn = 7;

/// The game random draws a cut makes, in the order it makes them
struct Random
{
	std::function<uint32_t(uint32_t)> integer;
	std::function<float(float)> real;
};

/// What the cut is worked out from
struct Inputs
{
	/// The harm, already kept to 0..1, and above 0
	float damage {0.0f};
	float burn {0.0f};
	float crush {0.0f};
	/// Where the body's groin is in the pose the game reads, and the size the body is drawn at
	glm::vec3 groin {0.0f};
	float size {1.0f};
	/// The share of the skin's room for wounds already taken (CreatureMarks: wounds / k_MaxMarks)
	float markedShare {0.0f};
	/// Where the effect came from, as a point in the world
	glm::vec3 from {0.0f};
};

/// A line the body is cut along: from a point, towards the groin (not made unit length), and the wound's kind and column
struct Cut
{
	glm::vec3 from {0.0f};
	glm::vec3 direction {0.0f};
	uint8_t type {0};
	uint8_t column {0};
};

/// The harm as the cut reads it: below 0 (or not a number) 0, above 1 1
[[nodiscard]] float KeptDamage(float damage);

/// The picture's column for a draw of k_ColumnDraws at a damage
[[nodiscard]] uint8_t Column(uint32_t draw, float damage);

/// The cut, drawing from the game random numbers as it goes; none when chance or a line too short gives none
[[nodiscard]] std::optional<Cut> Plan(const Inputs& inputs, const Random& random);

/// A creature that has just caught fire is cut k_CaughtFireCuts times about its groin: each a point drawn in the box
/// about the groin, then, for a line long enough, the kind (the rare one time in k_RareKindOneIn) and a column of
/// k_CaughtFireColumns. The cuts in the order drawn, a line too short making none
constexpr uint32_t k_CaughtFireCuts = 3;
constexpr uint32_t k_CaughtFireColumns = 8;
[[nodiscard]] std::vector<Cut> CaughtFire(glm::vec3 groin, float size, const Random& random);

/// Where on a skin of 256 texels a point on a triangle is, along one axis of the texture: the first corner's coordinate
/// plus s of the way to the second's and t to the third's, times 256, cut towards 0; anything past the last texel, a
/// negative amount too (read as a large unsigned one), is 255
[[nodiscard]] uint8_t Texel(float first, float second, float third, float s, float t);
} // namespace openblack::creature_effect_cut
