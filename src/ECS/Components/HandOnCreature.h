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
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureFeedback.h"

namespace openblack::ecs::components
{

/// On the player's hand while it is held to a creature: how it has stroked and slapped it so far
struct HandOnCreature
{
	entt::entity creature {entt::null};
	/// The running sum of strokes and slaps, from -1 to 1
	float sum {0.0f};
	/// The clocks strokes wait on, and the time since taking hold or the last stroke, which a slap waits on
	creature_feedback::StrokeClocks clocks;
	float slapReadyMs {0.0f};
	/// The part last stroked, forgotten when the hand comes off the body
	std::optional<creature_feedback::BodyPart> lastPart;
	/// Where the hand was on the plane through the creature facing the camera and the cursor, and whether the hand was
	/// on the body, at the last step
	std::optional<glm::vec3> lastPoint;
	glm::vec2 lastCursor {0.0f};
	bool wasOnBody {false};
	/// How fast the hand moved across the land at the last step, in units a second
	float speed {0.0f};
	/// Hard slaps in a row at one height
	creature_feedback::HardSlaps hardSlaps;
	/// The hand's cycle: taking hold starts it caressing
	creature_feedback::TouchCycle cycle {creature_feedback::TouchCycle::Tickle};
	/// How many times the last step's change of cycle started the hand's blend, and how many times in all since taking
	/// hold
	uint8_t blendStarts {0};
	uint32_t cycleBlends {0};
	/// How long the button has been held, and whether the hand has stroked or slapped since, which tell a click on the
	/// creature from a hold
	float heldMs {0.0f};
	bool strokedOrSlapped {false};
	/// The hand was put on the creature by a command (CreatureHandSystemInterface::Stroke or Slap), not by the button
	bool byCommand {false};
};

/// On the player's hand once it has let go of a creature: the sum of strokes and slaps it let go with, which the
/// creature's status panel goes on showing until the hand next takes hold of a creature
struct HandLastFeedback
{
	float sum {0.0f};
};

} // namespace openblack::ecs::components
