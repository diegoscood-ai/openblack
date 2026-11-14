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
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/Dance.h"
#include "ECS/DanceShapes.h"

namespace openblack::dance
{
struct DanceFile;
struct DanceKeyFrame;
} // namespace openblack::dance

// How a dance runs from turn to turn: when it starts, how fast it goes, how it shares its dancers between its groups and
// when its clock starts over. Pure rules on the dance's component, the same for every dance: a worship site's, a
// town's or a script's.

namespace openblack::ecs::dance_rules
{

/// The game's turns in a second, by which the dance's times are reckoned
inline constexpr uint32_t k_TurnsPerSecond = 10;
/// Waiting for the dancers on their way, a dance starts at the latest this many seconds after its first dancer came
inline constexpr uint32_t k_LongestWaitSeconds = 90;
/// Every dance is made at a quarter speed
inline constexpr float k_MadeSpeed = 0.25f;

/// What a key frame's action does to a group's membership (the dance's moves do the rest)
enum class ActionType : uint32_t
{
	/// The group's shape about the dance's place
	Formation = 14,
	/// Whether the group takes a fixed number of dancers, or a share of them
	Membership = 6,
	/// The kind of dancer the group takes
	DanceType = 15,
	/// The sexes the group takes
	Sexes = 16,
};

/// The rate the groups move at for a speed of 0 to 1: the speed in tenths, rounded down, times 0.4
[[nodiscard]] float RateForSpeed(float speed);

/// The dance is asked to go at a speed: its rate follows, and a new rate while it is danced starts it over
void SetSpeed(components::Dance& dance, float speed);

/// A worship site's dance follows how hard its dancers are chanting: danced while the intensity is above 0, stopped at
/// 0, at that speed
void SetWorshipSpeed(components::Dance& dance, float intensity);

/// Whether enough of its dancers have come for it to start: more than half of those on their way, or the longest wait
/// gone by since the first came. Notes the turn the first dancer came.
bool HasProperlyStarted(components::Dance& dance, uint32_t turn);

/// How many turns the dance's loop lasts before its clock starts over: 120 half seconds for each length of its loop
[[nodiscard]] uint32_t LoopTurns(const components::Dance& dance);

/// The groups' weights in a round: each shared group's quota over the largest number that divides them all, and the
/// round 100 over it. Nothing changes when no shared group has a quota.
void SetWeights(components::DanceGroups& groups);

/// What a key frame does to the groups, each action done to its groups the last listed first: their membership, and
/// their shapes and moves. A group numbered past the last makes one group more. Each time an action starts a group's
/// move, `startsMove` is told the group at once, before the next action or group: once for each action and group
/// The numbers the game names its dance groups by, one for each group it makes ("Untitled N"), counted over the whole
/// game: a group a dance's file names takes its number all the same
using UntitledNumbers = std::function<uint32_t()>;
/// The name the game gives a dance group as it makes it
[[nodiscard]] std::string UntitledName(uint32_t number);

void ApplyKeyFrame(components::Dance& dance, const dance::DanceKeyFrame& keyFrame,
                   const std::function<void(std::size_t)>& startsMove, const UntitledNumbers& untitled = {});
/// Every key frame up to a time on the clock, as a dance does once its file is read
void ApplyKeyFramesUpTo(components::Dance& dance, const dance::DanceFile& file, float clock,
                        const UntitledNumbers& untitled = {});

/// A newcomer joins the first group with a fixed number of dancers that has room and takes its dance type and sex,
/// else the shared group whose turn it is in the round that takes them, and the dance has one more dancer. None when
/// no group does.
std::optional<std::size_t> AddDancer(components::Dance& dance, entt::entity dancer, uint32_t danceType, uint32_t sex);
/// A dancer leaves its group; those after it move up a place
void RemoveDancer(components::Dance& dance, std::size_t group, entt::entity dancer);
/// The first dancer of the first group that has one other than `exclude`, looking in the first `groupCount` groups
/// only: the number of groups its file names
[[nodiscard]] entt::entity FirstDancer(const components::DanceGroups& groups, std::size_t groupCount, entt::entity exclude);

/// Whether a new dance has a footpath laid to it from a town's storage pit: every row but the worship dance and the
/// worship sites' dances
[[nodiscard]] bool LaysTownFootpath(DanceInfo type);

/// Whether a key frame is the one for a time on the clock: the same half second
[[nodiscard]] bool KeyFrameDue(float keyFrameTime, float clock);
/// The first key frame due at a time on the clock, if any
[[nodiscard]] const dance::DanceKeyFrame* DueKeyFrame(const dance::DanceFile& file, float clock);
/// The clock a turn on: one on, back to 0 once the dance's loop has gone by
[[nodiscard]] float NextClock(float clock, uint32_t loopLength);

/// One game turn of the dance: a stopped dance that starts by itself does once its dancers have come; one that lasts
/// a while stops once that is over; while danced with dancers, the key frame due on its clock is done, each group's
/// move goes on and its clock goes on, starting over at the end of its loop. `startsMove` is told each group a key
/// frame's action starts a move for, as it does (ApplyKeyFrame), before the groups' moves go on. `handDistance`, when
/// given, is the squared distance in metres from the dance to the hand, asked once a turn the dance is danced with
/// dancers (WatchHand), before its key frame.
void ProcessTurn(components::Dance& dance, uint32_t turn, const std::function<void(std::size_t)>& startsMove,
                 const std::function<float(const components::Dance&)>& handDistance = {},
                 const std::array<dance_shapes::Shape, dance_shapes::k_ShapeCount>* shapes = nullptr,
                 const std::function<bool(entt::entity)>& isDancing = {}, const UntitledNumbers& untitled = {});

/// A group's centre after its move of the turn: with a dancer, the place of its first dancer, and once it has had one
/// the turn before, the distance it has gone since as its speed. A first dancer that no longer dances (`isDancing`,
/// when given) leaves the group unmeasured that turn: the game's taking it out of the dance does nothing to a living in
/// no dance
void MeasureGroup(components::Dance& dance, std::size_t group,
                  const std::array<dance_shapes::Shape, dance_shapes::k_ShapeCount>& shapes,
                  const std::function<bool(entt::entity)>& isDancing = {});

/// The squared distance in metres from a dance to the hand: the dance's place with the land's height there plus its
/// own, each part taken to metres before the hand's is taken off
[[nodiscard]] float HandDistanceSquared(const map_coords::MapCoords& place, float landAltitude, glm::vec3 hand);
/// A turn of the hand watching a dance: 10 m or more away starts the count over; nearer counts a turn, and more than
/// ten seconds of turns in a row asks for the dance camera, for a dance not made by a script
void WatchHand(components::Dance& dance, float handDistanceSquared);

} // namespace openblack::ecs::dance_rules
