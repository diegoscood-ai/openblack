/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

/// A puff of smoke or dust: fifteen sprites of the smoke sheet scattered round a point, each flying off up and out,
/// spinning, growing and fading away. A blast throws up a brown puff of dust as it leaves its rubble, as does a tree taking
/// root again; a dead body going leaves a grey puff of smoke. The puffs themselves are ECS/DisappearSmoke's
namespace openblack::dust_puff
{

enum class Kind : uint8_t
{
	/// Grey, faint and slow: it drifts off and fades away over three seconds
	Smoke,
	/// Brown, thrown out fast and opaque, fading only in its last part over a second and a half
	Dust,
};

/// The smoke's grey and the dust's brown, 0xRRGGBB, each puff's own unless it is made with another
constexpr uint32_t k_SmokeColour = 0x808080u;
constexpr uint32_t k_Colour = 0x68503Du;

} // namespace openblack::dust_puff
