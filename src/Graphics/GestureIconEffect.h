/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include "Particles/ParticleEffect.h"

namespace openblack::graphics
{
/// The holder effect of a seed's graphic inside a gesture icon (Magic/Gestures/GestureIconSeeds.h): its sprites where
/// the step of this frame's draw left them, in its draw order, around the models' camera's focus. They are drawn at
/// once with the models' camera and moved on the screen to the box above the icon (Graphics/RendererGestureIcons.cpp)
struct GestureIconEffect
{
	std::vector<particles::Effect::DrawAtom> sprites;
};
} // namespace openblack::graphics
