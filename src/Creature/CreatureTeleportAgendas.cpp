/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTeleportAgendas.h"

#include "3D/MapCoords.h"

using namespace openblack::creature_mind;

namespace
{
/// A walk to within reach of a point, its head on the point, pulling a face. The point is kept as a map position keeps it
Step WalkTo(glm::vec2 point, openblack::creature_face::Cue face)
{
	namespace map_coords = openblack::map_coords;
	const glm::vec2 kept {map_coords::Quantise(point.x), map_coords::Quantise(point.y)};
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::ToPoint, .point = kept, .maxDistance = k_TeleportWalkWithin},
	        .face = face,
	        .gaze = Gaze {.point = glm::vec3 {kept.x, 0.0f, kept.y}}};
}
} // namespace

std::vector<Step> openblack::creature_mind::GoToTeleport(uint32_t stone, glm::vec2 stonePoint)
{
	return {WalkTo(stonePoint, creature_face::Cue::Puzzled), {.kind = Step::Kind::Teleport, .object = stone, .main = true}};
}

std::vector<Step> openblack::creature_mind::AfterTeleport(glm::vec2 arrival, std::optional<glm::vec2> destination)
{
	const glm::vec2 kept {map_coords::Quantise(arrival.x), map_coords::Quantise(arrival.y)};
	std::vector<Step> agenda {{.kind = Step::Kind::Transported, .movement = {.kind = Movement::Kind::ToPoint, .point = kept}}};
	if (destination.has_value())
	{
		agenda.push_back(WalkTo(*destination, creature_face::Cue::Smile));
	}
	agenda.push_back({.kind = Step::Kind::StopTeleportControl});
	return agenda;
}

uint16_t openblack::creature_mind::CarryTurns(uint32_t turnsPerSecond)
{
	return static_cast<uint16_t>(map_coords::FtoL(static_cast<float>(turnsPerSecond) * k_CarrySeconds));
}

CarryTurn openblack::creature_mind::StepCarry(uint16_t& turns, bool& fadingIn, uint32_t turnsPerSecond)
{
	const auto share = 1.0f / static_cast<float>(CarryTurns(turnsPerSecond));
	--turns;
	if (!fadingIn)
	{
		if (turns != 0)
		{
			return {.fizz = 1.0f - share * static_cast<float>(static_cast<int16_t>(turns))};
		}
		turns = CarryTurns(turnsPerSecond);
		fadingIn = true;
		return {.arrive = true};
	}
	if (turns != 0)
	{
		return {.fizz = share * static_cast<float>(static_cast<int16_t>(turns))};
	}
	return {.fizz = 0.0f, .done = true};
}
