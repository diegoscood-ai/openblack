/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's leash tie (ECS/LeashHandTie.h) with a fake leash: what the tie takes each thing to be, the packets the
// double click and the taps send for the next turn, and the turn's handlers, each call to the leash in order

#define LOCATOR_IMPLEMENTATIONS

#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/LeashTie.h"
#include "ECS/Components/LeashPost.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/LeashHandTie.h"
#include "Input/GamePackets.h"
#include "creature/CreatureSystemWorld.h"
#include "support/CreatureFakes.h"

using namespace openblack;
using openblack::creature_leash::HandTie;
using openblack::creature_leash::LeashTap;
using openblack::creature_leash::TieTargetKind;
using openblack::test::creature_loop_fakes::CallLog;
using openblack::test::creature_loop_fakes::FakeLeash;
using openblack::test::creature_loop_fakes::Id;
namespace leash_tie = openblack::ecs::leash_tie;
using game_packets::Packet;
using game_packets::Type;

namespace
{
class LeashHandTieTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		game_packets::Reset();
		_creature = test::creature_world::World::MakeCreature();
		_tree = test::creature_world::World::Registry().Create();
		_leash.playersCreature = _creature;
		_leash.leashed = true;
		_leash.known = {LeashType::Rope};
	}
	void TearDown() override
	{
		game_packets::Reset();
		game_packets::ClearHandlers();
	}

	/// The packets sent, as the next turn's handlers get them
	static std::vector<Packet> Sent()
	{
		static std::vector<Packet> seen;
		seen.clear();
		for (const auto type : {Type::LeashTie, Type::LeashActOnObject, Type::LeashActOnPoint})
		{
			game_packets::SetHandler(type, [](const Packet& packet) { seen.push_back(packet); });
		}
		game_packets::Flush();
		game_packets::DispatchQueuedPackets();
		return seen;
	}
	static Packet TieTo(entt::entity object)
	{
		return {.type = Type::LeashTie, .object = object, .player = PlayerNames::PLAYER_ONE};
	}
	[[nodiscard]] std::vector<std::string> Calls() const
	{
		std::vector<std::string> names;
		for (const auto& call : _log)
		{
			names.push_back(call.name);
		}
		return names;
	}

	test::creature_world::World _world;
	CallLog _log;
	FakeLeash _leash {_log};
	entt::entity _creature {entt::null};
	entt::entity _tree {entt::null};
};
} // namespace

TEST_F(LeashHandTieTest, EachThingIsTakenForWhatItIs)
{
	auto& registry = test::creature_world::World::Registry();
	const auto post = registry.Create();
	registry.Assign<ecs::components::LeashPost>(post);
	const auto icon = registry.Create();
	registry.Assign<ecs::components::SpellIcon>(icon);
	const auto highlight = registry.Create();
	registry.Assign<ecs::components::ScriptHighlight>(highlight);
	const auto seed = registry.Create();
	registry.Assign<ecs::components::OneOffSpellSeed>(seed);
	EXPECT_EQ(leash_tie::KindOf(_tree), TieTargetKind::Object);
	EXPECT_EQ(leash_tie::KindOf(_creature), TieTargetKind::Object);
	EXPECT_EQ(leash_tie::KindOf(post), TieTargetKind::LeashPost);
	EXPECT_EQ(leash_tie::KindOf(icon), TieTargetKind::SpellIcon);
	EXPECT_EQ(leash_tie::KindOf(highlight), TieTargetKind::ScriptHighlight);
	EXPECT_EQ(leash_tie::KindOf(seed), TieTargetKind::OneOffSpellSeed);
}

TEST_F(LeashHandTieTest, TheDoubleClicksFactsComeFromTheLeash)
{
	auto check = leash_tie::CheckDoubleClick(_leash, PlayerNames::PLAYER_ONE, _tree);
	EXPECT_TRUE(check.hasTarget);
	EXPECT_TRUE(check.hasCreature);
	EXPECT_FALSE(check.targetIsCreature);
	EXPECT_FALSE(check.tied);
	EXPECT_TRUE(check.leashInThisHand);
	_leash.tiedTo = _tree;
	check = leash_tie::CheckDoubleClick(_leash, PlayerNames::PLAYER_ONE, _tree);
	EXPECT_TRUE(check.tied);
	EXPECT_TRUE(check.targetIsTiedObject);
	check = leash_tie::CheckDoubleClick(_leash, PlayerNames::PLAYER_ONE, _creature);
	EXPECT_TRUE(check.targetIsCreature);
	EXPECT_FALSE(check.targetIsTiedObject);
	// no target, and no creature
	EXPECT_FALSE(leash_tie::CheckDoubleClick(_leash, PlayerNames::PLAYER_ONE, entt::null).hasTarget);
	_leash.playersCreature.reset();
	EXPECT_FALSE(leash_tie::CheckDoubleClick(_leash, PlayerNames::PLAYER_ONE, _tree).hasCreature);
}

