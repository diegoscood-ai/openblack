/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <vector>

#include <bgfx/defines.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Graphics/BonePalette.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderSamplers.h"

// The palette's shader as the game embeds it, compiled for Vulkan, and the one for meshes without bones
#include "generated/shaders/spirv/vs_object_palette_instanced.sc.bin.h"
#include "generated/shaders/spirv/vs_object_static_instanced.sc.bin.h"

using namespace openblack::graphics;

namespace
{

/// A bone turned, scaled and moved, as a posed bone is
glm::mat4 Bone(float angle, glm::vec3 offset)
{
	auto bone = glm::translate(glm::mat4(1.0f), offset);
	bone = glm::rotate(bone, angle, glm::normalize(glm::vec3(0.3f, 1.0f, -0.2f)));
	return glm::scale(bone, glm::vec3(1.0f, 1.5f, 0.75f));
}

} // namespace

TEST(BonePalette, BonesComeBackAsTheyWent)
{
	std::vector<glm::vec4> texels;
	const std::array first {Bone(0.5f, {1.0f, 2.0f, 3.0f}), Bone(-1.25f, {-4.0f, 0.5f, 7.0f})};
	const std::array second {Bone(2.0f, {0.0f, -1.0f, 0.25f}), Bone(0.0f, {0.0f, 0.0f, 0.0f}), Bone(3.0f, {9.0f, 9.0f, -9.0f})};

	EXPECT_EQ(bone_palette::Append(texels, first), 0);
	// The next instance's bones start after the first's
	EXPECT_EQ(bone_palette::Append(texels, second), first.size());
	EXPECT_EQ(texels.size(), (first.size() + second.size()) * bone_palette::k_TexelsPerBone);

	// Bit for bit: the palette holds the very floats the uniforms would
	for (uint32_t i = 0; i < first.size(); ++i)
	{
		EXPECT_EQ(bone_palette::BoneAt(texels, i), first.at(i)) << i;
	}
	for (uint32_t i = 0; i < second.size(); ++i)
	{
		EXPECT_EQ(bone_palette::BoneAt(texels, static_cast<uint32_t>(first.size()) + i), second.at(i)) << i;
	}
}

TEST(BonePalette, EachTexelIsAColumnAsTheUniformHoldsIt)
{
	std::vector<glm::vec4> texels;
	auto bone = Bone(0.75f, {3.0f, -2.0f, 1.0f});
	// Even a last row that is not (0, 0, 0, 1) goes through untouched
	bone[1][3] = 0.125f;
	bone_palette::Append(texels, std::span(&bone, 1));
	ASSERT_EQ(texels.size(), 4);
	for (int column = 0; column < 4; ++column)
	{
		EXPECT_EQ(texels.at(static_cast<size_t>(column)), bone[column]) << column;
	}
}

TEST(BonePalette, TheTextureHasRowsEnoughForTheBones)
{
	EXPECT_EQ(bone_palette::RowsFor(0), 1);
	EXPECT_EQ(bone_palette::RowsFor(1), 1);
	EXPECT_EQ(bone_palette::RowsFor(bone_palette::k_Width), 1);
	EXPECT_EQ(bone_palette::RowsFor(bone_palette::k_Width + 1), 2);
	// A thousand villagers of 22 bones: 88,000 texels
	EXPECT_EQ(bone_palette::RowsFor(1000 * 22 * bone_palette::k_TexelsPerBone), 86);
	// A bone never runs on into the next row
	EXPECT_EQ(bone_palette::k_Width % bone_palette::k_TexelsPerBone, 0);
}

