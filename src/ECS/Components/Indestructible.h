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

/// GameThingWithPos::Flags (+0x24) bit 0x4000, GAME_THING_WITH_POS_FLAG_INDESTRUCTIBLE: set and cleared by the script's
/// SET_INDESTRUCTABLE (GScript::SetIndestructable 0x6FDE20, for objects that are not script containers), the puzzle
/// objects (PuzzleGame, HanoiBlock, PuzzlePig) and GameOSFile::LoadInstance. It stops Object::SetLife's death and
/// destruction by spells, and Villager::Drowning 0x76A783 holds a drowning villager's counter at 10: he never drowns.
struct Indestructible
{
	int dummy;
};

} // namespace openblack::ecs::components
