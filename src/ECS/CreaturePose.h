/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs
{
class Registry;
}

namespace openblack::creature_morph
{
struct Morph;
}

namespace openblack::creature
{
struct CreatureRig;
}

/// Where a creature stands at the end of its turn. A creature's Transform moves once a turn, through here; between turns
/// only its drawn pose moves (components::CreatureDrawPose).
namespace openblack::ecs::creature_pose
{

/// Where a creature's body is drawn this frame
struct DrawnPlacement
{
	glm::vec3 position;
	glm::mat3 rotation;
};

/// The creature's Transform takes the turn's position, through the map cells (map_cells::MoveMapObject, which links it
/// to its new cell when it changes), and is turned to face along the heading (creature_locomotion::HeadingOf)
void CommitTurnPose(entt::entity creature, glm::vec3 position, float heading);

/// The heading a body's rotation faces: it is turned about y, and its mesh looks back along +z
[[nodiscard]] float ReadHeading(const glm::mat3& rotation);

/// The way a creature's body faces, as a body's yaw (0 along -z, a quarter turn along +x): its locomotion's heading
/// turned the other way round once it has first moved; before, the heading its body's `rotation` faces
[[nodiscard]] float BodyYaw(const Registry& registry, entt::entity creature, const glm::mat3& rotation);


/// Between turns: its drawn pose (components::CreatureDrawPose), while that pose belongs to the turn that put its
/// Transform where it is. Nothing otherwise: not moved by its locomotion yet, put somewhere else by something other than
/// its turn, or drawn by the hand or the physics. Only reads the registry, so it makes no storage. ecs::DrawnModel draws
/// the body there
[[nodiscard]] std::optional<DrawnPlacement> BetweenTurns(const Registry& registry, entt::entity creature);

/// The position and rotation ecs::DrawnModel draws the body at, from the same sources in the same order: the hand's
/// pose, the physics' pose, BetweenTurns, else its Transform. For the parts that need the body's rotation on its own
/// (where the eyes look ahead, the turn of what it holds); the matrix is ecs::DrawnBodyModel's. Needs a Transform
[[nodiscard]] DrawnPlacement DrawnPlacementOf(const Registry& registry, entt::entity creature);

/// The scale the body is drawn at, as ecs::DrawnModel draws it: the one its body was last given
/// (components::CreatureDrawPose::scale; smaller in its temple's pen), else its Transform's. Needs a Transform
[[nodiscard]] glm::vec3 DrawnScale(const Registry& registry, entt::entity creature);

/// The size the body is drawn at: the one it was last given (components::CreatureDrawPose::size), else its own
[[nodiscard]] float DrawnSize(float ownSize, std::optional<float> givenSize);

/// The size the body is drawn at, from its Creature and its components::CreatureDrawPose. Everything the game sizes by
/// the drawn body reads it here: its speeds, its animations' rate, its breath, its sounds, its leash, its height and
/// scale, the hand's touch, the follow camera, its hair and footprints. Needs a Creature
[[nodiscard]] float DrawnSize(const Registry& registry, entt::entity creature);

/// The size the body was last given, as given (components::CreatureDrawPose::given), else its own: its walking speeds
/// go by it. Needs a Creature
[[nodiscard]] float GivenSize(const Registry& registry, entt::entity creature);

/// How far along each axis the body is to be drawn: what it has become, from its alignment, the fatness it shows and
/// its strength, which the shape drawn takes in steps once a turn. Without a shown fatness yet, its own fatness.
/// Needs a Creature
[[nodiscard]] creature_morph::Morph TargetMorph(const Registry& registry, entt::entity creature);

/// How much of its own size the body is drawn at (DrawnScale over its Transform's scale, along x): smaller in its pen.
/// For what is sized apart from the body's matrix (the leash's collar without a bone), so that it shrinks with it
[[nodiscard]] float DrawnSizeShare(const Registry& registry, entt::entity creature);

/// The body's placement at a position, leaned onto the land there (creature_lean) by its radius on the ground at the
/// size it is drawn at. A creature another one carries takes no lean from the land. Without the land or its body's
/// measures, it is placed upright. Needs a Creature
[[nodiscard]] glm::mat4 LeanedModel(const Registry& registry, entt::entity creature, glm::vec3 position,
                                    const glm::mat3& rotation, glm::vec3 scale);

/// Where the creature's groin bone is in the pose the game reads (creature_turn_pose::Safe): the body as posed for the
/// turn, leaned onto the land at the size it is drawn at, or the pose kept while the hand holds it. None for a thing
/// that is not posed yet, or whose species has no action points
[[nodiscard]] std::optional<glm::vec3> GroinOf(entt::entity creature);
/// Where the creature's belly bone is, in the same pose as GroinOf
[[nodiscard]] std::optional<glm::vec3> BellyOf(entt::entity creature);
/// GroinOf and BellyOf with the creature's rig given
[[nodiscard]] std::optional<glm::vec3> GroinOf(const Registry& registry, entt::entity creature,
                                               const creature::CreatureRig& rig);
[[nodiscard]] std::optional<glm::vec3> BellyOf(const Registry& registry, entt::entity creature,
                                               const creature::CreatureRig& rig);

/// The angle the body model is placed at: a quarter turn past the body's yaw, one float sum
[[nodiscard]] float BodyModelAngle(float bodyYaw);

/// The body model's placement at a position: affine::PlacementMatrix(position, BodyModelAngle(bodyYaw), scale), with its
/// one scale
struct BodyModelPlaced
{
	glm::mat4 cells;
	float scale;
};
[[nodiscard]] BodyModelPlaced BodyModelPlacement(glm::vec3 position, float bodyYaw, float scale);

/// The creature's body model placed (components::CreatureBodyPlacement) where it stands now: its Transform's position,
/// its body's yaw (BodyYaw) and the scale it is drawn at (DrawnScale). Nothing for a thing without a Transform
void PlaceBody(Registry& registry, entt::entity creature);

/// PlaceBody for every creature, as every frame of the world's view places the bodies (the caller leaves out the frames
/// inside the citadel and those of the falling spell's film)
void PlaceBodies(Registry& registry);

} // namespace openblack::ecs::creature_pose
