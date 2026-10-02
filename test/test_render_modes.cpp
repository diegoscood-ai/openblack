/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// src/Graphics/RenderModes.h against the original: the tables 0xC38728 / 0xC387C8, GJUtils::SetMaterialProperties
// 0x57E120, the scaled ALPHAREF of 0x82E15C, the stage 0 alpha, the materials' culling. And against openblack before
// the module: every draw that wrote its bgfx state by hand must get the same bits from State(), and an L3D primitive
// the same state as the old Renderer::DrawSubMesh, but for the fixes of tmp_dis\unify\U4_changes.md.

#include <cstdint>

#include <array>
#include <cmath>
#include <optional>

#include <bgfx/bgfx.h>
#include <gtest/gtest.h>

#include "Graphics/RenderModes.h"

using namespace openblack::graphics::render_modes;

namespace
{
Mode M(uint32_t index)
{
	return static_cast<Mode>(index);
}

constexpr uint64_t k_WriteMask = BGFX_STATE_WRITE_MASK;
constexpr uint64_t k_Additive = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
constexpr uint64_t k_Premultiplied = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);

// ---- openblack before render_modes (k_MaterialTypeLut of L3DSubMesh.cpp, Renderer::DrawSubMesh) ----
struct OldLut
{
	bool depthWrite;
	int blend; // 0 disabled, 1 standard, 2 additive
	bool threshold;
};
constexpr std::array<OldLut, 19> k_OldLut = {{
    {true, 0, false}, {true, 1, false},  {true, 0, false},  {true, 1, false},  {true, 1, false},
    {true, 1, false}, {false, 1, false}, {false, 1, false}, {false, 1, false}, {true, 1, true},
    {true, 2, true},  {false, 2, true},  {true, 2, false},  {false, 2, false}, {false, 0, false},
    {true, 1, true},  {false, 1, true},  {false, 0, false}, {true, 1, true},
}};

uint64_t OldPrimitiveState(uint64_t state, uint32_t type, bool twoSided, bool reflection, bool isSky, bool msaa)
{
	const auto& prim = k_OldLut[type];
	const bool blended = prim.blend != 0 && !prim.threshold;
	const bool alphaToCoverage = msaa && !reflection && (state & BGFX_STATE_BLEND_MASK) == 0;
	if (!isSky && (state & BGFX_STATE_CULL_MASK) == 0 && !twoSided)
	{
		state |= reflection ? BGFX_STATE_CULL_CW : BGFX_STATE_CULL_CCW;
	}
	if (blended && (state & BGFX_STATE_BLEND_MASK) == 0)
	{
		state &= ~(BGFX_STATE_WRITE_A | (prim.depthWrite ? 0 : BGFX_STATE_WRITE_Z));
		state |= prim.blend == 2 ? k_Additive : BGFX_STATE_BLEND_ALPHA;
	}
	else if (blended && prim.blend == 2 && (state & BGFX_STATE_BLEND_MASK) == BGFX_STATE_BLEND_ALPHA)
	{
		state = (state & ~BGFX_STATE_BLEND_MASK) | k_Additive;
		if (!prim.depthWrite)
		{
			state &= ~BGFX_STATE_WRITE_Z;
		}
	}
	if (prim.threshold && !alphaToCoverage && (state & BGFX_STATE_BLEND_MASK) == 0)
	{
		state |= BGFX_STATE_BLEND_ALPHA;
	}
	return state | (alphaToCoverage && prim.threshold ? BGFX_STATE_BLEND_ALPHA_TO_COVERAGE : 0);
}

// ---- the new DrawSubMesh, as Renderer.cpp calls the module ----
uint64_t NewPrimitiveState(StateOptions options, Table table, std::optional<Mode> forced, uint32_t type, bool twoSided,
                           bool reflection, bool isSky, bool msaa)
{
	const auto& prim = k_OldLut[type];
	const bool blended = Desc(M(type)).blend != Blend::Disabled && !Desc(M(type)).alphaTest;
	EXPECT_EQ(blended, prim.blend != 0 && !prim.threshold) << type;
	const bool modelPass = table == Table::Normal && !forced.has_value();
	const auto mode = Select(forced.value_or(M(type)), table);
	if (!isSky && options.cull == Cull::None)
	{
		options.cull = CullFor(twoSided, reflection);
	}
	return PrimitiveState(mode, options, blended, msaa && !reflection && modelPass);
}

