/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "Creature/CreatureKnock.h"

namespace openblack::ecs::components
{

/// A creature's knock: whether its bones knock over what they move into this turn, and where each was last
/// (creature_knock; ecs::creature_body_knock). Made with the first change of its body that the knock looks at
struct CreatureKnock
{
	bool on {false};
	creature_knock::Bones bones {};
	/// The body's state as the knock last saw it, none before it first looked
	std::optional<creature_knock::BodyState> seen;
	/// The hand slapped the creature into reeling since the knock last looked: it sets the knock up and starts it
	bool slapped {false};
};

} // namespace openblack::ecs::components
