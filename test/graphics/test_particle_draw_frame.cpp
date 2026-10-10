/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// particles::draw (src/Graphics/ParticleDrawFrame.h): the particle draw list under raffclar's names, on fake data

#include <cstddef>
#include <cstdint>

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/ArgbColour.h"
#include "Graphics/ParticleDrawFrame.h"
#include "Graphics/ParticleDrawPath.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ZSort.h"
#include "Particles/LightSheet.h"

using namespace openblack;
using namespace openblack::particles::draw;
using openblack::graphics::render_modes::Mode;

TEST(ParticleDrawFrame, DrawPathNamesFollowTheirValues)
{
	EXPECT_EQ(k_DrawPathNames[static_cast<size_t>(DrawPath::Sorted)], "Sorted");
	EXPECT_EQ(k_DrawPathNames[static_cast<size_t>(DrawPath::Queued)], "Queued");
	EXPECT_EQ(k_DrawPathNames[static_cast<size_t>(DrawPath::Immediate)], "Immediate");
}

TEST(ParticleDrawFrame, SpriteMaterialsTakeTheCreatorsBlend)
{
	const std::pair<entt::id_type, entt::id_type> sheets {1u, 2u};
	EXPECT_EQ(SpriteMaterial(sheets, false, false).mode, Mode::AlphaTexturedAlphaNoZWrite);
	EXPECT_EQ(SpriteMaterial(sheets, false, true).mode, Mode::AlphaTexturedAlpha);
	EXPECT_EQ(SpriteMaterial(sheets, true, false).mode, Mode::AlphaTexturedAlphaAdditiveNoZWrite);
	EXPECT_EQ(SpriteMaterial(sheets, true, true).mode, Mode::AlphaTexturedAlphaAdditive);
	const auto material = SpriteMaterial(sheets, true, false);
	EXPECT_EQ(material.texture, 1u);
	EXPECT_EQ(material.alphaTexture, 2u);
}

TEST(ParticleDrawFrame, SpritesShareAMaterialOnlyWithTheSameSheetsAndBlend)
{
	const std::pair<entt::id_type, entt::id_type> sheets {1u, 2u};
	EXPECT_EQ(SpriteMaterial(sheets, true, false), SpriteMaterial(sheets, true, false));
	EXPECT_NE(SpriteMaterial(sheets, true, false), SpriteMaterial(sheets, false, false));
	EXPECT_NE(SpriteMaterial(sheets, true, false), SpriteMaterial(sheets, true, true));
	EXPECT_NE(SpriteMaterial(sheets, true, false), SpriteMaterial({3u, 2u}, true, false));
	EXPECT_NE(SpriteMaterial(sheets, true, false), SpriteMaterial({1u, 4u}, true, false));
}

TEST(ParticleDrawFrame, ClearLeavesNoListOfAnEarlierFrame)
{
	Frame frame;
	frame.sorted.sprites.emplace_back();
	frame.sorted.meshes.emplace_back();
	frame.sorted.chains.emplace_back();
	frame.sorted.atOnce.push_back(nullptr);
	frame.queued.emplace_back();
	frame.hand.emplace_back();
	frame.surfaces.emplace_back();
	frame.groundSurfaces.emplace_back();
	frame.sortedPieces.vertices.emplace_back();
	frame.orderedPieces.batches.emplace_back();
	frame.lightSheets.emplace_back();
	frame.Clear();
	EXPECT_TRUE(frame.sorted.sprites.empty());
	EXPECT_TRUE(frame.sorted.meshes.empty());
	EXPECT_TRUE(frame.sorted.chains.empty());
	EXPECT_TRUE(frame.sorted.atOnce.empty());
	EXPECT_TRUE(frame.queued.empty());
	EXPECT_TRUE(frame.hand.empty());
	EXPECT_TRUE(frame.surfaces.empty());
	EXPECT_TRUE(frame.groundSurfaces.empty());
	EXPECT_TRUE(frame.sortedPieces.vertices.empty());
	EXPECT_TRUE(frame.orderedPieces.batches.empty());
	EXPECT_TRUE(frame.lightSheets.empty());
}

namespace
{
/// A synthetic sheet of four points, of a colour, moved on a little so that its first points have a height and a
/// strength
std::shared_ptr<particles::LightSheet> FourPointSheet(float x, uint32_t rgb)
{
	auto sheet = std::make_shared<particles::LightSheet>();
	sheet->Start({{x, 1.0f, 0.0f}, {x + 2.0f, 1.0f, 0.0f}, {x + 4.0f, 1.0f, 1.0f}, {x + 6.0f, 2.0f, 1.0f}}, rgb, 3.0f, 0.03f);
	sheet->SetStrength(0.75f);
	sheet->Update(0.1f);
	return sheet;
}
} // namespace

