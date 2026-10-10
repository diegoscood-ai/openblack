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
#include <span>

#include <entt/entity/fwd.hpp>

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
// docs/bw1-notes/original-frame.md §6
namespace openblack::ecs::draw_gate
{

/// Whether an object is left out of this frame's draw: the list ran this frame, the object is in the map, it is not
/// marked DontDraw, and the list's pass did not call its Draw
[[nodiscard]] bool Hidden(bool listRan, bool inMap, bool dontDraw, bool drawn);

/// The frame's part for the draw: whether the list ran, and every object with a Mesh and a Transform that Hidden leaves
/// out, sorted, each with why (listed by the last rebuild or not). `drawn` is the list's DrawnThisFrame, in any order;
/// `inMap` tells whether the map files an object, and is called only when the list ran
void FillFrame(bool listRan, std::span<const entt::entity> drawn, const Registry& registry,
               const std::function<bool(entt::entity)>& inMap, graphics::ObjectListFrame& out);

} // namespace openblack::ecs::draw_gate
