/*******************************************************************************
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
#include <optional>
#include <utility>

#include "Common/Zoomer.h"

namespace openblack::gui
{

/// How the game fades its boxes as one replaces another. The box shown fades in from nothing over half a second. The
/// box it replaces either fades out under it over a fifth of a second, both drawn (a box shown over another, or the one
/// under it shown again), or is gone at once (a tab, which hides every box before it shows the new one).
template <typename Box>
class BoxFades
{
public:
	enum class Switch : uint8_t
	{
		CrossFade,
		Replace,
	};
	static constexpr float k_InSeconds = 0.5f;
	static constexpr float k_OutSeconds = 0.2f;

	/// The box shown afresh, from nothing; the one shown before fades out under it, or goes at once
	void Show(Box box, Switch how)
	{
		_leaving.reset();
		if (how == Switch::CrossFade && _shown.has_value())
		{
			LeaveShown();
		}
		_shown = Faded {.box = box};
		_shown->fade.SetDestinationWithSpeedAndTime(1.0f, 0.0f, k_InSeconds);
	}
	/// No box shown: the one shown fades out from where it is, and one still leaving goes at once
	void Hide()
	{
		_leaving.reset();
		if (_shown.has_value())
		{
			LeaveShown();
		}
		_shown.reset();
	}
	/// The box shown at once whole, as a question answered gives its box back
	void SnapShown()
	{
		if (_shown.has_value())
		{
			_shown->fade.SetPosition(1.0f);
		}
	}
	void Update(float seconds)
	{
		if (_shown.has_value())
		{
			_shown->fade.Update(seconds);
		}
		if (_leaving.has_value())
		{
			_leaving->fade.Update(seconds);
			// Gone once it has faded all the way out
			if (_leaving->fade.HasArrived() && _leaving->fade.value <= 0.0f)
			{
				_leaving.reset();
			}
		}
	}

	/// The box shown and its alpha, 0 to 1
	[[nodiscard]] std::optional<std::pair<Box, float>> Shown() const { return Of(_shown); }
	/// The box fading out under it and its alpha
	[[nodiscard]] std::optional<std::pair<Box, float>> Leaving() const { return Of(_leaving); }

private:
	struct Faded
	{
		Box box;
		Zoomer fade;
	};

	void LeaveShown()
	{
		// It fades out from its value and speed as they are
		_leaving = _shown;
		_leaving->fade.SetDestinationWithSpeedAndTime(0.0f, 0.0f, k_OutSeconds);
	}
	[[nodiscard]] static std::optional<std::pair<Box, float>> Of(const std::optional<Faded>& faded)
	{
		if (!faded.has_value())
		{
			return std::nullopt;
		}
		return std::pair {faded->box, std::clamp(faded->fade.value, 0.0f, 1.0f)};
	}

	std::optional<Faded> _shown;
	std::optional<Faded> _leaving;
};

} // namespace openblack::gui
