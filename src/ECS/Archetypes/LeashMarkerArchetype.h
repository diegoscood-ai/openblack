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

#include <entt/entity/fwd.hpp>

#include "Camera/BookmarkPulse.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::archetypes
{

/// The ring a player sees over where their creature was sent with the leash, and the creature's footprint inside it
/// (leash_marker): two yellow sprites facing the camera, from the sheet of markers and footprints. They are made for
/// the frame that draws them and are not entities, so that showing the marker leaves the game's entities as they were.
class LeashMarkerArchetype
{
public:
	/// One of the marker's sprites as it is drawn this frame: its Transform's scale is the half width and half height
	struct Placed
	{
		components::Sprite sprite;
		components::Transform transform;
	};

	/// The ring: cell leash_marker::k_RingCell of misc0.raw with its alpha misc0a.raw, blended and tinted yellow, as a
	/// camera bookmark's marker
	[[nodiscard]] static components::Sprite RingSprite();
	/// The species' footprint, from the same sheet, blended and yellow, turned a quarter turn
	[[nodiscard]] static components::Sprite FootprintSprite(CreatureType species);

	/// A creature's marker as it is drawn this frame at the pulse, the ring first and then the footprint: none while
	/// the creature has no target, and none before the pulse's first step
	[[nodiscard]] static std::optional<std::array<Placed, 2>> Drawn(const Registry& registry, entt::entity creature,
	                                                                const bookmark_pulse::Pulse& pulse);
};

} // namespace openblack::ecs::archetypes
