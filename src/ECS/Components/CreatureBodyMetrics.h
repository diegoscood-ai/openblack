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

/// What a creature's body measures, in its base mesh's units, taken once when the body is made, as the game takes them
/// when it loads a body: how far the rest pose's bones reach up and down from the mesh's origin
/// (creature_morph::RestHeight), and how far its bones reach out from its middle in the first frame of its stand
/// animation (creature_cast_moves::BoneReach). Its radius on the ground follows from them (creature_morph::Radius).
struct CreatureBodyMetrics
{
	float restHeight {0.0f};
	float reach {0.0f};
};

} // namespace openblack::ecs::components
