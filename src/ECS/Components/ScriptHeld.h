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

namespace openblack::ecs::components
{

/// A game thing the scripts know about: the original's script slot (511 of them, taken when a script adds the thing)
/// and the script bits of the thing's flag word. Whether a script holds it and whether one controls it are the tags
/// components::InScript and components::ScriptControlled (ECS/Components/ScriptControl.h), which an entity only has
/// with this component. The logic is in ECS/ScriptHeld.h.
struct ScriptHeld
{
	/// How many script variables hold it (the count stops at 255)
	uint8_t references {0};
	/// The slot was taken by a CREATE-type command; the finders (CALL, GET_...) take it as not created
	bool createdByScript {false};
	/// false: only the flag bits, no script slot (controlled by script on a thing no script took): Process leaves it
	/// alone
	bool hasSlot {true};
};

} // namespace openblack::ecs::components