// fs_object's alpha test with the uniforms of PrimitiveAlpha: whether a texel of alpha `texel` (0..255) is kept
bool Keeps(const ShaderAlpha& alpha, uint32_t texel, float opacity)
{
	if (alpha.ref < 0.0f)
	{
		return true;
	}
	const float tested = static_cast<float>(texel) / 255.0f * (alpha.source == AlphaSource::Modulate ? opacity : 1.0f);
	return std::floor(tested * 255.0f + 0.5f) >= std::floor(alpha.ref * 255.0f + 0.5f);
}

// the L3D types: 14 and 17 never come from an L3D file (asserts in L3DSubMesh::Load)
constexpr std::array<uint32_t, 17> k_L3dTypes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 18};
} // namespace

TEST(RenderModes, TablesAsTheBinary)
{
	// 0xC38728 (19 {fn, word} pairs), dumped from runblack.exe
	constexpr std::array<uint32_t, 19> k_Functions = {0x82D470, 0x82D5C0, 0x82D820, 0x82D920, 0x82DC20, 0x82DD90, 0x82DF10,
	                                                  0x82D6F0, 0x82DAA0, 0x82E080, 0x82E830, 0x82E9C0, 0x82EB50, 0x82ECD0,
	                                                  0x82DD90, 0x82E470, 0x82E6A0, 0x82D820, 0x82E2A0};
	// 0xC387C8, as the index of the mode with the same function
	constexpr std::array<uint32_t, 19> k_GlobalAlpha = {1, 1, 3, 3, 5, 5, 6, 7, 8, 15, 10, 11, 12, 13, 14, 15, 16, 3, 18};
	for (uint32_t i = 0; i < k_ModeCount; ++i)
	{
		EXPECT_EQ(k_Modes[i].function, k_Functions[i]) << i;
		EXPECT_EQ(static_cast<uint32_t>(Select(M(i), Table::GlobalAlpha)), k_GlobalAlpha[i]) << i;
		EXPECT_EQ(Select(M(i), Table::Normal), M(i));
		// same function, same states
		EXPECT_EQ(Desc(Select(M(i), Table::GlobalAlpha)).function, k_Functions[k_GlobalAlpha[i]]) << i;
	}
	// 14 = 5 and 17 = 2 (fix 6: the old LUT had both without Z write)
	EXPECT_EQ(State(Mode::Landscape), State(Mode::AlphaTexturedAlpha));
	EXPECT_EQ(State(Mode::Mode17), State(Mode::Textured));
	// the six alpha tested modes
	for (uint32_t i = 0; i < k_ModeCount; ++i)
	{
		const bool tested = i == 9 || i == 10 || i == 11 || i == 15 || i == 16 || i == 18;
		EXPECT_EQ(k_Modes[i].alphaTest, tested) << i;
	}
}

TEST(RenderModes, StatesOfTheModes)
{
	constexpr uint64_t k_Depth = BGFX_STATE_DEPTH_TEST_GREATER;
	// 6: SA / ISA, no Z write (0x82DF10)
	EXPECT_EQ(State(Mode::AlphaTexturedAlphaNz), BGFX_STATE_WRITE_RGB | k_Depth | BGFX_STATE_BLEND_ALPHA);
	// 10 / 11: SA / ONE (0x82E87C), 11 without Z (0x82EAAF)
	EXPECT_EQ(State(Mode::AlphaTexturedAlphaAdditiveChroma), BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z | k_Depth | k_Additive);
	EXPECT_EQ(State(Mode::AlphaTexturedAlphaAdditiveChromaNz), BGFX_STATE_WRITE_RGB | k_Depth | k_Additive);
	// 16 without Z (0x82E78F)
	EXPECT_EQ(State(Mode::TexturedChromaAlphaNz), BGFX_STATE_WRITE_RGB | k_Depth | BGFX_STATE_BLEND_ALPHA);
	// 18: ZERO / ONE (0x82E320, 0x82E2F8), Z
	EXPECT_EQ(State(Mode::ChromaJustZ), BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z | k_Depth |
	                                        BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ZERO, BGFX_STATE_BLEND_ONE));
	// the inline SetMaterial's culling: ((~flags5) & 1) * 2 + 1
	EXPECT_EQ(CullFor(true, false), Cull::None);
	EXPECT_EQ(CullFor(false, false), Cull::Ccw);
	EXPECT_EQ(CullFor(false, true), Cull::Cw);
}

