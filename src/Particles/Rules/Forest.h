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

#include <vector>

#include "Camera/CameraPathControl.h"

// The forest miracle's camera particle (Rules/Forest.cpp, ParticleAnimWithCameraCreator): an animated particle with no
// mesh that carries a camera path. Its first step that is the local player's places the path at the particle and asks
// for the camera (CameraPathSystemInterface::Begin). Once the particle is older than its pause the animation plays, and
// every frame the drawn frame gives the path its time; the last frame, or the particle's removal, lets the camera go.
// Wiki: docs/bw1-notes/particles.md and miracles.md, "Forest".

namespace openblack::psys::forest_camera
{

/// PauseBeforePlay when the spell file gives none, in seconds
inline constexpr float k_DefaultPauseSeconds = 4.0f;

/// What a camera particle keeps between its steps and its draws
struct Flags
{
	/// It still wants the camera. Cleared for good at a step that is not the local player's, or when it lets go
	bool wanted {true};
	/// The camera was asked for, at the first step that wanted it. It is asked once: a refusal is never retried
	bool asked {false};
	/// The path plays and the camera follows it: from a later step than the one that asked, once the particle is older
	/// than its pause
	bool playing {false};
};

/// What one step of the particle does
struct StepActions
{
	/// Place the path at the particle and ask for the camera
	bool begin {false};
	/// Let go of the camera
	bool release {false};
	/// Its animation plays from now on (the frame moves from the next step, this one's frame step being done)
	bool playAnim {false};
};

/// Letting go of the camera: only while the particle still wants it, and it never wants it again. Whether there was
/// anything to let go of
[[nodiscard]] constexpr bool LetGo(Flags& flags)
{
	const bool held = flags.wanted;
	flags.wanted = false;
	return held;
}

/// One step of the particle, after its frame step: `mine` is whether its effect is the local player's (an effect with no
/// spell is), `ageSeconds` its age and `pauseSeconds` its PauseBeforePlay. In the original's order: the wish is kept
/// only while it is mine; a step that is not mine, once the camera was asked for, lets go, which then finds the wish
/// already cleared and does nothing; while it wants the camera, the first step asks for it and a later one older than
/// the pause plays; and the animation plays once it is older than the pause, whatever the camera does
[[nodiscard]] constexpr StepActions Step(Flags& flags, bool mine, float ageSeconds, float pauseSeconds)
{
	StepActions actions;
	flags.wanted = flags.wanted && mine;
	if (!mine && flags.asked)
	{
		actions.release = LetGo(flags);
	}
	if (flags.wanted)
	{
		if (!flags.asked)
		{
			flags.asked = true;
			actions.begin = true;
		}
		else if (ageSeconds > pauseSeconds)
		{
			flags.playing = true;
		}
	}
	actions.playAnim = ageSeconds > pauseSeconds;
	return actions;
}

/// What the particle's draw does each frame
struct DrawActions
{
	/// The path's time at the drawn frame (camera_path::PathTimeFromFrame)
	int32_t pathMilliseconds {0};
	/// Let go of the camera
	bool release {false};
};

/// The particle's draw: its drawn frame of a clip of `clipMilliseconds` gives the path's time, and once the camera was
/// asked for, the last frame of the cycle lets it go
[[nodiscard]] constexpr DrawActions Draw(Flags& flags, int32_t clipMilliseconds, int32_t drawnFrame)
{
	DrawActions actions;
	actions.pathMilliseconds = camera_path::PathTimeFromFrame(clipMilliseconds, drawnFrame);
	if (flags.asked && camera_path::ReachedLastFrame(drawnFrame))
	{
		actions.release = LetGo(flags);
	}
	return actions;
}

/// The particle is removed: once the camera was asked for, it lets go. Whether there was anything to let go of
[[nodiscard]] constexpr bool Removed(Flags& flags)
{
	return flags.asked && LetGo(flags);
}

/// One live camera particle, for the debug tools
struct Watch
{
	/// The key its path is placed by (CameraPathSystemInterface::PathOwner)
	uint64_t owner {0};
	/// Its last drawn frame, -1 before its first draw
	int32_t drawnFrame {-1};
	Flags flags;
};

/// Once a frame, after the camera's update and before the draw, as the particles' draw: each camera particle's drawn
/// frame at the effect's draw fraction (`turnFraction`, or 1 for the effects stepped every frame) gives its path its
/// time and whether it plays (CameraPathSystemInterface::FollowAt), and the last frame lets the camera go
void UpdateFrame(float turnFraction);

/// The live camera particles, oldest first
[[nodiscard]] std::vector<Watch> Watches();

} // namespace openblack::psys::forest_camera
