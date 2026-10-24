/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTownTheft.h"

#include <limits>

#include "Creature/CreatureTownHelp.h"

using namespace openblack;
using namespace openblack::creature_town_theft;
using creature_mind::Gaze;
using creature_mind::Movement;
using creature_mind::ObjectOrder;

bool creature_town_theft::IsOwnStoragePit(bool built, std::optional<uint32_t> pitPlayer, std::optional<uint32_t> creaturePlayer)
{
	return built && pitPlayer == creaturePlayer;
}

bool creature_town_theft::IsOthersStoragePit(bool built, std::optional<uint32_t> pitPlayer,
                                             std::optional<uint32_t> creaturePlayer)
{
	return built && pitPlayer != creaturePlayer;
}

bool creature_town_theft::IsOthersField(std::optional<uint32_t> fieldPlayer, std::optional<uint32_t> creaturePlayer)
{
	return fieldPlayer != creaturePlayer;
}

std::optional<size_t> creature_town_theft::NearestTown(std::span<const float> metres)
{
	std::optional<size_t> nearest;
	float nearestMetres = std::numeric_limits<float>::max();
	for (size_t i = 0; i < metres.size(); ++i)
	{
		if (metres[i] < nearestMetres)
		{
			nearest = i;
			nearestMetres = metres[i];
		}
	}
	return nearest;
}

std::vector<Step> creature_town_theft::StealAndPutInTown(std::optional<uint32_t> pickUp, uint32_t pit, float height)
{
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PickUp, .object = *pickUp},
		                  .gaze = Gaze {.object = *pickUp, .goingTowards = true}});
	}
	agenda.push_back(
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = pit, .maxDistance = height * k_PutInTownHeights},
	     .gaze = Gaze {.object = pit, .goingTowards = true}});
	// It only puts the thing down, with the put-down clip
	agenda.push_back({.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::PutDown}});
	return agenda;
}

PotInfo creature_town_theft::StolenHandful(bool wood)
{
	return wood ? PotInfo::TreeLogsInHand : PotInfo::WheatInHand;
}

std::vector<Step> creature_town_theft::StealFromStore(uint32_t from, glm::vec2 fromPoint, uint32_t into, bool wood,
                                                      float height)
{
	auto agenda = creature_town_help::GiveHandfulToStore(
	    creature_town_help::HandfulSource {.thing = from, .point = fromPoint, .within = 0.0f, .look = StolenHandful(wood)},
	    into, height);
	// The walk to the pit: right up to it, looking at it as it goes
	agenda.front() = {.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::GoNearObject, .object = from, .maxDistance = 0.0f},
	                  .gaze = Gaze {.object = from, .goingTowards = true}};
	return agenda;
}
