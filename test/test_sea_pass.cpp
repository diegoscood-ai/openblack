/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// graphics::sea_pass (src/Graphics/SeaPass.h): the planes against the original's exact float bits, the side each
// mechanism keeps, the culling table against the expressions openblack used before sea_pass (C1..C4, copied here as
// the reference), the pass state, the mirror helpers and the colours.

#include <cstdint>

#include <bit>
#include <limits>
#include <optional>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Graphics/SeaPass.h"
#include "Graphics/ShadowMath.h"
#include "Particles/Creators/Mesh.h"

using namespace openblack::graphics;
using namespace openblack::graphics::sea_pass;
using render_modes::Cull;

TEST(SeaPass, PlanesAreTheBinarysDwords)
{
	// the default plane: y = 1, the rest 0
	EXPECT_EQ(std::bit_cast<uint32_t>(k_DefaultPlane.y), 0x3F800000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_DefaultPlane.x), 0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_DefaultPlane.z), 0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_DefaultPlane.w), 0u);
	// the swimmers' plane: y = -1, the other three 0
	EXPECT_EQ(std::bit_cast<uint32_t>(k_SwimPlane.y), 0xBF800000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_SwimPlane.x), 0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_SwimPlane.z), 0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_SwimPlane.w), 0u);
	// the net's and the shark's planes: y = -1, the rest 0
	for (const auto& plane : {k_NetPlane, k_SharkPlane})
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(plane.y), 0xBF800000u);
		EXPECT_EQ(std::bit_cast<uint32_t>(plane.x), 0u);
		EXPECT_EQ(std::bit_cast<uint32_t>(plane.z), 0u);
		EXPECT_EQ(std::bit_cast<uint32_t>(plane.w), 0u);
		EXPECT_EQ(Kept(Mechanism::CutByPlane, plane), SeaPlane::KeepBelow);
	}
}

namespace
{
/// The under-water test on a mirrored point: out if d > 0
bool UnderWaterKeeps(glm::vec4 plane, float y)
{
	const float mirroredY = -y;
	const float d = plane.y * mirroredY + plane.w;
	return !(d > 0.0f);
}
/// The cut-by-plane test on the point: out if d < 0
bool CutKeeps(glm::vec4 plane, float y)
{
	const float d = plane.y * y + plane.w;
	return !(d < 0.0f);
}
} // namespace

TEST(SeaPass, KeptSide)
{
	EXPECT_EQ(Kept(Mechanism::UnderWater, k_DefaultPlane), SeaPlane::KeepAbove);
	EXPECT_EQ(Kept(Mechanism::CutByPlane, k_SwimPlane), SeaPlane::KeepBelow);
	EXPECT_EQ(Kept(Mechanism::CutByPlane, k_DefaultPlane), SeaPlane::KeepAbove);
	EXPECT_EQ(Kept(Mechanism::UnderWater, k_SwimPlane), SeaPlane::KeepBelow);
	EXPECT_EQ(Kept(Mechanism::CutByPlane, glm::vec4(0.0f, 1.0f, 0.0f, 2.0f)), SeaPlane::None);

	for (const float y : {-1.0f, 0.0f, 1.0f})
	{
		for (const auto& plane : {k_DefaultPlane, k_SwimPlane})
		{
			EXPECT_EQ(KeptAt(Kept(Mechanism::UnderWater, plane), y), UnderWaterKeeps(plane, y)) << y;
			EXPECT_EQ(KeptAt(Kept(Mechanism::CutByPlane, plane), y), CutKeeps(plane, y)) << y;
		}
	}
	EXPECT_FALSE(KeptAt(SeaPlane::KeepAbove, -1.0f));
	EXPECT_TRUE(KeptAt(SeaPlane::KeepAbove, 0.0f));
	EXPECT_TRUE(KeptAt(SeaPlane::KeepAbove, 1.0f));
	EXPECT_TRUE(KeptAt(SeaPlane::KeepBelow, -1.0f));
	EXPECT_TRUE(KeptAt(SeaPlane::KeepBelow, 0.0f));
	EXPECT_FALSE(KeptAt(SeaPlane::KeepBelow, 1.0f));
	EXPECT_TRUE(KeptAt(SeaPlane::None, -1.0f));
	EXPECT_TRUE(KeptAt(SeaPlane::None, 1.0f));
}

