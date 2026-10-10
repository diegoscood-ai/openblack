/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::graphics
{
class L3DMesh;
}

namespace openblack::ecs::components
{
struct Transform;
}

namespace openblack::ecs::systems
{

/// The smoke from the homes' chimneys (components::ChimneySmoke), lit while someone is home, and the air the hand
/// stirs as it passes. A smoke moves on only in the frames its home is drawn in (see chimney_smoke::Advance)
class ChimneySmokeSystemInterface
{
public:
	virtual ~ChimneySmokeSystemInterface() = default;

	/// Gives a home whose mesh has a chimney its smoke, grey from a workshop
	virtual void Attach(entt::entity abode, const graphics::L3DMesh& mesh, const components::Transform& transform,
	                    bool workshop) = 0;
	/// Once a frame, before the smokes are drawn: where the hand is and the air it stirs, its speed easing towards its
	/// motion once a game turn
	virtual void UpdateHandWind() = 0;
	/// The frame's push on a smoke at this chimney: the hand's air if it passes close and quickly, else a random breath
	/// across the ground, which draws two random numbers
	[[nodiscard]] virtual glm::vec3 Drift(const glm::vec3& chimney) = 0;
};

} // namespace openblack::ecs::systems
