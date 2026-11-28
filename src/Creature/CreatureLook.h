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

#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// What a creature watches as it looks about. As its eyes sweep the land, each thing that it sees can take over from
/// what it watches: when it watches nothing, or when the thing catches its eye more than what it watches holds it. The
/// longer it watches one thing, the less a new thing catches its eye. It watches the thing until the thing goes out of
/// view.
namespace openblack::creature_look
{
/// The kinds of things a creature finds interesting, from most to least, then the loose statics: a toy as much as a
/// dove, any other as much as an abode
enum class Interest : uint8_t
{
	Dove,
	Citadel,
	Creature,
	Animal,
	Villager,
	Abode,
	Tree,
	Fixed,
	Toy,
	Static,
	/// Everything else, which catches its eye only when nothing else does
	None,
};
[[nodiscard]] float InterestOf(Interest kind);
/// How much a loose static catches a creature's eye: a toy more than twice as much as any other
[[nodiscard]] constexpr Interest StaticInterest(bool isToy)
{
	return isToy ? Interest::Toy : Interest::Static;
}

/// After this many whole seconds of watching one thing, nothing else catches the creature's eye
constexpr uint32_t k_BoredSeconds = 20;
/// How high a creature's eyes are above its feet at size 1
constexpr float k_HeadHeight = 15.0f;
/// A creature keeps a thing in view when it can see the thing, and the thing is less than a sixth of a turn from the
/// way its body faces. The game measures this difference without a wrap at a full turn. A thing in its own map cell is
/// always in view. The angles are game angles.
constexpr int32_t k_BodyViewParts = 6;

/// How many map cells a creature's eyes sweep for a creature shown at a size: a square whose side is the odd number the
/// square root of sixty times its size plus a hundred and sixty comes down to
[[nodiscard]] uint32_t SweepCells(float shownSize);
/// How far a creature looks about, in metres: bigger creatures see further
[[nodiscard]] float LookRange(float size);
/// Things within half the range are as interesting as they get. Further things are less so: nothing at the range, and
/// less than nothing past it.
[[nodiscard]] float DistanceFactor(float distance, float range);

/// A thing that the creature sees as its eyes sweep the land, and how far it is along the ground
struct Candidate
{
	uint32_t id;
	Interest kind;
	float distance;
};

/// What the creature can watch: a thing, or the mountains or the water that it finds on its map of the land as it moves
enum class Sight : uint8_t
{
	Thing,
	Mountains,
	Water,
};

/// How much each kind of sight catches the eye of one creature
struct Weights
{
	float things {0.0f};
	float mountains {0.0f};
	float water {0.0f};
};
[[nodiscard]] float WeightOf(const Weights& weights, Sight sight);

/// The mountains and the water have this interest
constexpr float k_LandInterest = 0.5f;

/// What the creature watches: the thing, or the place of the mountains or the water; its interest when the creature
/// started to watch it, where the creature looks to see it, and for how many turns it has watched it
struct Target
{
	std::optional<uint32_t> id;
	Sight sight {Sight::Thing};
	/// The place on the land of the mountains or the water, along the ground
	std::optional<glm::vec2> place;
	float interest {0.0f};
	glm::vec3 point {0.0f};
	uint32_t watchedTurns {0};
};
/// Whether the creature watches something
[[nodiscard]] bool Watches(const Target& target);

/// How much what it watches holds the creature: the creature's weight for that kind of sight, times the interest, times
/// the distance factor at the distance now (held between 0 and the range). Nothing holds it when it watches nothing.
[[nodiscard]] float Hold(const Target& target, float distance, float range, float weight);
/// How much a new sight catches the eye: ((boredom x weight) x distance factor) x interest, in this order. The boredom
/// is 1 less the whole seconds of watching (at most k_BoredSeconds) over k_BoredSeconds.
[[nodiscard]] float Catches(const Target& target, float weight, float interest, float distance, float range,
                            uint32_t turnsPerSecond);
/// A thing in view that the creature sees takes over what it watches, if it watches nothing or if the thing catches its
/// eye more than what it watches holds it. A thing catches its eye by the weight for things, its interest and the
/// distance factor, less as the whole seconds of watching go up to k_BoredSeconds. Returns whether the thing took over.
bool Consider(Target& target, const Candidate& candidate, float watchedDistance, float range, const Weights& weights,
              uint32_t turnsPerSecond);

/// The nearest mountains or water that the creature finds on its map as it moves, and how far they are along the ground
struct LandCandidate
{
	Sight sight {Sight::Mountains};
	glm::vec2 place {0.0f};
	float distance {0.0f};
};
/// The mountains or the water take over what the creature watches by the same rule as a thing, with the weight for
/// their kind and k_LandInterest. The creature does not have to see them. Returns whether they took over.
bool ConsiderLand(Target& target, const LandCandidate& candidate, float watchedDistance, float range, const Weights& weights,
                  uint32_t turnsPerSecond);
/// One turn of watching: while what the creature watches is in view, the creature watches it a turn longer. Else the
/// creature watches nothing.
void Watch(Target& target, bool inView);

/// Where the creature looks about: at what it watches, or else at its destination on the land. The destination is the
/// place that it last set out for, as high above the land as that place is. It does not change when the creature
/// arrives.
[[nodiscard]] glm::vec3 LookAboutPoint(const Target& target, glm::vec3 destination);
} // namespace openblack::creature_look
