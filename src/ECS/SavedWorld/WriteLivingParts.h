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
#include <vector>

#include <SaveParts.h>
#include <VillagerRecords.h>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "ECS/SavedWorld/WorldWriter.h"

namespace openblack
{
struct GLivingInfo;
}

// The parts every moving and living object writes after the object's values, in the original's order: the moving
// thing's word, a moving object's carried object and angles, the walk round obstacles and the living thing's values
// (see docs/wiki/engine/saved-games.md#writing-the-world-file). Each part is made from three sources, in this order:
// what openblack holds (the `Held` values, each absent where openblack holds none), the part the object's load kept,
// and, for an object made in play, the values its constructor leaves. A value none of them gives stops the write with
// a message naming the object.
namespace openblack::ecs::saved_world_write
{
/// The parts a living thing writes between the object's values and its class's own
struct LivingParts
{
	gamesave::MobilePart mobile;
	gamesave::WallHugPart move;
	gamesave::LivingFields living;

	bool operator==(const LivingParts&) const = default;
};

/// What openblack holds of a walk round obstacles; an absent value is not held
struct WallHugHeld
{
	std::optional<int16_t> turnsToStateChange;
	/// In whole map units a turn
	std::optional<uint16_t> speed;
	/// In 2048ths of a turn
	std::optional<uint16_t> angle;
	/// The move state's number
	std::optional<uint8_t> moveState;
	/// In a circling state: the turns to the object it walks round, and that object (none for water or the land's
	/// edge)
	std::optional<uint8_t> turnsToObject;
	std::optional<entt::entity> circleObject;
	std::optional<int8_t> turnsToStepRebuild;
	/// The footpath to take after the one it is on
	std::optional<entt::entity> nextFootpath;
	/// The goal, in metres, and its altitude
	std::optional<glm::vec2> goal;
	std::optional<float> goalAltitude;
	/// The object it walks to
	std::optional<entt::entity> target;
	/// The step of a turn, in whole map units
	std::optional<glm::ivec2> step;
};

/// The footpath a living thing is on, its node there (-1 for none) and its way along it
struct FootpathHeld
{
	entt::entity footpath {entt::null};
	int32_t node {-1};
	bool reverse {false};
};

/// The dance a living thing dances in, its group there and its place in the group; no dance when it dances none
struct DanceHeld
{
	entt::entity dance {entt::null};
	std::size_t group {0};
	uint8_t place {0};
};

/// What openblack holds of a living thing's values; an absent value is not held
struct LivingHeld
{
	std::optional<FootpathHeld> footpath;
	/// The top, final and stored states
	std::optional<std::array<uint8_t, 3>> states;
	std::optional<uint16_t> turnsSinceStateChange;
	/// The reaction it follows (its id in the reactions' list, 0 for none) and the object it reacts to
	std::optional<uint32_t> reaction;
	std::optional<entt::entity> reactionObject;
	std::optional<uint32_t> birthTurn;
	/// The status bits openblack holds, and which bits those are: the others are written as kept
	uint16_t status {0};
	uint16_t statusMask {0};
	std::optional<entt::entity> flock;
	/// Its place in its flock's member list
	std::optional<uint8_t> flockPlace;
	/// A child's walk about its creche: the path in the low word and the step in the high one
	std::optional<uint32_t> crecheWalk;
	/// Blue, green, red and alpha
	std::optional<std::array<uint8_t, 4>> specular;
	std::optional<DanceHeld> dance;
	/// The reactions it did lately, the oldest first
	std::optional<std::vector<gamesave::ReactionDone>> reactionsDone;
	/// It walks one of the camera editor's tracks
	bool walksATrack {false};
};

/// What openblack holds of an entity's walk: a villager's walk or an animal's, nothing for any other
[[nodiscard]] WallHugHeld WallHugHeldOf(const Registry& registry, entt::entity entity);
/// What openblack holds of an entity's living values: a villager's or an animal's, nothing for any other
[[nodiscard]] LivingHeld LivingHeldOf(const Registry& registry, entt::entity entity);

/// The parts as the living thing's constructor leaves them, for its class's row: its speed the row's default speed
/// and its strength the row's, every other value zero. Its birth turn is drawn at random when it is made, so it is
/// never taken from here. A class whose own constructor sets more puts its values in over these
[[nodiscard]] LivingParts LivingConstructorParts(const GLivingInfo& info);

/// The moving thing's word: as kept, zero for one made in play (nothing in the game sets it)
[[nodiscard]] gamesave::MobilePart MobilePartOf(const gamesave::MobilePart* kept);

/// A moving object's carried object, its x and z angles and its path, which openblack does not hold: as kept; for one
/// made in play the object it was made with (`madeWith`) and zeros. Nothing, and the write fails, for one made in play
/// whose object is not given
[[nodiscard]] std::optional<gamesave::MobileObjectPart> MobileObjectPartOf(WriteContext& context, entt::entity entity,
                                                                           const gamesave::MobileObjectPart* kept,
                                                                           std::optional<entt::entity> madeWith);

/// The walk round obstacles, from what openblack holds, else as kept, else as made. Nothing, and the write fails, for
/// a circling walk whose circle's entry distance, index and place no load kept, or a walk that steps through with no
/// turns to its next rebuild
[[nodiscard]] std::optional<gamesave::WallHugPart> WallHugPartOf(WriteContext& context, entt::entity entity,
                                                                 const gamesave::WallHugPart* kept,
                                                                 const gamesave::WallHugPart& made, const WallHugHeld& held);

/// The living thing's values, from what openblack holds, else as kept, else as made. Nothing, and the write fails, for
/// one made in play whose birth turn openblack does not hold, or one that walks a track no load kept the data of
[[nodiscard]] std::optional<gamesave::LivingFields> LivingFieldsOf(WriteContext& context, entt::entity entity,
                                                                   const gamesave::LivingFields* kept,
                                                                   const gamesave::LivingFields& made, const LivingHeld& held);

/// The three parts together, with what the entity's components hold
[[nodiscard]] std::optional<LivingParts> LivingPartsOf(WriteContext& context, entt::entity entity, const LivingParts* kept,
                                                       const LivingParts& made);
} // namespace openblack::ecs::saved_world_write