TEST(ParticleDrawFrame, AnEmptySheetAddsNothing)
{
	Frame frame;
	const std::array<particles::LightSheet::Vertex, 1> corner {
	    {{.position = {}, .uv = {}, .argb = 0xFF000000u, .specularArgb = 0u}}};
	const std::array<uint32_t, 3> triangle {0, 0, 0};
	AddLightSheet(frame, {}, {}, {});
	AddLightSheet(frame, corner, {}, {});
	AddLightSheet(frame, {}, triangle, {});
	EXPECT_TRUE(frame.lightSheets.empty());
	// nor does a sheet that was never started, nor a gone one
	const std::vector<std::shared_ptr<particles::LightSheet>> sheets {std::make_shared<particles::LightSheet>(), nullptr};
	AddLightSheets(frame, sheets);
	EXPECT_TRUE(frame.lightSheets.empty());
}

TEST(ParticleDrawFrame, ASheetBecomesOneSurfaceOfItsMaterial)
{
	Frame frame;
	const std::array<particles::LightSheet::Vertex, 3> corners {{
	    {.position = {1.0f, 2.0f, 3.0f}, .uv = {0.5f, 1.0f}, .argb = 0xFF000000u, .specularArgb = 0u},
	    {.position = {4.0f, 5.0f, 6.0f}, .uv = {0.5f, 0.75f}, .argb = 0xFF804020u, .specularArgb = 0x20402010u},
	    {.position = {7.0f, 8.0f, 9.0f}, .uv = {0.5f, 0.0f}, .argb = 0xFF000000u, .specularArgb = 0u},
	}};
	const std::array<uint32_t, 3> triangle {0, 2, 1};
	const glm::vec3 sortPoint {10.0f, 20.0f, 30.0f};
	AddLightSheet(frame, corners, triangle, sortPoint);
	ASSERT_EQ(frame.lightSheets.size(), 1u);
	const auto& surface = frame.lightSheets.front();
	EXPECT_EQ(surface.texture, "S_LightSheetStars");
	EXPECT_TRUE(surface.additive);
	EXPECT_FALSE(surface.writeDepth);
	EXPECT_TRUE(surface.doubleSided);
	EXPECT_TRUE(surface.useTextureAlpha);
	EXPECT_TRUE(surface.specularInPass);
	EXPECT_EQ(surface.path, DrawPath::Sorted);
	EXPECT_EQ(surface.origin, sortPoint);
	// the surface draw's mode for those flags is 13: SRCALPHA / ONE without depth writes
	EXPECT_EQ(graphics::render_modes::ModeFromProperties(
	              Mode::AlphaTexturedAlphaNoZWrite,
	              {.additive = surface.additive, .zWrite = surface.writeDepth, .alpha = surface.useTextureAlpha}),
	          Mode::AlphaTexturedAlphaAdditiveNoZWrite);
	EXPECT_EQ(static_cast<int>(Mode::AlphaTexturedAlphaAdditiveNoZWrite), 13);
	ASSERT_EQ(surface.vertices.size(), corners.size());
	for (size_t i = 0; i < corners.size(); ++i)
	{
		EXPECT_EQ(surface.vertices[i].position, corners[i].position);
		EXPECT_EQ(surface.vertices[i].uv, corners[i].uv);
	}
	// red first for bgfx; the specular takes the diffuse alpha in place of its own
	EXPECT_EQ(surface.vertices[0].abgr, 0xFF000000u);
	EXPECT_EQ(surface.vertices[1].abgr, 0xFF204080u);
	EXPECT_EQ(surface.vertices[1].specular, 0xFF102040u);
	EXPECT_EQ(surface.vertices[2].specular, 0xFF000000u);
	EXPECT_EQ(surface.indices, (std::vector<uint16_t> {0, 2, 1}));
}

