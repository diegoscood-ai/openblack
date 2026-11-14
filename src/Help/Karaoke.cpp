/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Karaoke.h"

#include "Common/TruncateToInt.h"

namespace openblack::help::karaoke
{

namespace
{
/// Per language index, whether the song is followed by the box
constexpr std::array<bool, 13> k_BouncingBall {
    true, true, true, true, true, true, true, true, false, true, false, false, true,
};
constexpr float k_MsToSeconds = 0.001f;
} // namespace

bool UsesBouncingBall(uint32_t language)
{
	return language < k_BouncingBall.size() && k_BouncingBall.at(language);
}

Highlight::Highlight(int32_t screenHeight)
{
	_y.SetPosition(static_cast<float>(screenHeight));
}

Box Highlight::Follow(float x, float y, float width, float lineHeight, std::array<uint8_t, 4> wordRgba, uint32_t frameMs)
{
	_x.SetDestinationWithSpeedAndTime(x, 0.0f, k_Seconds);
	_y.SetDestinationWithSpeedAndTime(y, 0.0f, k_Seconds);
	_width.SetDestinationWithSpeedAndTime(width, 0.0f, k_Seconds);
	const float seconds = static_cast<float>(frameMs) * k_MsToSeconds;
	_x.Update(seconds);
	_y.Update(seconds);
	_width.Update(seconds);
	std::array<uint8_t, 4> rgba = wordRgba;
	rgba[3] = static_cast<uint8_t>(wordRgba[3] >> 1);
	return Box {
	    .left = TruncateToInt(_x.value),
	    .top = TruncateToInt(_y.value),
	    .right = TruncateToInt(_width.value + _x.value),
	    .bottom = TruncateToInt(_y.value + lineHeight),
	    .rgba = rgba,
	};
}

} // namespace openblack::help::karaoke
