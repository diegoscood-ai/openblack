/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// A small world for the tests that play whole turns of the game's own systems: the test's registry with the world's
// lists, the map's and villagers' defaults, the map of what is in each cell, a flat dry land, the tables and the models, and a
// physics of its own with the game's class hooks over all of it. Each test runs twice, by its parameter: on synthetic data
// (zeroed tables with the rows the test fills in, and box models made by hand), and on the game's own (info.dat and the mesh
// pack), which skips itself when OPENBLACK_GAME_PATH is not set. The set-up of what a test plays comes from the testbed scene
// it is the twin of (testbed_scenarios::Find), so the scene goes on checking that nothing moved while the test checks each
// behaviour. Each service is put back when the test ends. Include it after LOCATOR_IMPLEMENTATIONS.

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <PackFile.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DSubMesh.h"
#include "Common/GameRandomTesting.h"
#include "Debug/TestbedScenarioRegistry.h"
#include "ECS/MapProduction.h"
#include "ECS/PhysicsGameHooks.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/ToBeDeleted.h"
#include "FileSystem/DefaultFileSystem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/Resources.h"
#include "support/GameInfoDat.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

namespace openblack::test::world
{
/// The data a world test runs on
enum class WorldData
{
	Synthetic,
	Game,
};

/// The game's folder and its info.dat (support/GameInfoDat.h)
using test::GamePath;
using test::ReadInfoDat;

/// The name a world test's data gives its runs
[[nodiscard]] inline std::string DataName(const ::testing::TestParamInfo<WorldData>& data)
{
	return data.param == WorldData::Game ? "GameData" : "Synthetic";
}

class WorldHarness: public ::testing::TestWithParam<WorldData>
{
protected:
	/// The land's cells' altitude everywhere, dry land, and its height
	static constexpr uint8_t k_GroundAltitude = 15;
	static inline const float k_Ground = FlatDryIsland::HeightOf(k_GroundAltitude);

	static void SetUpTestSuite()
	{
		const auto game = GamePath();
		if (!game.has_value())
		{
			return;
		}
		s_pack = std::make_unique<pack::PackFile>();
		if (s_pack->Open(*game / "Data" / "AllMeshes.g3d") != pack::PackResult::Success)
		{
			s_pack.reset();
		}
	}
	static void TearDownTestSuite() { s_pack.reset(); }

	void SetUp() override
	{
		std::unique_ptr<InfoConstants> info;
		std::optional<std::filesystem::path> game;
		if (GameData())
		{
			game = GamePath();
			if (!game.has_value())
			{
				GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
			}
			info = ReadInfoDat(*game);
			ASSERT_NE(info, nullptr) << "no info.dat of the game's size";
			ASSERT_NE(s_pack, nullptr) << "no mesh pack";
		}
		else
		{
			// every row zero but those the test fills in
			info = std::make_unique<InfoConstants>();
		}
		for (const auto* name : {"game", "scripting"})
		{
			if (spdlog::get(name) == nullptr)
			{
				spdlog::create<spdlog::sinks::null_sink_mt>(name);
			}
		}
		// the models build bgfx buffers
		bgfx::renderFrame(); // single-threaded
		bgfx::Init init {};
		init.type = bgfx::RendererType::Noop;
		ASSERT_TRUE(bgfx::init(init));
		_bgfx = true;
		_info = info.get();
		Locator::infoConstants::reset(info.release());
		// the random streams as the program starts them
		Locator::gameRandom::emplace<GameRandomTesting>();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		EmplaceWorldSystems();
		_fires = &Locator::fireSystem::value();
		EmplaceMapAndVillagerDefaults();
		// what is in each cell of the map, as the game asks it of the cells' lists
		Locator::entitiesMap::emplace<ecs::MapProduction>();
		Locator::resources::emplace<resources::Resources>();
		// the game's files, for what the systems load themselves (the landing dust's puff); on synthetic data there are
		// none, and what would be loaded is left out as the game leaves out a file it can't find
		auto& files = Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
		if (game.has_value())
		{
			files.SetGamePath(*game);
		}
		Locator::terrainSystem::emplace<FlatDryIsland>(k_GroundAltitude);
		// the physics, with the game's own parts for each kind of thing
		_dynamics =
		    &static_cast<ecs::systems::DynamicsSystem&>(Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>());
		_dynamics->SetClassHooks(std::make_unique<ecs::PhysicsGameHooks>());
		_ready = true;
	}

	void TearDown() override
	{
		if (_ready)
		{
			// the dead list goes before the services, as the game's close drains it
			ecs::ProcessDeadList(true);
			Locator::dynamicsSystem::reset();
			Locator::entitiesRegistry::reset();
			ResetMapAndVillagerDefaults();
			ResetWorldSystems();
		}
		// the models go before bgfx does
		Locator::resources::reset();
		if (_bgfx)
		{
			bgfx::shutdown();
		}
	}

	[[nodiscard]] bool GameData() const { return GetParam() == WorldData::Game; }
	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] ecs::systems::DynamicsSystem& Dynamics() const { return *_dynamics; }
	/// The world's fires
	[[nodiscard]] ecs::systems::FireSystemInterface& Fires() const { return *_fires; }
	/// The tables: the game's, or the synthetic ones the test fills in
	[[nodiscard]] InfoConstants& Info() const { return *_info; }