// Every hand-written state of the inventory (lh3d_render_modes_openblack.md section 3), with the old expression
TEST(RenderModes, SitesKeepTheirBits)
{
	constexpr uint64_t k_Depth = BGFX_STATE_DEPTH_TEST_GREATER;
	// #1 DrawCloud, #2 DrawMist, #4 DrawBoatSprite, #5 DrawHumanShadows: mode 6
	EXPECT_EQ(State(materials::k_Smoke), BGFX_STATE_WRITE_RGB | k_Depth | BGFX_STATE_BLEND_ALPHA);
	EXPECT_EQ(State(Mode::AlphaTexturedAlphaNz), BGFX_STATE_WRITE_RGB | k_Depth | BGFX_STATE_BLEND_ALPHA);
	// #3 DrawRainTile
	EXPECT_EQ(State(materials::k_Atmos, {.extra = BGFX_STATE_PT_LINES}),
	          BGFX_STATE_WRITE_RGB | k_Depth | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_PT_LINES);
	// #6 DrawFishShoals
	EXPECT_EQ(State(materials::k_Misc0, {.zFunc = ZFunc::Always}), BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ALPHA);
	// #8 DrawChimneySmoke (BGFX_STATE_BLEND_EQUATION_ADD is 0)
	EXPECT_EQ(State(materials::k_Smoke, {.writeAlpha = true, .premultiplied = true}),
	          0 | k_Depth | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | k_Premultiplied |
	              BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_ADD));
	// #9 drawSprite
	for (const bool additive : {false, true})
	{
		EXPECT_EQ(State(additive ? Mode::AlphaTexturedAlphaAdditiveNz : Mode::AlphaTexturedAlphaNz,
		                {.writeAlpha = true, .premultiplied = true}),
		          0 | k_Depth | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | (additive ? k_Additive : k_Premultiplied) |
		              BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_ADD));
	}
	// #10-#12 the PSys sprites, chains and surfaces: 13 / 6 / 12 / 5
	for (const bool additive : {false, true})
	{
		for (const bool writeDepth : {false, true})
		{
			const auto mode =
			    ModeFromProperties(Mode::AlphaTexturedAlphaNz, {.additive = additive, .zWrite = writeDepth, .alpha = true});
			EXPECT_EQ(State(mode), BGFX_STATE_WRITE_RGB | k_Depth | (additive ? k_Additive : BGFX_STATE_BLEND_ALPHA) |
			                           (writeDepth ? BGFX_STATE_WRITE_Z : 0));
		}
	}
	// #15 DrawSun (and its glare), #16 the moon's halo, #18 DrawWaterRings, #19 the hand's glow on the sea: mode 13
	EXPECT_EQ(State(Mode::AlphaTexturedAlphaAdditiveNz), BGFX_STATE_WRITE_RGB | k_Depth | k_Additive);
	EXPECT_EQ(State(materials::k_SmokeAdditive), BGFX_STATE_WRITE_RGB | k_Depth | k_Additive);
	EXPECT_EQ(State(materials::k_AtmosAdditive, {.zFunc = ZFunc::Always}), BGFX_STATE_WRITE_RGB | k_Additive);
	// #17 the moon
	for (const bool mirrored : {false, true})
	{
		EXPECT_EQ(State(Mode::AlphaTextured, {.cull = CullFor(false, mirrored), .zWrite = false}),
		          BGFX_STATE_WRITE_RGB | k_Depth | BGFX_STATE_BLEND_ALPHA |
		              (mirrored ? BGFX_STATE_CULL_CW : BGFX_STATE_CULL_CCW));
	}
	// #20 the sea
	EXPECT_EQ(State(Mode::AlphaTexturedAlpha,
	                {.zFunc = ZFunc::Always, .zWrite = false, .writeAlpha = true, .msaa = true, .blendInShader = true}),
	          BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);
	// #21 the land, and mirrored
	for (const bool reflection : {false, true})
	{
		for (const bool cullBack : {false, true})
		{
			constexpr uint64_t k_Land = 0u | k_WriteMask | k_Depth | k_Premultiplied | BGFX_STATE_MSAA;
			const auto land = State(Mode::Landscape, {.writeAlpha = true, .msaa = true, .premultiplied = true});
			EXPECT_EQ((reflection ? land & ~BGFX_STATE_WRITE_Z : land) | (cullBack ? BGFX_STATE_CULL_CCW : BGFX_STATE_CULL_CW),
			          (reflection ? k_Land & ~BGFX_STATE_WRITE_Z : k_Land) |
			              (cullBack ? BGFX_STATE_CULL_CCW : BGFX_STATE_CULL_CW));
		}
	}
	// #22 DrawHandToolTip, #23 DrawScreenOverlay
	EXPECT_EQ(State(Mode::TexturedChromaAlphaNz, {.zFunc = ZFunc::Always}), BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ALPHA);
	EXPECT_EQ(State(Mode::SmoothAlpha, {.zFunc = ZFunc::Always, .zWrite = false}),
	          BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ALPHA);
}

