/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "RenderFrameSystem.h"

#include <utility>

using namespace openblack;
using namespace openblack::ecs::systems;

void RenderFrameSystem::SetLandCells(land_light::Cells cells)
{
	_cells = std::move(cells);
}

void RenderFrameSystem::RestoreLandCells()
{
	_cells.cells = _cells.base;
	_cells.undrawn = _cells.base;
	_cells.drawn.clear();
}

void RenderFrameSystem::SetLandLuminosity(const std::vector<uint8_t>& luminosity, const std::vector<uint8_t>& undrawn)
{
	const auto put = [](std::vector<uint32_t>& cells, const std::vector<uint8_t>& bytes) {
		for (size_t i = 0; i < cells.size() && i < bytes.size(); ++i)
		{
			cells[i] = (cells[i] & 0x00FFFFFFu) | static_cast<uint32_t>(bytes[i]) << 24u;
		}
	};
	put(_cells.cells, luminosity);
	put(_cells.undrawn, undrawn);
}

void RenderFrameSystem::AddLandStamp(const land_light::Stamp& stamp)
{
	_stamps.push_back(stamp);
}

void RenderFrameSystem::ApplyLandStamps(std::span<const uint8_t> drawnBlocks)
{
	// the blocks whose copies of the cells take the stamps; the others' copies (undrawn) stay as they were
	_cells.drawn.assign(drawnBlocks.begin(), drawnBlocks.end());
	if (_stamps.empty())
	{
		return;
	}
	const auto stamped = land_light::StampedCells(_cells, drawnBlocks);
	for (const auto& stamp : _stamps)
	{
		land_light::ApplyStamp(stamp, _cells.firstCell, _cells.size, stamped, _cells.cells);
	}
}

void RenderFrameSystem::ClearLandStamps() noexcept
{
	_stamps.clear();
	++_stampFrame;
}

void RenderFrameSystem::SubmitMist(const mists::MistDesc& mist)
{
	_submittedMists.push_back(mist);
}

std::vector<mists::MistDesc> RenderFrameSystem::TakeSubmittedMists()
{
	auto submitted = std::move(_submittedMists);
	_submittedMists.clear();
	return submitted;
}
