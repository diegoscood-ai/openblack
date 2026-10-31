/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandFight.h"

#include "Common/GUtilsX87.h"
#include "Creature/CreatureFight.h"

using namespace openblack;

std::optional<uint32_t> ecs::hand_fight::BodyMove(Body body, float heightShare)
{
	switch (body)
	{
	case Body::Own:
		return 3u;
	case Body::Opponent:
		switch (creature_fight::BandOf(heightShare))
		{
		case creature_fight::Band::High:
			return 0u;
		case creature_fight::Band::Mid:
			return 1u;
		case creature_fight::Band::Low:
			return 2u;
		}
		break;
	case Body::Other:
		break;
	}
	return std::nullopt;
}

bool ecs::hand_fight::InsideArena(glm::vec2 centre, float radius, glm::vec2 point)
{
	namespace x87 = gutils::x87;
	const double across = x87::Sub(point.x, centre.x);
	const double along = x87::Sub(point.y, centre.y);
	const double distance = x87::Sqrt(x87::Add(x87::Mul(across, across), x87::Mul(along, along)));
	// a radius at or within the distance is outside; a distance that is not a number is not
	return !(static_cast<double>(radius) <= distance);
}
