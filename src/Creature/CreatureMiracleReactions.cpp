/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMiracleReactions.h"

#include "Creature/CreatureLocomotion.h"

using namespace openblack::creature_mind;

namespace
{
/// Turning to the place and pointing at it, the head is on the place, on the ground at the level of the sea
Gaze AtPlace(glm::vec2 point)
{
	return Gaze {.point = glm::vec3 {point.x, 0.0f, point.y}};
}

Step PointAt(glm::vec2 point, float seconds)
{
	return {.kind = Step::Kind::Object,
	        .order = {.kind = ObjectOrder::Kind::PointAt, .point = point, .seconds = seconds},
	        .face = openblack::creature_face::Cue::Amazed,
	        .gaze = AtPlace(point)};
}

Step TurnToFace(glm::vec2 point)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::TurnToFace, .point = point},
	        .face = openblack::creature_face::Cue::Amazed,
	        .gaze = AtPlace(point)};
}
} // namespace

std::optional<std::vector<Step>> openblack::creature_mind::RunAwayFromMiracle(glm::vec2 point, float height,
                                                                              const Random& random,
                                                                              const RunAwayPicker& pickRunAwayPoint)
{
	std::vector<Step> agenda;
	if (random(2) == 0)
	{
		// startled, with the frightened look: wide eyes, looking about
		agenda.push_back({.kind = Step::Kind::Action,
		                  .animation = k_FrightenedAnimation,
		                  .face = creature_face::Cue::Amazed,
		                  .heldEyes = Eyes::Wide,
		                  .frightenedLook = true});
	}
	// The point it runs to is picked now, as the plan is made: how much further than its species' distance it goes is
	// drawn after the start of fright
	const auto extra = random(creature_locomotion::k_RunAwayExtra);
	const auto runTo = pickRunAwayPoint ? pickRunAwayPoint(point, extra) : std::nullopt;
	if (!runTo.has_value())
	{
		return std::nullopt;
	}
	// the run is the main step, a walk to the point at the run speed; on the way its head has the look of a walk to
	// the point, read from where the walk is going
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToPoint,
	                               .point = *runTo,
	                               .run = true,
	                               .minDistance = 0.0f,
	                               .maxDistance = k_MiracleApproachHeights * height},
	                  .gaze = Gaze {.goingTowards = true, .towardsWalkEnd = true},
	                  .main = true});
	return agenda;
}

std::vector<Step> openblack::creature_mind::ExamineMiracle(glm::vec2 point, float height, const Random& random)
{
	std::vector<Step> agenda;
	const bool first = random(2) == 0;
	if (first)
	{
		agenda.push_back(TurnToFace(point));
		agenda.push_back(PointAt(point, k_FirstPointSeconds));
	}
	// On its way there its head has the look of a walk to the place
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToPoint,
	                               .point = point,
	                               .minDistance = 0.0f,
	                               .maxDistance = k_MiracleApproachHeights * height},
	                  .gaze = Gaze {.point = glm::vec3 {point.x, 0.0f, point.y}, .goingTowards = true}});
	agenda.push_back(TurnToFace(point));
	// the last step is the main one: puzzled when it pointed first, else the point
	if (first)
	{
		agenda.push_back(
		    {.kind = Step::Kind::Wait, .seconds = k_PuzzledSeconds, .face = creature_face::Cue::Puzzled, .main = true});
	}
	else
	{
		auto last = PointAt(point, k_LastPointSeconds);
		last.main = true;
		agenda.push_back(last);
	}
	return agenda;
}