// The L3D primitives: every type in every pass of DrawSubMesh, against the old code; only the fixes may differ
TEST(RenderModes, L3DPrimitivesAsBeforeButTheFixes)
{
	constexpr uint64_t k_OldModel = BGFX_STATE_WRITE_MASK | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;
	constexpr uint64_t k_OldFading = 0u | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER |
	                                 BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;
	constexpr uint64_t k_OldAdditive = 0u | BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | k_Additive | BGFX_STATE_MSAA;
	constexpr uint64_t k_OldHandShadow =
	    BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_EQUAL | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;
	constexpr uint64_t k_OldSky = 0 | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_CULL_CW |
	                              BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;
	for (const auto type : k_L3dTypes)
	{
		for (const bool twoSided : {false, true})
		{
			for (const bool reflection : {false, true})
			{
				for (const bool msaa : {false, true})
				{
					// the model pass: fix 1 (10, 11: SA / ONE, 11 without Z), fix 2 (16 without Z), fix 3 (18: ZERO / ONE)
					const auto oldModel = OldPrimitiveState(k_OldModel, type, twoSided, reflection, false, msaa);
					const auto newModel =
					    NewPrimitiveState(k_ModelPass, Table::Normal, std::nullopt, type, twoSided, reflection, false, msaa);
					const bool fixedModel = type == 10 || type == 11 || type == 16 || type == 18;
					if (!fixedModel)
					{
						EXPECT_EQ(newModel, oldModel) << "model pass, type " << type;
					}
					// the sky: the whole mesh culled
					if (!fixedModel)
					{
						EXPECT_EQ(NewPrimitiveState({.cull = Cull::Cw, .writeAlpha = true, .msaa = true}, Table::Normal,
						                            std::nullopt, type, twoSided, reflection, true, msaa),
						          OldPrimitiveState(k_OldSky, type, twoSided, reflection, true, msaa))
						    << "sky, type " << type;
					}
					// the fading pass (table 0xC387C8): fix 5 (6, 7, 8, 16 without Z) and fixes 1-3
					const auto oldFading = OldPrimitiveState(k_OldFading, type, twoSided, reflection, false, msaa);
					const auto newFading = NewPrimitiveState({.msaa = true}, Table::GlobalAlpha, std::nullopt, type, twoSided,
					                                         reflection, false, msaa);
					const bool fixedFading = fixedModel || type == 6 || type == 7 || type == 8;
					if (!fixedFading)
					{
						EXPECT_EQ(newFading, oldFading) << "fading, type " << type;
					}
					// the PSys additive atoms: always mode 13
					EXPECT_EQ(NewPrimitiveState({.msaa = true}, Table::GlobalAlpha, Mode::AlphaTexturedAlphaAdditiveNz, type,
					                            twoSided, reflection, false, msaa),
					          OldPrimitiveState(k_OldAdditive, type, twoSided, reflection, false, msaa))
					    << "additive atom, type " << type;
					// the hand's shadow on the objects (main view only): mode 6 for every primitive (fix 7: the old code
					// turned 12 / 13 additive) and the shadow material's CCW (fix 9: the old code took each primitive's)
					const auto newShadow = NewPrimitiveState({.zFunc = ZFunc::Equal, .cull = Cull::Ccw, .msaa = true},
					                                         Table::Normal, Mode::AlphaTexturedAlphaNz, type, twoSided,
					                                         false, false, msaa);
					EXPECT_NE(newShadow & BGFX_STATE_CULL_CCW, 0u) << "hand shadow, type " << type;
					if (type != 12 && type != 13 && !twoSided)
					{
						EXPECT_EQ(newShadow, OldPrimitiveState(k_OldHandShadow, type, twoSided, false, false, msaa))
						    << "hand shadow, type " << type;
					}
				}
			}
		}
	}
}

