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

#include <array>
#include <memory>

#include <entt/entity/fwd.hpp>

#include "3D/MapCoords.h"
#include "ECS/DanceMoves.h"
#include "Enums.h"

namespace openblack::dance
{
struct DanceFile;
}

namespace openblack::ecs::systems
{

/// The dances: made by the scripts, the towns and the worship sites, each going on a turn at a time
class DanceSystemInterface
{
public:
	virtual ~DanceSystemInterface() = default;

	/// A dance of a row of the dances' table about a map position, for a thing, dancing `duration` turns at a time (none for
	/// no end); none when the row is out of the table. A row whose file can't be opened makes a dance of one group and
	/// no key frames. Worship sites', towns' and scripts' dances are all made here. Every row but the worship dance and
	/// the worship sites' dances has a footpath laid to it from the storage pit of `town`, or with no town given of the
	/// town nearest the dance, however far (none when there is no such town or it has no storage pit): a script's and a
	/// creature's dance give none.
	virtual entt::entity Create(DanceInfo type, const map_coords::MapCoords& place, entt::entity owner, uint32_t duration,
	                            bool madeByScript, entt::entity town) = 0;
	/// The file of a row of the dances' table, read once; none for a row out of the table or a file that can't be opened
	[[nodiscard]] virtual std::shared_ptr<const dance::DanceFile> FileOf(DanceInfo /*type*/) { return nullptr; }
	/// A dance goes, its dancers going back to deciding what to do
	virtual void Destroy(entt::entity dance) = 0;
	/// Each turn, after the players and before the living: each dance's turn
	virtual void ProcessTurn() = 0;
	/// The shapes the dances' groups stand in, with those the game reads from its data folder
	[[nodiscard]] virtual const std::array<dance_shapes::Shape, dance_shapes::k_ShapeCount>& Shapes()
	{
		return dance_moves::Shapes();
	}
};

} // namespace openblack::ecs::systems
