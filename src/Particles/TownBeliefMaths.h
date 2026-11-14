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

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>

/// The players' symbols over a town centre's totem (TownCentreBelief, TownBelief.cpp), as pure functions: the
/// player the town believes in most has its symbol over the totem; each of the others circles round it, a level higher
/// for each player believed in more, wider, quicker and bigger the more it is believed in. The second one now and then
/// comes down to fight the first, with sparks while the fight is at its height. raffclar's names and layout, with the
/// original's numbers and order of operations.
namespace openblack::particles::town_belief
{

/// The players of a land: the 7 gods and the neutral player
inline constexpr size_t k_PlayerCount = 8;

/// The effect file's numbers (TownCentreBelief), each pair at no belief (At0) and full belief (At1); the defaults are
/// the rule's own, the file gives all but FightFracWhenEmitting
struct Params
{
	float radiusAt0 {10.0f};
	float radiusAt1 {2.0f};
	float speedAt0 {2.0f};
	float speedAt1 {10.0f};
	float scaleAt0 {0.0f};
	float scaleAt1 {1.0f};
	/// How high over the totem's top the symbols turn
	float heightAt1 {0.0f};
	/// Each player believed in more raises a symbol by this
	float heightPerLevel {2.0f};
	/// Whether the symbols' way round grows with the camera's distance, and by how much a unit of distance
	bool useFixedScale {true};
	float fixedScale {1.0f};
	float timeBetweenFightsAt0 {10.0f};
	float timeBetweenFightsAt1 {5.0f};
	float fightDurationAt0 {1.0f};
	float fightDurationAt1 {2.0f};
	/// How much faster a fighting symbol turns at the fight's height
	float speedUpDuringFight {2.0f};
	/// The sparks fly while the fight is past this
	float fightFracWhenEmitting {0.2f};
	/// How fast the tilt's sway goes round
	float angleSpeedPhaseSpeed {0.2f};
};

/// A symbol's own motion: its way round, the tilt of that way, the tilt's sway, and its fight's clock
struct Symbol
{
	/// Radians: round the totem (a2), and up from level (a1); both random at first
	float around {0.0f};
	float tilt {0.0f};
	float sway {0.0f};
	/// Seconds until the next fight; seconds into the fight and how long it lasts
	float untilFight {0.0f};
	float intoFight {0.0f};
	float fightLength {0.0f};
};

/// How a symbol stands after a step
struct Look
{
	/// From the point over the totem
	glm::vec3 offset {0.0f};
	float scale {0.0f};
	/// How far into its fight it is: 0 out of a fight, rising to 1 at its height and back to 0
	float fight {0.0f};
};

/// A belief as the symbols show it: 0..1, and 0 for none or not a number
[[nodiscard]] float Shown(float belief);

/// How many players the town believes in more than this one: those believed in more, or as much with a later number,
/// among all the players of the land (the neutral one included)
[[nodiscard]] int Rank(const std::array<float, k_PlayerCount>& beliefs, int player);

/// How much the symbols' way round grows at this distance from the camera: distance x FixedScale, 1..10; 1 without
/// UseFixedScale
[[nodiscard]] float DistanceScale(const Params& params, float distance);

/// One step of a symbol for its belief and rank, dt seconds on; random(a, b) is the effect's random number between a
/// and b, drawn only when a fight ends (the next wait, then the next fight's length)
[[nodiscard]] Look Step(Symbol& symbol, const Params& params, float belief, int rank, float dt, float distanceScale,
                        const std::function<float(float, float)>& random);

/// How strong the sparks of a fight are: the alpha 255 (1 - (2f - 1)^2) of the fight f clamped to 0..1, truncated
[[nodiscard]] uint8_t SparkAlpha(float fight);
/// Whether a fight throws sparks: past FightFracWhenEmitting
[[nodiscard]] bool Sparking(const Params& params, float fight);

/// The players who get a symbol, in the game's order: the slots before the neutral one whose type (`typeOf`: 0 for
/// nobody, 3 for the neutral player) is not 0; after a player, the walk stops at a neutral one
[[nodiscard]] std::vector<int> SymbolPlayers(const std::function<int32_t(int)>& typeOf);

} // namespace openblack::particles::town_belief
