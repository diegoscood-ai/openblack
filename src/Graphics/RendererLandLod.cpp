/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The land blocks the land and the mirrored land draw at their level of detail: the triangles the original emits for a block at
// a coarser grid, blended towards one or with seams towards a coarser neighbour (draw_list::BlockTriangles), built for the
// frame. The colour is the block's texture by position, so the coarser triangles show the same texels.
// docs/wiki/rendering/visible-blocks-and-lod.md#the-blocks-level-of-detail

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <array>
#include <vector>

#include <bgfx/bgfx.h>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "ECS/DrawList/LandClip.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/Mesh.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Renderer.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// Whether a block's level of detail draws other triangles than its full mesh: a coarser grid, a blend towards one or
/// seams towards a coarser neighbour
[[nodiscard]] bool DrawnAtItsLevelOfDetail(const ecs::draw_list::BlockLod& lod)
{
	return lod.meshLod != 0 || lod.meshBlending != 0 || lod.meshLodType != 0;
}
} // namespace

void Renderer::PrepareLandLod(LandIslandInterface& island) const
{
	_frameLandLod.clear();
	if (!Locator::objectDrawListSystem::has_value())
	{
		return;
	}
	const auto& drawList = Locator::objectDrawListSystem::value();
	const auto& blocks = island.GetBlocks();
	const auto states = drawList.BlockStates();
	// the list's blocks are the land's, in its order, one for each block with its record
	if (states.size() != blocks.size())
	{
		return;
	}
	const auto& lines = drawList.LodLinesNow();
	std::vector<LandVertex> vertices;
	std::vector<std::array<uint16_t, 3>> triangles;
	for (const uint16_t index : drawList.VisibleBlocks())
	{
		if (index >= blocks.size() || !DrawnAtItsLevelOfDetail(states[index].lod))
		{
			continue;
		}
		const auto& block = blocks[index];
		const auto& record = block.GetLndBlock();
		if (!record)
		{
			continue;
		}
		const auto mesh = ecs::draw_list::BlockTriangles(record->cells, block.GetMapPosition(), states[index].lod, lines);
		if (!mesh.has_value())
		{
			continue;
		}
		// the original's triangles wind the other way round from openblack's land mesh: each is taken last point first
		triangles.clear();
		triangles.reserve(mesh->triangles.size());
		for (const auto& triangle : mesh->triangles)
		{
			triangles.push_back({triangle.points[2], triangle.points[1], triangle.points[0]});
		}
		vertices.clear();
		block.BuildLodVertexList(vertices, island, triangles, mesh->altitudes);
		const auto count = static_cast<uint32_t>(vertices.size());
		if (count == 0)
		{
			continue;
		}
		bgfx::VertexLayout layout;
		layout.begin().skip(static_cast<uint8_t>(sizeof(LandVertex))).end();
		if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
		{
			// (openblack guard) a full transient buffer leaves the block on its full mesh
			continue;
		}
		bgfx::TransientVertexBuffer buffer;
		bgfx::allocTransientVertexBuffer(&buffer, count, layout);
		std::memcpy(buffer.data, vertices.data(), sizeof(LandVertex) * count);
		_frameLandLod.insert_or_assign(&block, buffer);
	}
}

void Renderer::BindLandBlock(RenderPass viewId, const LandBlock& block) const
{
	// the mirrored land draws every block as the land does (the same block preparation and draw), with the same
	// triangles
	if (viewId == RenderPass::Main || viewId == RenderPass::Reflection)
	{
		if (const auto found = _frameLandLod.find(&block); found != _frameLandLod.end())
		{
			const auto& vertexBuffer = block.GetMesh().GetVertexBuffer();
			bgfx::setVertexBuffer(0, &found->second, 0, found->second.size / static_cast<uint32_t>(sizeof(LandVertex)),
			                      toBgfx(vertexBuffer.GetLayoutHandle()));
			return;
		}
	}
	block.GetMesh().GetVertexBuffer().Bind();
}
