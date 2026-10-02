/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// graphics::sea_pass (src/Graphics/SeaPass.h): the planes against their bytes in runblack.exe (fn_0084A380 0x84A39A,
// GLandscape::Draw 0x5E4C4E), the side each mechanism keeps, the culling table against the expressions openblack used
// before sea_pass (C1..C4, copied here as the reference), the pass state, the mirror helpers and the colours.

#include <bit>
#include <cstdint>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Graphics/SeaPass.h"

using namespace openblack::graphics;
using namespace openblack::graphics::sea_pass;
using render_modes::Cull;

TEST(SeaPass, PlanesAreTheBinarysDwords)
{
	// fn_0084A380 0x84A390..0x84A3AE: c7 05 2c 31 f0 00 00 00 80 3f, the rest 0
	EXPECT_EQ(std::bit_cast<uint32_t>(k_DefaultPlane.y), 0x3F800000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_DefaultPlane.x), 0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_DefaultPlane.z), 0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_DefaultPlane.w), 0u);
	// 0x5E4C4E: c7 44 24 44 00 00 80 bf, the other three esi = 0
	EXPECT_EQ(std::bit_cast<uint32_t>(k_SwimPlane.y), 0xBF800000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_SwimPlane.x), 0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_SwimPlane.z), 0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_SwimPlane.w), 0u);
}

namespace
{
/// fn_00850FC0 0x85111C..0x851149 on a mirrored point: out if d > 0
bool UnderWaterKeeps(glm::vec4 plane, float y)
{
	const float mirroredY = -y; // fsubp 0x851094
	const float d = plane.y * mirroredY + plane.w;
	return !(d > 0.0f);
}
/// fn_00858BA0 0x858D49..0x858D80 on the point: out if d < 0
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
	// the table of PLAN_4 3.2
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
	// SetColorSpecular's edx and pushed dword (bytes: 6a 00 ba a0 a0 a0 65 at 0x5E496C, 6a 30 ba d0 a0 a0 65 at 0x5E4ACD,
	// 56 ba 70 30 30 ff at 0x5E4C68)
	EXPECT_EQ(k_HandColour, 0x65A0A0A0u);
	EXPECT_EQ(k_HandSpecular, 0u);
	EXPECT_EQ(k_CreatureColour, 0x65A0A0D0u);
	EXPECT_EQ(k_CreatureSpecular, 0x30u);
	EXPECT_EQ(k_SwimmerColour, 0xFF303070u);
	EXPECT_EQ(k_SwimmerSpecular, 0u);
	// [0xC37200] 00 50 c3 47, [0x8AB35C] 00 00 c0 40, [0x8AB244] cd cc 4c 3e
	EXPECT_EQ(std::bit_cast<uint32_t>(k_CreatureMaxBlockDistance), 0x47C35000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_CreatureMaxY), 0x40C00000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_CreatureMaxA0), 0x3E4CCCCDu);
}
