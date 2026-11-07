/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// The swirling vortices between the lands: the way out of a land, the way into the next and the volcano's glowing
/// mouth. Scripts make them, give the way out its town and flock, and fade them out; once a turn and twice a frame
/// they are stepped
class VortexSystemInterface
{
public:
	virtual ~VortexSystemInterface() = default;

	/// A vortex of a kind made at a point; entt::null for any other kind
	virtual entt::entity Create(glm::vec3 position, VortexType type) = 0;
	/// A vortex starts to fade out, whatever it was doing, and goes when it has. Whether the thing was a vortex.
	virtual bool StartFadeOut(entt::entity vortex) = 0;
	/// The town and the flock the way out of a land brings what comes through it to; nothing for the other kinds
	virtual void SetParameters(entt::entity vortex, entt::entity town, glm::vec3 position, float a, float b,
	                           entt::entity flock) = 0;

	/// At the end of each game turn's particle pass: each vortex's contents, its glow's step, the fades that are over and
	/// the land it levels
	virtual void ProcessTurn() = 0;
	/// Once a frame by the frame's game time, before the other effects' frame steps (they are drawn before the land):
	/// each vortex's hole in the land, and while it shows its swirl under the land, moved and stepped
	virtual void UpdateGroundEffects(float seconds) = 0;
	/// Once a frame by the frame's game time, after the other effects' frame steps: while a vortex shows, its effect over
	/// the land stepped, and every glow's brightness
	virtual void UpdateOverLandEffects(float seconds) = 0;
};

} // namespace openblack::ecs::systems
