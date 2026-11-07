/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The DontDraw mark that SET_HIGH_GRAPHICS_DETAIL writes (ECS/SuperVillager.cpp): on sets it before anything else,
// off clears it before its draw-list rebuild request, and a thing the script cannot find is left alone (a registry
// only: no files, no meshes). docs/wiki/rendering/object-draw-list.md#the-object-draw-list-stage-4m
// Also the model the op draws a thing with: its own high mesh, whatever villager type shares its tribe and job.
// docs/wiki/scripts/land1-intro.md

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <glm/glm.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "CHLApi.h"
#include "ECS/Components/DetailMesh.h"
#include "ECS/Components/DontDraw.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Events/DrawListEvents.h"
#include "ECS/Registry.h"
#include "ECS/SuperVillager.h"
#include "ECS/Systems/Implementations/HighDetailSystem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "support/DrawListRequests.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;

namespace
{
namespace super_villager = ecs::super_villager;
using ecs::Registry;
using ecs::components::DetailMesh;
using ecs::components::DontDraw;
using ecs::components::Mesh;
using ecs::components::SuperVillager;
using ecs::components::Transform;
using ecs::components::Villager;

class DontDrawMark: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the SuperVillager's creation logs to the game's logger, the natives to the scripts': null sinks
		for (const auto* name : {"game", "scripting"})
		{
			if (!spdlog::get(name))
			{
				spdlog::create<spdlog::sinks::null_sink_mt>(name);
			}
		}
		Locator::entitiesRegistry::emplace<Registry>();
		test::EmplaceWorldSystems();
		// entity 0 is "no thing" for a script id: the test's first entity must not be a scripted one
		Reg().Create();
		Locator::vm::emplace<lhvm::LHVM>();
		// the native reaches our SuperVillagers through the high-detail slot
		Locator::highDetailSystem::emplace<ecs::systems::HighDetailSystem>();
		// whether the thing carried the mark when each rebuild was asked for
		Locator::events::value().AddHandler<ecs::events::DrawListRebuildRequested>(
		    [this](const ecs::events::DrawListRebuildRequested&) { _markedAtRequest.push_back(Reg().AllOf<DontDraw>(_host)); });
	}
	void TearDown() override
	{
		super_villager::ReleaseAll();
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
	}
	static Registry& Reg() { return Locator::entitiesRegistry::value(); }
	/// A thing that is not a villager: the op makes it a SuperVillager of its own mesh, with no files read
	static entt::entity Host()
	{
		const auto host = Reg().Create();
		Reg().Assign<Transform>(host, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return host;
	}

	/// SET_HIGH_GRAPHICS_DETAIL as a script calls it: the bool pushed first, then the object
	void CallNative(uint32_t object, bool on)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "SET_HIGH_GRAPHICS_DETAIL"; });
		ASSERT_NE(native, table.end());
		ASSERT_EQ(native->stackIn, 2);
		auto& vm = Locator::vm::value();
		vm.Pushb(on);
		vm.Pusho(object);
		native->impl();
	}

	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::highDetailSystem> _restoreHighDetail;
	chlapi::CHLApi _api;
	test::DrawListRequests _requests;
	entt::entity _host = entt::null;
	std::vector<bool> _markedAtRequest;
};
} // namespace

TEST_F(DontDrawMark, OnSetsTheMarkBeforeTheRequest)
{
	_host = Host();
	super_villager::SetHighGraphicsDetail(_host, true);
	EXPECT_TRUE(Reg().AllOf<DontDraw>(_host));
	EXPECT_TRUE(Reg().AllOf<SuperVillager>(_host));
	EXPECT_EQ(_requests.Counts(), (std::vector<uint8_t> {1}));
	EXPECT_EQ(_markedAtRequest, (std::vector<bool> {true}));
	// on again: still marked, and it asks again
	super_villager::SetHighGraphicsDetail(_host, true);
	EXPECT_TRUE(Reg().AllOf<DontDraw>(_host));
	EXPECT_EQ(_requests.Counts(), (std::vector<uint8_t> {1, 1}));
}