TEST(ParticleDrawFrame, SheetsAreBuiltAndAddedNewestFirst)
{
	const auto older = FourPointSheet(0.0f, 0xFF0000u);
	const auto newer = FourPointSheet(100.0f, 0x00FF00u);
	// twins built by hand, as the frame must build them
	auto olderTwin = *older;
	auto newerTwin = *newer;
	std::vector<particles::LightSheet::Vertex> newerVertices;
	std::vector<uint32_t> newerTriangles;
	newerTwin.Build(newerVertices, newerTriangles);
	std::vector<particles::LightSheet::Vertex> olderVertices;
	std::vector<uint32_t> olderTriangles;
	olderTwin.Build(olderVertices, olderTriangles);

	Frame frame;
	const std::vector oldestFirst {older, newer};
	AddLightSheets(frame, oldestFirst);
	ASSERT_EQ(frame.lightSheets.size(), 2u);
	const auto expectBuilt = [](const psys::surf_revol::Surface& surface,
	                            const std::vector<particles::LightSheet::Vertex>& vertices,
	                            const std::vector<uint32_t>& triangles) {
		ASSERT_EQ(surface.vertices.size(), vertices.size());
		for (size_t i = 0; i < vertices.size(); ++i)
		{
			EXPECT_EQ(surface.vertices[i].position, vertices[i].position);
			EXPECT_EQ(surface.vertices[i].uv, vertices[i].uv);
			EXPECT_EQ(surface.vertices[i].abgr, argb_colour::ToAbgr(vertices[i].argb));
		}
		ASSERT_EQ(surface.indices.size(), triangles.size());
		for (size_t i = 0; i < triangles.size(); ++i)
		{
			EXPECT_EQ(surface.indices[i], triangles[i]);
		}
	};
	expectBuilt(frame.lightSheets[0], newerVertices, newerTriangles);
	expectBuilt(frame.lightSheets[1], olderVertices, olderTriangles);
	// the sheets themselves were built: their middles moved as their twins' did
	EXPECT_EQ(older->Middle(), olderTwin.Middle());
	EXPECT_EQ(newer->Middle(), newerTwin.Middle());
}

TEST(ParticleDrawFrame, ASheetTakesItsPlaceAtItsMiddleBeforeTheBuild)
{
	const auto sheet = FourPointSheet(0.0f, 0xFF0000u);
	Frame frame;
	const std::vector sheets {sheet};
	// the first frame: the middle as the game's allocator leaves it
	AddLightSheets(frame, sheets);
	ASSERT_EQ(frame.lightSheets.size(), 1u);
	EXPECT_EQ(frame.lightSheets[0].origin, glm::vec3(particles::LightSheet::k_UnbuiltMiddle));
	// the next frame: the mean of its points, as the first build left it
	const auto built = sheet->Middle();
	EXPECT_EQ(built, glm::vec3(3.0f, 1.25f, 0.5f));
	frame.Clear();
	AddLightSheets(frame, sheets);
	ASSERT_EQ(frame.lightSheets.size(), 1u);
	EXPECT_EQ(frame.lightSheets[0].origin, built);
}

TEST(ParticleDrawFrame, ASheetOnItsFirstFrameIsDrawnBehindEverything)
{
	// its key is about 5.6e17, far beyond any point of the land
	const glm::vec3 camera {2000.0f, 300.0f, 2000.0f};
	const glm::vec3 unbuilt {particles::LightSheet::k_UnbuiltMiddle};
	graphics::zsort::Queue<int> queue;
	queue.Begin();
	queue.Submit(1, graphics::zsort::Key({8000.0f, 0.0f, 8000.0f}, camera));
	queue.Submit(2, graphics::zsort::Key(unbuilt, camera));
	const auto drained = queue.Drain();
	ASSERT_EQ(drained.size(), 2u);
	EXPECT_EQ(*drained[0].item, 2);
	EXPECT_GT(drained[0].key, 5.5e17f);
}

TEST(ParticleDrawFrame, SheetsOfEqualKeysDrawTheNewestFirst)
{
	// two sheets on the same points have the same key: submitted in the frame's order, the newest first, the newest is
	// drawn first, as the game's list has it
	const auto older = FourPointSheet(0.0f, 0xFF0000u);
	const auto newer = FourPointSheet(0.0f, 0x00FF00u);
	Frame frame;
	const std::vector oldestFirst {older, newer};
	AddLightSheets(frame, oldestFirst);
	frame.Clear();
	AddLightSheets(frame, oldestFirst);
	ASSERT_EQ(frame.lightSheets.size(), 2u);
	const glm::vec3 camera {50.0f, 10.0f, -20.0f};
	graphics::zsort::Queue<size_t> queue;
	queue.Begin();
	for (size_t i = 0; i < frame.lightSheets.size(); ++i)
	{
		queue.Submit(i, graphics::zsort::Key(frame.lightSheets[i].origin, camera));
	}
	const auto drained = queue.Drain();
	ASSERT_EQ(drained.size(), 2u);
	EXPECT_EQ(drained[0].key, drained[1].key);
	// the bright corner of the first point: green for the newer sheet, red for the older (red is bgfx's low byte)
	const auto& first = frame.lightSheets[*drained[0].item].vertices[1].abgr;
	const auto& second = frame.lightSheets[*drained[1].item].vertices[1].abgr;
	EXPECT_EQ(first & 0xFFu, 0u);
	EXPECT_NE((first >> 8u) & 0xFFu, 0u);
	EXPECT_NE(second & 0xFFu, 0u);
}
