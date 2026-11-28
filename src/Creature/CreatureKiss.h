/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <optional>
#include <utility>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// Where two creatures' lips meet to kiss. Each species' file gives a moment of its two kissing animations, the high
/// kiss and the low kiss, and the point it kisses with: a point on its lips, else its head. That point, at that moment
/// of each animation and in the body's own space, is the creature's high and low kissing point; a kiss blends the two
/// animations by how far up between them the other creature's lips are.
namespace openblack::creature_kiss
{

/// The two kissing animations: the high kiss, then the low kiss (the species' animation table)
inline constexpr size_t k_KissHigh = 212;
inline constexpr size_t k_KissLow = 213;

/// A creature's kissing points, in its body's own space at its file's size
struct KissPoints
{
	/// At the high kiss's moment, and at the low kiss's
	glm::vec3 high {0.0f};
	glm::vec3 low {0.0f};
};

/// The point a creature kisses at, a share of the way from its low point to its high point, at its size
[[nodiscard]] glm::vec3 PointAt(const KissPoints& points, float share, float size);

/// Whether a creature can kiss another, and how far up its own kiss it kisses. The two can kiss only if the low point of
/// the creature whose high point is higher is no higher than the other's high point; that taller creature kisses at the
/// share of its own rise from its low point that brings it down to the other's high point, the other at its full
/// height. None when they can't
[[nodiscard]] std::optional<float> KissShare(const KissPoints& own, float ownSize, const KissPoints& other, float otherSize);

/// How far ahead of its middle a creature's lips are as it kisses another, at the share it kisses at; 0 when the two
/// can't kiss
[[nodiscard]] float KissingDistance(const KissPoints& own, float ownSize, const KissPoints& other, float otherSize);

/// How far each of two creatures stands from the point halfway between them to kiss: the mean of the two kissing
/// distances, each creature's own towards the other
[[nodiscard]] float MeanKissingDistance(const KissPoints& own, float ownSize, const KissPoints& other, float otherSize);

/// Where two creatures stand to kiss: each the distance from the point halfway between them, on its own side of it.
/// The first of the pair is the creature's place, the second the other's. Two creatures on the same point have no side,
/// and both stand on it
[[nodiscard]] std::pair<glm::vec2, glm::vec2> KissPlaces(glm::vec2 self, glm::vec2 other, float distance);

} // namespace openblack::creature_kiss
