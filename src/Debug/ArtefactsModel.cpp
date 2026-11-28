/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ArtefactsModel.h"

#include <algorithm>
#include <iterator>

#include <fmt/format.h>

#include "ECS/ArtefactRules.h"

using namespace openblack;
using namespace openblack::debug;
using namespace openblack::debug::artefacts;

std::string_view artefacts::LookOf(float value)
{
	if (ecs::artefact_rules::ShowsSymbol(value))
	{
		return "glows, symbol";
	}
	return ecs::artefact_rules::Glows(value) ? "glows" : "plain";
}

std::string artefacts::WhereOf(const ArtefactAt& artefact)
{
	if (artefact.inTown)
	{
		return fmt::format("town {}", entt::to_integral(artefact.town));
	}
	if (artefact.inSite)
	{
		return fmt::format("site {}", entt::to_integral(artefact.site));
	}
	return "none";
}

std::vector<Row> artefacts::Rows(std::span<const ArtefactAt> artefacts)
{
	std::vector<Row> rows;
	rows.reserve(artefacts.size());
	std::ranges::transform(artefacts, std::back_inserter(rows), [](const ArtefactAt& artefact) {
		auto row = Row {
		    .entity = artefact.entity,
		    .where = WhereOf(artefact),
		    .owner = artefact.player.has_value() ? fmt::format("{}", static_cast<int>(*artefact.player)) : "none",
		    .value = artefact.value,
		    .look = LookOf(artefact.value),
		    .dance = !artefact.dancers.has_value() ? "none"
		             : artefact.dancing            ? fmt::format("dancing, {}", *artefact.dancers)
		                                           : fmt::format("waiting, {}", *artefact.dancers),
		};
		if (artefact.dancers.has_value() && artefact.footpaths != 0)
		{
			row.dance += fmt::format(", {} footpath{}", artefact.footpaths, artefact.footpaths == 1 ? "" : "s");
		}
		return row;
	});
	std::ranges::sort(rows, {}, [](const Row& row) { return entt::to_integral(row.entity); });
	return rows;
}