TEST_F(LeashHandTieTest, ADoubleClickOnAThingSendsTheTieForTheNextTurn)
{
	EXPECT_EQ(leash_tie::DoubleClick(&_leash, PlayerNames::PLAYER_ONE, _tree), HandTie::Tie);
	const auto sent = Sent();
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent.front().type, Type::LeashTie);
	EXPECT_EQ(sent.front().object, _tree);
	EXPECT_EQ(sent.front().player, PlayerNames::PLAYER_ONE);
	// the leash itself is not touched until the packet is handled
	EXPECT_TRUE(_log.empty());
}

TEST_F(LeashHandTieTest, ADoubleClickOnWhatItIsTiedToSendsTheUntie)
{
	_leash.tiedTo = _tree;
	EXPECT_EQ(leash_tie::DoubleClick(&_leash, PlayerNames::PLAYER_ONE, _tree), HandTie::Untie);
	const auto sent = Sent();
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent.front().type, Type::LeashTie);
	EXPECT_EQ(sent.front().object, entt::entity {entt::null});
}

TEST_F(LeashHandTieTest, ADoubleClickOnASeedOrAPostSendsNothing)
{
	auto& registry = test::creature_world::World::Registry();
	const auto seed = registry.Create();
	registry.Assign<ecs::components::OneOffSpellSeed>(seed);
	const auto post = registry.Create();
	registry.Assign<ecs::components::LeashPost>(post);
	// the seed is the hand's to tap, the post is nothing
	EXPECT_EQ(leash_tie::DoubleClick(&_leash, PlayerNames::PLAYER_ONE, seed), HandTie::Tap);
	EXPECT_EQ(leash_tie::DoubleClick(&_leash, PlayerNames::PLAYER_ONE, post), HandTie::Nothing);
	EXPECT_EQ(leash_tie::DoubleClick(nullptr, PlayerNames::PLAYER_ONE, _tree), HandTie::Nothing);
	EXPECT_TRUE(Sent().empty());
}

TEST_F(LeashHandTieTest, ATapOnAThingWithTheLeashInTheHandSendsTheCreatureToIt)
{
	EXPECT_TRUE(leash_tie::TapObject(&_leash, PlayerNames::PLAYER_ONE, _tree));
	auto sent = Sent();
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent.front().type, Type::LeashActOnObject);
	EXPECT_EQ(sent.front().object, _tree);
	// tied, or with no leash on, the tap is the hand's own
	_leash.tiedTo = _tree;
	EXPECT_FALSE(leash_tie::TapObject(&_leash, PlayerNames::PLAYER_ONE, _tree));
	_leash.tiedTo.reset();
	_leash.leashed = false;
	EXPECT_FALSE(leash_tie::TapObject(&_leash, PlayerNames::PLAYER_ONE, _tree));
	EXPECT_FALSE(leash_tie::TapObject(&_leash, PlayerNames::PLAYER_ONE, entt::null));
	EXPECT_TRUE(Sent().empty());
}

TEST_F(LeashHandTieTest, ATapOnTheLandWithTheLeashInTheHandSendsThePoint)
{
	const glm::vec3 point(250.0f, 3.0f, 410.0f);
	EXPECT_EQ(leash_tie::TapLand(&_leash, PlayerNames::PLAYER_ONE, point), LeashTap::ActOnPoint);
	auto sent = Sent();
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent.front().type, Type::LeashActOnPoint);
	EXPECT_EQ(sent.front().position, point);
	// -0 in the first coordinate is not the origin
	EXPECT_EQ(leash_tie::TapLand(&_leash, PlayerNames::PLAYER_ONE, glm::vec3(-0.0f, 0.0f, 0.0f)), LeashTap::ActOnPoint);
	EXPECT_EQ(Sent().size(), 1u);
	// a tied leash leaves the tap to the hand
	_leash.tiedTo = _tree;
	EXPECT_EQ(leash_tie::TapLand(&_leash, PlayerNames::PLAYER_ONE, point), LeashTap::NotLeash);
	EXPECT_EQ(leash_tie::TapLand(nullptr, PlayerNames::PLAYER_ONE, point), LeashTap::NotLeash);
	EXPECT_TRUE(Sent().empty());
}

TEST_F(LeashHandTieTest, ATapOnTheLandAtTheOriginIsTakenAndSendsNothing)
{
	// the leash held, tied, or no leash service at all: nothing is sent and the hand does not tap
	EXPECT_EQ(leash_tie::TapLand(&_leash, PlayerNames::PLAYER_ONE, glm::vec3(0.0f)), LeashTap::Nothing);
	EXPECT_EQ(leash_tie::TapLand(&_leash, PlayerNames::PLAYER_ONE, glm::vec3(0.0f, 0.0f, -0.0f)), LeashTap::Nothing);
	_leash.tiedTo = _tree;
	EXPECT_EQ(leash_tie::TapLand(&_leash, PlayerNames::PLAYER_ONE, glm::vec3(0.0f)), LeashTap::Nothing);
	EXPECT_EQ(leash_tie::TapLand(nullptr, PlayerNames::PLAYER_ONE, glm::vec3(0.0f)), LeashTap::Nothing);
	EXPECT_TRUE(Sent().empty());
	EXPECT_TRUE(_log.empty());
}