TEST_F(DontDrawMark, OffClearsTheMarkBeforeTheRequest)
{
	_host = Host();
	super_villager::SetHighGraphicsDetail(_host, true);
	_requests.Clear();
	_markedAtRequest.clear();
	super_villager::SetHighGraphicsDetail(_host, false);
	EXPECT_FALSE(Reg().AllOf<DontDraw>(_host));
	EXPECT_FALSE(Reg().AllOf<SuperVillager>(_host));
	EXPECT_EQ(_requests.Counts(), (std::vector<uint8_t> {1}));
	EXPECT_EQ(_markedAtRequest, (std::vector<bool> {false}));
	// off for a thing that was never marked: nothing to clear, and it still asks
	_host = Host();
	super_villager::SetHighGraphicsDetail(_host, false);
	EXPECT_FALSE(Reg().AllOf<DontDraw>(_host));
	EXPECT_EQ(_requests.Counts(), (std::vector<uint8_t> {1, 1}));
}

TEST_F(DontDrawMark, AThingTheScriptCannotFindIsLeftAlone)
{
	_host = Host();
	// no object, then an object no longer there
	CallNative(0, true);
	const auto gone = Host();
	Reg().Destroy(gone);
	CallNative(static_cast<uint32_t>(gone), true);
	EXPECT_FALSE(Reg().AllOf<DontDraw>(_host));
	EXPECT_TRUE(super_villager::List().empty());
	EXPECT_TRUE(_requests.Counts().empty());
	// the thing itself, through the same op
	CallNative(static_cast<uint32_t>(_host), true);
	EXPECT_TRUE(Reg().AllOf<DontDraw>(_host));
	EXPECT_EQ(_requests.Counts(), (std::vector<uint8_t> {1}));
}

namespace
{
/// SET_HIGH_GRAPHICS_DETAIL on a villager whose high mesh is none of the family's: it is drawn with its own high mesh.
/// The info table's special villagers all share one tribe and one job, as in info.dat: the piper first, then the hermit
class SuperVillagerHighMesh: public DontDrawMark
{
protected:
	void SetUp() override
	{
		DontDrawMark::SetUp();
		_info = std::make_unique<InfoConstants>();
		for (const auto& [type, high] : {std::pair {VillagerInfo::PiedPiper, MeshId::PersonPiedPiper},
		                                 std::pair {VillagerInfo::Hermit, MeshId::PersonHermit}})
		{
			auto& row = _info->villager.at(static_cast<size_t>(type));
			row.tribeType = Tribe::NONE;
			row.villagerNumber = VillagerNumber::Forester;
			row.highDetail = high;
			row.stdDetail = high;
		}
		Locator::infoConstants::emplace(*_info);
	}

