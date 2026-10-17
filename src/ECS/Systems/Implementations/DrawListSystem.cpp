/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "DrawListSystem.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>

#include "Common/EventManager.h"
#include "ECS/DrawList/LandClip.h"
#include "ECS/Events/DrawListEvents.h"

using namespace openblack::ecs;
using namespace openblack::ecs::systems;

DrawListSystem::DrawListSystem(openblack::EventManager* eventManager)
    : _alive(std::make_shared<char>())
{
	if (eventManager != nullptr)
	{
		eventManager->AddHandler<events::DrawListRebuildRequested>(
		    [this, alive = std::weak_ptr<char>(_alive)](const events::DrawListRebuildRequested& event) {
			    if (!alive.expired())
			    {
				    OnRebuildRequested(event.count);
			    }
		    });
	}
}

void DrawListSystem::OnRebuildRequested(uint8_t count)
{
	_rebuild.count = count;
}

void DrawListSystem::OnClearMap()
{
	_list.entries.clear();
	_list.active.clear();
	_blocks.clear();
}

void DrawListSystem::Update(const DrawListFrame& frame, const DrawListInputs& inputs, const draw_list::Consumer& consumer)
{
	if (inputs.objects == nullptr)
	{
		std::fputs("DrawListSystem::Update: no object probe in the inputs\n", stderr);
		std::abort();
	}
	auto& objects = *inputs.objects;
	// A land's blocks start from their states at its load, and the land's creation places the level-of-detail lines
	if (_blocks.size() != inputs.blocks.size())
	{
		_blocks.clear();
		_blockCoords.clear();
		_blockLookup.fill(0);
		for (size_t i = 0; i < inputs.blocks.size(); ++i)
		{
			const auto& block = inputs.blocks[i];
			_blocks.push_back(block.initial);
			_blockCoords.push_back(block.coords);
			if (block.coords.x >= 0 && block.coords.x < draw_list::k_BlocksPerSide && block.coords.y >= 0 &&
			    block.coords.y < draw_list::k_BlocksPerSide)
			{
				_blockLookup[static_cast<size_t>(block.coords.x) * static_cast<size_t>(draw_list::k_BlocksPerSide) +
				             static_cast<size_t>(block.coords.y)] = static_cast<uint16_t>(i + 1);
			}
		}
		_lodLines = draw_list::LodLinesAtLandCreation(_lodLines, inputs.detailIndex);
	}

	// The lines follow the camera, once a frame before any block is culled
	_lodLines = draw_list::RebuildLodLines(_lodLines, frame.camera.eye, frame.camera.viewDirection);
	_slotOfBlock.resize(inputs.blocks.size());
	for (size_t i = 0; i < inputs.blocks.size(); ++i)
	{
		const auto& block = inputs.blocks[i];
		_blocks[i] = draw_list::NextBlockState(_blocks[i], block.mapPos, block.highestAltitude, frame.camera, _lodLines);
		_slotOfBlock[i] = block.slot;
	}
	draw_list::MarkSeams(_blocks, _blockCoords, _blockLookup);
	draw_list::SortVisible(_blocks, _visible);

	// The land pass: each visible block's land clip. A block whose result is not the one it had the last frame it was
	// visible raises the land flag. A block without its cells, or with no answer, is left as it is
	_landFlag = false;
	for (const uint16_t index : _visible)
	{
		const auto& block = inputs.blocks[index];
		if (block.cells.size() != draw_list::k_BlockCells)
		{
			continue;
		}
		auto& state = _blocks[index];
		const auto keeps = draw_list::KeepsFrontTriangle(block.cells.first<draw_list::k_BlockCells>(), block.mapPos,
		                                                 frame.camera, state.partlyOutside, state.lod, _lodLines);
		if (keeps.has_value() && draw_list::TakeLandClipResult(state, *keeps))
		{
			_landFlag = true;
		}
	}

	if (draw_list::RebuildDue(_rebuild, inputs.turn, _landFlag))
	{
		draw_list::StartRebuild(_rebuild, inputs.turn);
		std::span<const entt::entity> global;
		if (inputs.blockArrays != nullptr)
		{
			for (size_t slot = 0; slot < _slots.size(); ++slot)
			{
				_slots[slot] = inputs.blockArrays->blocks[slot].Objects();
			}
			global = inputs.blockArrays->global.Objects();
		}
		else
		{
			_slots.fill({});
		}
		draw_list::Collect(_visible, _blocks, {.slotOfBlock = _slotOfBlock, .slots = _slots}, global, objects,
		                   draw_list::k_VanishObjectDistance, _list);
	}

	// Each Draw the pass calls is recorded before it runs
	_drawn.clear();
	const draw_list::Consumer recorded = [this, &consumer](entt::entity object) {
		_drawn.push_back(object);
		return consumer(object);
	};
	_lastPassFull = draw_list::TakeFullPass(_rebuild, frame.camera.eye, frame.camera.focus);
	if (_lastPassFull)
	{
		draw_list::FullPass(_list, objects, recorded);
	}
	else
	{
		draw_list::StillPass(_list, objects, recorded);
	}
}

size_t DrawListSystem::Count() const
{
	return _list.entries.size();
}

uint8_t DrawListSystem::RebuildCount() const
{
	return _rebuild.count;
}

uint32_t DrawListSystem::RebuildTurn() const
{
	return _rebuild.lastTurn;
}

bool DrawListSystem::LastPassFull() const
{
	return _lastPassFull;
}

bool DrawListSystem::LandFlag() const
{
	return _landFlag;
}

std::span<const uint16_t> DrawListSystem::VisibleBlocks() const
{
	return _visible;
}

std::span<const entt::entity> DrawListSystem::Entries() const
{
	return _list.entries;
}

draw_list::Active DrawListSystem::ActiveAt(size_t index) const
{
	return index < _list.active.size() ? _list.active[index] : draw_list::Active::No;
}

std::span<const entt::entity> DrawListSystem::DrawnThisFrame() const
{
	return _drawn;
}

std::optional<draw_list::Active> DrawListSystem::ActiveOf(entt::entity object) const
{
	if (object == entt::null)
	{
		return std::nullopt;
	}
	const auto found = std::find(_list.entries.begin(), _list.entries.end(), object);
	if (found == _list.entries.end())
	{
		return std::nullopt;
	}
	return ActiveAt(static_cast<size_t>(found - _list.entries.begin()));
}