TEST_F(LeashHandTieTest, TheTieTiesPullsAwayAndActsOnTheThingInThatOrder)
{
	EXPECT_TRUE(leash_tie::ApplyTie(&_leash, TieTo(_tree)));
	EXPECT_EQ(Calls(), (std::vector<std::string> {"leash.TieTo", "leash.PullAwayFromAction", "leash.ActOn"}));
	EXPECT_EQ(_log.at(0).args, (std::vector<float> {Id(_creature), Id(_tree)}));
	EXPECT_EQ(_log.at(2).args, (std::vector<float> {Id(_creature), Id(_tree)}));
}

TEST_F(LeashHandTieTest, ATieThatDoesNotTakeStillPullsAwayAndActsOnTheThing)
{
	_leash.tieTakes = false;
	EXPECT_TRUE(leash_tie::ApplyTie(&_leash, TieTo(_tree)));
	EXPECT_EQ(Calls(), (std::vector<std::string> {"leash.TieTo", "leash.PullAwayFromAction", "leash.ActOn"}));
	EXPECT_EQ(_log.at(2).args, (std::vector<float> {Id(_creature), Id(_tree)}));
}

TEST_F(LeashHandTieTest, TheTieWithNoThingPutsTheLeashBackInTheHand)
{
	EXPECT_TRUE(leash_tie::ApplyTie(&_leash, TieTo(entt::null)));
	EXPECT_EQ(Calls(), (std::vector<std::string> {"leash.ReturnToHand"}));
	EXPECT_EQ(_log.front().args, (std::vector<float> {Id(_creature)}));
}

TEST_F(LeashHandTieTest, TheTieNeedsTheLearningLeashKnownALeashOnAndWorking)
{
	_leash.known.clear();
	EXPECT_FALSE(leash_tie::ApplyTie(&_leash, TieTo(_tree)));
	_leash.known = {LeashType::Rope};
	_leash.leashed = false;
	EXPECT_FALSE(leash_tie::ApplyTie(&_leash, TieTo(_tree)));
	_leash.leashed = true;
	_leash.works = false;
	EXPECT_FALSE(leash_tie::ApplyTie(&_leash, TieTo(entt::null)));
	_leash.works = true;
	_leash.playersCreature.reset();
	EXPECT_FALSE(leash_tie::ApplyTie(&_leash, TieTo(_tree)));
	EXPECT_FALSE(leash_tie::ApplyTie(nullptr, TieTo(_tree)));
	EXPECT_TRUE(_log.empty());
}

TEST_F(LeashHandTieTest, TheActOnAThingNeedsOnlyALeashThatWorks)
{
	const Packet act {.type = Type::LeashActOnObject, .object = _tree, .player = PlayerNames::PLAYER_ONE};
	// no test of the leash being on, or of what the creature knows
	_leash.leashed = false;
	_leash.known.clear();
	EXPECT_TRUE(leash_tie::ApplyActOnObject(&_leash, act));
	EXPECT_EQ(Calls(), (std::vector<std::string> {"leash.ActOn"}));
	_log.clear();
	// taken off since the tap, the act still goes ahead: with no leash on, the leash counts as working
	_leash.works = false;
	EXPECT_TRUE(leash_tie::ApplyActOnObject(&_leash, act));
	EXPECT_EQ(Calls(), (std::vector<std::string> {"leash.ActOn"}));
	_log.clear();
	// on and not working, nothing
	_leash.leashed = true;
	EXPECT_FALSE(leash_tie::ApplyActOnObject(&_leash, act));
	_leash.works = true;
	_leash.playersCreature.reset();
	EXPECT_FALSE(leash_tie::ApplyActOnObject(&_leash, act));
	_leash.playersCreature = _creature;
	EXPECT_FALSE(leash_tie::ApplyActOnObject(
	    &_leash, {.type = Type::LeashActOnObject, .object = entt::null, .player = PlayerNames::PLAYER_ONE}));
	EXPECT_TRUE(_log.empty());
}

TEST_F(LeashHandTieTest, TheActOnAPointIsCheckedButNotCarriedOutYet)
{
	const Packet act {.type = Type::LeashActOnPoint, .position = glm::vec3(5.0f), .player = PlayerNames::PLAYER_ONE};
	EXPECT_TRUE(leash_tie::ApplyActOnPoint(&_leash, act));
	_leash.works = false;
	EXPECT_FALSE(leash_tie::ApplyActOnPoint(&_leash, act));
	_leash.works = true;
	_leash.leashed = false;
	EXPECT_FALSE(leash_tie::ApplyActOnPoint(&_leash, act));
	EXPECT_TRUE(_log.empty());
}
