/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature help system over a registry of its own, a player service of its own, a fake of the creature hand and a
// recorder of the script VM: only the local player's creature is told of, a message raised while the hand holds the
// creature waits and is told the turn after it lets go, the help switched off drops what is raised, a creature fainting
// tells why, and the prompts on how to look after it come one at a time, at most every two minutes

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Creature/CreatureHelp.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureHelp.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureHelpSystem.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "GameClock.h"
#include "Help/ScriptControl.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureHelpSystem;
using openblack::ecs::systems::PlayerSystem;
using Kind = openblack::creature_help::Kind;

namespace
{
/// The creature the hand holds, if any; everything else does nothing
class FakeCreatureHand final: public ecs::systems::CreatureHandSystemInterface
{
public:
	std::optional<entt::entity> held;

	bool Grab(entt::entity) override { return false; }
	[[nodiscard]] bool MayHold(entt::entity) const override { return true; }
	std::optional<HandPose> Update(const glm::vec3&, const glm::vec3&, glm::vec2, float, float) override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<Feedback> Release() override { return std::nullopt; }
	[[nodiscard]] std::optional<HandPose> Pose() const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override { return held; }
	[[nodiscard]] std::optional<CreatureHit> CreatureAlong(const glm::vec3&, const glm::vec3&) const override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<entt::entity> CreatureUnderHand() const override { return std::nullopt; }
	void SetCreatureUnderHand(std::optional<entt::entity>) override {}
	bool Stroke(entt::entity, creature_feedback::BodyPart) override { return false; }
	bool Slap(entt::entity, float, bool, bool) override { return false; }
	[[nodiscard]] bool IsHeldByCommand() const override { return false; }
	[[nodiscard]] float GetFeedbackSum() const override { return 0.0f; }
	[[nodiscard]] float GetLastFeedbackSum() const override { return 0.0f; }
	void FollowWithCamera(entt::entity) override {}
	void GiveCameraBack() override {}
	void SetInteractionCamera(bool) override {}
	[[nodiscard]] bool InteractionCamera() const override { return false; }
	[[nodiscard]] std::optional<creature_interact::Holder> CameraTakenFrom() const override { return std::nullopt; }
	[[nodiscard]] std::optional<creature_interact::Holder> LastTakenOff() const override { return std::nullopt; }
};

/// The interface's pick, set by the test; nothing else is found
class FakePicking final: public ecs::systems::PickingSystemInterface
{
public:
	Pick pick;

	[[nodiscard]] std::optional<glm::vec3> LandAlong(glm::vec3, glm::vec3) const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> LandOrSeaAlong(glm::vec3, glm::vec3, glm::vec3) const override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<glm::vec3> LandUnderPixel(glm::vec3, glm::vec3, bool) const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> LandFrom2DPoint(glm::ivec2, bool, float) const override { return std::nullopt; }
	void PickUnderCursor(const Frame&) override {}
	[[nodiscard]] const Pick& GetPick() const override { return pick; }
	[[nodiscard]] std::optional<screen_pick::MeshHit> FeelModel(entt::entity, glm::vec3, glm::vec3) const override
	{
		return std::nullopt;
	}
};

/// A script the help started, with the number it pushed first, if any
struct Started
{
	std::string script;
	std::optional<float> pushed;
};

/// The game's turn, set for one test and put back at its end
class TurnAt
{
public:
	explicit TurnAt(uint32_t turn)
	    : _previous(game_clock::Turn())
	{
		game_clock::SetTurn(turn);
	}
	~TurnAt() { game_clock::SetTurn(_previous); }
	TurnAt(const TurnAt&) = delete;
	TurnAt& operator=(const TurnAt&) = delete;
	TurnAt(TurnAt&&) = delete;
	TurnAt& operator=(TurnAt&&) = delete;

	static void Set(uint32_t turn) { game_clock::SetTurn(turn); }

private:
	uint32_t _previous;
};

constexpr glm::vec3 k_CreatureAt {100.0f, 0.0f, 100.0f};

class CreatureHelpSystemTest: public ::testing::Test
{
protected:
	CreatureHelpSystemTest()
	    : _system(Recorder())
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_players = &static_cast<PlayerSystem&>(Locator::playerSystem::emplace<PlayerSystem>());
		_hand = &static_cast<FakeCreatureHand&>(Locator::creatureHandSystem::emplace<FakeCreatureHand>());
		// a camera looking at the creature from 50 units south of it
		auto& camera = Locator::camera::emplace(k_CreatureAt);
		camera.SetOrigin(k_CreatureAt + glm::vec3(0.0f, 20.0f, -50.0f)).SetFocus(k_CreatureAt);
		// no species' tables, no dialogue owner, no temple: the help's own defaults
		Locator::infoConstants::reset();
		Locator::dialogueControlSystem::reset();
		Locator::temple::reset();
	}

