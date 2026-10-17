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

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

/// The pure rules of a villager's or an animal's landing: the original's rows of its body, its landing pose and
/// heading, and its drawn rotation (ecs::villager_physics applies them); how hard a knock must be to hurt it; how a
/// villager's life moves its town's count of injured people (ecs::life applies it); and whether a villager runs from or
/// points at a thing thrown from the hand, and when that thing is no longer in the air (ecs::villager_reactions)
namespace openblack::physics::living
{

/// The landType without a physics body
inline constexpr uint16_t k_LandTypeWithoutBody = 3;

/// The rows right / up / fwd (as glm columns 0 / 1 / 2) of the ORIGINAL's object matrix for openblack's DRAWN rotation
/// (the Transform's, with its pi / 2). The bodies are built with the original's rows, so this is only for a drawn
/// matrix: ObjectYAngle's fallback (YawFromRotation of the Transform).
/// The original builds the body from the world matrix = affine::AngleY(y angle) (no pi / 2 in it), but draws a
/// villager / animal at the y angle + pi / 2; openblack's Transform is the DRAWN rotation
/// (animal_ai::detail::FaceAngle: affine::AngleY(theta + pi / 2), as PathfindingSystem's InitializeStep) and the
/// body is built from it (DynamicsSystem BuildShape(transform.rotation)).
/// So body = M x K, with K the turn by pi / 2 about the object's own up axis: body column 0 = M's fwd row, column 1 =
/// M's up row, column 2 = -(M's right row). It follows any rigid motion of the body (the rows turn together), so for
/// the turn-start and the current matrix alike: right = -body[2], up = body[1], fwd = body[0]. (inferred from the two
/// readings above; not verified in game: the landing trace prints both candidates, `landing:` lines)
[[nodiscard]] glm::mat3 OriginalRows(const glm::mat3& body);
/// The original's rows of a physics body: from the hand (FromHand) the original builds the body from the held
/// object's DRAWN matrix, as openblack does (HandHolding: the hand's Transform); the world path from the world matrix,
/// which openblack builds as the original (WorldMatrixRows(ObjectYAngle, S)). Both are the rows as they are:
/// `fromHand` is kept for the callers' reading
[[nodiscard]] glm::mat3 BodyRows(const glm::mat3& body, bool fromHand);

/// The creature desires EndPhysics's empathy asks for
inline constexpr uint8_t k_DesireCompassion = 1;
inline constexpr uint8_t k_DesireAnger = 2;

/// The villager's landing pose, from the original's turn-start rows: a = right.y. a < -0.5 or NaN -> 1, yaw =
/// GetYAngle(up) stored as it is (no pi, no wrap), ANGER 0.5; a > 0.5 -> 2, yaw = WrapAngle(float(GetYAngle(up) + pi)),
/// ANGER 0.5; else 0, yaw = WrapAngle(float(GetYAngle(fwd) + pi)), COMPASSION 0.1
struct VillagerLandingPose
{
	uint16_t landType;
	float yaw; ///< the 3D angle EndPhysics gives SetYAngle (the game angle keeps it)
	uint8_t desire;
	float amount;
};
[[nodiscard]] VillagerLandingPose VillagerLandingPoseOf(const glm::mat3& turnStartRows);

/// The animal's landType from the original's turn-start right.y: a > 0.5 -> 1; a < -0.5 or NaN -> 2; else 0. The
/// villager's mapping reversed (not shared code in the original)
[[nodiscard]] uint16_t AnimalLandType(float rightY);
/// The animal's landing yaw: WrapAngle(float(GetYAngle(the CURRENT fwd row) + pi))
[[nodiscard]] float AnimalLandingYaw(const glm::vec3& currentFwdRow);
/// The yaw of the original's rows: float(ArcTanOctant(fwd.z, -fwd.x)) (as map_cells' YAngleOf). The physics takes it
/// before the class's EndPhysics and sets the angles again from it after: the y angle the landed object is drawn at
/// TODO(affine::DecomposeYXZ): one shared copy once affine has DecomposeYXZ (also map_cells' YAngleOf)
[[nodiscard]] float YawFromRotation(const glm::mat3& rows);
/// The drawn rotation of a y angle: affine::AngleY(yAngle + pi / 2) (FaceAngle's)
[[nodiscard]] glm::mat3 DrawnRotation(float yAngle);


/// A villager is counted among its town's injured while its life is below this
inline constexpr float k_InjuredLife = 0.7f;
/// How a villager's life changing moves its town's count of injured people: one more when it falls from above the mark
/// to below it, one fewer when it rises from below to above, and no change when it is at the mark either side
[[nodiscard]] int InjuredChange(float before, float after);

/// A knock harder than this many times a living thing's own weight hurts it
inline constexpr float k_HurtingKnock = 2.0f;
/// The share of the physics impact effect each multiple of its weight past that does
inline constexpr float k_CrushPerKnock = 0.03f;
/// The share of the physics impact effect a knock of so many times its weight does to a villager or an animal: (knock -
/// 2) x 0.03 when the knock is strictly above 2, none for a softer one
[[nodiscard]] std::optional<float> LivingCrush(float knock);

/// What a villager does about a thing thrown from the hand
enum class FlyingObjectResponse : uint8_t
{
	/// It runs: the thing is nearer than it flies in two seconds
	Run,
	/// It points at it
	Point,
};
/// The seconds of a thrown thing's flight a villager keeps away from
inline constexpr float k_RunSeconds = 2.0f;
/// Run when the distance across the map is strictly below two seconds of the thing's speed; at it, beyond it, or for a
/// value that compares with nothing, it points
[[nodiscard]] FlyingObjectResponse RespondToFlyingObject(float distance, float speed);
/// A physics body's speed from its velocity, its squares summed z, y, then x
[[nodiscard]] float FlyingSpeed(glm::vec3 velocity);
/// A villager amazed by a magic shield notices a thrown thing only while the shield's radius is strictly greater than the
/// distance across the map from the shield to the thing
[[nodiscard]] bool ShelterSeesFlyer(float distance, float radius);
/// A thing in the physics is still really in the air while it moves faster than this, in metres a second
inline constexpr float k_AirborneSpeed = 3.0f;
/// or while its centre less the land under it, plus its radius, is more than this
inline constexpr float k_AirborneHeight = 0.2f;
/// Whether a thing in the physics is still really in the air: faster than 3 m/s, or more than 0.2 m up (its centre's
/// height less the land's, plus its radius); both strict
[[nodiscard]] bool IsActuallyInTheAir(float speed, float centreHeight, float landHeight, float radius);

/// A creature's mass from its size and how fat and how strong its body is to become (each -1 to 1)
[[nodiscard]] float CreatureMass(float scale, float thinFat, float weakStrong);
/// The most a creature counts a knock as, in multiples of its weight
inline constexpr float k_CreatureKnockCap = 100.0f;
/// The share of the crush effect each multiple does to a creature, and the least that counts
inline constexpr float k_CreatureCrushPerKnock = 0.01f;
inline constexpr float k_CreatureLeastCrush = 0.005f;
/// How much of the crush effect a turn's mean force does to a creature of a mass, none for less than counts: the knock
/// in multiples of its weight, at most 100 (and 100 when it compares with nothing), times 0.01
[[nodiscard]] std::optional<float> CreatureCrush(float impact, float creatureMass);
/// How heavy what a creature carries is for it, 0 to 1: the weight carried over the creature's own. Nothing when the
/// creature weighs nothing or its weight compares with nothing, nothing for a share below nothing or that compares with
/// nothing, and 1 for a share above it
[[nodiscard]] float CarriedShare(float carried, float creatureWeight);

} // namespace openblack::physics::living
