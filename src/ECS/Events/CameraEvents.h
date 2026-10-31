/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::events
{
/// A script's control of the camera has ended and the player's camera is made again: what is kept for it outside the
/// camera's model (the camera help icons by the hand) starts again as a new camera's does
struct PlayerCameraRestarted
{
};
} // namespace openblack::ecs::events
