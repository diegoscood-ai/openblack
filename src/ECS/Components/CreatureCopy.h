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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Creature/TattooEditor.h"
#include "Enums.h"

namespace openblack::ecs::components
{

/// The Creature Cave's copy of the player's creature: a body of its own that stands by the room's fire while the player
/// is in the temple, made as the room starts and gone when the player leaves. It is not a creature (it has no Creature
/// component), so nothing that looks for creatures finds it
struct CreatureCopy
{
	/// The creature it is a copy of
	entt::entity source {entt::null};
	CreatureType species {CreatureType::Unknown};
	/// Its size, held within the limits a body is drawn at
	float size {1.0f};
	/// The way it faces, towards the fire
	float heading {0.0f};
	/// Whether it sleeps by the fire rather than sitting (the coin tossed as the room starts)
	bool sleeping {false};
	/// The flag its rest is started with, tossed after the coin
	bool restFlag {false};
	/// The room's fire, which it faces and, sitting, looks at, and which lights it
	glm::vec3 fire {0.0f};
	/// The creature's alignment as the copy was made, -1 evil to 1 good, which its sparkles go by
	float alignment {0.0f};
	/// Whether its rest has started, in the room's first frame
	bool started {false};
	/// A turn on the spot to a heading, asked for after the room was drawn, which starts as the copy next moves on; and
	/// the turn it is making
	std::optional<float> turnRequested;
	std::optional<creature_tattoo_editor::CopyTurn> turn;
};

} // namespace openblack::ecs::components
