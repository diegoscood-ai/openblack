/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>

#include <LNDFile.h>
#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/DrawList/BlockCull.h"
#include "ECS/DrawList/GameBlocks.h"
#include "ECS/DrawList/ObjectList.h"

namespace openblack::ecs::systems
{
/// One land block as the draw list reads it on a frame
struct DrawListBlock
{
	/// The block's map position, its lowest corner
	glm::vec2 mapPos {};
	/// The block's highest altitude, which sets its box's height
	int32_t highestAltitude {};
	/// The block's slot in the block table, (block x << 5) + block z
	uint16_t slot {};
	/// The block's state at the land's load, before its first frame (draw_list::InitialBlockState)
	draw_list::BlockState initial {};
	/// The block's place on the block grid, (block x, block z), which the seam pass looks up its neighbours by
	glm::ivec2 coords {};
	/// The block's 17 x 17 cell records as the land holds them now (draw_list::k_BlockCells). Without them the land
	/// clip gives no answer for the block
	std::span<const lnd::LNDCell> cells;
};

/// The drawn frame the list runs for
struct DrawListFrame
{
	draw_list::DrawCamera camera;
};

/// What the list reads besides the camera
struct DrawListInputs
{
	/// The land's blocks, in the land's order
	std::span<const DrawListBlock> blocks;
	/// Every block's objects and those outside every block, as the map filed them; null for none
	const draw_list::GameBlocks* blockArrays {nullptr};
	/// The game turn
	uint32_t turn {};
	/// The objects' availability, DontDraw mark, 3D object kind and listed mark, which a rebuild writes. Update stops
	/// with a message without it
	draw_list::EntityProbe* objects {nullptr};
	/// The detail index the land was made with, 0..4 (draw_list::DetailIndexOf): it places the level-of-detail lines
	/// when a land's blocks start (draw_list::LodLinesAtLandCreation)
	int32_t detailIndex {4};
};

/// The original's object draw list for the whole game: which objects have their Draw run on each drawn frame, and the
/// rebuild requests. The objects keep their own listed marks (DrawListInputs::objects). Writers ask for rebuilds
/// through events::DrawListRebuildRequested, never through this. docs/bw1-notes/original-frame.md §6
class DrawListSystemInterface
{
public:
	virtual ~DrawListSystemInterface() = default;

	/// The number of rebuilds still asked for becomes `count` (a store: the last request wins)
	virtual void OnRebuildRequested(uint8_t count) = 0;
	/// The map is cleared: the list is emptied, and the objects keep their listed marks. The land's blocks are made
	/// again after it, so at the next Update every block starts again from its state at the land's load
	/// (DrawListBlock::initial), and the level-of-detail lines are placed as the land's creation places them. The
	/// rebuild requests and the last rebuild's turn are not changed
	virtual void OnClearMap() = 0;
	/// One drawn frame: the level-of-detail lines rebuilt from the camera; each block's cull, distance and level of
	/// detail, then the seams and the visible blocks' order; the land clip of each visible block, nearest first, any
	/// change in a block's result raising the land flag; a rebuild if one is due; then the full or the still pass,
	/// which calls `consumer` for each entry it draws, in list order
	virtual void Update(const DrawListFrame& frame, const DrawListInputs& inputs, const draw_list::Consumer& consumer) = 0;

	/// The entries in the list, the nulled ones included
	[[nodiscard]] virtual size_t Count() const = 0;
	/// The rebuilds still asked for
	[[nodiscard]] virtual uint8_t RebuildCount() const = 0;
	/// The turn of the last rebuild
	[[nodiscard]] virtual uint32_t RebuildTurn() const = 0;
	/// The last Update ran the full pass (false before the first)
	[[nodiscard]] virtual bool LastPassFull() const = 0;
	/// The land flag at the last Update: some visible block's land clip gave another result than the last frame it was
	/// visible
	[[nodiscard]] virtual bool LandFlag() const = 0;
	/// The visible blocks of the last Update, as indices into its blocks, nearest first
	[[nodiscard]] virtual std::span<const uint16_t> VisibleBlocks() const = 0;
	/// The entries, null for one a pass nulled
	[[nodiscard]] virtual std::span<const entt::entity> Entries() const = 0;
	/// What the last full pass found for the entry at `index` (No past the end)
	[[nodiscard]] virtual draw_list::Active ActiveAt(size_t index) const = 0;
	/// What the last full pass found for the object; none when it is not in the list
	[[nodiscard]] virtual std::optional<draw_list::Active> ActiveOf(entt::entity object) const = 0;
	/// The objects whose Draw the last Update called, in the order it called them (empty before the first). A frame
	/// that does not run the list leaves the last one's
	[[nodiscard]] virtual std::span<const entt::entity> DrawnThisFrame() const = 0;
};
} // namespace openblack::ecs::systems
