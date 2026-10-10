/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The collar of the temples' leash posts: leash.l3d names a skin it does not carry, and the game gives each of its
// textured primitives the rope's texture instead.

#include <cstdint>
#include <cstdlib>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "Resources/SharedAssets.h"
#include "support/BgfxShutdown.h"

using namespace openblack;

namespace
{
constexpr uint32_t k_Collar = resources::shared_assets::k_LeashCollarTexture.value();

/// bgfx with no renderer, for the sub-meshes' buffers
bool InitNoopBgfx()
{
	for (const auto* name : {"game", "graphics"})
	{
		if (spdlog::get(name) == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>(name);
		}
	}
	bgfx::renderFrame(); // single-threaded
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	return bgfx::init(init);
}

/// One triangle in a material with the skin `skin`
graphics::L3DSubMesh::GeneratedPrimitive Triangle(uint32_t skin)
{
	graphics::L3DSubMesh::GeneratedPrimitive primitive {};
	primitive.material.skinID = skin;
	primitive.positions = {glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)};
	primitive.uvs = {glm::vec2(0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 1.0f)};
	primitive.indices = {0, 1, 2};
	return primitive;
}
} // namespace

TEST(LeashCollar, OnlyTheTexturedPrimitivesTakeTheNewSkin)
{
	ASSERT_TRUE(InitNoopBgfx());
	const test::BgfxShutdown bgfxShutdown;
	{
		graphics::L3DMesh mesh("collar");
		ASSERT_TRUE(mesh.LoadGenerated({Triangle(graphics::L3DSubMesh::k_NoSkin), Triangle(7u)}));
		mesh.SetSkinOfTextured(k_Collar);
		ASSERT_EQ(mesh.GetNumSubMeshes(), 1);
		const auto& primitives = mesh.GetSubMeshes()[0]->GetPrimitives();
		ASSERT_EQ(primitives.size(), 2u);
		// the untextured one stays untextured, the textured one takes the new skin
		EXPECT_EQ(primitives[0].skinID, graphics::L3DSubMesh::k_NoSkin);
		EXPECT_EQ(primitives[1].skinID, k_Collar);
	}
}

/// With OPENBLACK_GAME_PATH set to the install: Data/Misc/leash.l3d has two sub-meshes of one primitive each, both
/// textured with a skin the file does not carry; once retextured, both draw with the collar's texture
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(LeashCollar, LeashL3dTakesTheRopesTexture)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream stream(std::filesystem::path(game) / "Data" / "Misc" / "leash.l3d", std::ios::binary);
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	ASSERT_FALSE(bytes.empty());
	ASSERT_TRUE(InitNoopBgfx());
	const test::BgfxShutdown bgfxShutdown;
	{
		graphics::L3DMesh mesh("leash");
		ASSERT_TRUE(mesh.LoadFromBuffer(bytes));
		EXPECT_TRUE(mesh.GetSkins().empty());
		ASSERT_EQ(mesh.GetNumSubMeshes(), 2);
		for (const auto& subMesh : mesh.GetSubMeshes())
		{
			ASSERT_EQ(subMesh->GetPrimitives().size(), 1u);
			EXPECT_EQ(subMesh->GetPrimitives()[0].skinID, 0xFEA6C2F6u);
		}
		mesh.SetSkinOfTextured(k_Collar);
		for (const auto& subMesh : mesh.GetSubMeshes())
		{
			EXPECT_EQ(subMesh->GetPrimitives()[0].skinID, k_Collar);
		}
	}
}