	/// The model into the cache: the game's from its pack, or, on synthetic data, a box from `low` to `high`; with a
	/// `cell`, each of its sides is a grid of squares that size or smaller, two triangles each
	void LoadMesh(MeshId mesh, glm::vec3 low, glm::vec3 high, std::optional<float> cell = std::nullopt) const
	{
		auto& meshes = Locator::resources::value().GetMeshes();
		if (meshes.Contains(resources::HashIdentifier(mesh)))
		{
			return;
		}
		if (GameData())
		{
			meshes.Load(mesh, resources::L3DLoader::FromBufferTag {}, std::string("game mesh"),
			            s_pack->GetMesh(static_cast<uint32_t>(mesh)));
			return;
		}
		meshes.Load(mesh, resources::L3DLoader::FromGeneratedTag {}, std::string("test box"),
		            std::vector {cell.has_value() ? GridBox(low, high, *cell) : Box(low, high)});
	}

	/// The testbed scene this test is the twin of
	[[nodiscard]] static const testbed_scenarios::Scenario& Scene(std::string_view id)
	{
		const auto* scene = testbed_scenarios::Find(id);
		if (scene == nullptr)
		{
			throw std::invalid_argument(std::string("no testbed scene ") + std::string(id));
		}
		return *scene;
	}

	/// One game turn: the map of the cells brought up to date as the turn starts, what the test's world does before the
	/// physics, the physics, what it does after, and the dead list as the turn's end frees it
	void Turn()
	{
		Locator::entitiesMap::value().Sync();
		BeforePhysics();
		_dynamics->ProcessTurn();
		AfterPhysics();
		ecs::ProcessDeadList(false);
	}
	/// A world's own steps of the turn before and after the physics (none here)
	virtual void BeforePhysics() {}
	virtual void AfterPhysics() {}

private:
	/// A box's twelve triangles, facing out
	[[nodiscard]] static graphics::L3DGeneratedPrimitive Box(glm::vec3 low, glm::vec3 high)
	{
		graphics::L3DGeneratedPrimitive box {};
		for (int corner = 0; corner < 8; ++corner)
		{
			box.positions.emplace_back((corner & 1) != 0 ? high.x : low.x, (corner & 2) != 0 ? high.y : low.y,
			                           (corner & 4) != 0 ? high.z : low.z);
		}
		constexpr std::array<uint16_t, 36> k_Faces = {0, 2, 1, 1, 2, 3, 4, 5, 6, 5, 7, 6, 0, 1, 4, 1, 5, 4,
		                                              2, 6, 3, 3, 6, 7, 0, 4, 2, 2, 4, 6, 1, 3, 5, 3, 7, 5};
		box.indices.assign(k_Faces.begin(), k_Faces.end());
		return box;
	}

	/// A box whose six sides are grids of squares no bigger than `cell`, two triangles each, facing out as Box's
	[[nodiscard]] static graphics::L3DGeneratedPrimitive GridBox(glm::vec3 low, glm::vec3 high, float cell)
	{
		graphics::L3DGeneratedPrimitive box {};
		const glm::vec3 size = high - low;
		// each side: its first corner, the edge along which a row of squares runs and the edge the rows climb, so that
		// the turn from the first to the second faces out
		const std::array<std::array<glm::vec3, 3>, 6> k_Sides {{
		    {low, glm::vec3(0.0f, size.y, 0.0f), glm::vec3(size.x, 0.0f, 0.0f)},
		    {glm::vec3(low.x, low.y, high.z), glm::vec3(size.x, 0.0f, 0.0f), glm::vec3(0.0f, size.y, 0.0f)},
		    {low, glm::vec3(size.x, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, size.z)},
		    {glm::vec3(low.x, high.y, low.z), glm::vec3(0.0f, 0.0f, size.z), glm::vec3(size.x, 0.0f, 0.0f)},
		    {low, glm::vec3(0.0f, 0.0f, size.z), glm::vec3(0.0f, size.y, 0.0f)},
		    {glm::vec3(high.x, low.y, low.z), glm::vec3(0.0f, size.y, 0.0f), glm::vec3(0.0f, 0.0f, size.z)},
		}};
		for (const auto& [corner, across, up] : k_Sides)
		{
			const auto n = static_cast<uint16_t>(std::max(1.0f, std::ceil(glm::length(across) / cell)));
			const auto m = static_cast<uint16_t>(std::max(1.0f, std::ceil(glm::length(up) / cell)));
			const auto first = static_cast<uint16_t>(box.positions.size());
			for (uint16_t j = 0; j <= m; ++j)
			{
				for (uint16_t i = 0; i <= n; ++i)
				{
					box.positions.push_back(corner + across * (static_cast<float>(i) / static_cast<float>(n)) +
					                        up * (static_cast<float>(j) / static_cast<float>(m)));
				}
			}
			const auto at = [first, n](uint16_t i, uint16_t j) { return static_cast<uint16_t>(first + j * (n + 1) + i); };
			for (uint16_t j = 0; j < m; ++j)
			{
				for (uint16_t i = 0; i < n; ++i)
				{
					box.indices.insert(box.indices.end(), {at(i, j), at(i + 1, j), at(i, j + 1)});
					box.indices.insert(box.indices.end(), {at(i + 1, j), at(i + 1, j + 1), at(i, j + 1)});
				}
			}
		}
		return box;
	}

	static inline std::unique_ptr<pack::PackFile> s_pack;

	// put back in reverse order
	RestoreService<Locator::infoConstants> _restoreInfo;
	RestoreService<Locator::gameRandom> _restoreRandom;
	RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	RestoreService<Locator::resources> _restoreResources;
	RestoreService<Locator::filesystem> _restoreFiles;
	RestoreService<Locator::entitiesMap> _restoreMap;
	RestoreService<Locator::terrainSystem> _restoreTerrain;
	RestoreService<Locator::dynamicsSystem> _restoreDynamics;
	InfoConstants* _info {nullptr};
	ecs::systems::DynamicsSystem* _dynamics {nullptr};
	ecs::systems::FireSystemInterface* _fires {nullptr};
	bool _bgfx {false};
	bool _ready {false};
};
} // namespace openblack::test::world
