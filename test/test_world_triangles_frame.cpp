/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The lines and triangles game code draws straight into the world, gathered into a frame: each primitive's vertices go
// on in the order they come, a primitive in the same material, mode table, alpha and owner as the last joins its draw
// and any other starts a new one, and each colour is turned from the game's 0xAARRGGBB into the bytes the graphics card
// reads. A sprite is drawn adding its light to what is behind it, or blended over it by its alpha.
// docs/wiki/project/progress/rendering/render_pipeline.md, sprites_and_billboards.md

#include <cstdint>

#include <array>

#include <gtest/gtest.h>

#include "Graphics/RenderModes.h"
#include "Graphics/WorldTriangles.h"

using namespace openblack::graphics;
using world_triangles::Frame;
using world_triangles::MaterialRef;
using world_triangles::Vertex;

namespace
{
std::array<Vertex, 3> Triangle(float x)
{
	return {{{.position = {x, 0.0f, 0.0f}, .uv = {0.0f, 0.0f}, .abgr = 1},
	         {.position = {x + 1.0f, 0.0f, 0.0f}, .uv = {1.0f, 0.0f}, .abgr = 2},
	         {.position = {x, 1.0f, 0.0f}, .uv = {0.0f, 1.0f}, .abgr = 3}}};
}
} // namespace

// Primitives of the same material, table, alpha and owner join one draw; another starts a new one; the order is kept
TEST(WorldTrianglesFrame, PrimitivesJoinTheLastDrawWhenAllTheyAreDrawnWithIsTheSame)
{
	Frame frame;
	const MaterialRef rock {.meshId = 7, .subMesh = 0, .primitive = 1};
	const MaterialRef tree {.meshId = 8, .subMesh = 0, .primitive = 0};
	frame.Append(rock, render_modes::Table::Normal, 255, Triangle(0.0f));
	frame.Append(rock, render_modes::Table::Normal, 255, Triangle(1.0f));
	frame.Append(tree, render_modes::Table::Normal, 255, Triangle(2.0f));
	frame.Append(tree, render_modes::Table::Normal, 128, Triangle(3.0f));
	frame.Append(rock, render_modes::Table::Normal, 255, Triangle(4.0f));
	ASSERT_EQ(frame.vertices.size(), 15u);
	for (size_t i = 0; i < 5; ++i)
	{
		EXPECT_EQ(frame.vertices.at(i * 3).position.x, static_cast<float>(i)) << i;
	}
	ASSERT_EQ(frame.batches.size(), 4u);
	EXPECT_EQ(frame.batches.at(0).material, rock);
	EXPECT_EQ(frame.batches.at(0).firstVertex, 0u);
	EXPECT_EQ(frame.batches.at(0).vertexCount, 6u);
	EXPECT_EQ(frame.batches.at(1).material, tree);
	EXPECT_EQ(frame.batches.at(1).vertexCount, 3u);
	EXPECT_EQ(frame.batches.at(2).globalAlpha, 128u);
	EXPECT_EQ(frame.batches.at(3).material, rock);
	EXPECT_EQ(frame.batches.at(3).firstVertex, 12u);
	// another owner starts its own draw, and nothing is appended for no vertices
	int owner = 0;
	frame.Append(rock, render_modes::Table::Normal, 255, Triangle(5.0f), &owner);
	frame.Append(rock, render_modes::Table::Normal, 255, {});
	EXPECT_EQ(frame.batches.size(), 5u);
	EXPECT_EQ(frame.batches.back().tag, &owner);
}

// A colour is turned from 0xAARRGGBB into the ABGR bytes the graphics card reads
TEST(WorldTrianglesFrame, AColourIsTurnedIntoTheBytesTheCardReads)
{
	EXPECT_EQ(world_triangles::ToAbgr(0xFF112233u), 0xFF332211u);
	EXPECT_EQ(world_triangles::ToAbgr(0x80FF0000u), 0x800000FFu);
}

// A sprite either adds its light to what is behind it, as a glow, or blends over it by its alpha
TEST(WorldTrianglesFrame, ASpriteAddsItsLightOrBlendsByItsAlpha)
{
	EXPECT_EQ(render_modes::SpriteMode(true), render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite);
	EXPECT_EQ(render_modes::SpriteMode(false), render_modes::Mode::AlphaTexturedAlphaNoZWrite);
}
