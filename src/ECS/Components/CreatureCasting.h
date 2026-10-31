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

#include <array>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// A creature going about casting a miracle: how it goes near, gets away from or turns to face what it casts at, the
/// miracle it cast and holds with the beams of a cast from above, and a try at a miracle that fizzled, which it shows
/// its embarrassment at once it chooses what to do next
struct CreatureCasting
{
	enum class Move : uint8_t
	{
		None,
		GoNear,
		GetAway,
		TurnToFace,
		/// Going to where it can throw into the target from
		ToThrowPosition,
	};
	enum class Outcome : uint8_t
	{
		Running,
		Done,
		Failed,
	};
	Move move {Move::None};
	Outcome outcome {Outcome::Running};
	entt::entity target {entt::null};
	/// The distance it keeps, and the turns it holds still once facing
	float keep {0.0f};
	float settleSeconds {0.0f};
	/// The stage of the move, the turns it has gone on, and the turns it has yet to hold still
	uint8_t phase {0};
	uint32_t turns {0};
	uint32_t holdTurns {0};
	/// Where it stood when it last looked whether it is stuck, and where it last set off for
	glm::vec2 stuckCheck {0.0f};
	std::optional<glm::vec2> destination;

	/// The miracle it cast and holds until it lets go, and the beams of a cast from above, which start at its hands:
	/// the one at the hand's mirror first (each the spot visual's object). Letting go closes the beams down but keeps
	/// them, on the hands, until their objects go with their effects or a cast from above makes new ones
	entt::entity held {entt::null};
	std::array<entt::entity, 2> beams {entt::null, entt::null};

	/// A try at a miracle that fizzled: what at, and which miracle
	struct Fizzle
	{
		entt::entity target;
		MagicType magicType;
	};
	std::optional<Fizzle> fizzle;
	/// What the last try at a thing that fizzled was at. A try at a place that fizzles is shown about this thing.
	entt::entity lastFizzleTarget {entt::null};
};

} // namespace openblack::ecs::components
