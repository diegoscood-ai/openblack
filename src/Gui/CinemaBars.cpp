/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CinemaBars.h"

#include <cmath>

#include <algorithm>
#include <limits>

using namespace openblack::gui;

void CinemaBars::Set(bool on, float transitionSeconds)
{
	if (on == _on)
	{
		return;
	}
	// a reversal in the middle of the slide goes on from where the bars are: the percentage before the switch, then the
	// timer = (on ? p : 1 - p) * wideScreenTime * 1000
	const float p = Fraction();
	_transitionSeconds = transitionSeconds;
	_on = on;
	_timer = (on ? p : 1.0f - p) * _transitionSeconds * 1000.0f;
}

float CinemaBars::Fraction() const
{
	const float x = _transitionSeconds > 0.0f ? std::abs(_timer * 0.001f / _transitionSeconds) : 1.0f;
	return std::clamp(_on ? x : 1.0f - x, 0.0f, 1.0f);
}

void CinemaBars::Update(float gameMilliseconds)
{
	_timer += gameMilliseconds;
	_fraction = Fraction();
}

void CinemaBars::Snap()
{
	_timer = -std::numeric_limits<float>::max(); // -FLT_MAX
}

int CinemaBars::BarHeight(int width, int height, float fraction)
{
	// trunc((H - W * 0.5625) * f) / 2, integer division; none on screens wider than 16:9
	const auto bars = static_cast<int>((static_cast<float>(height) - static_cast<float>(width) * 0.5625f) * fraction);
	return std::max(0, bars / 2);
}
