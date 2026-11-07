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

namespace openblack::graphics
{
struct IntroLightOverlay;
} // namespace openblack::graphics

namespace openblack::ecs::systems
{

/// The opening's arrival of the god and the boy's rescue, which the opening's script plays: the light that falls from
/// the sky onto the boy in the sea, the camera chasing it, and the god's hand lifting him out and setting him down
/// beside his parents, the boy held in its grip (docs/wiki/scripts/land1-intro.md#the-intro-hand-and-the-light)
class IntroSystemInterface
{
public:
	virtual ~IntroSystemInterface() = default;

	/// A script plays one of the opening's specials: 0 the light falls, 1 the camera chases it, 2 the camera is the
	/// script's again, 4 the hand stands ready to set the boy down, 5 it sets him down holding him, and again to let him
	/// go. The others aren't this system's.
	virtual void Play(int32_t special) = 0;
	/// All of it goes at once: the light, the hand, the chasing camera
	virtual void ReleaseAll() = 0;
	/// Whether the lifting hand's clip has come round, which it never does as it plays once
	[[nodiscard]] virtual bool HasLiftFinished() const = 0;

	/// Each drawn frame, by its game milliseconds: those that follow the hand are given its grip as the last frame left
	/// it, then the light falls or fades and the hand's clip plays on
	virtual void Update(uint32_t milliseconds) = 0;

	/// What the light draws this frame, for the renderer: where it is sorted among what blends, and its sprites
	virtual void FillFrame(graphics::IntroLightOverlay& out) const = 0;
};

} // namespace openblack::ecs::systems
