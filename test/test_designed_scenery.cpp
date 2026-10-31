/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scenery the game places by the land's number rather than by the land's script: on Land 4 the stranded ark, with
// the sound of running water, and the dinosaur skeleton pressed into the landscape
// (docs/wiki/world/sea-life-and-scenery.md#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur). The
// synthetic tests put empty meshes in the cache under the two models' names; the real-data test, which skips itself
// without OPENBLACK_GAME_PATH, loads the game's own models from Data/Misc.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <L3DFile.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Game/BankTables.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Transform.h"
#include "ECS/DesignedScenery.h"
#include "ECS/Registry.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/Resources.h"
#include "support/BgfxShutdown.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace designed_scenery = openblack::ecs::designed_scenery;

namespace
{
/// The ark's and the dinosaur's models, as the game names them in the mesh cache
const entt::id_type k_ArkMesh = resources::HashIdentifier(std::string("misc/arche"));
const entt::id_type k_DinosaurMesh = resources::HashIdentifier(std::string("misc/dinosaur"));

/// The land under the scenery: not flat, so each model's height is the land's at its own point
float LandHeight(glm::vec2 at)
{
	return 100.0f + (at.x * 0.01f) + (at.y * 0.002f);
}

/// The bytes of an L3D file with no sub-mesh, no skin and no footprint: an empty mesh
std::vector<uint8_t> EmptyMesh()
{
	constexpr uint32_t k_None = std::numeric_limits<uint32_t>::max();
	std::vector<uint8_t> file(sizeof(l3d::L3DHeader));
	l3d::L3DHeader header {};
	std::memcpy(header.magic.data(), "L3D0", header.magic.size());
	header.flags = l3d::L3DMeshFlags::None;
	header.size = static_cast<uint32_t>(file.size());
	header.submeshOffsetsOffset = k_None;
	header.skinOffsetsOffset = k_None;
	header.extraDataOffset = k_None;
	header.footprintDataOffset = k_None;
	std::memcpy(file.data(), &header, sizeof(header));
	return file;
}

/// The sound tags made at script markers and the tags deleted, in order
class MarkerSoundTags final: public ecs::systems::SoundTagSystemInterface
{
public:
	struct Marker
	{
		glm::vec3 point;
		int mode;
		int loops;
		bool is3D;
		audio::SfxBank bank;
	};

	void ProcessTurn() override {}
	void SetActive(TagId /*tag*/, bool /*active*/) override {}
	TagId CreatePointSound(const glm::vec3& /*point*/, int /*sample*/, int /*mode*/, int /*loops*/, bool /*extra3DFlag*/,
	                       bool /*is3D*/, audio::SfxBank /*bank*/, int /*delay*/) override
	{
		return ++_next;
	}
	TagId CreatePointSound(const glm::vec3& /*point*/, int /*sample*/, int /*mode*/, int /*loops*/, bool /*extra3DFlag*/,
	                       bool /*is3D*/, audio::AtmosType /*bank*/, int /*delay*/) override
	{
		return ++_next;
	}
	TagId Create(entt::entity /*thing*/, const glm::vec3& /*offset*/, int /*sample*/, bool /*track*/, int /*mode*/,
	             int /*loops*/, bool /*extra3DFlag*/, bool /*is3D*/, audio::SfxBank /*bank*/, int /*delay*/) override
	{
		return ++_next;
	}
	TagId CreateAtMarker(const glm::vec3& point, int /*sample*/, int mode, int loops, bool is3D, audio::SfxBank bank) override
	{
		markers.push_back({.point = point, .mode = mode, .loops = loops, .is3D = is3D, .bank = bank});
		return ++_next;
	}
	void Remove(entt::entity /*thing*/, int /*sample*/, audio::SfxBank /*bank*/, bool /*stop*/) override {}
	void Delete(TagId tag) override
	{
		if (tag != 0)
		{
			deleted.push_back(tag);
		}
	}
	[[nodiscard]] bool Exists(TagId /*tag*/) const override { return false; }
	void Clear() override {}
	[[nodiscard]] std::optional<glm::vec3> TagSoundPoint(TagId /*tag*/) const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> Point(TagId /*tag*/) const override { return std::nullopt; }