TEST(RenderModes, ModeFromProperties)
{
	// 0x57E120: 4 -> 6; !alpha -> 3; additive -> 13; with Z 6 -> 5, 13 -> 12, 8 -> 3, 16 -> 9; without 5 -> 6, 12 -> 13,
	// 3 / 2 -> 8, 9 -> 16
	const MaterialProperties alphaOnly {.alpha = true};
	EXPECT_EQ(ModeFromProperties(Mode::AlphaTextured, alphaOnly), Mode::AlphaTexturedAlphaNz);
	EXPECT_EQ(ModeFromProperties(Mode::AlphaTexturedAlpha, alphaOnly), Mode::AlphaTexturedAlphaNz);
	EXPECT_EQ(ModeFromProperties(Mode::Textured, alphaOnly), Mode::TexturedAlphaNz);
	EXPECT_EQ(ModeFromProperties(Mode::TexturedChroma, alphaOnly), Mode::TexturedChromaAlphaNz);
	EXPECT_EQ(ModeFromProperties(Mode::AlphaTexturedAlphaAdditive, alphaOnly), Mode::AlphaTexturedAlphaAdditiveNz);
	EXPECT_EQ(ModeFromProperties(Mode::Smooth, alphaOnly), Mode::Smooth);
	const MaterialProperties z {.zWrite = true, .alpha = true};
	EXPECT_EQ(ModeFromProperties(Mode::AlphaTextured, z), Mode::AlphaTexturedAlpha);
	EXPECT_EQ(ModeFromProperties(Mode::TexturedAlphaNz, z), Mode::TexturedAlpha);
	EXPECT_EQ(ModeFromProperties(Mode::TexturedChromaAlphaNz, z), Mode::TexturedChroma);
	EXPECT_EQ(ModeFromProperties(Mode::Textured, z), Mode::Textured);
	EXPECT_EQ(ModeFromProperties(Mode::Smooth, {.zWrite = true}), Mode::TexturedAlpha);
	EXPECT_EQ(ModeFromProperties(Mode::Smooth, {}), Mode::TexturedAlphaNz);
	EXPECT_EQ(ModeFromProperties(Mode::ChromaJustZ, {.additive = true, .zWrite = true}), Mode::AlphaTexturedAlphaAdditive);
	EXPECT_EQ(ModeFromProperties(Mode::ChromaJustZ, {.additive = true}), Mode::AlphaTexturedAlphaAdditiveNz);
}

