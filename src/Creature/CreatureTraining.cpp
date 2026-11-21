/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTraining.h"

#include <algorithm>

using namespace openblack;

void creature_training::CountCarriedOut(std::map<uint32_t, uint32_t>& counts, uint32_t action, bool controlledByScript)
{
	if (!controlledByScript)
	{
		++counts[action];
	}
}

uint32_t creature_training::TimesCarriedOut(const std::map<uint32_t, uint32_t>& counts, uint32_t action)
{
	const auto found = counts.find(action);
	return found != counts.end() ? found->second : 0;
}

bool creature_training::CountedAtItsAgendaEnd(uint32_t action)
{
	return action != k_FightAction;
}

std::optional<entt::entity> creature_training::HighlightToPointOut(const map_coords::MapCoords& from,
                                                                   const HighlightsInCell& inCell)
{
	const auto met = HighlightsToPointOut(from, inCell);
	return met.empty() ? std::nullopt : std::optional(met.back());
}

std::vector<entt::entity> creature_training::HighlightsToPointOut(const map_coords::MapCoords& from,
                                                                  const HighlightsInCell& inCell)
{
	std::vector<entt::entity> met;
	auto coords = from;
	map_coords::Spiral spiral;
	for (int32_t cells = k_PointOutCells; cells != 0; --cells)
	{
		if (map_coords::InBounds(coords))
		{
			const auto highlights = inCell(map_coords::Cell(coords));
			const auto found =
			    std::ranges::find_if(highlights, [](const HighlightInCell& highlight) { return !highlight.tipSign; });
			if (found != highlights.end())
			{
				met.push_back(found->entity);
			}
		}
		map_coords::AddCells(coords, spiral.Next());
	}
	return met;
}