	std::vector<Marker> markers;
	/// The tags deleted, none counted for "no tag"
	std::vector<TagId> deleted;

private:
	TagId _next {0};
};

class DesignedSceneryTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::terrainSystem::emplace<test::HeightFieldIsland>(LandHeight);
		Locator::soundTagSystem::emplace<MarkerSoundTags>();
	}

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		Locator::terrainSystem::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	static MarkerSoundTags& Tags() { return static_cast<MarkerSoundTags&>(Locator::soundTagSystem::value()); }

	/// The land number the land's script set (SET_LAND_NUMBER)
	static void SetLand(int32_t land) { Locator::mapScriptSystem::value().Globals().landNumber = land; }

	/// Every entity wearing the mesh, in the registry's order
	static std::vector<entt::entity> Wearing(entt::id_type mesh)
	{
		std::vector<entt::entity> found;
		Reg().Each<const Mesh>([&found, mesh](entt::entity entity, const Mesh& worn) {
			if (worn.id == mesh)
			{
				found.push_back(entity);
			}
		});
		return found;
	}

	/// The empty models in the cache under the ark's and the dinosaur's names
	static void PutEmptyModels()
	{
		auto& meshes = Locator::resources::value().GetMeshes();
		meshes.Load(k_ArkMesh, resources::L3DLoader::FromBufferTag {}, std::string("test ark"), EmptyMesh());
		meshes.Load(k_DinosaurMesh, resources::L3DLoader::FromBufferTag {}, std::string("test dinosaur"), EmptyMesh());
	}

	const test::RestoreService<Locator::soundTagSystem> _restoreTags;
	const test::RestoreService<Locator::resources> _restoreResources;
};
} // namespace

// Land 4 has the ark at (3538, 2129) on the land, turned 8.9728 radians and at scale 1.1, and the dinosaur skeleton at
// (2690, 2590) on the land, turned 1.57 radians at scale 1. Both are morphable objects, which the land's shape melts
// into it, as the original
// (docs/wiki/world/sea-life-and-scenery.md#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur)
TEST_F(DesignedSceneryTest, LandFourHasTheArkAndTheDinosaurOnTheLandMeltedIntoIt)
{
	Locator::resources::emplace<resources::Resources>();
	PutEmptyModels();
	SetLand(4);
	designed_scenery::Update(0.0f);

	const auto arks = Wearing(k_ArkMesh);
	ASSERT_EQ(arks.size(), 1u);
	const auto& ark = Reg().Get<const Transform>(arks.front());
	const glm::vec2 arkAt(3538.0f, 2129.0f);
	EXPECT_EQ(ark.position, glm::vec3(arkAt.x, LandHeight(arkAt), arkAt.y));
	EXPECT_EQ(ark.rotation, affine::AngleY(8.9728f));
	EXPECT_EQ(ark.scale, glm::vec3(1.1f));
	EXPECT_TRUE(Reg().AnyOf<MorphWithTerrain>(arks.front()));

	const auto dinosaurs = Wearing(k_DinosaurMesh);
	ASSERT_EQ(dinosaurs.size(), 1u);
	const auto& dinosaur = Reg().Get<const Transform>(dinosaurs.front());
	const glm::vec2 dinosaurAt(2690.0f, 2590.0f);
	EXPECT_EQ(dinosaur.position, glm::vec3(dinosaurAt.x, LandHeight(dinosaurAt), dinosaurAt.y));
	EXPECT_EQ(dinosaur.rotation, affine::AngleY(1.57f));
	EXPECT_EQ(dinosaur.scale, glm::vec3(1.0f));
	EXPECT_TRUE(Reg().AnyOf<MorphWithTerrain>(dinosaurs.front()));

	// made once: the next frames keep them
	designed_scenery::Update(100.0f);
	EXPECT_EQ(Wearing(k_ArkMesh).size(), 1u);
	EXPECT_EQ(Wearing(k_DinosaurMesh).size(), 1u);
}

