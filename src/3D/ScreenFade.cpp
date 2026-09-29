/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScreenFade.h"

#include <algorithm>
#include <cmath>

using namespace openblack;

namespace
{
constexpr uint32_t k_AlphaMask = 0xFF000000u;
constexpr uint32_t WithAlpha(uint32_t colour, uint32_t alpha)
{
	return (colour & ~k_AlphaMask) | (alpha << 24);
}
} // namespace

void ScreenFade::FadeTo(uint8_t red, uint8_t green, uint8_t blue, float seconds)
{
	// SET_FADE truncates its float arguments and passes the time as a signed char
	const auto time = static_cast<int8_t>(static_cast<int>(seconds));
	const uint32_t rgb = (static_cast<uint32_t>(red) << 16) | (static_cast<uint32_t>(green) << 8) | blue;
	if (time <= 0)
	{
		_colour = WithAlpha(rgb, 255);
		_rate = 0.0f;
		return;
	}
	// the alpha byte keeps its old value until the next turn
	_colour = (_colour & k_AlphaMask) | rgb;
	_current = 0.0f;
	_rate = 255.0f / (static_cast<float>(time) * 10.0f);
}

void ScreenFade::FadeBackToNormal(float seconds)
{
	const auto time = static_cast<int8_t>(static_cast<int>(seconds));
	if (time <= 0)
	{
		_colour = WithAlpha(_colour, 0);
		_rate = 0.0f;
		return;
	}
	_current = 255.0f;
	_rate = -255.0f / (static_cast<float>(time) * 10.0f);
}

void ScreenFade::ProcessTurn()
{
	if (_rate == 0.0f)
	{
		return;
	}
	const float value = _current + _rate;
	if (value > 255.0f)
	{
		_colour = WithAlpha(_colour, 255);
		_rate = 0.0f;
	}
	else if (value < 0.0f)
	{
		_colour = WithAlpha(_colour, 0);
		_rate = 0.0f;
	}
	else
	{
		_current = value;
		_colour = WithAlpha(_colour, static_cast<uint32_t>(value));
	}
}

void ScreenFade::SetWideScreen(bool on, float transitionSeconds)
{
	if (on == _wideOn)
	{
		return;
	}
	_wideTime = transitionSeconds;
	// a reversal in the middle of the slide goes on from where the bars are
	const float p = _wideFraction;
	_wideOn = on;
	_wideTimer = (on ? p : 1.0f - p) * _wideTime * 1000.0f;
}

void ScreenFade::UpdateWideScreen(float gameMilliseconds)
{
	_wideTimer += gameMilliseconds;
	const float x = _wideTime > 0.0f ? std::abs(_wideTimer * 0.001f / _wideTime) : 1.0f;
	_wideFraction = std::clamp(_wideOn ? x : 1.0f - x, 0.0f, 1.0f);
}

int ScreenFade::LetterboxHeight(int width, int height, float fraction)
{
	// trunc((H - W * 0.5625) * f) / 2, integer division; none on screens wider than 16:9
	const auto bars = static_cast<int>((static_cast<float>(height) - static_cast<float>(width) * 0.5625f) * fraction);
	return std::max(0, bars / 2);
}
