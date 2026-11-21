/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "Graphics/Mists.h"
#include "Particles/ParticleEffect.h"

namespace openblack::graphics
{
/// The holder effect of a seed's graphic inside a gesture icon (Magic/Gestures/GestureIconSeeds.h): its sprites and its
/// mists where the step of this frame's draw left them, in its draw order, around the models' camera's focus. They are
/// drawn at once with the models' camera and moved on the screen to the box above the icon
/// (Graphics/RendererGestureIcons.cpp)
struct GestureIconEffect
{
	/// One draw of the walk: a run of sprites, or a mist
	struct Draw
	{
		std::vector<particles::Effect::DrawAtom> sprites;
		std::optional<mists::MistDesc> mist;
	};
	std::vector<Draw> draws;
};

/// A holder effect's draws from its atoms in their draw order: each run of sprites (but not an atom laying a glowing
/// shell), each mist `mistOf` describes, in the walk's order; the other atoms (meshes, chains, the rest) draw nothing
[[nodiscard]] GestureIconEffect
IconEffectOf(std::vector<particles::Effect::OrderedItem> items,
             const std::function<std::optional<mists::MistDesc>(const particles::Effect::DrawAtom&)>& mistOf);
} // namespace openblack::graphics