// The ark has the sound of running water: a script marker's tag at (3538, 0, 2129), at the height 0 whatever the land
// there, looping for ever, in 3D, from the in-game bank, mode 2, as the original. The dinosaur has none
// (docs/wiki/world/sea-life-and-scenery.md#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur)
TEST_F(DesignedSceneryTest, TheArkHasTheSoundOfRunningWaterAtItsPointAtHeightNought)
{
	Locator::resources::emplace<resources::Resources>();
	PutEmptyModels();
	SetLand(4);
	designed_scenery::Update(0.0f);
	designed_scenery::Update(100.0f);

	ASSERT_EQ(Tags().markers.size(), 1u);
	const auto& water = Tags().markers.front();
	EXPECT_EQ(water.point, glm::vec3(3538.0f, 0.0f, 2129.0f));
	EXPECT_EQ(water.loops, -1);
	EXPECT_EQ(water.mode, 2);
	EXPECT_TRUE(water.is3D);
	EXPECT_EQ(water.bank, audio::SfxBank::InGame);
}

// Another land number has neither, and when the land number changes the ark, the dinosaur and the ark's sound go, as
// the original (docs/wiki/world/sea-life-and-scenery.md#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur)
TEST_F(DesignedSceneryTest, OnlyLandFourHasThemAndANewLandNumberTakesThemAway)
{
	Locator::resources::emplace<resources::Resources>();
	PutEmptyModels();
	SetLand(1);
	designed_scenery::Update(0.0f);
	EXPECT_TRUE(Wearing(k_ArkMesh).empty());
	EXPECT_TRUE(Wearing(k_DinosaurMesh).empty());
	EXPECT_TRUE(Tags().markers.empty());

	SetLand(4);
	designed_scenery::Update(0.0f);
	const auto arks = Wearing(k_ArkMesh);
	const auto dinosaurs = Wearing(k_DinosaurMesh);
	ASSERT_EQ(arks.size(), 1u);
	ASSERT_EQ(dinosaurs.size(), 1u);
	EXPECT_TRUE(Tags().deleted.empty());

	SetLand(5);
	designed_scenery::Update(0.0f);
	EXPECT_FALSE(Reg().Valid(arks.front()));
	EXPECT_FALSE(Reg().Valid(dinosaurs.front()));
	EXPECT_EQ(Tags().deleted.size(), 1u);
	EXPECT_EQ(Tags().markers.size(), 1u);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
/// With the game's own models from Data/Misc: Land 4's ark and dinosaur are made, and of the two only the dinosaur's
/// model holds a landscape feature, the footprint that presses it into the land, as the original
/// (docs/wiki/world/sea-life-and-scenery.md#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur)
TEST_F(DesignedSceneryTest, realLandFourModelsAndTheDinosaursFootprint)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const test::ScopedDefaultFileSystem fileSystem;
	Locator::filesystem::value().SetGamePath(game);
	// the config is the test's only when it had none: then it goes with the test
	const test::RestoreService<Locator::config> config;
	if (!Locator::config::has_value())
	{
		Locator::config::emplace();
	}
	// the models' sub-meshes and skins build bgfx buffers and textures
	bgfx::renderFrame(); // single-threaded
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	ASSERT_TRUE(bgfx::init(init));
	const test::BgfxShutdown bgfxShutdown;
	{
		// the game's models go before the bgfx shutdown, also after a failed ASSERT
		const test::RestoreService<Locator::resources> restoreResources;
		Locator::resources::emplace<resources::Resources>();
		SetLand(4);
		designed_scenery::Update(0.0f);
		EXPECT_EQ(Wearing(k_ArkMesh).size(), 1u);
		EXPECT_EQ(Wearing(k_DinosaurMesh).size(), 1u);

		auto& meshes = Locator::resources::value().GetMeshes();
		ASSERT_TRUE(meshes.Contains(k_ArkMesh));
		ASSERT_TRUE(meshes.Contains(k_DinosaurMesh));
		EXPECT_TRUE(meshes.Handle(k_DinosaurMesh)->ContainsLandscapeFeature());
		EXPECT_FALSE(meshes.Handle(k_ArkMesh)->ContainsLandscapeFeature());
		// the registry's meshes go with the models
		Locator::entitiesRegistry::value().Reset();
	}
}
