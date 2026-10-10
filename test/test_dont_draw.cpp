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
// only: no files, no meshes). docs/bw1-notes/original-frame.md §6

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <string_view>
#include <vector>

#include <LHVM.h>
#include <glm/glm.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Components/DontDraw.h"
#include "ECS/Components/Transform.h"
#include "ECS/Events/DrawListEvents.h"
#include "ECS/Registry.h"
#include "ECS/SuperVillager.h"
#include "Locator.h"
#include "support/DrawListRequests.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;

namespace
{
namespace super_villager = ecs::super_villager;
using ecs::Registry;
using ecs::components::DontDraw;
using ecs::components::SuperVillager;
using ecs::components::Transform;

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