	/// The scripts the help starts, recorded in place of the VM
	help::script_control::Vm Recorder()
	{
		help::script_control::Vm vm;
		vm.pushFloat = [this](float value) { _pushed = value; };
		vm.startScript = [this](std::string_view name, uint32_t) {
			_started.push_back({.script = std::string(name), .pushed = _pushed});
			_pushed.reset();
		};
		return vm;
	}

	/// A grown up creature with a mind and desires; the player's creature unless said otherwise
	entt::entity MakeCreature(bool players = true, glm::vec3 at = k_CreatureAt)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto creature = registry.Create();
		registry.Assign<Transform>(creature, Transform {.position = at, .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
		registry.Assign<Creature>(creature,
		                          Creature {.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Tiger, .mind = 0});
		auto& mind = registry.Assign<CreatureMindState>(creature);
		mind.desires = creature_desires::Desires {};
		if (players)
		{
			_players->SetPrimaryCreature(PlayerNames::PLAYER_ONE, creature);
		}
		return creature;
	}

	static bool Waiting(entt::entity creature, Kind kind)
	{
		const auto* help = std::as_const(Locator::entitiesRegistry::value()).TryGet<const CreatureHelpMemory>(creature);
		if (help == nullptr)
		{
			return false;
		}
		const auto& entries = help->memory.entries.at(static_cast<size_t>(kind));
		return std::ranges::any_of(entries, &creature_help::Entry::waiting);
	}

	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const test::RestoreService<Locator::playerSystem> _restorePlayers;
	const test::RestoreService<Locator::creatureHandSystem> _restoreHand;
	const test::RestoreService<Locator::camera> _restoreCamera;
	const test::RestoreService<Locator::infoConstants> _restoreInfo;
	const test::RestoreService<Locator::dialogueControlSystem> _restoreDialogue;
	const test::RestoreService<Locator::temple> _restoreTemple;
	std::optional<float> _pushed;
	std::vector<Started> _started;
	PlayerSystem* _players {nullptr};
	FakeCreatureHand* _hand {nullptr};
	CreatureHelpSystem _system;
};
} // namespace

TEST_F(CreatureHelpSystemTest, OnlyThePlayersCreatureIsToldOf)
{
	const TurnAt turn(10);
	const auto other = MakeCreature(false, k_CreatureAt + glm::vec3(10.0f, 0.0f, 0.0f));
	const auto creature = MakeCreature();
	_system.Raise(other, {.kind = Kind::LessonNormalAction, .parameter = 3});
	EXPECT_TRUE(_started.empty());
	EXPECT_TRUE(_system.GetTold().empty());

	_system.Raise(creature, {.kind = Kind::LessonNormalAction, .parameter = 3});
	ASSERT_EQ(_started.size(), 1u);
	EXPECT_EQ(_started.front().script, "HelpForCreaturePointAtCreature");
	ASSERT_TRUE(_started.front().pushed.has_value());
	EXPECT_FLOAT_EQ(*_started.front().pushed, 879.0f);
	ASSERT_EQ(_system.GetTold().size(), 1u);
	EXPECT_EQ(_system.GetTold().front().creature, creature);
	EXPECT_EQ(_system.GetTold().front().text, 879u);
	EXPECT_EQ(_system.GetTold().front().turn, 10u);
}

TEST_F(CreatureHelpSystemTest, HeldInTheHandItWaitsAndIsToldOnceLetGo)
{
	const TurnAt turn(10);
	const auto creature = MakeCreature();
	_hand->held = creature;
	_system.Raise(creature, {.kind = Kind::LessonNormalAction, .parameter = 3});
	EXPECT_TRUE(_started.empty());
	EXPECT_TRUE(Waiting(creature, Kind::LessonNormalAction));

	// let go, the next turn raises it again and tells it: it waits no more
	_hand->held.reset();
	TurnAt::Set(11);
	_system.ProcessTurn();
	ASSERT_EQ(_started.size(), 1u);
	EXPECT_EQ(_started.front().script, "HelpForCreaturePointAtCreature");
	EXPECT_EQ(_started.front().pushed, 879.0f);
	EXPECT_FALSE(Waiting(creature, Kind::LessonNormalAction));
}

