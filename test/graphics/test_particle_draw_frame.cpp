/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// particles::draw (src/Particles/ParticleDrawFrame.h): the particle draw list under raffclar's names, on fake data

#include <cstddef>
#include <cstdint>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/ArgbColour.h"
#include "Graphics/Haze.h"
#include "Graphics/ParticleDrawPath.h"
#include "Graphics/ZSort.h"
#include "Particles/LightSheet.h"
#include "Particles/ParticleDrawFrame.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellFile.h"
#include "Visual/RenderModes.h"

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

namespace
{
/// Whether the sprite creator Sprite0 of a file with these properties is lit by the land, as the effect made from the
/// file reads it (ParticleSpriteCreator has no registered factory: the effect reads its common properties); nullopt
/// when the effect has no such creator
std::optional<bool> SpriteUsesLandscapeColour(const std::string& properties)
{
	const auto file = particles::File::Parse("BEGINPROPERTIES\nENDPROPERTIES\n"
	                                         "BEGINCLASS ParticleSpriteCreator Sprite0\nBEGINPROPERTIES\n" +
	                                             properties + "ENDPROPERTIES\nENDCLASS\n",
	                                         "test");
	if (!file.has_value())
	{
		return std::nullopt;
	}
	const particles::Effect effect(std::make_shared<const particles::File>(*file), glm::vec3(0.0f), 1.0f);
	const auto* creator = effect.FindCreator("Sprite0");
	return creator != nullptr ? std::optional(creator->useLandscapeColour) : std::nullopt;
}
} // namespace

TEST(ParticleDrawFrame, SpriteCreatorsReadUseLandscapeColorOffByDefault)
{
	const auto lit = SpriteUsesLandscapeColour("PROPERTY UseLandscapeColor BOOL 1\n");
	const auto plain = SpriteUsesLandscapeColour("PROPERTY UsePlayerColor BOOL 0\n");
	ASSERT_TRUE(lit.has_value());
	ASSERT_TRUE(plain.has_value());
	EXPECT_TRUE(*lit);
	EXPECT_FALSE(*plain);
}

// The original's sprite draw run through the oracle, the land light given and the haze call captured: the colour
// handed to the haze is (light x colour) >> 8 in every channel, the alpha too, and the atom's specular is what the
// haze gets; with the haze off the sprite keeps both
TEST(ParticleDrawFrame, LandscapeLitSpritesTakeTheLandsLightInEveryChannel)
{
	struct Case
	{
		uint32_t light, colour, specular, lit;
	};
	constexpr std::array<Case, 4> k_Cases {{
	    {.light = 0x3FC08040u, .colour = 0xFF8040C0u, .specular = 0x00000000u, .lit = 0x3E602030u},
	    {.light = 0x40FF7F01u, .colour = 0x80FFFFFFu, .specular = 0x11223344u, .lit = 0x20FE7E00u},
	    // the creator's alpha scale is already in the colour's alpha: 0xC0 x 0x80 / 255 = 0x60
	    {.light = 0x3F7FFFFFu, .colour = 0x60204080u, .specular = 0xFF000000u, .lit = 0x170F3F7Fu},
	    {.light = 0x437F7F7Fu, .colour = 0x01FEFDFCu, .specular = 0x00FFFFFFu, .lit = 0x007E7D7Du},
	}};
	for (const auto& c : k_Cases)
	{
		const auto colours = LandscapeLitSprite(c.colour, c.specular, c.light, graphics::haze::Params {}, 650.0f);
		EXPECT_EQ(colours.colour, c.lit) << std::hex << c.light << " " << c.colour;
		EXPECT_EQ(colours.specular, c.specular) << std::hex << c.light << " " << c.colour;
	}
}

// With the haze on, the lit colour is hazed as an object at the sprite's depth: its R, G and B scaled, the specular
// the haze's
TEST(ParticleDrawFrame, LandscapeLitSpritesAreHazedAsAnObjectThere)
{
	const graphics::haze::Params haze {.on = true,
	                                   .nearDistance = 400.0f,
	                                   .farDistance = 900.0f,
	                                   .range = 500.0f,
	                                   .k = 211,
	                                   .colour = {63.0f, 70.0f, 65.0f},
	                                   .packed = 0x3F4641u};
	uint32_t hazed = 0x3E602030u;
	const auto specular = graphics::haze::ApplyObject(haze, 650.0f, 0x00000000u, &hazed);
	const auto colours = LandscapeLitSprite(0xFF8040C0u, 0x00000000u, 0x3FC08040u, haze, 650.0f);
	EXPECT_EQ(colours, (SpriteColours {.colour = hazed, .specular = specular}));
	EXPECT_NE(hazed, 0x3E602030u);
	// closer than near: untouched
	EXPECT_EQ(LandscapeLitSprite(0xFF8040C0u, 0x11223344u, 0x3FC08040u, haze, 100.0f),
	          (SpriteColours {.colour = 0x3E602030u, .specular = 0x11223344u}));
}

// The original's sprite draw run through the oracle without UseLandscapeColor keeps the atom's specular as it is (no
// light, no haze) and the sprite's vertices carry it; the device adds its red, green and blue, never its alpha. So only
// a specular with some red, green or blue needs the program that adds it: the defence sphere's sparks (SpecColor 30,
// 30, 30), not a specular of alpha only
TEST(ParticleDrawFrame, SpritesAddTheirOwnSpecularOnlyWhenItHasAColour)
{
	EXPECT_FALSE(SpriteAddsSpecular(false, 0x00000000u));
	EXPECT_FALSE(SpriteAddsSpecular(false, 0xFF000000u));
	EXPECT_TRUE(SpriteAddsSpecular(false, 0x001E1E1Eu));
	EXPECT_TRUE(SpriteAddsSpecular(false, 0x00FFFFFFu));
	EXPECT_TRUE(SpriteAddsSpecular(false, 0x11223344u));
	// a sprite lit by the land always takes the haze's specular
	EXPECT_TRUE(SpriteAddsSpecular(true, 0x00000000u));
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

TEST(ParticleDrawFrame, AHiddenSheetIsKeptButNotDrawn)
{
	const auto hidden = FourPointSheet(0.0f, 0xFF0000u);
	const auto shown = FourPointSheet(100.0f, 0x00FF00u);
	hidden->SetHidden(true);
	const auto middle = hidden->Middle();
	Frame frame;
	AddLightSheets(frame, std::vector {hidden, shown});
	// only the shown one, and the hidden one is not built either: its middle stays where it was
	ASSERT_EQ(frame.lightSheets.size(), 1u);
	EXPECT_EQ(hidden->Middle(), middle);
	EXPECT_NE(shown->Middle(), glm::vec3(particles::LightSheet::k_UnbuiltMiddle));
	hidden->SetHidden(false);
	Frame next;
	AddLightSheets(next, std::vector {hidden, shown});
	EXPECT_EQ(next.lightSheets.size(), 2u);
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
	const auto expectBuilt = [](const particles::surf_revol::Surface& surface,
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