TEST(SeaPass, FaceCullIsTheOldExpressions)
{
	for (const auto pass : {RenderPass::Main, RenderPass::Reflection})
	{
		const auto state = ForPass(pass);
		const bool cullBack = pass == RenderPass::Reflection; // DrawSceneDesc::cullBack (Game.cpp / Renderer.cpp)
		for (const bool twoSided : {false, true})
		{
			for (const bool mirrorInSea : {false, true})
			{
				// C1 Renderer.cpp DrawMesh: CullFor(twoSided, viewId == Reflection && !mirrorInSea)
				EXPECT_EQ(state.FaceCull(Surface::Model, twoSided, mirrorInSea),
				          render_modes::CullFor(twoSided, pass == RenderPass::Reflection && !mirrorInSea));
			}
		}
		// C2 DrawMoon: CullFor(false, mirrored), mirrored only in the reflection
		EXPECT_EQ(state.FaceCull(Surface::Model, false, false), render_modes::CullFor(false, pass == RenderPass::Reflection));
		// C3 the sky: cullBack ? Cw : Ccw
		EXPECT_EQ(state.FaceCull(Surface::Sky, false, false), cullBack ? Cull::Cw : Cull::Ccw);
		// C4 the land: cullBack ? BGFX_STATE_CULL_CCW : BGFX_STATE_CULL_CW
		EXPECT_EQ(state.FaceCull(Surface::Land, false, false), cullBack ? Cull::Ccw : Cull::Cw);
	}
	// the expected culling table
	EXPECT_EQ(ForPass(RenderPass::Main).FaceCull(Surface::Model, true, false), Cull::None);
	EXPECT_EQ(ForPass(RenderPass::Reflection).FaceCull(Surface::Model, true, true), Cull::None);
	EXPECT_EQ(ForPass(RenderPass::Main).FaceCull(Surface::Model, false, false), Cull::Ccw);
	EXPECT_EQ(ForPass(RenderPass::Reflection).FaceCull(Surface::Model, false, false), Cull::Cw);
	EXPECT_EQ(ForPass(RenderPass::Reflection).FaceCull(Surface::Model, false, true), Cull::Ccw);
}

TEST(SeaPass, PassState)
{
	const auto reflection = ForPass(RenderPass::Reflection);
	EXPECT_TRUE(reflection.mirrored);
	EXPECT_EQ(reflection.landLightScale, 0.5f);
	EXPECT_FALSE(reflection.landWriteZ);
	EXPECT_FALSE(reflection.smallBump);
	const auto main = ForPass(RenderPass::Main);
	EXPECT_FALSE(main.mirrored);
	EXPECT_EQ(main.landLightScale, 1.0f);
	EXPECT_TRUE(main.landWriteZ);
	EXPECT_TRUE(main.smallBump);
}

// The original draws the mirrored land with the same block draw and block texture as the land, in the same material
// mode, so the river channels baked into the block's alpha show the reflection's sky through them as they show the sea
// on the land; only the Z write is off
TEST(SeaPass, MirroredLandKeepsTheLandsBlend)
{
	const auto land =
	    render_modes::State(render_modes::Mode::Landscape, {.writeAlpha = true, .msaa = true, .premultiplied = true});
	const auto mirrored = ForPass(RenderPass::Reflection).LandState(land);
	EXPECT_EQ(mirrored & BGFX_STATE_BLEND_MASK, land & BGFX_STATE_BLEND_MASK);
	EXPECT_NE(land & BGFX_STATE_BLEND_MASK, 0u);
	EXPECT_EQ(mirrored, land & ~BGFX_STATE_WRITE_Z);
	EXPECT_EQ(ForPass(RenderPass::Main).LandState(land), land);
}

