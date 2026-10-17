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

#include <algorithm>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

/// A miracle's camera path: how its points and its time are taken, when it lets the camera go, and when the player
/// takes the camera back from it. The path keeps the camera until a key moving it left, right, forwards or backwards
/// would move it this frame, or the hand grips the land. A movement key moves the camera 400 units a second of the
/// camera's frame time, and only a step of at least a whole unit counts: the camera's frame time is the frame's whole
/// milliseconds as seconds, at most a tenth of a second, so a key held through a frame of 2 ms or less doesn't take the
/// camera back. Rotating, tilting and zooming never do.
namespace openblack::camera_path
{

/// The camera's frame time is no longer than this, in seconds
inline constexpr float k_MaxFrameSeconds = 0.1f;
/// Seconds in a millisecond, as the game's single precision constant
inline constexpr float k_SecondsPerMillisecond = 0.001f;
/// How far a movement key moves the camera in a second of its frame time
inline constexpr float k_KeyStepPerSecond = 400.0f;

/// The camera's frame time for a frame of whole milliseconds
[[nodiscard]] constexpr float FrameSeconds(uint32_t frameMilliseconds)
{
	return std::min(static_cast<float>(frameMilliseconds) * k_SecondsPerMillisecond, k_MaxFrameSeconds);
}

/// Whether a movement key held this frame would move the camera at least a whole unit
[[nodiscard]] constexpr bool KeyStepMoves(uint32_t frameMilliseconds)
{
	return static_cast<int32_t>(FrameSeconds(frameMilliseconds) * k_KeyStepPerSecond) != 0;
}

/// Whether the player takes the camera back: a movement key that would move it, or the hand gripping the land
[[nodiscard]] constexpr bool TakesCameraBack(bool movementKey, uint32_t frameMilliseconds, bool grippingLand)
{
	return (movementKey && KeyStepMoves(frameMilliseconds)) || grippingLand;
}

/// A placed path follows its animation's drawn frame: one cycle of the animation is this many frames
inline constexpr int32_t k_FramesPerCycle = 1000;
/// Once the path plays, the camera is sent each frame to the path's point to arrive this long after; before, it glides
/// onto the path over the pause and this long more
inline constexpr float k_PlacedLagSeconds = 0.3f;

/// How long the camera takes to glide onto a placed path before it plays: the pause and the lag, added in single
/// precision
[[nodiscard]] constexpr float PlacedGlideSeconds(float pauseSeconds)
{
	return pauseSeconds + k_PlacedLagSeconds;
}

/// The path's time at a drawn frame of its animation: the clip's whole milliseconds times the frame over the frames of
/// a cycle, in integers (the division rounds towards zero), so the time moves on in steps of whole frames
[[nodiscard]] constexpr int32_t PathTimeFromFrame(int32_t clipMilliseconds, int32_t frame)
{
	return clipMilliseconds * frame / k_FramesPerCycle;
}

/// Whether a drawn frame lets the camera go: the last frame of the cycle or past it
[[nodiscard]] constexpr bool ReachedLastFrame(int32_t frame, int32_t framesPerCycle = k_FramesPerCycle)
{
	return frame >= framesPerCycle - 1;
}

/// A path's point placed in the world by its placement (rotation and scale in the columns, then the translation), each
/// coordinate summed in the original's order: z, then y, then x, then the translation, one single precision step at a
/// time
[[nodiscard]] inline glm::vec3 PlacePoint(const glm::mat4& placement, const glm::vec3& point)
{
	glm::vec3 placed;
	for (int i = 0; i < 3; ++i)
	{
		// One product or sum a statement, so that none is fused
		const float z = point.z * placement[2][i];
		const float y = point.y * placement[1][i];
		const float x = point.x * placement[0][i];
		const float zy = z + y;
		const float zyx = zy + x;
		placed[i] = zyx + placement[3][i];
	}
	return placed;
}

} // namespace openblack::camera_path
