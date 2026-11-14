/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The worship site's mesh has no skin of its own: it wears the first skin of its temple's mesh.

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "Common/Zip.h"
#include "ECS/Components/WorshipSite.h"
#include "Graphics/WorldTriangles.h"
#include "Worship/WorshipSite.h"
#include "support/BgfxShutdown.h"

using namespace openblack;

namespace
{
/// A compressed mesh of Data/Citadel/OutsideMeshes inflated (a u32 of its inflated size, then the deflated bytes);
/// empty when it cannot be read
std::vector<uint8_t> ReadOutsideMesh(const std::filesystem::path& game, const char* file)
{
	std::ifstream stream(game / "Data" / "Citadel" / "OutsideMeshes" / file, std::ios::binary);
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	uint32_t size = 0;
	if (bytes.size() < sizeof(size))
	{
		return {};
	}
	std::memcpy(&size, bytes.data(), sizeof(size));
	return zip::Inflate(std::vector<uint8_t>(bytes.begin() + sizeof(size), bytes.end()), size);
}

/// One primitive of a mesh built here: a single triangle in one material
struct SyntheticPrimitive
{
	l3d::L3DMaterial::Type type {l3d::L3DMaterial::Type::Textured};
	uint32_t skin {0};
};

/// One sub-mesh of a mesh built here
struct SyntheticSubMesh
{
	std::vector<SyntheticPrimitive> primitives;
	bool physics {false};
};

/// The bytes of an L3D file laid out as the game's are: the header, the sub-mesh table and headers, each sub-mesh's
/// primitive table, each primitive's header with its three vertices and one triangle, then the skins (blank texels)
std::vector<uint8_t> SyntheticL3d(const std::vector<SyntheticSubMesh>& subMeshes, const std::vector<uint32_t>& skins = {})
{
	constexpr uint32_t k_None = std::numeric_limits<uint32_t>::max();
	std::vector<uint8_t> file;
	const auto reserve = [&file](size_t bytes) {
		const auto at = static_cast<uint32_t>(file.size());
		file.resize(file.size() + bytes);
		return at;
	};
	const auto write = [&file](uint32_t at, const auto& value) { std::memcpy(file.data() + at, &value, sizeof(value)); };

	l3d::L3DHeader header {};
	std::memcpy(header.magic.data(), "L3D0", header.magic.size());
	header.submeshCount = static_cast<uint32_t>(subMeshes.size());
	header.skinCount = static_cast<uint32_t>(skins.size());
	header.skinOffsetsOffset = k_None;
	header.extraDataOffset = k_None;
	reserve(sizeof(header));
	header.submeshOffsetsOffset = reserve(subMeshes.size() * sizeof(uint32_t));
	std::vector<uint32_t> subMeshAt;
	for (size_t i = 0; i < subMeshes.size(); ++i)
	{
		subMeshAt.push_back(reserve(sizeof(l3d::L3DSubmeshHeader)));
		write(header.submeshOffsetsOffset + static_cast<uint32_t>(i * sizeof(uint32_t)), subMeshAt.back());
	}
	constexpr std::array<l3d::L3DVertex, 3> k_Vertices {{
	    {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
	    {{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
	    {{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
	}};
	constexpr std::array<uint16_t, 4> k_Triangle {0, 1, 2, 0}; // padded to four bytes
	for (size_t i = 0; i < subMeshes.size(); ++i)
	{
		const auto& source = subMeshes[i];
		l3d::L3DSubmeshHeader subMesh {};
		subMesh.flags.isPhysics = source.physics ? 1u : 0u;
		subMesh.numPrimitives = static_cast<uint32_t>(source.primitives.size());
		subMesh.primitivesOffset = reserve(source.primitives.size() * sizeof(uint32_t));
		subMesh.bonesOffset = k_None;
		write(subMeshAt[i], subMesh);
		for (size_t j = 0; j < source.primitives.size(); ++j)
		{
			const auto& material = source.primitives[j];
			const auto at = reserve(sizeof(l3d::L3DPrimitiveHeader));
			write(subMesh.primitivesOffset + static_cast<uint32_t>(j * sizeof(uint32_t)), at);
			l3d::L3DPrimitiveHeader primitive {};
			primitive.material.type = material.type;
			primitive.material.skinID = material.skin;
			primitive.numVertices = static_cast<uint32_t>(k_Vertices.size());
			primitive.verticesOffset = reserve(sizeof(k_Vertices));
			write(primitive.verticesOffset, k_Vertices);
			primitive.numTriangles = 1;
			primitive.trianglesOffset = reserve(sizeof(k_Triangle));
			write(primitive.trianglesOffset, k_Triangle);
			primitive.groupsOffset = k_None;
			primitive.vertexBlendsOffset = k_None;
			write(at, primitive);
		}
	}
	if (!skins.empty())
	{
		header.skinOffsetsOffset = reserve(skins.size() * sizeof(uint32_t));
		const auto texture = std::make_unique<l3d::L3DTexture>(); // blank texels
		for (size_t i = 0; i < skins.size(); ++i)
		{
			texture->id = skins[i];
			const auto at = reserve(sizeof(l3d::L3DTexture));
			write(header.skinOffsetsOffset + static_cast<uint32_t>(i * sizeof(uint32_t)), at);
			write(at, *texture);
		}
	}
	header.size = static_cast<uint32_t>(file.size());
	write(0, header);
	return file;
}
} // namespace

/// With OPENBLACK_GAME_PATH set to the install: B_WORSHIP's materials name a skin it does not carry; once its skin
/// source is the first temple's mesh and every primitive takes that mesh's first skin, each primitive draws with the
/// temple's own texture, the one the alignment blend rewrites
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(WorshipSiteSkin, everyPrimitiveWearsTheTemplesFirstSkin)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	for (const auto* name : {"game", "graphics"})
	{
		if (spdlog::get(name) == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>(name);
		}
	}
	const auto templeBytes = ReadOutsideMesh(game, "b_first_temple_l3d.zzz");
	const auto siteBytes = ReadOutsideMesh(game, "B_WORSHIP_l3d.zzz");
	ASSERT_FALSE(templeBytes.empty());
	ASSERT_FALSE(siteBytes.empty());
	// the sub-meshes and skins build bgfx buffers and textures
	bgfx::renderFrame(); // single-threaded
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	ASSERT_TRUE(bgfx::init(init));
	const test::BgfxShutdown bgfxShutdown;
	{
		auto temple = std::make_shared<graphics::L3DMesh>("b_first_temple");
		ASSERT_TRUE(temple->LoadFromBuffer(templeBytes));
		graphics::L3DMesh site("B_WORSHIP");
		ASSERT_TRUE(site.LoadFromBuffer(siteBytes));

		// the temple carries one skin; the site none, and its materials ask for skin 1
		const auto skin = temple->GetFirstSkin();
		ASSERT_TRUE(skin.has_value());
		EXPECT_EQ(*skin, 0xFC0B7D34u);
		EXPECT_FALSE(site.GetFirstSkin().has_value());
		EXPECT_TRUE(site.GetSkins().empty());
		ASSERT_EQ(site.GetNumSubMeshes(), 2);
		EXPECT_EQ(site.GetSubMeshes()[0]->GetPrimitives().at(0).skinID, 1u);

		site.SetSkinSource(temple);
		site.SetSkin(*skin);

		EXPECT_EQ(site.GetFirstSkin(), skin);
		const auto* templeTexture = temple->GetSkins().at(*skin).get();
		for (uint8_t i = 0; i < site.GetNumSubMeshes(); ++i)
		{
			// every sub-mesh, the physics one included, as the original sets every material's texture
			for (const auto& primitive : site.GetSubMeshes()[i]->GetPrimitives())
			{
				EXPECT_EQ(primitive.skinID, *skin) << "sub-mesh " << static_cast<int>(i);
				EXPECT_EQ(graphics::world_triangles::PrimitiveTexture(site, primitive.skinID), templeTexture)
				    << "sub-mesh " << static_cast<int>(i);
			}
		}
	}
}

/// everyPrimitiveWearsTheTemplesFirstSkin on meshes built here: a temple that carries one skin, and a site of two
/// sub-meshes (the second a physics one) that carries none and whose materials ask for skin 1; once the temple is the
/// site's skin source and every primitive takes its first skin, each primitive draws with the temple's own texture
TEST(WorshipSiteSkin, everyPrimitiveWearsTheTemplesFirstSkinSynthetic)
{
	constexpr uint32_t k_TempleSkin = 0x00ABCDEFu;
	constexpr uint32_t k_SkinTheSiteAsksFor = 1;
	for (const auto* name : {"game", "graphics"})
	{
		if (spdlog::get(name) == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>(name);
		}
	}
	const auto templeBytes = SyntheticL3d({{.primitives = {{.skin = k_TempleSkin}}}}, {k_TempleSkin});
	const auto siteBytes = SyntheticL3d({
	    {.primitives = {{.skin = k_SkinTheSiteAsksFor}, {.skin = k_SkinTheSiteAsksFor}}},
	    {.primitives = {{.skin = k_SkinTheSiteAsksFor}}, .physics = true},
	});
	// the sub-meshes and skins build bgfx buffers and textures
	bgfx::renderFrame(); // single-threaded
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	ASSERT_TRUE(bgfx::init(init));
	const test::BgfxShutdown bgfxShutdown;
	{
		auto temple = std::make_shared<graphics::L3DMesh>("synthetic temple");
		ASSERT_TRUE(temple->LoadFromBuffer(templeBytes));
		graphics::L3DMesh site("synthetic site");
		ASSERT_TRUE(site.LoadFromBuffer(siteBytes));

		const auto skin = temple->GetFirstSkin();
		ASSERT_TRUE(skin.has_value());
		EXPECT_EQ(*skin, k_TempleSkin);
		EXPECT_FALSE(site.GetFirstSkin().has_value());
		EXPECT_TRUE(site.GetSkins().empty());
		ASSERT_EQ(site.GetNumSubMeshes(), 2);
		EXPECT_TRUE(site.GetSubMeshes()[1]->IsPhysics());
		EXPECT_EQ(site.GetSubMeshes()[0]->GetPrimitives().at(0).skinID, k_SkinTheSiteAsksFor);

		site.SetSkinSource(temple);
		site.SetSkin(*skin);

		EXPECT_EQ(site.GetFirstSkin(), skin);
		const auto* templeTexture = temple->GetSkins().at(*skin).get();
		for (uint8_t i = 0; i < site.GetNumSubMeshes(); ++i)
		{
			// every sub-mesh, the physics one included
			for (const auto& primitive : site.GetSubMeshes()[i]->GetPrimitives())
			{
				EXPECT_EQ(primitive.skinID, *skin) << "sub-mesh " << static_cast<int>(i);
				EXPECT_EQ(graphics::world_triangles::PrimitiveTexture(site, primitive.skinID), templeTexture)
				    << "sub-mesh " << static_cast<int>(i);
			}
		}
	}
}

/// The six site slots stand round the citadel heart: slot n at the heart's angle plus n sevenths of a turn (2 pi / 7,
/// as one float, as the original), whatever the heart's own angle, and the step does not depend on which slots are
/// taken
/// (docs/wiki/magic/temple-and-citadel.md#the-citadel-and-its-six-slots)
TEST(WorshipSiteSlots, aSlotsAngleIsTheHeartsPlusItsSeventhsOfATurn)
{
	constexpr float k_Seventh = 2.0f * 3.14159265358979f / 7.0f;
	ecs::components::CitadelWorship citadel;
	// the heart at angle 0: slot 0 is the heart's angle, slot 1 one seventh of a turn, the float itself
	EXPECT_EQ(worship::site::SlotAngle(citadel, 0), 0.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(worship::site::SlotAngle(citadel, 1)), 0x3F65C8FAu);
	for (const float heart : {0.0f, 0.25f, -1.5f, 3.0f})
	{
		citadel.heartYAngle = heart;
		for (int slot = 0; slot < static_cast<int>(ecs::components::CitadelWorship::k_Sites); ++slot)
		{
			EXPECT_FLOAT_EQ(worship::site::SlotAngle(citadel, slot), heart + static_cast<float>(slot) * k_Seventh)
			    << "heart " << heart << " slot " << slot;
		}
	}
	// a taken slot does not move the others
	citadel.heartYAngle = 0.25f;
	const float fourth = worship::site::SlotAngle(citadel, 4);
	citadel.sites.at(0) = entt::entity {7};
	EXPECT_EQ(worship::site::SlotAngle(citadel, 4), fourth);
}