TEST_F(CreatureHelpSystemTest, SwitchedOffWhatIsRaisedIsLetGo)
{
	const TurnAt turn(10);
	const auto creature = MakeCreature();
	_system.SetHelpOn(false);
	EXPECT_FALSE(_system.IsHelpOn());
	_system.Raise(creature, {.kind = Kind::LessonNormalAction, .parameter = 3});
	EXPECT_TRUE(_started.empty());
	EXPECT_FALSE(Waiting(creature, Kind::LessonNormalAction));

	_system.SetHelpOn(true);
	_system.Raise(creature, {.kind = Kind::LessonNormalAction, .parameter = 3});
	EXPECT_EQ(_started.size(), 1u);
}

TEST_F(CreatureHelpSystemTest, FaintingTellsWhy)
{
	const TurnAt turn(10);
	const auto creature = MakeCreature();
	auto& needs = Locator::entitiesRegistry::value().Assign<CreatureNeeds>(creature);
	needs.started = true;
	needs.needs.life = 0.0f;
	_system.Fainting(creature);
	ASSERT_EQ(_started.size(), 1u);
	EXPECT_EQ(_started.front().script, "HelpForCreatureJustText");
	// badly hurt: the fourth of the body's problems
	EXPECT_EQ(_started.front().pushed, 871.0f);
	ASSERT_EQ(_system.GetTold().size(), 1u);
	EXPECT_EQ(_system.GetTold().front().kind, Kind::PhysicalProblem);
	EXPECT_EQ(_system.GetTold().front().parameter, 3u);
}

TEST_F(CreatureHelpSystemTest, PromptsComeOneAtATimeEveryTwoMinutes)
{
	const TurnAt turn(1209);
	MakeCreature();
	// not yet more than two minutes since the game began
	_system.ProcessTurn();
	EXPECT_TRUE(_started.empty());

	// then the first prompt: how to put the leash on it
	TurnAt::Set(1210);
	_system.ProcessTurn();
	ASSERT_EQ(_started.size(), 1u);
	EXPECT_EQ(_started.back().script, "HelpForCreatureJustText");
	EXPECT_EQ(_started.back().pushed, 1084.0f);

	// two minutes after it, not before, the next: how to make it run, never told yet
	TurnAt::Set(2419);
	_system.ProcessTurn();
	EXPECT_EQ(_started.size(), 1u);
	TurnAt::Set(2420);
	_system.ProcessTurn();
	ASSERT_EQ(_started.size(), 2u);
	EXPECT_EQ(_started.back().pushed, 1085.0f);
}

TEST_F(CreatureHelpSystemTest, PromptsNeedTheCreatureInView)
{
	const TurnAt turn(1210);
	MakeCreature();
	// the camera turned away from it
	Locator::camera::value().SetFocus(k_CreatureAt + glm::vec3(0.0f, 0.0f, -100.0f));
	_system.ProcessTurn();
	EXPECT_TRUE(_started.empty());
}

// The hand kept over a creature that thinks nothing of its own yet is still followed, as the game follows it every
// turn outside the creature's thinking: nine tenths of the last reading and a tenth for the hand being over it
TEST_F(CreatureHelpSystemTest, TheHandOverACreatureIsFollowedWhenItDoesNotThink)
{
	const test::RestoreService<Locator::pickingSystem> restorePicking;
	auto& picking = static_cast<FakePicking&>(Locator::pickingSystem::emplace<FakePicking>());
	const TurnAt turn(10);
	const auto creature = MakeCreature();
	Locator::entitiesRegistry::value().Get<CreatureMindState>(creature).desires.reset();
	picking.pick.object = creature;
	_system.ProcessTurn();
	_system.ProcessTurn();
	const auto* help = std::as_const(Locator::entitiesRegistry::value()).TryGet<const CreatureHelpMemory>(creature);
	ASSERT_NE(help, nullptr);
	EXPECT_FLOAT_EQ(help->memory.handOver, creature_help::NextHandOver(creature_help::NextHandOver(0.0f, true), true));
	EXPECT_FLOAT_EQ(help->memory.handOver, 0.19f);
	EXPECT_TRUE(_started.empty());
}