	/// A special villager made with `stdMesh` as its Mesh and `highMesh` as its high mesh
	static entt::entity Special(MeshId stdMesh, MeshId highMesh)
	{
		const auto thing = Host();
		auto& villager = Reg().Assign<Villager>(thing);
		villager.tribe = Tribe::NONE;
		villager.number = VillagerNumber::Forester;
		Reg().Assign<Mesh>(thing, resources::HashIdentifier(stdMesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
		Reg().Assign<DetailMesh>(thing, resources::HashIdentifier(highMesh));
		return thing;
	}

	test::RestoreService<Locator::infoConstants> _restoreInfo;
	std::unique_ptr<InfoConstants> _info;
};
} // namespace

TEST_F(SuperVillagerHighMesh, ASpecialVillagerKeepsItsOwnModel)
{
	// the hermit shares the piper's tribe and job, and is still drawn as the hermit
	_host = Special(MeshId::PersonHermit, MeshId::PersonHermit);
	super_villager::SetHighGraphicsDetail(_host, true);
	ASSERT_TRUE(Reg().AllOf<SuperVillager>(_host));
	EXPECT_EQ(Reg().Get<Mesh>(_host).id, resources::HashIdentifier(MeshId::PersonHermit));
	EXPECT_EQ(Reg().Get<SuperVillager>(_host).hdMesh, 0u);
	EXPECT_EQ(Reg().Get<SuperVillager>(_host).eyeType, -1);
}

TEST_F(SuperVillagerHighMesh, TheHighMeshIsDrawnUntilTheRelease)
{
	// a thing whose own mesh is not its high one: the high one while it is a SuperVillager, its own one after
	_host = Special(MeshId::PersonCelticFemaleA1, MeshId::PersonHermit);
	super_villager::SetHighGraphicsDetail(_host, true);
	EXPECT_EQ(Reg().Get<Mesh>(_host).id, resources::HashIdentifier(MeshId::PersonHermit));
	super_villager::SetHighGraphicsDetail(_host, false);
	EXPECT_FALSE(Reg().AllOf<SuperVillager>(_host));
	EXPECT_EQ(Reg().Get<Mesh>(_host).id, resources::HashIdentifier(MeshId::PersonCelticFemaleA1));
}

TEST_F(SuperVillagerHighMesh, AThingWithNoHighMeshKeepsItsMesh)
{
	_host = Host();
	Reg().Assign<Mesh>(_host, resources::HashIdentifier(MeshId::PersonHermit), static_cast<int8_t>(0), static_cast<int8_t>(0));
	super_villager::SetHighGraphicsDetail(_host, true);
	ASSERT_TRUE(Reg().AllOf<SuperVillager>(_host));
	EXPECT_EQ(Reg().Get<Mesh>(_host).id, resources::HashIdentifier(MeshId::PersonHermit));
	EXPECT_EQ(Reg().Get<SuperVillager>(_host).savedMesh, 0u);
}

TEST_F(SuperVillagerHighMesh, TheTopDetailDrawsWhatTheSuperVillagerShowsUntilTheRelease)
{
	// a villager whose Mesh is its std one: while it is a SuperVillager its high mesh is drawn at every detail level,
	// the top one included, and after the release both are its own again
	_host = Special(MeshId::PersonCelticFemaleA2, MeshId::PersonHermit);
	super_villager::SetHighGraphicsDetail(_host, true);
	EXPECT_EQ(Reg().Get<Mesh>(_host).id, resources::HashIdentifier(MeshId::PersonHermit));
	EXPECT_EQ(Reg().Get<DetailMesh>(_host).high, resources::HashIdentifier(MeshId::PersonHermit));
	super_villager::SetHighGraphicsDetail(_host, false);
	EXPECT_EQ(Reg().Get<Mesh>(_host).id, resources::HashIdentifier(MeshId::PersonCelticFemaleA2));
	EXPECT_EQ(Reg().Get<DetailMesh>(_host).high, resources::HashIdentifier(MeshId::PersonHermit));
}

TEST(SuperVillagerDrawnMeshes, TheHdMeshGoesIntoBothDrawnMeshes)
{
	// the opening's father: his std and high meshes give way to the HD body, below the top detail and at it
	constexpr super_villager::DrawnMeshes k_Own {.mesh = 2, .high = 1};
	constexpr auto k_Shown = super_villager::WhileSuperVillager(k_Own, 9);
	static_assert(k_Shown.mesh == 9);
	static_assert(k_Shown.high == std::optional<entt::id_type> {9});
	EXPECT_EQ(k_Shown.mesh, 9u);
	EXPECT_EQ(k_Shown.high, std::optional<entt::id_type> {9});
}

TEST(SuperVillagerDrawnMeshes, AThingWithNoHighMeshOnlyChangesItsMesh)
{
	const auto shown = super_villager::WhileSuperVillager({.mesh = 2, .high = std::nullopt}, 9);
	EXPECT_EQ(shown.mesh, 9u);
	EXPECT_FALSE(shown.high.has_value());
}

TEST(SuperVillagerDrawnMeshes, NothingShownLeavesBoth)
{
	const auto shown = super_villager::WhileSuperVillager({.mesh = 2, .high = 1}, 0);
	EXPECT_EQ(shown.mesh, 2u);
	EXPECT_EQ(shown.high, std::optional<entt::id_type> {1});
}

TEST(SuperVillagerDrawnMeshes, AHiddenThingStaysHidden)
{
	// not drawn now (no Mesh, no mesh kept): only its high mesh takes what is shown
	const auto shown = super_villager::WhileSuperVillager({.mesh = 0, .high = 1}, 9);
	EXPECT_EQ(shown.mesh, 0u);
	EXPECT_EQ(shown.high, std::optional<entt::id_type> {9});
}
