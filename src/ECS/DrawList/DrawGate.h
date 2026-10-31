/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>
#include <span>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs
{
class Registry;
}

namespace openblack::graphics
{
struct ObjectListFrame;
}

// Which objects the object draw list leaves out of a frame's draw: only those the map files (held and flying objects,
// the hand and the effects are drawn outside the list), and not one marked DontDraw, whose look something else draws.
// Besides them, a Norse Gate's lamps, which only the gate's own Draw draws, right after it finds the gate on the screen,
// a wood or food pile sunk wholly into the land, which its own Draw does not draw, and the object a villager carries,
// which the villager's own Draw draws, left out with its villager.
// docs/wiki/rendering/object-draw-list.md#the-object-draw-list-stage-4m
namespace openblack::ecs::draw_gate
{

/// Whether an object is left out of this frame's draw: the list ran this frame, the object is in the map, it is not
/// marked DontDraw, and the list's pass did not call its Draw
[[nodiscard]] bool Hidden(bool listRan, bool inMap, bool dontDraw, bool drawn);

/// Whether a wood or food pile is sunk wholly into the land, so that its Draw draws nothing: its sink offset (its
/// altitude above the land, 0 when fully raised) is at or below minus its height. While the offset is above that, the
/// pile is drawn
[[nodiscard]] bool PileBuried(float offset, float height);

/// Whether a Norse Gate's lamp is left out of this frame's draw: the list ran this frame and its gate's Draw did not
/// find the gate on the screen (or did not run)
[[nodiscard]] bool GateLampHidden(bool listRan, bool gateOnScreen);

/// Where a Norse Gate's lamp takes its light from: the gate's draw lights and hazes the gate at the gate's own place
/// and hands the same colour and specular to both its lamps before it draws them, so a lamp is lit at its gate's
/// position, not its own. Nothing for anything else, or for a lamp whose gate is gone
[[nodiscard]] std::optional<glm::vec3> LampLightOrigin(const Registry& registry, entt::entity entity);

/// The frame's part for the draw: whether the list ran, and every object with a Mesh and a Transform that Hidden (a gate's
/// lamp: GateLampHidden) leaves out, and every other wood or food pile that PileBuried leaves out, sorted, each with why
/// (listed by the last rebuild or not, its gate not on the screen, or buried), and every carried object whose villager
/// is left out, for its villager's reason. `drawn` is the list's DrawnThisFrame and `onScreen` its OnScreenThisFrame, in
/// any order; `inMap` tells whether the map files an object, and is called only when the list ran
void FillFrame(bool listRan, std::span<const entt::entity> drawn, std::span<const entt::entity> onScreen,
               const Registry& registry, const std::function<bool(entt::entity)>& inMap, graphics::ObjectListFrame& out);

} // namespace openblack::ecs::draw_gate
