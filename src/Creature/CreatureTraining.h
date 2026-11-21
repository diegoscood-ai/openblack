/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <map>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "3D/MapCoords.h"

/// The rules the creature's training scripts read and steer the creature by, free of the game so that tests can check
/// them: how often it has done each action, and which lesson highlight it points out.
namespace openblack::creature_training
{

/// Each action the creature carries out to its end counts once, unless a script controls the creature then
void CountCarriedOut(std::map<uint32_t, uint32_t>& counts, uint32_t action, bool controlledByScript);
/// How many times the creature has carried out an action
[[nodiscard]] uint32_t TimesCarriedOut(const std::map<uint32_t, uint32_t>& counts, uint32_t action);

/// The fight action, by the game's numbering
constexpr uint32_t k_FightAction = 21;
/// Whether a plan carried out to the end of its agenda counts its action there: every action but the fight, which
/// counts once the creature that set out to fight is found the stronger after it, its own fight system telling the mind
[[nodiscard]] bool CountedAtItsAgendaEnd(uint32_t action);

/// The creature points out a lesson highlight from the 81 cells in a spiral out from its own
constexpr int32_t k_PointOutCells = 81;

/// A highlight standing in a cell, and whether it is a sign giving a tip rather than a scroll
struct HighlightInCell
{
	entt::entity entity {entt::null};
	bool tipSign {false};
};
/// The highlights in a cell of the map, in the order the cell keeps them
using HighlightsInCell = std::function<std::vector<HighlightInCell>(glm::ivec2 cell)>;

/// The highlights the creature is told to point out, in the order the walk meets them: in each cell of the spiral the
/// first highlight that isn't a tip sign
[[nodiscard]] std::vector<entt::entity> HighlightsToPointOut(const map_coords::MapCoords& from, const HighlightsInCell& inCell);
/// The highlight the creature ends up pointing out: the last of those the walk meets, as each one met replaces the one
/// before
[[nodiscard]] std::optional<entt::entity> HighlightToPointOut(const map_coords::MapCoords& from,
                                                              const HighlightsInCell& inCell);

} // namespace openblack::creature_training
