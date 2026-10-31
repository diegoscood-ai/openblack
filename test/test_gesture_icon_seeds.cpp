/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The seed's graphic inside a gesture icon: its places through the map position, its mesh's draws (a player's seed and
// each creature spell phial's look), its bands and its far cut, and the move on the screen its holder effect's sprites
// take. The numbers are the original game's own, bit for bit, read by running its draws of the graphic

#include <cstdint>

#include <array>
#include <bit>

#include <glm/ext/matrix_double4x4.hpp>
#include <glm/ext/vector_double3.hpp>
#include <glm/ext/vector_double4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <gtest/gtest.h>

#include "Magic/Gestures/GestureIconModels.h"
#include "Magic/Gestures/GestureIconSeeds.h"

using namespace openblack;
using namespace openblack::magic::gestures;

namespace
{
uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

float Float(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

/// A matrix's three axes as rows (the game's layout), nine words
std::array<uint32_t, 9> AxesBits(const glm::mat4& matrix)
{
	std::array<uint32_t, 9> out {};
	for (int c = 0; c < 3; ++c)
	{
		for (int j = 0; j < 3; ++j)
		{
			out.at(static_cast<size_t>((c * 3) + j)) = Bits(matrix[c][j]);
		}
	}
	return out;
}

/// The graphic's places as the runs had them: the mesh ten units ahead, 0.18 below the focus
IconSeedPlaces RunPlaces()
{
	IconSeedPlaces places;
	places.point = {0.0f, 0.0f, 10.0f};
	places.mesh = {0.0f, Float(0xBE3851ECu), 10.0f};
	places.effect = {0.0f, Float(0x3D1374BDu), 10.0f};
	return places;
}

/// A seed of scale 0.8 with a mesh, a creature spell phial of `phial`'s look when given
IconSeedLook RunLook(std::optional<CreatureReceiveSpellType> phial = std::nullopt)
{
	return {.drawScale = 0.8f,
	        .meshHeight = -1.5f,
	        .holderHeight = 0.3f,
	        .useMesh = true,
	        .phial = phial,
	        .meshHalfDiagonal = 2.0f,
	        .bandHalfDiagonal = 5.0f};
}

constexpr uint32_t k_Scale = 0x3DC49BA6u; // 0.8 x 0.12
} // namespace

TEST(GestureIconSeeds, ThePlacesGoThroughTheMapPositionWithTheLandUnderThem)
{
	struct Case
	{
		float ground;
		uint32_t meshY;
	};
	// the mesh 1.5 x 0.12 below, the effect 0.3 x 0.12 above, the land's height taken away and added back
	for (const auto& c : {Case {0.0f, 0xBE3851EBu}, Case {13.7f, 0xBE385200u}, Case {-2.25f, 0xBE3851F0u}})
	{
		const auto places = IconSeedPlacesAt({0.0f, 0.0f, 10.0f}, 0.12f, c.ground, -1.5f, 0.3f);
		EXPECT_EQ(places.point, glm::vec3(0.0f, 0.0f, 10.0f));
		EXPECT_EQ(Bits(places.mesh.y), c.meshY);
		EXPECT_EQ(Bits(places.effect.y), 0x3D1374BDu);
		EXPECT_EQ(places.mesh.x, 0.0f);
		EXPECT_EQ(places.mesh.z, 10.0f);
	}
	// where a graphic is made: the map's corner at a scale of 1
	const auto made = IconSeedPlacesAt({0.0f, 0.0f, 0.0f}, 1.0f, 0.0f, -1.5f, 0.3f);
	EXPECT_EQ(made.mesh, glm::vec3(0.0f, -1.5f, 0.0f));
	EXPECT_EQ(made.effect, glm::vec3(0.0f, 0.3f, 0.0f));
}

TEST(GestureIconSeeds, APlayersSeedTurnsWithTheIconsAlpha)
{
	IconSeedGraphic graphic;
	constexpr std::array<std::array<uint32_t, 9>, 3> k_Frames {{
	    {0x3DC42E11u, 0, 0x3BCF77A4u, 0, k_Scale, 0, 0xBBCF77A4u, 0, 0x3DC42E11u},
	    {0x3DC2E5CCu, 0, 0x3C4F0402u, 0, k_Scale, 0, 0xBC4F0402u, 0, 0x3DC2E5CCu},
	    {0x3DC0C444u, 0, 0x3C9AB2B6u, 0, k_Scale, 0, 0xBC9AB2B6u, 0, 0x3DC0C444u},
	}};
	constexpr std::array<uint32_t, 3> k_Spins {0x3D872B02u, 0x3E072B02u, 0x3E4AC083u};
	for (size_t frame = 0; frame < k_Frames.size(); ++frame)
	{
		const auto draw = StepIconSeed(graphic, RunLook(), RunPlaces(), 33, 0xBF, 0x00FF2010u);
		ASSERT_EQ(draw.meshes.size(), 1u);
		const auto& mesh = draw.meshes.front();
		EXPECT_EQ(AxesBits(mesh.matrix), k_Frames.at(frame));
		EXPECT_EQ(glm::vec3(mesh.matrix[3]), RunPlaces().mesh);
		EXPECT_EQ(Bits(mesh.scale), k_Scale);
		EXPECT_EQ(mesh.colour, 0xBFFFFFFFu);
		EXPECT_TRUE(mesh.fading);
		EXPECT_FALSE(mesh.landLight);
		EXPECT_EQ(mesh.uv, glm::vec2(0.0f));
		EXPECT_EQ(Bits(graphic.spin), k_Spins.at(frame));
		EXPECT_TRUE(draw.bands.empty());
		EXPECT_EQ(draw.effectOrigin, RunPlaces().effect);
		EXPECT_EQ(draw.effectMagnitude, k_IconSeedScale);
		EXPECT_EQ(draw.effectAlpha, 0xBF);
	}
	// opaque icons: no alpha of its own
	IconSeedGraphic opaque;
	const auto draw = StepIconSeed(opaque, RunLook(), RunPlaces(), 33, 0xFF, 0x00FF2010u);
	EXPECT_EQ(draw.meshes.front().colour, 0xFFFFFFFFu);
	EXPECT_FALSE(draw.meshes.front().fading);
	// a seed without a mesh shows its holder effect only, its turn kept
	IconSeedGraphic holderOnly;
	auto look = RunLook();
	look.useMesh = false;
	EXPECT_TRUE(StepIconSeed(holderOnly, look, RunPlaces(), 33, 0xBF, 0u).meshes.empty());
	EXPECT_EQ(holderOnly.spin, 0.0f);
}

TEST(GestureIconSeeds, APhialKeepsItsColourAndShowsItsFrames)
{
	// the hungry phial (the inverse spiral's): its colour as it is, its own alpha switched on, its frames at -15 a second
	IconSeedGraphic graphic;
	const auto look = RunLook(CreatureReceiveSpellType::CreatureReceiveSpellHungry);
	constexpr std::array<glm::vec2, 3> k_Frames {{{0.875f, 0.375f}, {0.875f, 0.375f}, {0.75f, 0.375f}}};
	constexpr std::array<uint32_t, 3> k_UvFrames {0x41FC0A3Du, 0x41F8147Au, 0x41F41EB7u};
	constexpr std::array<uint32_t, 3> k_Pulses {0x3C872B02u, 0x3D072B02u, 0x3D4AC083u};
	for (size_t frame = 0; frame < k_Frames.size(); ++frame)
	{
		const auto draw = StepIconSeed(graphic, look, RunPlaces(), 33, 0xBF, 0u);
		ASSERT_EQ(draw.meshes.size(), 1u);
		EXPECT_EQ(draw.meshes.front().colour, 0xFFFFFFFFu);
		EXPECT_TRUE(draw.meshes.front().fading);
		EXPECT_EQ(draw.meshes.front().uv, k_Frames.at(frame));
		EXPECT_EQ(Bits(graphic.uvFrame), k_UvFrames.at(frame));
		EXPECT_EQ(Bits(graphic.pulsePhase), k_Pulses.at(frame));
	}
	EXPECT_EQ(AxesBits(StepIconSeed(graphic, look, RunPlaces(), 0, 0xBF, 0u).meshes.front().matrix),
	          (std::array<uint32_t, 9> {0x3DC0C444u, 0, 0x3C9AB2B6u, 0, k_Scale, 0, 0xBC9AB2B6u, 0, 0x3DC0C444u}));
}

TEST(GestureIconSeeds, TheFatAndTheThinPhialsSquashWithThePulse)
{
	IconSeedGraphic fat;
	const auto fatLook = RunLook(CreatureReceiveSpellType::CreatureReceiveSpellFat);
	EXPECT_EQ(AxesBits(StepIconSeed(fat, fatLook, RunPlaces(), 33, 0xBF, 0u).meshes.front().matrix),
	          (std::array<uint32_t, 9> {0x3E33454Eu, 0, 0x3C3D95D2u, 0, k_Scale, 0, 0xBC3D95D2u, 0, 0x3E33454Eu}));
	EXPECT_EQ(AxesBits(StepIconSeed(fat, fatLook, RunPlaces(), 33, 0xBF, 0u).meshes.front().matrix),
	          (std::array<uint32_t, 9> {0x3E3994CCu, 0, 0x3CC51EB8u, 0, k_Scale, 0, 0xBCC51EB8u, 0, 0x3E3994CCu}));
	IconSeedGraphic thin;
	const auto thinLook = RunLook(CreatureReceiveSpellType::CreatureReceiveSpellThin);
	EXPECT_EQ(AxesBits(StepIconSeed(thin, thinLook, RunPlaces(), 33, 0xBF, 0u).meshes.front().matrix),
	          (std::array<uint32_t, 9> {0x3D5B2C9Du, 0, 0x3B67C8E0u, 0, k_Scale, 0, 0xBB7EADAFu, 0, 0x3D70D28Fu}));
	EXPECT_EQ(AxesBits(StepIconSeed(thin, thinLook, RunPlaces(), 33, 0xBF, 0u).meshes.front().matrix),
	          (std::array<uint32_t, 9> {0x3D49C789u, 0, 0x3BD65349u, 0, k_Scale, 0, 0xBBEF49E2u, 0, 0x3D61480Du}));
}

TEST(GestureIconSeeds, TheBigPhialIsDrawnAgainGrowingAndFading)
{
	IconSeedGraphic graphic;
	const auto look = RunLook(CreatureReceiveSpellType::CreatureReceiveSpellBig);
	struct Frame
	{
		std::array<uint32_t, 9> first;
		std::array<uint32_t, 9> grown;
		uint32_t grownScale;
		uint32_t grownColour;
	};
	const std::array<Frame, 2> k_Frames {{
	    {{0x3DC42E11u, 0, 0x3BCF77A4u, 0, k_Scale, 0, 0xBBCF77A4u, 0, 0x3DC42E11u},
	     {0x3DC76ABBu, 0, 0x3BD2E3FCu, 0, 0x3DC7DA1Fu, 0, 0xBBD2E3FCu, 0, 0x3DC76ABBu},
	     0x3E188AE8u,
	     0xBAFFFFFFu},
	    {{0x3DC2E5CCu, 0, 0x3C4F0402u, 0, k_Scale, 0, 0xBC4F0402u, 0, 0x3DC2E5CCu},
	     {0x3DC9544Bu, 0, 0x3C55D8E0u, 0, 0x3DCB1898u, 0, 0xBC55D8E0u, 0, 0x3DC9544Bu},
	     0x3E1D9316u,
	     0xB7FFFFFFu},
	}};
	for (const auto& frame : k_Frames)
	{
		const auto draw = StepIconSeed(graphic, look, RunPlaces(), 33, 0xBF, 0u);
		ASSERT_EQ(draw.meshes.size(), 2u);
		EXPECT_EQ(AxesBits(draw.meshes[0].matrix), frame.first);
		EXPECT_EQ(draw.meshes[0].colour, 0xBFFFFFFFu);
		EXPECT_EQ(Bits(draw.meshes[0].scale), k_Scale);
		EXPECT_EQ(AxesBits(draw.meshes[1].matrix), frame.grown);
		EXPECT_EQ(Bits(draw.meshes[1].scale), frame.grownScale);
		EXPECT_EQ(draw.meshes[1].colour, frame.grownColour);
		EXPECT_TRUE(draw.meshes[1].fading);
	}
}

TEST(GestureIconSeeds, TheSmallPhialShrinksThenShowsItsShadow)
{
	IconSeedGraphic graphic;
	const auto look = RunLook(CreatureReceiveSpellType::CreatureReceiveSpellSmall);
	struct Frame
	{
		std::array<uint32_t, 9> shrunk;
		uint32_t shrunkScale;
		std::array<uint32_t, 9> shadow;
	};
	const std::array<Frame, 2> k_Frames {{
	    {{0x3DC0F167u, 0, 0x3BCC0B4Du, 0, 0x3DC15D2Du, 0, 0xBBCC0B4Du, 0, 0x3DC0F167u},
	     0x3DC15D2Du,
	     {0x3DC42E11u, 0, 0x3BCF77A4u, 0, k_Scale, 0, 0xBBCF77A4u, 0, 0x3DC42E11u}},
	    {{0x3DBC774Du, 0, 0x3C482F23u, 0, 0x3DBE1EB4u, 0, 0xBC482F23u, 0, 0x3DBC774Du},
	     0x3DBE1EB4u,
	     {0x3DC2E5CCu, 0, 0x3C4F0402u, 0, k_Scale, 0, 0xBC4F0402u, 0, 0x3DC2E5CCu}},
	}};
	for (const auto& frame : k_Frames)
	{
		const auto draw = StepIconSeed(graphic, look, RunPlaces(), 33, 0xBF, 0u);
		ASSERT_EQ(draw.meshes.size(), 2u);
		EXPECT_EQ(AxesBits(draw.meshes[0].matrix), frame.shrunk);
		EXPECT_EQ(Bits(draw.meshes[0].scale), frame.shrunkScale);
		EXPECT_EQ(draw.meshes[0].colour, 0xBFFFFFFFu);
		EXPECT_EQ(AxesBits(draw.meshes[1].matrix), frame.shadow);
		EXPECT_EQ(draw.meshes[1].colour, 0x3BFFFFFFu);
		EXPECT_TRUE(draw.meshes[1].fading);
	}
}

TEST(GestureIconSeeds, TheInvisiblePhialTakesTheLandsLightAndPulsesItsAlpha)
{
	IconSeedGraphic graphic;
	const auto look = RunLook(CreatureReceiveSpellType::CreatureReceiveSpellInvisible);
	for (const uint32_t alpha : {0x69u, 0x73u})
	{
		const auto draw = StepIconSeed(graphic, look, RunPlaces(), 33, 0xBF, 0u);
		ASSERT_EQ(draw.meshes.size(), 1u);
		EXPECT_TRUE(draw.meshes.front().landLight);
		EXPECT_TRUE(draw.meshes.front().fading);
		EXPECT_EQ(draw.meshes.front().colour >> 24u, alpha);
	}
}

TEST(GestureIconSeeds, TheFreezePhialHasItsFrozenLookAndASlowerPulse)
{
	IconSeedGraphic graphic;
	const auto draw =
	    StepIconSeed(graphic, RunLook(CreatureReceiveSpellType::CreatureReceiveSpellFreeze), RunPlaces(), 100, 0xFF, 0u);
	EXPECT_TRUE(draw.frozen);
	EXPECT_TRUE(draw.meshes.empty());
	EXPECT_EQ(graphic.pulsePhase, 0.35f * 0.1f);
}

TEST(GestureIconSeeds, ThePowerUpBandsTurnInThePlayersColourTwiceALevel)
{
	IconSeedGraphic graphic;
	graphic.powerUp = 0;
	auto look = RunLook();
	look.useMesh = false;
	const auto first = StepIconSeed(graphic, look, RunPlaces(), 33, 0xBF, 0x00FF2010u);
	ASSERT_EQ(first.bands.size(), 2u);
	EXPECT_EQ(Bits(graphic.bandSpin), 0x3D072B02u);
	EXPECT_EQ(Bits(graphic.bandAngle), 0x3EAE0760u);
	// 0.2 x 0.5 x 0.12, the colour's alpha (120 x the icons' alpha) >> 8
	EXPECT_EQ(Bits(first.bands[0].scale), 0x3C449BA6u);
	EXPECT_EQ(first.bands[0].colour, 0x59FF2010u);
	EXPECT_EQ(glm::vec3(first.bands[0].matrix[3]), glm::vec3(0.0f, 0.0f, 10.0f));
	// (approximate) the band's turn towards the models' camera is the world's bands' (billboard::BandToEye), within a few
	// float steps of the original's
	constexpr std::array<uint32_t, 9> k_Level0 {0xBBE32F84u, 0xBB5CAE41u, 0xBC16B0C4u, 0x3C18D7DEu, 0x3A986B84u,
	                                            0xBBF461CEu, 0x3B439037u, 0xBC3BBE90u, 0x3AFF0D5Cu};
	const auto axes = AxesBits(first.bands[0].matrix);
	for (size_t k = 0; k < axes.size(); ++k)
	{
		EXPECT_NEAR(Float(axes.at(k)), Float(k_Level0.at(k)), 1e-8f) << k;
	}
	EXPECT_EQ(AxesBits(first.bands[1].matrix), axes);
	// two levels, opaque icons
	IconSeedGraphic two;
	two.powerUp = 1;
	const auto draw = StepIconSeed(two, look, RunPlaces(), 33, 0xFF, 0x00FF2010u);
	ASSERT_EQ(draw.bands.size(), 4u);
	EXPECT_EQ(draw.bands[2].colour, 0x77FF2010u);
	constexpr std::array<uint32_t, 9> k_Level1 {0x3C389976u, 0xBB1A8FDDu, 0x3B5E2897u, 0xBB3B1463u, 0x3B2EFB44u,
	                                            0x3C39E2D3u, 0xBB439037u, 0xBC3BBE90u, 0x3AFF0D5Cu};
	const auto level1 = AxesBits(draw.bands[2].matrix);
	for (size_t k = 0; k < level1.size(); ++k)
	{
		EXPECT_NEAR(Float(level1.at(k)), Float(k_Level1.at(k)), 1e-8f) << k;
	}
	// no power-up: no band, its angles still
	IconSeedGraphic none;
	EXPECT_TRUE(StepIconSeed(none, look, RunPlaces(), 33, 0xFF, 0u).bands.empty());
	EXPECT_EQ(none.bandAngle, 0.0f);
}

TEST(GestureIconSeeds, AModelIsLeftOutWhenItsReachIsShortOfItsDistance)
{
	// 30 x its half diagonal, squared, against the squared distance 104 from the models' camera
	const glm::vec3 focus(0.0f, 0.0f, 10.0f);
	EXPECT_TRUE(IconModelInReach(focus, 0.34f, 0.1f));
	EXPECT_TRUE(IconModelInReach(focus, 0.33994f, 0.1f));
	EXPECT_FALSE(IconModelInReach(focus, 0.33993f, 0.1f));
	EXPECT_FALSE(IconModelInReach(focus, 0.0f, 0.1f));
}

TEST(GestureIconSeeds, ASpritesCornerIsDrawnWhereTheModelsAre)
{
	// the world's camera somewhere over the land, the models' camera at its eye
	const glm::dmat4 projection(glm::perspective(0.9f, 4.0f / 3.0f, 1.5f, 10000.0f));
	const glm::dmat4 worldView(
	    glm::lookAt(glm::vec3(1000.0f, 120.0f, 1500.0f), glm::vec3(1100.0f, 40.0f, 1600.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
	const glm::dmat4 iconView(glm::lookAt(k_IconModelEye, k_IconModelFocus, glm::vec3(0.0f, 1.0f, 0.0f)));
	const glm::dmat4 iconToClip = projection * iconView;
	const glm::dmat4 worldToClip = projection * worldView;
	const glm::dmat4 clipToWorld = glm::inverse(worldToClip);
	const glm::vec2 screen(1024.0f, 768.0f);
	const auto pixelOf = [&](const glm::dmat4& toClip, glm::vec3 point) {
		const glm::dvec4 clip = toClip * glm::dvec4(glm::dvec3(point), 1.0);
		return glm::dvec3((clip.x / clip.w + 1.0) * 0.5 * screen.x, (1.0 - clip.y / clip.w) * 0.5 * screen.y, clip.w);
	};
	const glm::vec3 corner(0.05f, -0.1f, 10.2f);
	const glm::vec2 offset(478.72f, 330.24f);
	const auto drawn = pixelOf(iconToClip, corner);
	const auto placed = pixelOf(worldToClip, IconPointInWorld(iconToClip, clipToWorld, offset, screen, corner));
	EXPECT_NEAR(placed.x, drawn.x + offset.x, 1e-2);
	EXPECT_NEAR(placed.y, drawn.y + offset.y, 1e-2);
	// at the same depth along the view
	EXPECT_NEAR(placed.z, drawn.z, 1e-3);
	EXPECT_GT(placed.z, 0.0);
	// held on the screen
	const auto held = pixelOf(worldToClip, IconPointInWorld(iconToClip, clipToWorld, {5000.0f, -5000.0f}, screen, corner));
	EXPECT_NEAR(held.x, screen.x - 1.0, 1e-2);
	EXPECT_NEAR(held.y, 0.0, 1e-2);
}
