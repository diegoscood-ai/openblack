/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptFade.h"

#include "Graphics/ArgbColour.h"

using namespace openblack::gui;

namespace
{
constexpr uint32_t k_AlphaMask = 0xFF000000u;
constexpr uint32_t WithAlpha(uint32_t colour, uint32_t alpha)
{
	return (colour & ~k_AlphaMask) | (alpha << 24);
}
} // namespace

void ScriptFade::FadeTo(uint8_t red, uint8_t green, uint8_t blue, float seconds)
{
	// SET_FADE truncates its float arguments and passes the time as a signed char
	const auto time = static_cast<int8_t>(static_cast<int>(seconds));
	const uint32_t rgb = openblack::argb_colour::Argb(red, green, blue);
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

void ScriptFade::FadeBackToNormal(float seconds)
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

void ScriptFade::ProcessTurn()
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
