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
#include <functional>
#include <vector>

#include "Creature/CreatureDesires.h"
#include "Enums.h"

namespace openblack
{
namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;
} // namespace openblack

/// What a new creature draws as it is made, in the game's order: its body's strength and fatness, then each desire's
/// decay and growing time, then each desire's sources, then its innate personality, then a value for each of its
/// previous actions. The game draws them all on its synchronised stream.
namespace openblack::creature_creation
{
/// The innate personality a species' row holds: kindness, aggression, lethargy, friendliness, communicativeness, then
/// four not yet named
constexpr size_t k_PersonalityCount = 9;

/// How far a source's start value and threshold are each moved, either way
struct SourceSpread
{
	float value {0.0f};
	float threshold {0.0f};
};

/// What a species' creature is made from, taken from the game's creature tables
struct Setup
{
	float strength {0.0f};
	float startFatness {0.0f};
	std::array<creature_desires::DesireSetup, creature_desires::k_DesireCount> desires {};
	/// One for each of the desire's sources, in their order
	std::array<std::vector<SourceSpread>, creature_desires::k_DesireCount> spreads {};
	std::array<float, k_PersonalityCount> personality {};
	/// How many previous actions get a value: one for each of the creature's actions
	size_t actionCount {0};
};

/// A new creature as its draws make it
struct Made
{
	float strength {0.0f};
	float fatness {0.0f};
	creature_desires::Desires desires;
	std::array<float, k_PersonalityCount> personality {};
	std::vector<uint32_t> previousActions;
};

/// The stream the draws come from. Each is asked only for a range that is not 0, as the game's draws make none for 0
struct Random
{
	/// A whole number below the range
	std::function<uint32_t(uint32_t)> rand;
	/// A float from 0 towards the range
	std::function<float(float)> floatRand;
};

/// The species' setup from the game's tables: its row's strength, start fatness and personality, its desires as
/// they start with their sources, and how far each source is spread
[[nodiscard]] Setup SetupFor(const InfoConstants& info, CreatureType species);

/// The draws of a new creature, in the game's order:
/// - strength, then fatness: each the species' value moved by up to 0.1 either way
/// - for each desire in turn: its decay, drawn in its range, then its growing time moved by up to a fifth either way
/// - for each desire, for each of its sources: its value moved by its spread and kept within 0 and 1, then its
///   threshold the same way
/// - each personality value moved by up to 0.1 (the first five), 0.3 (the sixth) or 0.4 (the last three) and kept
///   within 0 and 1, drawing again at each test the clamp makes: 2, 3 or 4 draws each
/// - one whole number below 120 for each previous action
[[nodiscard]] Made Make(const Setup& setup, const Random& random);
} // namespace openblack::creature_creation
