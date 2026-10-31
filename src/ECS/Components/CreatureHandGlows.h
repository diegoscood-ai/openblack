/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>

#include "Creature/CreatureHandGlow.h"

namespace openblack::ecs::components
{

/// The glows on a creature's body (see creature_hand_glow): their powers and scales, and where the renderer draws each
/// one this frame. Made the first time a glow is given.
struct CreatureHandGlows
{
	creature_hand_glow::Glows glows;
	/// Each glow where this frame's draw of the body put it, none for a place without a glow or whose bone the body
	/// lacks, and none at all for a body not drawn this frame
	std::array<std::optional<creature_hand_glow::Placed>, creature_hand_glow::k_PlaceCount> placed;
};

} // namespace openblack::ecs::components
