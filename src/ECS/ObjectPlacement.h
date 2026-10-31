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
/// that has a placement: one that has none (a class whose creation makes none yet, a living thing) is left as it is
void PlaceAgain(Registry& registry, entt::entity entity);

/// The placement an object's model has now: its matrix (T(p) R S as a glm::mat4, the translation its column 3) and its
/// one scale
struct Placed
{
	glm::mat4 cells;
	float scale;
};

/// The object's placement: its own while its Transform is the one it was made from; when the Transform has moved
/// since, the placement its class's constructor makes from it now (as PlaceAgain would) while its turn is that
/// constructor's turn of its angles, else the Transform's model (a turn no constructor makes: the physics, the hand).
/// The Transform's model and its uniform scale for an object that has none. Needs a Transform
[[nodiscard]] Placed Of(const Registry& registry, entt::entity entity);

/// Of(registry, entity).cells
[[nodiscard]] glm::mat4 Cells(const Registry& registry, entt::entity entity);

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
