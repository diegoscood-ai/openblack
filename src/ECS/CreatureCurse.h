/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::saved_game_load
{

/// The script's curse of the local player's creature, as the game object keeps it last in its record: the creature's
/// height, strength and alignment when the game was saved, or three 0s with no creature
struct CreatureCurse
{
	float height {0.0f};
	float strength {0.0f};
	float alignment {0.0f};

	bool operator==(const CreatureCurse&) const = default;
};

} // namespace openblack::ecs::saved_game_load