TEST(BonePalette, TheFirstBoneRidesInTheMatrixWithoutMovingIt)
{
	auto placement = glm::scale(glm::translate(glm::mat4(1.0f), {120.0f, 4.0f, -88.0f}), glm::vec3(1.1f));
	// The fade and the texture offset keep their places
	placement[0][3] = 0.25f;
	placement[1][3] = -1.5f;
	for (const uint32_t first : {0u, 22u, 21'978u, 1'000'000u})
	{
		auto model = placement;
		bone_palette::SetFirstBone(model, first);
		EXPECT_EQ(bone_palette::FirstBone(model), first);
		// Only the third column's w carries it, which the object shaders never read
		model[2][3] = 0.0f;
		EXPECT_EQ(model, placement);
	}
}

TEST(BonePalette, OnlyDepthWritingUnblendedDrawsMayChangeOrder)
{
	const uint64_t opaque = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_CULL_CCW;
	EXPECT_TRUE(bone_palette::DrawsInAnyOrder(opaque | BGFX_STATE_DEPTH_TEST_LESS));
	EXPECT_TRUE(bone_palette::DrawsInAnyOrder(opaque | BGFX_STATE_DEPTH_TEST_LEQUAL));
	EXPECT_TRUE(bone_palette::DrawsInAnyOrder(opaque | BGFX_STATE_DEPTH_TEST_GREATER));
	EXPECT_TRUE(bone_palette::DrawsInAnyOrder(opaque | BGFX_STATE_DEPTH_TEST_GEQUAL));
	// Without a depth test, or one that does not order, the last draw wins
	EXPECT_FALSE(bone_palette::DrawsInAnyOrder(opaque));
	EXPECT_FALSE(bone_palette::DrawsInAnyOrder(opaque | BGFX_STATE_DEPTH_TEST_ALWAYS));
	EXPECT_FALSE(bone_palette::DrawsInAnyOrder(opaque | BGFX_STATE_DEPTH_TEST_EQUAL));
	// Without writing the depth, a later draw covers an earlier one
	EXPECT_FALSE(bone_palette::DrawsInAnyOrder((opaque & ~BGFX_STATE_WRITE_Z) | BGFX_STATE_DEPTH_TEST_LESS));
	// Blended, the order of the draws is the order of the blends
	EXPECT_FALSE(bone_palette::DrawsInAnyOrder(opaque | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_BLEND_ALPHA));
	EXPECT_FALSE(bone_palette::DrawsInAnyOrder(opaque | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_BLEND_ADD));
}

TEST(BonePalette, TheModelPassDrawsItsOpaqueModesInAnyOrder)
{
	using render_modes::Mode;
	const auto inAnyOrder = [](Mode mode) {
		return bone_palette::DrawsInAnyOrder(render_modes::PrimitiveState(mode, render_modes::k_ModelPass, false));
	};
	// Most villagers' and animals' meshes are of mode 2
	EXPECT_TRUE(inAnyOrder(Mode::Textured));
	EXPECT_TRUE(inAnyOrder(Mode::Smooth));
	// The chroma modes blend their alpha, a texture's filtered edges too; the others blend or write no depth
	EXPECT_FALSE(inAnyOrder(Mode::TexturedChroma));
	EXPECT_FALSE(inAnyOrder(Mode::AlphaTextured));
	EXPECT_FALSE(inAnyOrder(Mode::AlphaTexturedAlphaNoZWrite));
	EXPECT_FALSE(inAnyOrder(Mode::AlphaTexturedAlphaAdditive));
}

TEST(BonePalette, TheShaderReadsItsBonesFromThePaletteAlone)
{
	// No more is uploaded with each draw than for a mesh without bones, but for the palette's own vec4
	const auto palette = shader_samplers::ReadSpirvUniformBufferSize(vs_object_palette_instanced_spv);
	const auto single = shader_samplers::ReadSpirvUniformBufferSize(vs_object_static_instanced_spv);
	ASSERT_TRUE(palette.has_value());
	ASSERT_TRUE(single.has_value());
	EXPECT_LE(*palette, *single + (4 * sizeof(float)));
	// The bones are read from the palette at its own stage
	const auto samplers = shader_samplers::ReadSpirvSamplers(vs_object_palette_instanced_spv);
	ASSERT_TRUE(samplers.has_value());
	const auto found = std::ranges::find(*samplers, std::string("s_bonePalette"), &shader_samplers::Sampler::name);
	ASSERT_NE(found, samplers->end());
	EXPECT_EQ(found->stage, 2);
}
