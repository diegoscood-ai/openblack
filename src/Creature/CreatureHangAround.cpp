/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHangAround.h"

#include <cmath>

#include <algorithm>

#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_hang_around;
using creature_face::Cue;
using creature_mind::Gaze;
using creature_mind::Movement;
using creature_mind::ObjectOrder;
using creature_mind::Step;
namespace animations = openblack::creature_layers::animations;

namespace
{
Step WalkTo(glm::vec2 point, float within)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::ToPoint, .point = point, .maxDistance = within},
	        .gaze = Gaze {.point = glm::vec3 {point.x, 0.0f, point.y}, .goingTowards = true}};
}

Step TurnTo(glm::vec3 point)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::TurnToFace, .point = {point.x, point.z}},
	        .gaze = Gaze {.point = point}};
}

/// Sitting a while, the main step
Step Sit(const creature_mind::Random& random)
{
	return {.kind = Step::Kind::Static,
	        .seconds = k_SitSeconds + creature_emote::FloatRandom(random, k_SitExtraSeconds),
	        .animation = animations::k_Sit,
	        .sequence = {animations::k_StartSit, animations::k_Sit, animations::k_EndSit},
	        .mirrorAtRandom = true,
	        .main = true};
}

/// A tired yawn with sleepy eyes, the main step
Step Yawn()
{
	return {.kind = Step::Kind::Action, .animation = animations::k_Tired, .sleepyEyes = true, .face = Cue::Idle, .main = true};
}
} // namespace

glm::vec3 creature_hang_around::WayOut(glm::vec2 home, glm::vec3 templeHeart)
{
	const glm::vec3 away = glm::vec3(home.x, 0.0f, home.y) - templeHeart;
	if (std::abs(away.x) <= k_SameSpot && std::abs(away.z) <= k_SameSpot)
	{
		return {1.0f, 0.0f, 0.0f};
	}
	const float inverse = 1.0f / std::sqrt((away.x * away.x) + (away.y * away.y) + (away.z * away.z));
	return {away.x * inverse, away.y * inverse, away.z * inverse};
}

std::vector<Step> creature_hang_around::Agenda(const Places& places, const creature_mind::Random& random)
{
	std::vector<Step> agenda;
	agenda.push_back(WalkTo(places.home, std::min(k_HomeReach, places.height)));
	agenda.push_back({.kind = Step::Kind::Wait,
	                  .seconds = creature_emote::WholeTurnSeconds(
	                      k_WaitSeconds + creature_emote::FloatRandom(random, k_WaitExtraSeconds), places.turnsPerSecond),
	                  .face = Cue::Idle});
	if (!places.templeHeart.has_value())
	{
		agenda.push_back(Yawn());
		return agenda;
	}
	const auto heart = *places.templeHeart;
	const auto lot = random(k_Lots);
	if (lot == 0)
	{
		agenda.push_back(Yawn());
		return agenda;
	}
	if (lot <= k_LastOutLot)
	{
		const glm::vec3 home(places.home.x, 0.0f, places.home.y);
		const auto way = WayOut(places.home, heart);
		const auto out = home + (way * (places.confinementRadius * k_OutShare));
		const auto view = home + (way * k_ViewDistance);
		agenda.push_back(WalkTo({out.x, out.z}, k_OutReach));
		agenda.push_back(TurnTo(view));
		if (random(2) == 0)
		{
			agenda.push_back(Sit(random));
		}
		else
		{
			agenda.push_back({.kind = Step::Kind::Object,
			                  .order = {.kind = ObjectOrder::Kind::PointAt, .point = {view.x, view.z}, .pointHeight = view.y},
			                  .face = Cue::Amazed,
			                  .gaze = Gaze {.point = view},
			                  .main = true});
		}
	}
	// Out from home and back, or in the last lots straight away, it turns to the temple and sits
	agenda.push_back(TurnTo(heart));
	agenda.push_back(Sit(random));
	return agenda;
}
