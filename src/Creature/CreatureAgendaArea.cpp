/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureAgendaArea.h"

#include <array>
#include <utility>

#include "Creature/LeashRules.h"

using namespace openblack;
using creature_mind::Movement;
using creature_mind::Step;

bool creature_agenda_area::LeavesArea(const Step& step, glm::vec2 creature, glm::vec2 keptAt, float radius)
{
	if (step.kind != Step::Kind::Move)
	{
		return false;
	}
	switch (step.movement.kind)
	{
	case Movement::Kind::ToPoint:
		return creature_leash::LeavesKeptArea(std::array {step.movement.point}, keptAt, radius);
	case Movement::Kind::Nearby:
		return creature_leash::LeavesKeptArea(std::array {creature + step.movement.point}, keptAt, radius);
	default:
		return false;
	}
}

std::vector<Step> creature_agenda_area::WithoutWalksOut(std::vector<Step> agenda, glm::vec2 creature, glm::vec2 keptAt,
                                                        float radius)
{
	std::vector<Step> kept;
	kept.reserve(agenda.size());
	for (auto& step : agenda)
	{
		if (!LeavesArea(step, creature, keptAt, radius))
		{
			kept.push_back(std::move(step));
			continue;
		}
		// The main step is the last step the agenda has as it gets its main step
		if (step.main && !kept.empty())
		{
			kept.back().main = true;
		}
	}
	return kept;
}
