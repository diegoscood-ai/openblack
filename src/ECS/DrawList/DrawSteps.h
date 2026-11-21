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

#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// The effects the original moves on only inside their object's Draw, which the landscape's object draw list calls: on a
// frame the list does not run, or does not reach the object, they stand still. Their steps come after the list's in the
// frame, so they read this frame's list. docs/wiki/rendering/object-draw-list.md#the-object-draw-list-stage-4m
namespace openblack::ecs::draw_steps
{

/// Whether a frame steps such an effect: the list ran this frame (`listRan`) and called the object's Draw (`drawn`),
/// and that Draw found the object on the screen where the effect's step waits for it (`onScreen`; true where it does
/// not wait). With the debug switch `everyFrame` every frame steps it, as before
[[nodiscard]] constexpr bool Steps(bool everyFrame, bool listRan, bool drawn, bool onScreen) noexcept
{
	return everyFrame || (listRan && drawn && onScreen);
}

/// The object draw list's part of the frame that those steps read
struct ListFrame
{
	/// The debug switch: every frame steps them
	bool everyFrame {false};
	/// The list ran this frame
	bool ran {false};
	/// The objects whose Draw the list called this frame
	std::span<const entt::entity> drawn;
	/// Of those, the ones their Draw found on the screen
	std::span<const entt::entity> onScreen;

	/// Steps for an object whose Draw steps the effect only when it finds the object on the screen
	[[nodiscard]] bool StepsOnScreen(entt::entity object) const;
	/// Steps for an object whose Draw steps the effect, on the screen or not
	[[nodiscard]] bool StepsDrawn(entt::entity object) const;
};

/// This frame's, from the object draw list and the debug switch (EngineConfig::effectsSteppedEveryFrame). With no list
/// (a test that makes no game), every frame steps them
[[nodiscard]] ListFrame ThisFrame();

/// The order in which the Z sorter's drain calls back the Z objects queued, in this order, at these points: far to
/// near from the camera (graphics::zsort::Key), the first queued first among equal keys. For an effect stepped in its
/// Z object's callback; the indices into `points`
[[nodiscard]] std::vector<size_t> DrainOrder(std::span<const glm::vec3> points, const glm::vec3& camera);

} // namespace openblack::ecs::draw_steps