TEST(SeaPass, Mirror)
{
	const glm::vec3 p(3.0f, -2.5f, 7.0f);
	EXPECT_EQ(Unmirror(Unmirror(p)), p);
	EXPECT_EQ(Unmirror(p), glm::vec3(3.0f, 2.5f, 7.0f));
	const auto view = glm::lookAt(glm::vec3(10.0f, 20.0f, 30.0f), glm::vec3(0.0f, -5.0f, 2.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const auto a = UnmirrorView(view) * glm::vec4(p, 1.0f);
	const auto b = view * glm::vec4(Unmirror(p), 1.0f);
	for (int i = 0; i < 4; ++i)
	{
		EXPECT_FLOAT_EQ(a[i], b[i]);
	}
	// the same matrix as the moon's view x scale(1, -1, 1) before sea_pass
	const auto scaled = view * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
	for (int c = 0; c < 4; ++c)
	{
		for (int r = 0; r < 4; ++r)
		{
			EXPECT_EQ(UnmirrorView(view)[c][r], scaled[c][r]);
		}
	}
}

TEST(SeaPass, Draws)
{
	EXPECT_TRUE(Cut(SeaPlane::KeepBelow, 0xFF303070u, 0u, RenderPass::Reflection).unmirror);
	EXPECT_FALSE(Cut(SeaPlane::KeepAbove, 0xFFFFFFFFu, 0u, RenderPass::Main).unmirror);
	EXPECT_EQ(Cut(SeaPlane::KeepBelow, 0xFF303070u, 7u, RenderPass::Reflection).light, SeaLight::Cut);
	EXPECT_EQ(Cut(SeaPlane::KeepBelow, 0xFF303070u, 7u, RenderPass::Reflection).specular, 7u);
	const auto hand = UnderWater(k_HandColour, k_HandSpecular);
	EXPECT_FALSE(hand.unmirror);
	EXPECT_EQ(hand.plane, SeaPlane::KeepAbove);
	EXPECT_EQ(hand.light, SeaLight::Constant);
	EXPECT_EQ(hand.argb, 0x65A0A0A0u);
	EXPECT_FALSE(UnderWaterLastDraw().unmirror);
	EXPECT_EQ(UnderWaterLastDraw().plane, SeaPlane::KeepAbove);
	EXPECT_EQ(UnderWaterLastDraw().light, SeaLight::LastDraw);
	EXPECT_EQ(SeaDraw {}.light, SeaLight::Normal);
	EXPECT_EQ(SeaDraw {}.plane, SeaPlane::None);
	const auto atoms = CutAtoms(RenderPass::Main);
	EXPECT_EQ(atoms.light, SeaLight::Cut);
	EXPECT_EQ(atoms.plane, SeaPlane::KeepAbove);
	EXPECT_TRUE(atoms.perInstanceColour);
	EXPECT_FALSE(atoms.unmirror);
	EXPECT_EQ(atoms.argb >> 24, 0xFFu);
	EXPECT_FALSE(Cut(SeaPlane::KeepAbove, 0u, 0u, RenderPass::Main).perInstanceColour);
}

// A PSys mesh atom cut by the sea plane keeps its own alpha: the cut's vertex alpha is the alpha of the colour the
// atom's object is given (its instance's fade), times the cut draw's own alpha, 0xFF
TEST(SeaPass, CutAtomsTakeTheirOwnAlpha)
{
	EXPECT_EQ(CutAtoms(RenderPass::Main).argb >> 24, 0xFFu);
	openblack::particles::mesh_atoms::Instance atom {};
	atom.cutByPlane = true;
	atom.alpha = 1.0f;
	EXPECT_FALSE(openblack::particles::mesh_atoms::Fade(atom).has_value()); // opaque: drawn at full alpha
	atom.alpha = 0.4f;
	ASSERT_TRUE(openblack::particles::mesh_atoms::Fade(atom).has_value());
	EXPECT_FLOAT_EQ(1.0f - *openblack::particles::mesh_atoms::Fade(atom), 0.4f);
	atom.alpha = 1.0f;
	atom.translucent = true; // additive: its row carries its alpha even when whole
	ASSERT_TRUE(openblack::particles::mesh_atoms::Fade(atom).has_value());
	EXPECT_FLOAT_EQ(*openblack::particles::mesh_atoms::Fade(atom), 0.0f);
}

TEST(SeaPass, PackClip)
{
	EXPECT_EQ(PackClip(UnderWater(k_HandColour, 0u)), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
	EXPECT_EQ(PackClip(Cut(SeaPlane::KeepBelow, 0u, 0u, RenderPass::Reflection)), glm::vec4(-1.0f, 1.0f, 0.0f, 0.0f));
	EXPECT_EQ(PackClip(Cut(SeaPlane::KeepAbove, 0u, 0u, RenderPass::Main)), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
	EXPECT_EQ(PackClip(SeaDraw {}), k_NoClip);
}

TEST(SeaPass, Colours)
{
	// the constant colour and specular of the hand, the creature and the swimmers in the pass
	EXPECT_EQ(k_HandColour, 0x65A0A0A0u);
	EXPECT_EQ(k_HandSpecular, 0u);
	EXPECT_EQ(k_CreatureColour, 0x65A0A0D0u);
	EXPECT_EQ(k_CreatureSpecular, 0x30u);
	EXPECT_EQ(k_SwimmerColour, 0xFF303070u);
	EXPECT_EQ(k_SwimmerSpecular, 0u);
	// the creature's limits bit for bit: 100000, 6 and 0.2
	EXPECT_EQ(std::bit_cast<uint32_t>(k_CreatureMaxBlockDistance), 0x47C35000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_CreatureMaxY), 0x40C00000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_CreatureMaxFizz), 0x3E4CCCCDu);
}

namespace
{
/// A land of blocks that all exist and are in view at a distance, but the ones a test marks
struct FakeBlocks
{
	shadow_math::BlockState state {.exists = true, .visible = true, .distance = 500.0f};
	std::optional<std::pair<int, int>> asked;
	[[nodiscard]] shadow_math::BlockAt At()
	{
		return [this](int x, int z) {
			asked = std::pair(x, z);
			return state;
		};
	}
};
} // namespace

TEST(SeaPassCreature, ACreatureInTheShallowsInViewIsMirrored)
{
	FakeBlocks blocks;
	EXPECT_TRUE(ReflectsCreature({1000.0f, 2.0f, 1700.0f}, 0.0f, blocks.At()));
	// the block of cells (100, 170): 16 cells a block
	EXPECT_EQ(blocks.asked, std::optional(std::pair(6, 10)));
}

TEST(SeaPassCreature, ItsYMustBeBelowSixAndItsFizzBelowAFifth)
{
	FakeBlocks blocks;
	EXPECT_TRUE(ReflectsCreature({1000.0f, 5.999f, 1000.0f}, 0.0f, blocks.At()));
	EXPECT_FALSE(ReflectsCreature({1000.0f, 6.0f, 1000.0f}, 0.0f, blocks.At()));
	EXPECT_TRUE(ReflectsCreature({1000.0f, 2.0f, 1000.0f}, 0.19999f, blocks.At()));
	EXPECT_FALSE(ReflectsCreature({1000.0f, 2.0f, 1000.0f}, 0.2f, blocks.At()));
	// a value that is not a number passes the compares
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_TRUE(ReflectsCreature({1000.0f, nan, 1000.0f}, 0.0f, blocks.At()));
	EXPECT_TRUE(ReflectsCreature({1000.0f, 2.0f, 1000.0f}, nan, blocks.At()));
}

TEST(SeaPassCreature, ItsBlockMustExistBeInViewAndNotPastTheVanishDistance)
{
	FakeBlocks blocks;
	blocks.state.distance = 100000.0f;
	EXPECT_TRUE(ReflectsCreature({1000.0f, 2.0f, 1000.0f}, 0.0f, blocks.At()));
	blocks.state.distance = 100000.01f;
	EXPECT_FALSE(ReflectsCreature({1000.0f, 2.0f, 1000.0f}, 0.0f, blocks.At()));
	blocks.state.distance = std::numeric_limits<float>::quiet_NaN();
	EXPECT_TRUE(ReflectsCreature({1000.0f, 2.0f, 1000.0f}, 0.0f, blocks.At()));
	blocks.state = {.exists = true, .visible = false, .distance = 500.0f};
	EXPECT_FALSE(ReflectsCreature({1000.0f, 2.0f, 1000.0f}, 0.0f, blocks.At()));
	blocks.state = {.exists = false, .visible = true, .distance = 500.0f};
	EXPECT_FALSE(ReflectsCreature({1000.0f, 2.0f, 1000.0f}, 0.0f, blocks.At()));
}

TEST(SeaPassCreature, ItsCellMustBeInsideTheFirst512)
{
	FakeBlocks blocks;
	// truncated toward zero: just west of the land is still cell 0
	EXPECT_TRUE(ReflectsCreature({-5.0f, 2.0f, 1000.0f}, 0.0f, blocks.At()));
	EXPECT_FALSE(ReflectsCreature({-10.0f, 2.0f, 1000.0f}, 0.0f, blocks.At()));
	EXPECT_TRUE(ReflectsCreature({5119.0f, 2.0f, 1000.0f}, 0.0f, blocks.At()));
	EXPECT_FALSE(ReflectsCreature({5120.0f, 2.0f, 1000.0f}, 0.0f, blocks.At()));
	EXPECT_FALSE(ReflectsCreature({1000.0f, 2.0f, 5120.0f}, 0.0f, blocks.At()));
}
