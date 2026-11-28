/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

// The whole collection's alpha fade over a time window (AR_FadeCollectionAlpha). Free of state, so it is tested on its
// own.

namespace openblack::particles::maths
{

/// The alpha a collection takes at age `age` (its own age, or the time since its effect began closing) with a step of
/// `dt`: inside [start, stop] the line from `from` to `to`, and `to` on the window's last step, the one whose next would
/// pass `stop`; after the window `to` again only when `holdAfterStop`, at every step. None leaves the alpha as it is
[[nodiscard]] constexpr std::optional<float> CollectionFadeAlpha(float age, float dt, float start, float stop, float from,
                                                                 float to, bool holdAfterStop) noexcept
{
	if (age >= start && age <= stop)
	{
		if (age + dt > stop)
		{
			return to;
		}
		return stop > start ? from + (age - start) / (stop - start) * (to - from) : to;
	}
	if (holdAfterStop && age > stop)
	{
		return to;
	}
	return std::nullopt;
}

} // namespace openblack::particles::maths
