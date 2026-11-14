/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

/// What the scripts' SET_TARGET does to a thing (docs/wiki/scripts/script-commands.md#set_target-176): it is thrown so
/// that, under gravity alone, it lands on the target after the given time. Pure rules: the native gathers the facts
/// and acts on the answer.
namespace openblack::ecs::script_target
{

/// The gravity the throw is worked out with, in metres a second squared
constexpr float k_Gravity = 9.81f;
/// Each spin draw is GameFloatRand(k_SpinDraw), centred on 0 and scaled
constexpr float k_SpinDraw = 200.0f;
constexpr float k_SpinCentre = 100.0f;
constexpr float k_SpinScale = 0.01f;

/// The velocity that takes a thing from `from` to `to` in `time` seconds under gravity: the difference over the time,
/// with the fall made up on y. In the game's order of float operations: the fall is -g x (t x t x 0.5), the height
/// difference minus that fall, each difference times 1 / t
[[nodiscard]] constexpr glm::vec3 LaunchVelocity(const glm::vec3& from, const glm::vec3& to, float time)
{
	const float fall = -k_Gravity * (time * time * 0.5f);
	const float perSecond = 1.0f / time;
	return {(to.x - from.x) * perSecond, ((to.y - from.y) - fall) * perSecond, (to.z - from.z) * perSecond};
}

/// One axis of the throw's spin from its draw of GameFloatRand(k_SpinDraw): between -1 and 1
[[nodiscard]] constexpr float SpinFromDraw(float draw)
{
	return (draw - k_SpinCentre) * k_SpinScale;
}

} // namespace openblack::ecs::script_target
