/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ObjectDrawListSystem.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>

#include "Common/EventManager.h"
#include "ECS/DrawList/LandClip.h"
#include "ECS/Events/DrawListEvents.h"

using namespace openblack::ecs;
using namespace openblack::ecs::systems;

ObjectDrawListSystem::ObjectDrawListSystem(openblack::EventManager* eventManager)
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

void ObjectDrawListSystem::OnRebuildRequested(uint8_t count)
{
	_rebuild.count = count;
}

void ObjectDrawListSystem::OnClearMap()
{
	_list.entries.clear();
	_list.active.clear();
	_blocks.clear();
	_drawnBlocks.fill(0);
}

void ObjectDrawListSystem::Update(const DrawListFrame& frame, const DrawListInputs& inputs, const draw_list::Consumer& consumer)
{
	if (inputs.objects == nullptr)
	{
		std::fputs("ObjectDrawListSystem::Update: no object probe in the inputs\n", stderr);
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
		_textureLine = draw_list::TextureLineAtLandCreation(_textureLine, inputs.detailIndex, draw_list::k_TexturePoolCount);
	}

	// The lines follow the camera, once a frame before any block is culled
	_lodLines = draw_list::RebuildLodLines(_lodLines, frame.camera.eye, frame.camera.viewDirection);
	_textureLine = draw_list::RebuildTextureLine(_textureLine, frame.camera.eye, frame.camera.viewDirection);
	_slotOfBlock.resize(inputs.blocks.size());
	for (size_t i = 0; i < inputs.blocks.size(); ++i)
	{
		const auto& block = inputs.blocks[i];
		_blocks[i] = draw_list::NextBlockState(_blocks[i], block.mapPos, block.highestAltitude, frame.camera, _lodLines);
		_slotOfBlock[i] = block.slot;
	}
	_drawnBlocks = draw_list::DrawnBlocks(_blocks, _slotOfBlock);
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
	// The blocks whose texture the land's draw asks for, nearest first: the visible ones with a land triangle left
	// whose texture shows, at its own level or under its tile (a block beyond the line shows its tile only and is never
	// asked for)
	_textureRequests.clear();
	for (const uint16_t index : _visible)
	{
		const auto& state = _blocks[index];
		if ((state.visibility & draw_list::k_InViewBit) != 0 &&
		    draw_list::BlockTextureLevel(inputs.blocks[index].mapPos, _textureLine, false) != draw_list::TextureLevel::TileOnly)
		{
			_textureRequests.push_back(index);
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
	_onScreen.clear();
	const draw_list::Consumer recorded = [this, &consumer](entt::entity object) {
		_drawn.push_back(object);
		const auto onScreen = consumer(object);
		// the on-screen flag is set before every Draw, so one with no ported Draw keeps it
		if (onScreen.value_or(true))
		{
			_onScreen.push_back(object);
		}
		return onScreen;
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

size_t ObjectDrawListSystem::Count() const
{
	return _list.entries.size();
}

uint8_t ObjectDrawListSystem::RebuildCount() const
{
	return _rebuild.count;
}

uint32_t ObjectDrawListSystem::RebuildTurn() const
{
	return _rebuild.lastTurn;
}

bool ObjectDrawListSystem::LastPassFull() const
{
	return _lastPassFull;
}

bool ObjectDrawListSystem::LandFlag() const
{
	return _landFlag;
}

std::span<const uint16_t> ObjectDrawListSystem::VisibleBlocks() const
{
	return _visible;
}

std::span<const uint8_t> ObjectDrawListSystem::DrawnBlocks() const
{
	return _drawnBlocks;
}

std::span<const entt::entity> ObjectDrawListSystem::Entries() const
{
	return _list.entries;
}

draw_list::Active ObjectDrawListSystem::ActiveAt(size_t index) const
{
	return index < _list.active.size() ? _list.active[index] : draw_list::Active::No;
}

std::span<const entt::entity> ObjectDrawListSystem::DrawnThisFrame() const
{
	return _drawn;
}

std::span<const entt::entity> ObjectDrawListSystem::OnScreenThisFrame() const
{
	return _onScreen;
}

std::optional<draw_list::Active> ObjectDrawListSystem::ActiveOf(entt::entity object) const
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
