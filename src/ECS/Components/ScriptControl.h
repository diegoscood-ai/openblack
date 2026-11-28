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

/// A script holds a reference to the object
struct InScript
{
};

/// A script controls the object: it was made by a script, or a script moved, attached or set the state of it. The
/// creature of the player at this machine is not hurt by what strikes it while under a script's control, and a villager
/// under one returns to what the script had it doing after it lands
struct ScriptControlled
{
};

/// A point a script marked: a place and nothing else
struct ScriptMarker
{
	/// (openblack) the order the markers were made in (ScriptMarkerArchetype::Create, 1 up): the game's list of things
	/// with a place takes each at its head, so the higher, the newer
	uint32_t creationStamp {0};
};

} // namespace openblack::ecs::components
