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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureLayers.h"

namespace openblack::creature_fight
{
enum class State : uint8_t;
}

/// The creature's knock: while its body falls into a fight, reels from a blow or a slap, lies down to sleep or gets up
/// from it, or faints, eight of its bones knock over what they move into. Each turn, a bone that has moved more than a
/// share of the creature's size since its last place knocks what it may in its map cell: a villager or an animal is
/// pushed off at seven times the bone's move, a home has pieces broken off it, anything else that can fly is pushed
/// off like a villager. Every sum and product is rounded to a float after each operation, in the order written.
/// Docs: docs/wiki/creature/pose-and-animation.md#the-knock.
namespace openblack::creature_knock
{
/// The knock bones: the head, the rump, the right and left hands, the right and left feet, and the right and left
/// armpits, in that order
inline constexpr size_t k_BoneCount = 8;

/// One of the knock bones: which bone of the body, how wide it knocks, and where it last was in the world
struct Bone
{
	uint32_t bone {0};
	float radius {0.0f};
	glm::vec3 place {0.0f};
};
using Bones = std::array<Bone, k_BoneCount>;

/// The bones of the body the knock bones are, the left ones the mirrors of the right
struct BoneIndices
{
	uint32_t head;
	uint32_t rump;
	uint32_t rightHand;
	uint32_t leftHand;
	uint32_t rightFoot;
	uint32_t leftFoot;
	uint32_t rightArmpit;
	uint32_t leftArmpit;
};

/// The creature stands fifteen times its drawn size high
inline constexpr float k_StandingHeightPerSize = 15.0f;
/// The head and the feet are drawn bigger on a smaller creature: 1.8 less half the size, and 1.5 less a quarter of it,
/// the size kept to 0..2
inline constexpr float k_HeadScaleAtSizeZero = 1.8f;
inline constexpr float k_HeadScalePerSize = 0.5f;
inline constexpr float k_FeetScaleAtSizeZero = 1.5f;
inline constexpr float k_FeetScalePerSize = 0.25f;
inline constexpr float k_MaxProportionSize = 2.0f;
/// The share of the standing height each bone knocks within: an eighth, the rump a sixth
inline constexpr float k_BoneShare = 0.125f;
inline constexpr float k_RumpShare = 1.0f / 6.0f;
/// A bone knocks only once it moved more than three quarters of the drawn size since its last place
inline constexpr float k_MovePerSize = 0.75f;
/// What a bone pushes off flies at seven times the bone's move
inline constexpr float k_PushPerMove = 7.0f;
/// A villager, an animal or a thing that can fly is reached within its own radius and three tenths of the bone's
inline constexpr float k_ReachShare = 0.3f;
/// A home is struck at four tenths of ten times the bone's move, along the move
inline constexpr float k_HomeBlowPerMove = 10.0f;
inline constexpr float k_HomeBlowShare = 0.4f;
/// A map cell is ten metres wide
inline constexpr float k_CellsPerMetre = 0.1f;

/// The knock bones as the knock is set up, for a creature drawn at `drawnSize`: each bone's radius from the standing
/// height; their places stay as they were
[[nodiscard]] Bones Initialise(const Bones& before, const BoneIndices& indices, float drawnSize);

/// The squared move a bone must pass before it knocks, for a creature drawn at `drawnSize`
[[nodiscard]] float MoveThresholdSq(float drawnSize);

/// The bone's move from its last place: its new place less the last, and whether the squared length (x, then z, then y
/// added) passes the threshold
struct Move
{
	glm::vec3 by;
	bool knocks;
};
[[nodiscard]] Move MoveOf(glm::vec3 last, glm::vec3 now, float thresholdSq);

/// The map cell a bone at `place` knocks in: its x and z tenths, cut towards zero
[[nodiscard]] glm::ivec2 CellOf(glm::vec3 place);

/// What a thing is to the knock
struct Facts
{
	/// It has a model
	bool hasModel {false};
	bool indestructible {false};
	bool creature {false};
	bool templeHeart {false};
	bool tree {false};
	bool storagePit {false};
	bool townCentre {false};
	bool fragment {false};
	bool villager {false};
	bool animal {false};
	/// A bird of the dove class
	bool dove {false};
	bool immovable {false};
	/// Any living thing, and a model that plays animations of its own
	bool living {false};
	bool animatedModel {false};
	bool home {false};
	bool field {false};
	/// It can be thrown or knocked into the physics
	bool canBecomePhysicsObject {false};
};
/// Whether the knock may take a thing: never one with no model, marked indestructible, a creature, the temple's heart,
/// a tree, a storage pit, a town centre or a fragment; a villager or an animal other than a bird unless it is
/// immovable; never any other living thing nor an animated model; a home unless it is a field; anything else that can
/// fly and is not immovable
[[nodiscard]] bool CanKnock(const Facts& facts);

/// What the knock does to a thing it may take
enum class Kind : uint8_t
{
	/// A villager or an animal: pushed off when it stands within reach
	Living,
	/// A home: struck when within reach and the bone meets one of its model's points
	Home,
	/// Anything else that can fly and is not immovable: pushed off when within reach and the bone meets one of its
	/// model's points
	Thing,
	/// Nothing
	None,
};
[[nodiscard]] Kind KindOf(const Facts& facts);

/// Whether a thing standing at `at` (x and z) with `radius` is within the reach of a bone at `bone`: a living thing or
/// a thing within its radius and three tenths of the bone's, a home within its radius and the bone's whole. The
/// squared distance is dz x dz + dx x dx
[[nodiscard]] bool WithinReach(Kind kind, glm::vec2 at, float radius, glm::vec3 bone, float boneRadius);

/// The model's point the bone may meet, in its world: the matrix's rows and place, and one of the model's points
struct Rows
{
	glm::vec3 x;
	glm::vec3 y;
	glm::vec3 z;
	glm::vec3 place;
};
/// A point of a model through its rows: for each axis, the y row's share, then the x row's, then the z row's, then the
/// place, each sum a float
[[nodiscard]] glm::vec3 ToWorld(const Rows& rows, glm::vec3 point);
/// Whether any of the points, through the rows, lies closer to `at` than `radius`: the squared distance dz, then dy,
/// then dx squared and added, below the radius squared
[[nodiscard]] bool MeetsAnyPoint(const Rows& rows, std::span<const glm::vec3> points, glm::vec3 at, float radius);

/// The velocity a pushed thing flies off at: seven times the bone's move. The move is left that much bigger for the
/// next thing the bone knocks in the cell
[[nodiscard]] glm::vec3 Push(glm::vec3& move);

/// The blow a home takes: along the move, at its length times ten times four tenths. The move is left of length one (or
/// left as it is when it is no move at all) for the next thing the bone knocks in the cell
[[nodiscard]] glm::vec3 HomeBlow(glm::vec3& move);

/// What a change of the body's state does to the knock
enum class Mark : uint8_t
{
	/// The knock starts from where the bones are now
	Start,
	/// The bones' radii are set from the creature's size, then the knock starts from where they are now
	SetUpAndStart,
	/// The knock stops
	Stop,
};
/// A fighter entering a state: its fight's start, a blow, a step or a reel, and the start of a cast start the knock;
/// fainting sets it up and starts it; its stance, lying fainted and the fight's end stop it; the rest leave it
[[nodiscard]] std::optional<Mark> FightMark(creature_fight::State state);

/// What the body plays, as the knock sees it
enum class BodyState : uint8_t
{
	Standing,
	SleepStart,
	Sleeping,
	SleepEnd,
	Fainting,
	LyingFainted,
	GettingUp,
	/// Played by the fight, which marks the knock itself
	Fighting,
	Other,
};
/// The body's state from what it plays: standing when it plays no action; a sequence whose loop is sleeping, by its
/// part; the faint's sequence (the faint, the faint held and the getting up), by its part; a body the fight plays its
/// moves on; anything else
[[nodiscard]] BodyState BodyStateOf(const creature_layers::BodyAction& body);
/// The body taking a new state: standing, sleeping on and lying fainted stop the knock; lying down to sleep, getting up
/// from it and fainting set it up and start it; the rest leave it
[[nodiscard]] std::optional<Mark> BodyMark(BodyState state);
} // namespace openblack::creature_knock
