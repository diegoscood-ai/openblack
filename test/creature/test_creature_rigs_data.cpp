/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Integration: every species' rig from the game's own .cbn files, and the skin meshes they name; each species' rest
// height, bones' reach and radius as the creature archetype measures them from its base mesh and stand. Skipped without
// the game's data (OPENBLACK_GAME_PATH or OPENBLACK_TEST_GAME_PATH)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <bit>
#include <string>
#include <vector>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "FileSystem/FileSystemInterface.h"
#include "Resources/Loaders.h"
#include "Resources/Resources.h"
#include "support/BgfxShutdown.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"

using namespace openblack;

namespace
{
const char* GamePath()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	return game != nullptr ? game : std::getenv("OPENBLACK_TEST_GAME_PATH");
}

/// What the game measures of a species' body when it loads it, as float bits: the rest height, the bones' reach and the
/// radius at size 1 (docs/bw1-notes/creature.md, "The creature's radius")
struct Measures
{
	CreatureType species;
	uint32_t restHeight;
	uint32_t reach;
	uint32_t radius;
};
constexpr std::array k_Measures {
    Measures {CreatureType::Cow, 0x423FD051u, 0x416FC317u, 0x4095FF2Fu},
    Measures {CreatureType::Tiger, 0x429EDFFAu, 0x41C4AE2Fu, 0x40948E0Fu},
    Measures {CreatureType::Leopard, 0x429EDFFAu, 0x41C4AE2Fu, 0x40948E0Fu},
    Measures {CreatureType::Wolf, 0x429EDFFAu, 0x41C4AFCDu, 0x40948F47u},
    Measures {CreatureType::Lion, 0x429EDFFAu, 0x41C4AE2Fu, 0x40948E0Fu},
    Measures {CreatureType::Horse, 0x425E039Cu, 0x4153AB52u, 0x4064D12Au},
    Measures {CreatureType::Tortoise, 0x4240C2DAu, 0x41B178D6u, 0x40DCF6CDu},
    Measures {CreatureType::Zebra, 0x42601088u, 0x4153AB51u, 0x4062B91Bu},
    Measures {CreatureType::BrownBear, 0x4258E5ECu, 0x41940C6Bu, 0x40A3D133u},
    Measures {CreatureType::PolarBear, 0x4258E5EBu, 0x41940C6Au, 0x40A3D133u},
    Measures {CreatureType::Sheep, 0x423F182Cu, 0x417012B7u, 0x4096C1BAu},
    Measures {CreatureType::Chimp, 0x4240E332u, 0x41AB1FC2u, 0x40D4EBBFu},
    Measures {CreatureType::Ogre, 0x4291DE76u, 0x412E7D8Au, 0x400F8BAAu},
    Measures {CreatureType::Mandrill, 0x4240E332u, 0x41AB1FC2u, 0x40D4EBBFu},
    Measures {CreatureType::Rhino, 0x423FD04Eu, 0x416FC317u, 0x4095FF31u},
    Measures {CreatureType::Gorilla, 0x4240E332u, 0x41AB1FC2u, 0x40D4EBBFu},
    Measures {CreatureType::GiantApe, 0x4240E332u, 0x41AB1FC2u, 0x40D4EBBFu},
};
} // namespace

