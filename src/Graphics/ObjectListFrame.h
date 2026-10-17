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

#include <algorithm>
#include <vector>

#include <entt/entity/entity.hpp>

// The object draw list's part of the frame, decided before the draw as Graphics/SuperVillagerFrame.h is: Game.cpp fills
// it once a frame (ecs::draw_gate::FillFrame, next to super_villager::FillFrame) and the Renderer reads it through
// DrawSceneDesc::objectList; nothing in the draw asks the list or the registry for it.
namespace openblack::graphics
{

/// Why the list leaves an object out of this frame
enum class ObjectListHidden : uint8_t
{
	/// The last rebuild did not take it into the list
	NotListed,
	/// It is in the list, and this frame's pass did not call its Draw
	NotDrawn,
};

/// What the draw needs of this frame's object draw list
struct ObjectListFrame
{
	/// The list ran this frame. When it did not, nothing is hidden
	bool ran {false};
	/// The objects with a mesh the list leaves out this frame, sorted
	std::vector<entt::entity> hidden;
	/// Beside each of `hidden`, why
	std::vector<ObjectListHidden> reasons;
	/// The draw leaves `hidden` out (EngineConfig::drawFollowsList); when not, they are drawn and only read out
	bool applied {false};
	/// The objects the list drew, in the order it called their draws (graphics::object_order)
	std::vector<entt::entity> visited;
	/// The objects the list's tail draws after its passes, in its order (object_order::TailOrder)
	std::vector<entt::entity> tail;
	/// The physics objects, drawn after the leashes, in their order
	std::vector<entt::entity> physics;
	/// The flying physics objects wholly under the water, which nothing draws
	std::vector<entt::entity> sunk;

	/// Whether the draw leaves the object out of this frame
	[[nodiscard]] bool LeftOut(entt::entity object) const
	{
		return ran && applied && std::ranges::binary_search(hidden, object);
	}
};

} // namespace openblack::graphics
