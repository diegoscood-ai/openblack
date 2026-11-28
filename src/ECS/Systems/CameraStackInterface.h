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
#include <cstdint>

#include <memory>

#include "Camera/CameraModeStack.h"

namespace openblack
{
class CameraModel;
} // namespace openblack

namespace openblack::ecs::systems
{

/// The camera's stack of modes (docs/wiki/camera/game-camera.md#openblack), with the camera's models the new player's
/// modes went over, each kept by its mode's number until that mode is popped (camera_stack::ModeStack::PushPlayer).
/// The script camera (Camera/ScriptCamera.h) drives both (Locator::cameraStack)
class CameraStackInterface
{
public:
	virtual ~CameraStackInterface() = default;

	/// The stack of modes
	[[nodiscard]] virtual camera_stack::ModeStack& Modes() = 0;
	[[nodiscard]] virtual const camera_stack::ModeStack& Modes() const = 0;
	/// The camera's model the player's mode `id` went over, kept for it after the ones kept before
	virtual void Keep(uint32_t id, std::unique_ptr<CameraModel> model) = 0;
	/// The camera's model kept for the player's mode `id`, taken: none when it keeps none
	[[nodiscard]] virtual std::unique_ptr<CameraModel> TakeKept(uint32_t id) = 0;
	/// Every model kept for the player's mode `id` goes
	virtual void DropKept(uint32_t id) = 0;
	/// The kept models no player's mode keeps any more (popped, or deleted from the bottom of a full stack) go
	virtual void DropLost() = 0;
	/// The number of models kept
	[[nodiscard]] virtual std::size_t KeptCount() const = 0;
	/// Every mode goes, then every kept model
	virtual void Clear() = 0;
};

} // namespace openblack::ecs::systems
