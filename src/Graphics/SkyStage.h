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

#include <array>
#include <span>

#include "Graphics/RenderPass.h"

/// What the sky stage of a pass draws, in order, before the sea, the land and everything else: the dome, then the sun,
/// then the moon (its glow and its disc), and the clouds sent to the frame's sorted queue of blended things. Nothing
/// else: the stars are in the night domes, the moon's dark side is its texture and the sun has no flares. Wiki:
/// docs/wiki/world/sky.md#sky-sun-moon-and-clouds-original
namespace openblack::graphics::sky_stage
{

enum class Part : uint8_t
{
	Dome,
	Sun,
	Moon,
	/// Each cloud queued with the frame's blended things, sorted far to near with them
	Clouds,
};

/// The main pass's sky: the dome, the sun, the moon, then the clouds into the sorted queue
inline constexpr std::array<Part, 4> k_Main {Part::Dome, Part::Sun, Part::Moon, Part::Clouds};
/// The sea's reflection: the dome and the mirrored moon, but no sun
inline constexpr std::array<Part, 2> k_Reflection {Part::Dome, Part::Moon};
/// Any other pass that draws the sky: the dome alone
inline constexpr std::array<Part, 1> k_DomeOnly {Part::Dome};
/// Inside the temple, seen through its openings: the dome and the moon, no sun (and so no glare)
inline constexpr std::array<Part, 2> k_Temple {Part::Dome, Part::Moon};

/// The sky stage of the world's pass `pass`
[[nodiscard]] constexpr std::span<const Part> Parts(RenderPass pass)
{
	if (pass == RenderPass::Main)
	{
		return k_Main;
	}
	if (pass == RenderPass::Reflection)
	{
		return k_Reflection;
	}
	return k_DomeOnly;
}

/// The sky stage inside the temple, whatever its pass
[[nodiscard]] constexpr std::span<const Part> TempleParts()
{
	return k_Temple;
}

} // namespace openblack::graphics::sky_stage
