/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <optional>

#include "3D/TempleSounds.h"

/// Clicking one of the challenge room's challenges: the room's camera turns to it first, and the challenge is chosen
/// once the camera has come all the way. Choosing one fades the room to white with a woosh; once the fade is over the
/// game leaves the temple and starts the challenge's script.
namespace openblack::temple_challenge_pick
{

enum class Click : uint8_t
{
	/// The camera turns to the clicked challenge
	TurnTo,
	/// The clicked challenge is chosen, if it can be
	Choose,
};

/// What a click does for the camera's blend toward the challenges: choose only at exactly all the way (an unordered
/// blend counts as all the way)
[[nodiscard]] inline Click ClickOn(float cameraBlend)
{
	return cameraBlend == 1.0f || std::isnan(cameraBlend) ? Click::Choose : Click::TurnTo;
}

/// A challenge won already, its success exactly 1 (or unordered), is not chosen again
[[nodiscard]] inline bool CanChoose(float success)
{
	return !(success == 1.0f || std::isnan(success));
}

/// The woosh of a challenge as it is chosen, if it can be
[[nodiscard]] inline std::optional<temple_sounds::Options> ChosenWoosh(float success, uint32_t tickCount)
{
	if (!CanChoose(success))
	{
		return std::nullopt;
	}
	return temple_sounds::Woosh(tickCount);
}

} // namespace openblack::temple_challenge_pick
