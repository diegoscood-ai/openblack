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

#include <chrono>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs::systems
{

/// The sounds that the buildings of the towns make by themselves, the ambience of the time of day (AbodeSounds)
class AbodeSoundSystemInterface
{
public:
	virtual ~AbodeSoundSystemInterface() = default;
	/// Once a frame, with the game time of the frame. Unused: the buildings' sounds come from the object draw list
	/// (Draw), as each building is drawn
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;
	/// A building of the land's object draw list is drawn, on screen or not, with the frame's game milliseconds: one
	/// with an ambience rolls for it and, near the camera, plays the sound of the time of day. Nothing for anything else
	virtual void Draw(entt::entity object, uint32_t frameMs) = 0;
	/// The number of sounds that the buildings made since the game started, for the debug readout
	[[nodiscard]] virtual uint32_t GetSoundCount() const = 0;
};

} // namespace openblack::ecs::systems