TEST(RenderModes, AlphaRef)
{
	// the normal table: the material's +4 as it is (no - 5: fix 4)
	EXPECT_EQ(AlphaRef(Mode::TexturedChroma, Table::Normal, 200), 200);
	EXPECT_EQ(AlphaRef(Mode::ChromaJustZ, Table::Normal, 0x32), 0x32);
	// [0xECA658] / [0xECA65C]
	EXPECT_EQ(AlphaRef(Mode::TexturedChroma, Table::Normal, 200, 10), 10);
	// no alpha test
	EXPECT_EQ(AlphaRef(Mode::AlphaTexturedAlphaNz, Table::Normal, 200), 0);
	// 0x82E557: max(0, ftol(ref * A / 255 - 5)), only for 9 / 15 with the table 0xC387C8
	EXPECT_EQ(AlphaRef(Mode::TexturedChromaAlpha, Table::GlobalAlpha, 200, std::nullopt, 255), 195);
	EXPECT_EQ(AlphaRef(Mode::TexturedChromaAlpha, Table::GlobalAlpha, 200, std::nullopt, 128), 95); // 100.39 - 5
	EXPECT_EQ(AlphaRef(Mode::TexturedChromaAlpha, Table::GlobalAlpha, 200, std::nullopt, 6), 0);    // 4.7 - 5 < 0
	EXPECT_EQ(AlphaRef(Mode::TexturedChromaAlpha, Table::GlobalAlpha, 200, uint8_t {100}, 255), 95);
	EXPECT_EQ(AlphaRef(Mode::TexturedChromaAlphaNz, Table::GlobalAlpha, 200, std::nullopt, 128), 200);
	EXPECT_EQ(AlphaRef(Mode::AlphaTexturedAlphaAdditiveChroma, Table::GlobalAlpha, 200, std::nullopt, 128), 200);
	EXPECT_EQ(AlphaByte(1.0f), 255);
	EXPECT_EQ(AlphaByte(0.0f), 0);
	EXPECT_EQ(AlphaByte(0.5f), 128);
}

TEST(RenderModes, Materials)
{
	// the inline SetMaterial's culling from +5 bit 0: the two-sided materials draw both faces
	for (const auto& material : {materials::k_Smoke, materials::k_SmokeAdditive, materials::k_Misc0, materials::k_Atmos,
	                             materials::k_AtmosAdditive})
	{
		EXPECT_TRUE(material.TwoSided());
		EXPECT_EQ(State(material), State(material.mode));
		EXPECT_EQ(State(material, {}, true), State(material.mode));
	}
	EXPECT_NE(materials::k_Atmos.flags & k_Tiling, 0);
	// CreateMaterial 0x82FD30 leaves +5 = 0: CCW (the hand's shadow material, 0x87FE12), CW through a mirror
	const Material created {Mode::AlphaTexturedAlphaNz};
	EXPECT_FALSE(created.TwoSided());
	EXPECT_EQ(State(created), State(created.mode) | BGFX_STATE_CULL_CCW);
	EXPECT_EQ(State(created, {}, true), State(created.mode) | BGFX_STATE_CULL_CW);
	// a cull in the options wins
	EXPECT_EQ(State(materials::k_Smoke, {.cull = Cull::Ccw}), State(created.mode) | BGFX_STATE_CULL_CCW);
}

