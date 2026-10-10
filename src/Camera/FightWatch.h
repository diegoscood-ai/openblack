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

namespace openblack::camera
{

/// The player's own camera watching a creature fight: it flies to look at the arena, then keeps both fighters in view
/// until the fight is over or the player looks away
class FightWatch
{
public:
	virtual ~FightWatch() = default;
	/// Starts watching the fight between two creatures in an arena, flying to look at it
	virtual void StartFight(entt::entity fighterA, entt::entity fighterB, glm::vec3 arenaCentre, float arenaRadius) = 0;
	/// Stops watching at once, with no lingering on the arena
	virtual void EndFightNow() = 0;
	/// The fight is over: the camera lingers on it for three seconds, then ends the watch
	virtual void EndFight() = 0;
	/// Whether the camera is watching a fight now, including while it lingers on the arena after the fight
	[[nodiscard]] virtual bool IsWatchingFight() const = 0;
	/// Whether the camera, from where it is and where it looks, is too far from an arena to be watching a fight in it
	[[nodiscard]] virtual bool WantToQuitFight(glm::vec3 arenaCentre, float arenaRadius) const = 0;
};

} // namespace openblack::camera