TEST(CreatureRigsData, EverySpeciesLoadsWithItsSkinMeshes)
{
	const char* game = GamePath();
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	if (!spdlog::get("game"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
	const test::ScopedDefaultFileSystem fileSystem;
	Locator::filesystem::value().SetGamePath(game);
	resources::Resources resources;
	resources::LoadCreatureRigs(resources);

	auto& rigs = resources.GetCreatureRigs();
	const auto& files = resources.GetL3DFiles();
	const auto meshDirectory = Locator::filesystem::value().GetPath<filesystem::Path::CreatureMesh>();
	for (int32_t s = static_cast<int32_t>(CreatureType::Cow); s <= static_cast<int32_t>(CreatureType::GiantApe); ++s)
	{
		const auto species = static_cast<CreatureType>(s);
		const auto id = creature::GetRigId(species);
		ASSERT_TRUE(rigs.Contains(id)) << "species " << s;
		const auto& rig = *rigs.Handle(id);
		EXPECT_FALSE(rig.animations.front().empty()) << "species " << s;
		for (size_t m = 0; m < rig.meshNames.size(); ++m)
		{
			const auto& name = rig.meshNames.at(m);
			if (name.empty() || !rig.hasMesh.at(m) || !Locator::filesystem::value().Exists(meshDirectory / (name + ".l3d")))
			{
				continue;
			}
			EXPECT_TRUE(files.Contains(entt::hashed_string(("creature/skins/" + name).c_str()).value())) << name;
		}
	}
	const auto& ape = *rigs.Handle(creature::GetRigId(CreatureType::GiantApe));
	EXPECT_TRUE(ape.eyes.has_value());
	EXPECT_TRUE(ape.tattooSites.has_value());
	EXPECT_TRUE(ape.actionPoints.has_value());
	EXPECT_TRUE(ape.leashBone.has_value());
}

TEST(CreatureRigsData, EverySpeciesMeasuresAsTheGameMeasuresIt)
{
	const char* game = GamePath();
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
	const test::ScopedDefaultFileSystem fileSystem;
	Locator::filesystem::value().SetGamePath(game);
	// the meshes' sub-meshes and skins build bgfx buffers and textures
	bgfx::renderFrame(); // single-threaded
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	ASSERT_TRUE(bgfx::init(init));
	const test::BgfxShutdown bgfxShutdown;
	// made after bgfx, so the caches and their meshes go before it shuts down
	const test::RestoreService<Locator::resources> restoreResources;
	Locator::resources::emplace<resources::Resources>();
	auto& resources = Locator::resources::value();
	resources::LoadCreatureRigs(resources);

	// the species' base meshes, loaded from the creature mesh folder as the game loads them: under the id their file name
	// gives them (only the base ones, which are all the archetype reads)
	auto& rigs = resources.GetCreatureRigs();
	auto& meshes = resources.GetMeshes();
	std::vector<entt::id_type> baseIds;
	for (const auto& expected : k_Measures)
	{
		baseIds.push_back(creature::GetIdFromType(expected.species, creature::CreatureBody::Appearance::Base));
	}
	Locator::filesystem::value().Iterate(Locator::filesystem::value().GetPath<filesystem::Path::CreatureMesh>(), false,
	                                     [&meshes, &baseIds](const std::filesystem::path& file) {
		                                     const auto meshId = creature::GetIdFromMeshName(file.stem().string());
		                                     if (meshId.has_value() && std::ranges::find(baseIds, *meshId) != baseIds.end() &&
		                                         !meshes.Contains(*meshId))
		                                     {
			                                     meshes.Load(*meshId, resources::L3DLoader::FromDiskTag {}, file);
		                                     }
	                                     });

	for (const auto& expected : k_Measures)
	{
		const auto s = static_cast<int32_t>(expected.species);
		const auto meshId = creature::GetIdFromType(expected.species, creature::CreatureBody::Appearance::Base);
		ASSERT_TRUE(meshes.Contains(meshId)) << "species " << s;
		const auto metrics = ecs::archetypes::CreatureArchetype::BodyMetrics(expected.species);
		EXPECT_EQ(std::bit_cast<uint32_t>(metrics.restHeight), expected.restHeight)
		    << "species " << s << ": " << metrics.restHeight;
		EXPECT_EQ(std::bit_cast<uint32_t>(metrics.reach), expected.reach) << "species " << s << ": " << metrics.reach;
		EXPECT_EQ(std::bit_cast<uint32_t>(creature_morph::Radius(1.0f, metrics.restHeight, metrics.reach)), expected.radius)
		    << "species " << s;
		// the size it is drawn at comes from the same rest height
		EXPECT_EQ(std::bit_cast<uint32_t>(ecs::archetypes::CreatureArchetype::DrawnScale(expected.species, 1.0f)),
		          std::bit_cast<uint32_t>(creature_morph::DrawnScale(1.0f, metrics.restHeight)))
		    << "species " << s;

		// the stand keys every bone of the base mesh, so no bone is posed from the animation's default frame
		ASSERT_TRUE(rigs.Contains(creature::GetRigId(expected.species))) << "species " << s;
		const auto& rig = *rigs.Handle(creature::GetRigId(expected.species));
		const auto* stand = rig.GetAnimation(creature::CreatureRig::Mesh::Base, creature_animation::k_StandAnimation);
		ASSERT_NE(stand, nullptr) << "species " << s;
		const auto bones = meshes.Handle(meshId)->GetBoneParents().size();
		for (uint32_t bone = 0; bone < bones; ++bone)
		{
			EXPECT_NE(std::ranges::find(stand->rotatedJoints, bone), stand->rotatedJoints.end()) << "species " << s;
			EXPECT_NE(std::ranges::find(stand->translatedJoints, bone), stand->translatedJoints.end()) << "species " << s;
		}
	}
}
