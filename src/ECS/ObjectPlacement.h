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
#include <utility>

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct Transform;
}

/// An object's own placement (components::ObjectMatrix), made with the game's two constructors at the points where the
/// game makes it (docs/wiki/engine/matrices-and-zoomer.md#the-objects-own-placement)
namespace openblack::ecs::object_placement
{

/// An object that stands upright: affine::PlacementMatrix(point, yAngle, scale), with the angle and the scale kept
void PlaceYaw(Registry& registry, entt::entity entity, const glm::vec3& point, float yAngle, float scale);

/// An object that leans: affine::PlacementYXZ(point, angles.y, angles.x, angles.z, scale), angles in radians about x, y
/// and z, with the y angle and the scale kept
void PlaceYXZ(Registry& registry, entt::entity entity, const glm::vec3& point, const glm::vec3& angles, float scale);

/// The game's own setters (an angle, the scale, a script's placing): the placement made again from the Transform's
/// position and uniform scale and the thing's angles, upright or leaning as placed_turn::CanLean says. The angles are the
/// ones the thing keeps: placed_turn's kept angles while they are its turn's, else the placement's own y angle while
/// the turn is still AngleY of it (a thing that stands upright), else the angles taken off its turn. Only for an object
/// that has a placement: one that has none (a class whose creation makes none yet) is left as it is. A villager or an
/// animal is placed as the original's setters place it (an angle, a scale, a script's placing): upright where it is,
/// at its own y angle (villager_physics::ObjectYAngle) and its scale (PlaceLiving). A creature's body is not placed
void PlaceAgain(Registry& registry, entt::entity entity);

/// A villager's or an animal's 3D object placed by the game itself, as its creation and its own setters place it:
/// upright at `point`, the yaw constructor (affine::PlacementMatrix) at its y angle (no quarter turn on) and its
/// scale, kept as the matrix the game reads (components::LastDrawnMatrix) until its next draw or setter
void PlaceLiving(Registry& registry, entt::entity entity, const glm::vec3& point, float yAngle, float scale);

/// A villager's or an animal's y angle set by its own setter (a game angle, an angle): when it is not the one it had
/// (`before`), its 3D object is placed again where it is now, at the new angle and its scale (PlaceLiving), as the
/// original's angle setter places it. Nothing for any other thing
void PlaceLivingTurned(Registry& registry, entt::entity entity, float before, float yAngle);

/// The same for the thing whose Transform this is, in the entities registry (the setters given only its Transform):
/// nothing for a Transform that registry does not hold
void PlaceLivingTurnedOf(const components::Transform& transform, float before, float yAngle);

/// A draw leaves the matrix a thing is drawn with on it (components::LastDrawnMatrix), and the game reads that one until
/// the next: a villager or an animal (a thing drawn between turns, components::DrawPosition) as its draw in the object
/// draw list draws it, ecs::DrawnModel's pose for the game with the slope shear (between its turn's two places, turned,
/// a bird banked; the hand's or the physics' pose while one draws it; in flight, out of the map cells, the physics'
/// frame pose writes it, DynamicsSystem::UpdateFrame); a creature as its body is made ready to draw,
/// every frame: its Transform's place and turn at the size its body is drawn at (creature_pose::DrawnScale), or the
/// hand's or the physics' pose while one draws it. Nothing for any other thing. Needs a Transform
void KeepDrawn(Registry& registry, entt::entity entity);

/// The hand's draw of a villager or an animal it holds sets its 3D object's matrix whole, as the original's holding
/// state draws it: kept as the matrix the game reads (the hand's pose, ecs::DrawnModel), so that it stays after the
/// hand lets go until its next draw. The angle it was last placed with stays. Nothing for anything else
void KeepHeld(Registry& registry, entt::entity entity);

/// The placement an object's model has now: its matrix (T(p) R S as a glm::mat4, the translation its column 3) and its
/// one scale
struct Placed
{
	glm::mat4 cells;
	float scale;
};

/// The object's placement as the game reads it: while the hand holds it (but a creature), the matrix the hand draws it
/// with (ecs::DrawnModel's hand pose); else the matrix its last draw left (KeepDrawn, KeepHeld), however its Transform
/// has moved since; else its own placement (OwnOf). Needs a Transform
[[nodiscard]] Placed Of(const Registry& registry, entt::entity entity);

/// The object's placement as the game reads it this frame: a thing in the physics' flight that keeps no matrix of its
/// own from that flight (not a villager or an animal, whose draw keeps one, KeepDrawn; not one the hand holds; not a
/// creature, whose body is placed from the same pose) at the physics' pose this frame (ecs::DrawnModel, no slope shear),
/// with its own scale, as the physics' frame pose writes the 3D object's matrix and leaves its scale; anything else as
/// Of. Needs a Transform
[[nodiscard]] Placed DrawnOf(const Registry& registry, entt::entity entity);

/// Of(registry, entity).cells
[[nodiscard]] glm::mat4 Cells(const Registry& registry, entt::entity entity);

/// The object's own placement, never what a draw left: its own while its Transform is the one it was made from; when
/// the Transform has moved since, the placement its class's constructor makes from it now (as PlaceAgain would) while
/// its turn is that constructor's turn of its angles, else the Transform's model (a turn no constructor makes: the
/// physics, the hand). The Transform's model and its uniform scale for an object that has none. For the draw itself
/// (ecs::DrawnModel). Needs a Transform
[[nodiscard]] Placed OwnOf(const Registry& registry, entt::entity entity);

/// The placement an object has before a move the game makes without its model; none for an object without one
[[nodiscard]] std::optional<Placed> KeptBefore(const Registry& registry, entt::entity entity);

/// After such a move: the placement stays as it was before it, kept for the Transform the move left, so the object is
/// drawn, picked and tested on the screen where it was. A later move of the Transform reads it as placed again (Of).
/// Nothing without a kept placement
void LeaveBehind(Registry& registry, entt::entity entity, const std::optional<Placed>& kept);

/// A move the game makes without its model (a script's move of a thing that is not alive): `move` moves the Transform,
/// and the model's placement is left where it was
template <typename Move>
void MoveWithoutModel(Registry& registry, entt::entity entity, Move&& move)
{
	const auto kept = KeptBefore(registry, entity);
	std::forward<Move>(move)();
	LeaveBehind(registry, entity, kept);
}

} // namespace openblack::ecs::object_placement
