/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// SPECIAL_EFFECT_POSITION, as a script calls it: a spot visual is started at the point, given the local player and
// added to the scripts' things as one the script created, through a fake particle system and a fake table of the
// scripts' things that record what they are handed (a registry only: no files).
// docs/wiki/physics/particles.md#the-original

#include <cstdint>

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Enums.h"
#include "GameClock.h"
#include "Locator.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// Makes an object for every spot visual it is asked for, unless told to make none, and records the players given
class SpotVisualRecordingParticles final: public test::InertParticleSystem
{
public:
	struct Started
	{
		SpotVisualType type;
		glm::vec3 position;
		std::optional<int> turns;
	};

	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> turns, entt::entity /*owner*/,
	                             float /*magnitude*/) override
	{
		started.push_back({.type = type, .position = position, .turns = turns});
		return makesNone ? entt::null : Locator::entitiesRegistry::value().Create();
	}
	void SetSpotVisualPlayer(entt::entity visual, int player) override { players.emplace_back(visual, player); }

	bool makesNone {false};
	std::vector<Started> started;
	std::vector<std::pair<entt::entity, int>> players;
};

/// The scripts' table of objects, as far as the native reaches it: what each Register was given
class RegisteredThings final: public ecs::systems::ScriptObjectsSystemInterface
{
public:
	bool Register(entt::entity object, bool createdByScript) override
	{
		registered.emplace_back(object, createdByScript);
		return true;
	}
	void EnterNative(uint32_t /*native*/) override {}
	entt::entity Fetch(entt::entity object) override { return object; }
	void AddReference(entt::entity /*object*/) override {}
	void RemoveReference(entt::entity /*object*/) override {}
	void ReleaseFromScript(entt::entity /*object*/) override {}
	void Replace(entt::entity /*from*/, entt::entity /*to*/) override {}
	void Reset() override {}
	void ReleaseUnreferenced() override {}
	bool Disband(entt::entity /*container*/) override { return false; }
	[[nodiscard]] std::vector<ecs::systems::ScriptObjectPlace> Places() const override { return {}; }
	[[nodiscard]] uint32_t TimesFull() const override { return 0; }
	void ClearForNewLand() override {}

	std::vector<std::pair<entt::entity, bool>> registered;
};

class SpotVisualPositionNative: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the natives log to the scripts' logger: a null sink, as the other script tests
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::vm::emplace<lhvm::LHVM>();
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		// entity 0 is "no thing" for a script id: the test's first entity must not be a scripted one
		registry.Create();
		_particles =
		    &static_cast<SpotVisualRecordingParticles&>(Locator::particleSystem::emplace<SpotVisualRecordingParticles>());
		_things = &static_cast<RegisteredThings&>(Locator::scriptObjects::emplace<RegisteredThings>());
	}

	/// The native as a script calls it: the effect pushed first, then the point and the seconds; it gives back the
	/// visual's object
	uint32_t Call(int32_t effect, glm::vec3 point, float seconds)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "SPECIAL_EFFECT_POSITION"; });
		EXPECT_NE(native, table.end());
		if (native == table.end())
		{
			return 0;
		}
		auto& vm = Locator::vm::value();
		vm.Pushi(effect);
		vm.Pushv(point.x);
		vm.Pushv(point.y);
		vm.Pushv(point.z);
		vm.Pushf(seconds);
		native->impl();
		return vm.Pop().uintVal;
	}

	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::particleSystem> _restoreParticles;
	test::RestoreService<Locator::playerSystem> _restorePlayers;
	test::RestoreService<Locator::scriptObjects> _restoreThings;
	chlapi::CHLApi _api;
	SpotVisualRecordingParticles* _particles {nullptr};
	RegisteredThings* _things {nullptr};
};
} // namespace

TEST_F(SpotVisualPositionNative, TheVisualStartsAtThePointForTheLocalPlayer)
{
	Locator::playerSystem::reset();
	const auto visual = Call(static_cast<int32_t>(SpotVisualType::Smoke), glm::vec3(100.0f, 2.0f, 200.0f), 3.0f);
	ASSERT_EQ(_particles->started.size(), 1u);
	EXPECT_EQ(_particles->started.front().type, SpotVisualType::Smoke);
	EXPECT_EQ(_particles->started.front().position, glm::vec3(100.0f, 2.0f, 200.0f));
	ASSERT_NE(visual, 0u);
	// without the players' service the local player is the first one
	const std::vector<std::pair<entt::entity, int>> k_Given {
	    {static_cast<entt::entity>(visual), static_cast<int>(PlayerNames::PLAYER_ONE)}};
	EXPECT_EQ(_particles->players, k_Given);
	// the script holds it as a thing it created
	const std::vector<std::pair<entt::entity, bool>> k_Registered {{static_cast<entt::entity>(visual), true}};
	EXPECT_EQ(_things->registered, k_Registered);
}

TEST_F(SpotVisualPositionNative, TheSecondsBecomeTheGamesTurns)
{
	// whole turns of the seconds, the turns a second worked out first; 0 stays 0 (the visual closes at its first turn)
	// and a negative count stays negative (for ever), not the entry's own life
	const auto turnsFor = [this](float seconds) {
		Call(static_cast<int32_t>(SpotVisualType::Smoke), glm::vec3(0.0f), seconds);
		return _particles->started.back().turns;
	};
	EXPECT_EQ(turnsFor(3.0f), std::optional(game_clock::TicksForSeconds(3.0f)));
	EXPECT_EQ(turnsFor(0.1f), std::optional(game_clock::TicksForSeconds(0.1f)));
	EXPECT_EQ(turnsFor(0.0f), std::optional(0));
	EXPECT_EQ(turnsFor(-1.0f), std::optional(game_clock::TicksForSeconds(-1.0f)));
	EXPECT_LT(*turnsFor(-1.0f), 0);
}

TEST_F(SpotVisualPositionNative, AVisualNotMadeGivesNoThing)
{
	_particles->makesNone = true;
	EXPECT_EQ(Call(static_cast<int32_t>(SpotVisualType::Smoke), glm::vec3(0.0f), 3.0f), 0u);
	// the player goes to no visual, which the particle system leaves alone
	for (const auto& [object, player] : _particles->players)
	{
		EXPECT_EQ(object, entt::entity {entt::null});
	}
	// and the script is given nothing to hold
	EXPECT_TRUE(_things->registered.empty());
}
