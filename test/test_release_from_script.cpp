/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The intro opcodes: the scripts' release of a
// script container (ReleaseControlFromScript: the disband and the deletion of a created flock at a task's end,
// the members' references of one not controlled) and the vagrants' list (the villagers' ReleaseFromScript).

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHeld.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Flocks.h"
#include "ECS/Registry.h"
#include "ECS/ScriptContainers.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Town/TownVillagers.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "creature/CreatureSystemFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace flocks = openblack::ecs::flocks;
namespace held = openblack::ecs::script_held;

namespace
{
class ReleaseTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		// the scripts' logger (ScriptContainers / ScriptHeld log their errors there): a null sink, as in
		// test_interface_interaction; without one SPDLOG_LOGGER_ERROR dereferences a null logger
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		// entity 0 is "no thing" for a script id (0 -> none), as the game has made
		// other things before: the test's first entity must not be a scripted one
		Locator::entitiesRegistry::value().Create();
		ecs::town_villagers::ClearVagrants();
	}

	void TearDown() override
	{
		ecs::town_villagers::ClearVagrants();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity MakeVillager()
	{
		const auto e = Reg().Create();
		auto& v = Reg().Assign<Villager>(e);
		v.town = entt::null;
		v.abode = entt::null;
		Reg().Assign<Transform>(e, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return e;
	}
};
} // namespace

TEST_F(ReleaseTest, VagrantsHeadFirstOnce)
{
	const auto a = MakeVillager();
	const auto b = MakeVillager();
	EXPECT_TRUE(ecs::town_villagers::AddToVagrants(a));
	EXPECT_TRUE(ecs::town_villagers::AddToVagrants(b));
	EXPECT_FALSE(ecs::town_villagers::AddToVagrants(a));
	EXPECT_EQ(ecs::town_villagers::Vagrants(), (std::vector<entt::entity> {b, a}));
}

TEST_F(ReleaseTest, ACreatedFlockGoesAtTheTasksEnd)
{
	const auto f = ecs::script_containers::CreateFlock(glm::vec3(0.0f));
	const auto a = MakeVillager();
	const auto b = MakeVillager();
	flocks::AddMember(f, a);
	flocks::AddMember(f, b);
	// a script variable takes it (controlled: it was created) and lets it go
	held::IncrementReference(f);
	EXPECT_TRUE(held::IsControlledByScript(f));
	held::DecrementReference(f);
	// ReleaseControlFromScript(f, id, 0): disbanded (the villagers only removed), then marked to be deleted
	held::Process();
	EXPECT_FALSE(Reg().Valid(f));
	EXPECT_TRUE(flocks::FlockOf(a) == entt::null);
	EXPECT_TRUE(flocks::FlockOf(b) == entt::null);
}

TEST_F(ReleaseTest, ReleaseFromScriptKeepsTheFlock)
{
	const auto f = ecs::script_containers::CreateFlock(glm::vec3(0.0f));
	const auto a = MakeVillager();
	flocks::AddMember(f, a);
	held::IncrementReference(f);
	// RELEASE_FROM_SCRIPT: its controlled members first (none), then the flock: no longer controlled, not deleted
	held::ReleaseFromScript(f);
	EXPECT_FALSE(held::IsControlledByScript(f));
	EXPECT_TRUE(Reg().Valid(f));
	EXPECT_TRUE(flocks::FlockOf(a) == f);
	// not controlled: nothing more
	held::ReleaseFromScript(f);
	EXPECT_TRUE(Reg().Valid(f));
}

TEST_F(ReleaseTest, TheScriptTagsFollowTheSlot)
{
	using ecs::components::InScript;
	using ecs::components::ScriptControlled;
	using ecs::components::ScriptHeld;
	// found by a script: in it, not controlled
	const auto found = Reg().Create();
	held::AddScriptThing(found, false);
	EXPECT_FALSE(Reg().AllOf<InScript>(found));
	held::IncrementReference(found);
	EXPECT_TRUE(Reg().AllOf<InScript>(found));
	EXPECT_FALSE(Reg().AllOf<ScriptControlled>(found));
	EXPECT_TRUE(held::IsInScript(found));
	EXPECT_FALSE(held::IsControlledByScript(found));
	// created by a script: in it and controlled, until RELEASE_FROM_SCRIPT, which keeps it in the script
	const auto made = Reg().Create();
	held::AddScriptThing(made, true);
	held::IncrementReference(made);
	EXPECT_TRUE(Reg().AllOf<InScript>(made));
	EXPECT_TRUE(Reg().AllOf<ScriptControlled>(made));
	held::ReleaseFromScript(made);
	EXPECT_FALSE(Reg().AllOf<ScriptControlled>(made));
	EXPECT_TRUE(Reg().AllOf<InScript>(made));
	// controlled without a slot (the vortex): the tag and a slot-less ScriptHeld, which the release leaves alone
	const auto carried = Reg().Create();
	held::SetControlledByScript(carried, true);
	EXPECT_TRUE(held::IsControlledByScript(carried));
	EXPECT_FALSE(held::IsInScript(carried));
	// the last references go: the slots are freed with their tags
	held::DecrementReference(found);
	held::DecrementReference(made);
	held::Process();
	for (const auto thing : {found, made})
	{
		EXPECT_FALSE(Reg().AllOf<ScriptHeld>(thing));
		EXPECT_FALSE(Reg().AllOf<InScript>(thing));
		EXPECT_FALSE(Reg().AllOf<ScriptControlled>(thing));
	}
	EXPECT_TRUE(Reg().AllOf<ScriptControlled>(carried));
	held::SetControlledByScript(carried, false);
	EXPECT_FALSE(held::IsControlledByScript(carried));
	EXPECT_TRUE(Reg().AllOf<ScriptHeld>(carried));
}

TEST_F(ReleaseTest, AThingAScriptMadeHidesItsNameForGood)
{
	using ecs::components::NameHidden;
	// found by a script: its name shows
	const auto found = Reg().Create();
	held::AddScriptThing(found, false);
	EXPECT_FALSE(Reg().AllOf<NameHidden>(found));
	// made by a script: hidden, and still hidden once the script's slot is freed
	const auto made = Reg().Create();
	held::AddScriptThing(made, true);
	EXPECT_TRUE(Reg().AllOf<NameHidden>(made));
	held::IncrementReference(made);
	held::DecrementReference(made);
	held::Process();
	EXPECT_FALSE(held::IsInScript(made));
	EXPECT_TRUE(Reg().AllOf<NameHidden>(made));
	// a thing already in a script that a script then takes as made hides its name too
	held::IncrementReference(found);
	held::AddScriptThing(found, true);
	EXPECT_TRUE(Reg().AllOf<NameHidden>(found));
}

// A creature given a script state, by a flock or a dance taking it or letting it go as by SET_SCRIPT_STATE, takes no
// state of its own: it plays what the script last gave it, whatever the state
TEST_F(ReleaseTest, ACreatureGivenAScriptStatePlaysWhatTheScriptGaveIt)
{
	const test::RestoreService<Locator::creatureMindSystem> restoreMinds;
	auto& minds =
	    static_cast<test::creature_fakes::FakeMind&>(Locator::creatureMindSystem::emplace<test::creature_fakes::FakeMind>());
	const auto creature = Reg().Create();
	Reg().Assign<ecs::components::Creature>(creature);
	Reg().Assign<Transform>(creature, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	held::SetLivingScriptState(creature, 27);
	held::SetLivingScriptState(creature, 4);
	EXPECT_EQ(minds.playedForScript, (std::vector<entt::entity> {creature, creature}));
}
