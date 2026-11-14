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

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace openblack::ecs::components
{

/// What keeps a cell of the land from being clear for a creature
enum class ClearCellBlock : uint8_t
{
	/// Not land a creature may stand on
	NotLand,
	/// A temple's heart, which takes its cell outright
	TempleHeart,
	/// A thing it must go round, by the shape it keeps clear
	Shape,
	/// Asked for a place with nothing living in it: a living thing standing there
	Living,
};

/// The thing that keeps a cell from being clear, and why; no thing for land it may not stand on
struct ClearCellBlocker
{
	ClearCellBlock why {ClearCellBlock::NotLand};
	entt::entity by {entt::null};
};

/// The debug tools' readout of a creature's last search for a clear area. Only the debug tools give a creature this
/// component, and nothing in the game reads it, so a creature without it searches exactly as with it
struct CreatureClearAreaReadout
{
	struct BlockedCell
	{
		glm::ivec2 cell;
		ClearCellBlocker blocker;
	};
	struct Search
	{
		/// The turn it searched at, how wide an area it looked for, and the point it looked about
		uint32_t turn {0};
		float width {0.0f};
		glm::vec2 point {0.0f};
		/// Whether nothing living might stand in the area
		bool nothingLiving {false};
		/// The middle of the nearest clear area, and what the search gave once an area that keeps the creature had its
		/// say
		std::optional<glm::vec2> found;
		std::optional<glm::vec2> result;
		/// Every cell of the window it looked through that was not clear, in the order it looked at them
		std::vector<BlockedCell> blocked;
	};
	/// The last search. The search reads the world only, so it writes this through the read-only view it is given
	mutable std::optional<Search> last;
};

} // namespace openblack::ecs::components
