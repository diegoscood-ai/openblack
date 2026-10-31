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

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/AxisAlignedBoundingBox.h"
#include "Graphics/RegionOnScreen.h"

/// The scripts' "is it on the screen" tests: GAME_THING_FIELD_OF_VIEW 011 and POS_FIELD_OF_VIEW 012. Both go through
/// the drawn camera's world-to-clipping matrix (billboard::CameraFrame::clipMatrices.worldToClipping, as
/// ShadowMath::BlockVisible), the near plane (billboard::CameraFrame::nearZ) and the screen size.
namespace openblack::field_of_view
{

/// The drawn camera's screen tests are graphics::region_on_screen's (Graphics/RegionOnScreen.h: ScreenView,
/// PointOnScreen, SphereOnScreen)
using View = graphics::region_on_screen::ScreenView;

/// The main camera of openblack as the view (Locator::camera, Locator::windowing); false when there is none
[[nodiscard]] bool CurrentView(View& out);
/// POS_FIELD_OF_VIEW 012 on a view: false inside the citadel (the game's citadel sequence, as the scripts'
/// INSIDE_TEMPLE reads it), else PointOnScreen
[[nodiscard]] bool PosInView(const View& view, bool insideCitadel, const glm::vec3& point);
/// The same on the main camera (CurrentView) and the game's sequence (game_clock::IsInsideCitadel); false without a view
[[nodiscard]] bool PosInView(const glm::vec3& point);
/// Whether GAME_THING_FIELD_OF_VIEW takes a thing as an object (the original's cast to its object class): what the
/// scripts call an object (script_type::IsObject), less a script's marker and a dance (neither is an object in the original).
/// Whether it has a Mesh does not decide it
[[nodiscard]] bool IsObjectForTheScreenTest(entt::entity thing);
/// The point the game tests for a thing that is not an object: its map coordinates' (x and z on the map's fixed point,
/// truncated, and back; the land under that point plus its height above the land), from openblack's position
[[nodiscard]] glm::vec3 MapPointOf(const glm::vec3& position);
/// The point of a thing that is not an object: MapPointOf its Transform position; a script's timer, which has none, at
/// the map's corner (0, the land there, 0); a dance at its place; nothing for anything else openblack keeps no place for
[[nodiscard]] std::optional<glm::vec3> PointOfThing(entt::entity thing);
/// GAME_THING_FIELD_OF_VIEW 011 on a view: nothing, or inside the citadel: false. An object: its 3D object's mesh (none
/// -> false, as an object openblack keeps no Mesh for, a fish farm) bounding sphere through SphereOnScreen. Any other thing
/// with a position: the point (x, altitude + its height offset, z) through PointOnScreen, PointOfThing
[[nodiscard]] bool ThingInView(const View& view, bool insideCitadel, entt::entity thing);
/// The same on the main camera (CurrentView) and the game's sequence; false without a view
[[nodiscard]] bool ThingInView(entt::entity thing);
/// An object's bounding radius for the screen test: its 3D object's scale times its mesh's half diagonal (the half
/// extents (max - min) x 0.5, then sqrt((hz hz + hy hy) + hx hx)), one product
[[nodiscard]] float ObjectSphereRadius(const AxisAlignedBoundingBox& meshBox, float scale);
/// The object's mesh (none -> false), then its bounding sphere through SphereOnScreen, on the main camera. No citadel
/// test (ThingInView's caller has it). Also false without a Mesh or without a view (openblack: no camera / window, the
/// unit tests)
[[nodiscard]] bool ObjectOnScreen(entt::entity thing);
/// The same on a view
[[nodiscard]] bool ObjectOnScreen(const View& view, entt::entity thing);

/// GAME_THING_CAN_VIEW_CAMERA 339: whether a thing sees the camera within an angle of `degrees` in front of it.
/// The degrees as half the angle in radians: (degrees x 0.5) x the float of pi / 180, each step a float
[[nodiscard]] float HalfAngleOfDegrees(float degrees);
/// The test for an object that is not a creature, on the ground: d = camera - thing (x and z, float differences). For
/// the edge e = yAngle - halfAngle, dz cos e - dx sin e must not be below 0 (not a number: false); for e = yAngle +
/// halfAngle it must not be above 0 (not a number passes). The cosine is kept wide, the sine rounded to a float, each
/// product and difference a float. Both edges are lines through the thing, so this is the angle in front of it only
/// below a half turn: at 180 degrees it is the half of the ground in front, at a full turn only a camera on the line
[[nodiscard]] bool CameraWithinAngle(glm::vec2 thing, float yAngle, float halfAngle, glm::vec2 camera);
/// For a creature: the turn cut into 360 / degrees parts, truncated towards 0 and taken unsigned. 0 (wider than a full
/// turn) has no answer: the game would divide by it
[[nodiscard]] std::optional<uint32_t> CreatureViewParts(float degrees);
/// The whole test on a thing a script holds, an object, against the drawn camera. A creature: the direction from its map
/// position to the camera's is close to the way it looks (creature_player_attention::IsDirectionClose with
/// CreatureViewParts; its look as creature_perceived_desires::LookYaw), or the camera is over its own map cell. Any
/// other object: CameraWithinAngle from its map position, with its Y angle (a villager's or an animal's own, else the
/// angle the scripts read). False without a place, without a camera, and for a creature with no answer
[[nodiscard]] bool ThingCanViewCamera(entt::entity thing, float degrees);

} // namespace openblack::field_of_view
