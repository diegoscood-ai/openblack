/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GlowShellLook.h"

#include <cmath>

using namespace openblack;

namespace
{
/// The game turns its milliseconds into seconds with this
constexpr float k_SecondsPerMillisecond = 0.001f;
} // namespace

glm::vec2 glow_shell::Slide(glm::vec2 slide, glm::vec2 speed, uint32_t milliseconds)
{
	const float seconds = static_cast<float>(milliseconds) * k_SecondsPerMillisecond;
	const float across = seconds * speed.x;
	const float up = seconds * speed.y;
	return {std::fmod(across + slide.x, 1.0f), std::fmod(up + slide.y, 1.0f)};
}

bool glow_shell::TooFar(float distanceSquared, float halfDiagonal, float scale)
{
	const float reach = halfDiagonal * scale * k_FarCut;
	return reach * reach < distanceSquared;
}
