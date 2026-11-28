/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include "ECS/Systems/WalkPathSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The CHL WALK_PATH of a thing that is not living
/// (docs/wiki/beings/villager-speed-and-scripts.md#walk_path-177-of-a-mobileobject): it follows the focus way of the camera
/// track "Track<number>" of Data\camera.edt, timed by its position way, 100 ms of the track per game turn.
/// OPENBLACK_WALK_PATH_TRACE=1 logs every turn's sample, segment, t and point
class WalkPathSystem final: public WalkPathSystemInterface
{
public:
	/// Into the walking list once, and a new walk: the track, to, forward, step 100, current = from * duration. False
	/// when the track cannot be read (the original then crashes on the null ScriptedCamera)
	bool Start(entt::entity thing, int32_t number, bool forward, float from, float to) override;
	/// Every game turn after the sharks' process: each thing of the list moves along its track
	void ProcessTurn() override;
	[[nodiscard]] const std::vector<entt::entity>& Walking() const override { return _walking; }

private:
	/// The walking list: a new walker at its head, out of it at its walk's end or once it is gone
	std::vector<entt::entity> _walking;
};

} // namespace openblack::ecs::systems
