/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerPlayerSeen.h"

#include <algorithm>

#include "Common/GUtilsDistance.h"

using openblack::ecs::components::ComputerMind;
using openblack::ecs::components::ComputerMindSeen;
using openblack::map_coords::MapCoords;

namespace openblack::computer_player
{

namespace
{
/// A cell's own reach, added to the mind's
constexpr float k_CellReach = 10.0f;
/// The most cells the look goes over
constexpr uint32_t k_MostCells = 99999;
} // namespace

bool SameSeen(const ComputerMindSeen& a, const ComputerMindSeen& b)
{
	return a.thing == b.thing && a.forest == b.forest;
}

void ForgetSeenBefore(std::vector<ComputerMindSeen>& seen, uint32_t turn, uint32_t old)
{
	std::erase_if(seen, [turn, old](const ComputerMindSeen& entry) { return turn - entry.turn > old; });
}

void ForgetSeenGone(ComputerMind& mind, const SeenWorld& world)
{
	std::erase_if(mind.seen, [&world](const ComputerMindSeen& entry) { return !world.StillThere(entry); });
}

void ScanAroundHand(ComputerMind& mind, const SeenWorld& world, const MapCoords& hand, uint32_t turn)
{
	if (mind.seen.size() > k_SeenMost && turn % k_SeenForgetEvery == 0)
	{
		ForgetSeenBefore(mind.seen, turn, k_SeenOld);
	}
	const float reach = k_CellReach + k_SeenReach;
	MapCoords cursor = hand;
	map_coords::Spiral spiral;
	for (uint32_t steps = k_MostCells; steps != 0; --steps)
	{
		const auto objects = world.CellObjects(map_coords::CellOf(cursor.x), map_coords::CellOf(cursor.z));
		if (objects.has_value())
		{
			// the look ends at the first cell on the land that is not nearer than its reach
			if (!(reach > gutils::GetDistanceInMetres(hand, cursor)))
			{
				return;
			}
			for (const auto object : *objects)
			{
				const auto seen = world.SeenAs(object);
				if (!seen.has_value() ||
				    std::ranges::any_of(mind.seen, [&seen](const ComputerMindSeen& entry) { return SameSeen(entry, *seen); }))
				{
					continue;
				}
				auto entry = *seen;
				entry.turn = turn;
				mind.seen.insert(mind.seen.begin(), entry);
			}
		}
		map_coords::AddCells(cursor, spiral.Next());
	}
}

} // namespace openblack::computer_player
