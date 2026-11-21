/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{

/// A bank of mist (components::Mist) fading from one size and transparency to another, one step a game turn, as a
/// script's SET_MIST_FADE starts it (ECS/MistFades.h). Once the last step is taken it stays, all its steps taken, and the
/// mist as it ended
struct MistFade
{
	/// How many steps the fade takes: its seconds x 10 turns
	float steps {0.0f};
	/// The steps taken so far
	float taken {0.0f};
	/// What each step adds to the mist's size
	float sizeStep {0.0f};
	/// The transparency, 0..255, as a float: the mist's alpha is its whole part
	float alpha {0.0f};
	/// What each step adds to the transparency
	float alphaStep {0.0f};
};

} // namespace openblack::ecs::components
