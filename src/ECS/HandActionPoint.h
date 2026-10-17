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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Registry.h"

/// The interface's action point: where the hand's actions (presses, taps, applies, the influence test) take place.
/// The game clears it every frame unless an action is in progress; an empty point stands for that cleared point, the
/// map's corner (0, 0, 0). Pure logic, no Locator.
namespace openblack::ecs::hand_action_point
{

/// What one frame gives the action point
struct Frame
{
	/// This frame's collide: the object under the cursor, else the land under it
	std::optional<glm::vec3> collide;
	/// This frame's land under the cursor
	std::optional<glm::vec3> land;
	/// The position of the object the action in progress holds on to
	std::optional<glm::vec3> actionObject;
	/// The point as the last frame left it
	std::optional<glm::vec3> previous;
	/// An action is in progress (the grip, the grab, a seed armed for the release, the bubble's grip)
	bool actionInProgress {false};
};

/// The action point of a frame. With no action in progress it is this frame's collide, or cleared when the cursor finds
/// nothing. While an action is in progress it is never cleared: the action's object, else the land under the cursor,
/// else the point it had
[[nodiscard]] inline std::optional<glm::vec3> ActionPoint(const Frame& frame)
{
	if (!frame.actionInProgress)
	{
		return frame.collide;
	}
	if (frame.actionObject)
	{
		return frame.actionObject;
	}
	if (frame.land)
	{
		return frame.land;
	}
	return frame.previous;
}

/// The interface's object of a frame: the one under the cursor while no action is in progress; while one is, the
/// object it kept, which nothing under the cursor changes
[[nodiscard]] inline std::optional<entt::entity> CollideObject(std::optional<entt::entity> kept,
                                                               std::optional<entt::entity> underCursor, bool actionInProgress)
{
	return actionInProgress ? kept : underCursor;
}

/// What the hand can interact with, as the game's draw collision and the interface's check take it: an available
/// object, and an abode only once it has been started (built more than 0)
[[nodiscard]] constexpr bool Interactable(bool available, bool abode, float percentBuilt)
{
	return available && (!abode || percentBuilt != 0.0f);
}

/// The interface's object while it is still in the registry; none once it has been destroyed
[[nodiscard]] inline std::optional<entt::entity> ExistingObject(std::optional<entt::entity> object, const Registry& registry)
{
	return object.has_value() && registry.Valid(*object) ? object : std::nullopt;
}

} // namespace openblack::ecs::hand_action_point