// fs_object's alpha test and stage 0 alpha (u_skyAlphaThreshold.y / .w) in the mode a primitive is drawn in
TEST(RenderModes, PrimitiveAlpha)
{
	// the modes that neither blend nor test, the SELECTARG1(TEXTURE) ones and the MODULATE ones
	EXPECT_EQ(PrimitiveAlpha(Mode::Smooth, Table::Normal, 0).source, AlphaSource::None);
	EXPECT_EQ(PrimitiveAlpha(Mode::Textured, Table::Normal, 0).source, AlphaSource::None);
	EXPECT_EQ(PrimitiveAlpha(Mode::AlphaTextured, Table::Normal, 0).source, AlphaSource::Texture);
	EXPECT_EQ(PrimitiveAlpha(Mode::TexturedChroma, Table::Normal, 0x96).source, AlphaSource::Texture); // 0x82E120
	EXPECT_EQ(PrimitiveAlpha(Mode::ChromaJustZ, Table::Normal, 0x96).source, AlphaSource::Texture);    // 0x82E384
	EXPECT_EQ(PrimitiveAlpha(Mode::TexturedChromaAlpha, Table::Normal, 0x96).source, AlphaSource::Modulate); // 0x82E510
	EXPECT_EQ(PrimitiveAlpha(Mode::AlphaTexturedAlphaAdditiveChroma, Table::Normal, 0x96).source, AlphaSource::Modulate);
	EXPECT_EQ(PrimitiveAlpha(Mode::TexturedChromaAlphaNz, Table::Normal, 0x96).source, AlphaSource::Modulate);
	EXPECT_EQ(PrimitiveAlpha(Mode::TexturedAlpha, Table::Normal, 0).source, AlphaSource::Modulate);
	// no alpha test: -1
	EXPECT_EQ(PrimitiveAlpha(Mode::AlphaTexturedAlphaNz, Table::Normal, 0x96).ref, -1.0f);
	// fix 8: a chroma primitive with ALPHAREF 0 keeps every texel and blends with its texture alpha (mode 9: SELECTARG1,
	// SRCALPHA / INVSRCALPHA); the old shader drew it opaque
	const auto zeroRef = PrimitiveAlpha(Mode::TexturedChroma, Table::Normal, 0);
	EXPECT_EQ(zeroRef.ref, 0.0f);
	EXPECT_EQ(zeroRef.source, AlphaSource::Texture);
	EXPECT_TRUE(Keeps(zeroRef, 0, 1.0f));
	// fix 4 (stage output): type 9 fading at A = 128 is drawn in 15 (0xC387C8); ALPHAREF 150 * 128 / 255 - 5 = 70
	// (0x82E557) is tested against texture x 128 / 255 (MODULATE 0x82E510): the texels from about 140 stay
	const auto fading = PrimitiveAlpha(Select(Mode::TexturedChroma, Table::GlobalAlpha), Table::GlobalAlpha, 0x96, 128);
	EXPECT_EQ(std::lround(fading.ref * 255.0f), 70);
	EXPECT_EQ(fading.source, AlphaSource::Modulate);
	const float opacity = 128.0f / 255.0f;
	EXPECT_FALSE(Keeps(fading, 100, opacity)); // the raw texture alpha (before) kept it: 100 >= 70
	EXPECT_FALSE(Keeps(fading, 138, opacity));
	EXPECT_TRUE(Keeps(fading, 140, opacity));
	EXPECT_TRUE(Keeps(fading, 255, opacity));
	// the same primitive not fading: mode 9, its own ALPHAREF on the texture alpha
	const auto opaque = PrimitiveAlpha(Mode::TexturedChroma, Table::Normal, 0x96);
	EXPECT_FALSE(Keeps(opaque, 149, 1.0f));
	EXPECT_TRUE(Keeps(opaque, 150, 1.0f));
	// a fading textured primitive (2 -> 3, 0x82D920) blends with texture x object alpha
	EXPECT_EQ(PrimitiveAlpha(Select(Mode::Textured, Table::GlobalAlpha), Table::GlobalAlpha, 0, 128).source,
	          AlphaSource::Modulate);
	// a forced mode decides the alpha test too (fix 10): a chroma primitive in a PSys additive atom (13) or under the
	// hand's shadow (6) is not alpha tested, and its alpha is texture x diffuse
	for (const auto forced : {Mode::AlphaTexturedAlphaAdditiveNz, Mode::AlphaTexturedAlphaNz})
	{
		for (const auto table : {Table::Normal, Table::GlobalAlpha})
		{
			const auto alpha = PrimitiveAlpha(Select(forced, table), table, 0x96, 128);
			EXPECT_EQ(alpha.ref, -1.0f);
			EXPECT_EQ(alpha.source, AlphaSource::Modulate);
		}
	}
	// the static and physics shadows (normal table): the material's ALPHAREF as it is, -1 without the test; ref 0 keeps
	// every texel, as the old 0 = solid
	for (const auto type : k_L3dTypes)
	{
		const auto alpha = PrimitiveAlpha(M(type), Table::Normal, 0x96);
		EXPECT_EQ(alpha.ref >= 0.0f, k_OldLut[type].threshold) << type;
		if (alpha.ref >= 0.0f)
		{
			EXPECT_EQ(std::lround(alpha.ref * 255.0f), 0x96) << type;
		}
	}
	EXPECT_TRUE(Keeps(PrimitiveAlpha(Mode::TexturedChroma, Table::Normal, 0), 0, 1.0f));
}
