/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandClickRules.h"

#include "Common/GUtilsDistance.h"

using namespace openblack;
using openblack::ecs::components::HandClicked;

bool hand_click::IsForgotten(uint32_t turn, uint32_t clickedTurn, uint32_t millisecondsPerTurn)
{
	// The turns times the turn's length, then the milliseconds in seconds: 150 turns of 100 ms come to 15.0000010,
	// which is more than 15
	constexpr float k_SecondsPerMillisecond = 0.001f;
	const float seconds =
	    static_cast<float>(turn - clickedTurn) * static_cast<float>(millisecondsPerTurn) * k_SecondsPerMillisecond;
	return seconds > k_ForgetSeconds;
}

void hand_click::Forget(HandClicked& clicked, uint32_t turn, uint32_t millisecondsPerTurn)
{
	if (IsForgotten(turn, clicked.placeTurn, millisecondsPerTurn))
	{
		ClearPlace(clicked);
	}
	if (IsForgotten(turn, clicked.thingTurn, millisecondsPerTurn))
	{
		ClearThing(clicked);
	}
}

bool hand_click::MarksThing(bool holdsLooseLeash, bool isReward)
{
	return !(holdsLooseLeash && isReward);
}

void hand_click::ClickThing(HandClicked& clicked, entt::entity thing, uint32_t turn)
{
	clicked.thing = thing;
	clicked.thingTurn = turn;
}

void hand_click::ClickPlace(HandClicked& clicked, const map_coords::MapCoords& place, uint32_t turn)
{
	// The thing is forgotten, though its turn is kept
	clicked.thing = entt::null;
	clicked.place = place;
	clicked.placeTurn = turn;
}

void hand_click::ClickReleased(HandClicked& clicked, std::optional<entt::entity> under, bool marks,
                               const std::optional<map_coords::MapCoords>& place, uint32_t turn)
{
	if (under.has_value() && marks)
	{
		ClickThing(clicked, *under, turn);
		return;
	}
	ClickPlace(clicked, place.value_or(map_coords::MapCoords {}), turn);
}

bool hand_click::IsThingClicked(const HandClicked& clicked, entt::entity thing)
{
	return thing != entt::null && clicked.thing == thing;
}

bool hand_click::IsPlaceClicked(const HandClicked& clicked, const map_coords::MapCoords& position, float radius)
{
	return gutils::GetDistanceInMetres(clicked.place, position) <= radius;
}

void hand_click::ClearThing(HandClicked& clicked)
{
	clicked.thing = entt::null;
}

void hand_click::ClearPlace(HandClicked& clicked)
{
	// The turn is kept
	clicked.place = {};
}
