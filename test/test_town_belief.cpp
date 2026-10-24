/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The town belief (ECS/Town/TownBelief): SetBelief's
// cap, Init and the ctor order, the per-turn Fold (pending, recent, boredom, the desires' cost, the
// conversion), ProcessOncePerTurn, CheckLosingBelief's help sprite and the land script's SET_TOWN_BELIEF*.
// The values are those of info.dat.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/EventManager.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/FloatingNumber.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Events/TownBeliefEvents.h"
#include "ECS/Influence/InfluenceState.h"
#include "ECS/Registry.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownBelief.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LandBalance.h"
#include "Locator.h"
#include "Worship/TownCentreSpellIcon.h"
#include "creature/CreatureSystemFakes.h"
#include "support/LandFakes.h"
#include "support/ParticleFakes.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace tb = openblack::ecs::town_belief;

namespace
{
constexpr size_t P(PlayerNames player)
{
	return static_cast<size_t>(player);
}

constexpr size_t k_One = P(PlayerNames::PLAYER_ONE);
constexpr size_t k_Two = P(PlayerNames::PLAYER_TWO);
constexpr size_t k_Neutral = P(PlayerNames::NEUTRAL);

/// The spot visuals started and the players given to them
class RecordingParticles final: public test::InertParticleSystem
{
public:
	std::vector<SpotVisualType> started;
	std::vector<int> players;

	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 /*position*/, std::optional<int> /*turns*/,
	                             entt::entity /*owner*/, float /*magnitude*/) override
	{
		started.push_back(type);
		return Locator::entitiesRegistry::value().Create();
	}
	void SetSpotVisualPlayer(entt::entity /*visual*/, int player) override { players.push_back(player); }
};

class TownBeliefTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		// the belief, player and town values of info.dat
		info->belief.claimedTownBeliefMultiplier = 1.5f;
		info->belief.lostATownBeliefInPlayerMultiplier = 0.9f;
		info->belief.minimumThreshold = 0.25f;
		info->belief.beliefLeftWhenHelpSpritesWarn = 0.05f;
		info->player.computerPlayerBeliefChangeDecay = 0.997f;
		info->town.beliefInNeutralPlayer = 0.5f;
		// the town desires' belief scale, delay and threshold decay
		const std::array<float, 17> scales = {6e-5f, 5e-5f, 2e-5f, 1.3e-4f, 0.0f,  2e-5f, 3e-5f, 0.0f, 0.0f,
		                                      0.0f,  0.0f,  0.0f,  3e-5f,   2e-5f, 0.0f,  2e-5f, 2e-5f};
		for (size_t d = 0; d < scales.size(); ++d)
		{
			auto& desire = info->townDesire.at(d);
			desire.desireToBeliefScale = scales.at(d);
			desire.desireAffectsBeliefAfter = d == 14 ? 5.0f : 1.0f;
			desire.desireToBeliefThresholdDecay = 2.1e-5f;
		}
		// the reactions' boredom additions: 0.00056, 0.00028 for 0, 3, 5 and 28
		for (size_t k = 0; k < info->reaction.size(); ++k)
		{
			const bool half = k == 0 || k == 3 || k == 5 || k == 28;
			info->reaction.at(k).additionToTownBoredomMultipliers = half ? 0.00028f : 0.00056f;
		}
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		// a town taken over starts its fireworks: no particles run here
		Locator::particleSystem::emplace<test::InertParticleSystem>();
		land_balance::Reset();
		game_clock::SetTurn(1);
		influence::detail::MapGlobals().landNumber = 0;
		tb::detail::ClearForTests();
		// A fresh event manager per test: its handlers capture this fixture
		Locator::events::emplace<EventManager>();
		auto& events = Locator::events::value();
		_help.clear();
		events.AddHandler<ecs::events::TownBeliefHelp>(
		    [this](const ecs::events::TownBeliefHelp& event) { _help.emplace_back(event.kind, event.town); });
		_tips.clear();
		events.AddHandler<ecs::events::TownBeliefToolTip>(
		    [this](const ecs::events::TownBeliefToolTip& event) { _tips.emplace_back(event.text, event.value); });
	}

	void TearDown() override
	{
		Locator::terrainSystem::reset();
		Locator::handSystem::reset();
		Locator::events::reset();
		tb::detail::ClearForTests();
		influence::detail::MapGlobals().landNumber = 0;
		land_balance::Reset();
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static entt::entity MakeTown(int id, PlayerNames owner)
	{
		return ecs::archetypes::TownArchetype::Create(id, glm::vec3(100.0f * static_cast<float>(id), 0.0f, 0.0f), owner,
		                                              Tribe::CELTIC);
	}

	static Town& TownOf(entt::entity town) { return Locator::entitiesRegistry::value().Get<Town>(town); }
	static TownBelief& BeliefOf(entt::entity town) { return TownOf(town).belief; }

	std::vector<std::pair<tb::detail::HelpSprite, entt::entity>> _help;
	std::vector<std::pair<uint32_t, float>> _tips; ///< help::tooltips::Force(text, value)
};
} // namespace

// 1. SetBelief clamps to the cap only
TEST_F(TownBeliefTest, SetBeliefClampsToCapOnly)
{
	TownBelief belief {};
	tb::SetBelief(belief, PlayerNames::PLAYER_ONE, 12.0f);
	EXPECT_EQ(belief.belief.at(k_One), 10.0f); // the default cap 10 (Init)
	tb::SetBelief(belief, PlayerNames::PLAYER_ONE, -3.0f);
	EXPECT_EQ(belief.belief.at(k_One), -3.0f); // no lower clamp
	tb::SetCap(belief, PlayerNames::PLAYER_ONE, 2.0f);
	tb::SetBelief(belief, PlayerNames::PLAYER_ONE, 5.0f);
	EXPECT_EQ(belief.belief.at(k_One), 2.0f);
	EXPECT_EQ(tb::GetCap(belief, PlayerNames::PLAYER_ONE), 2.0f);
	EXPECT_EQ(tb::GetBeliefInPlayer(belief, PlayerNames::PLAYER_ONE), 2.0f);
	tb::SetBelief(belief, PlayerNames::PLAYER_ONE, 2.0f); // equal: v itself
	EXPECT_EQ(belief.belief.at(k_One), 2.0f);
}

// 2. Init and the ctor order (Init before beliefInNeutralPlayer: the neutral belief 0 until the first fold)
TEST_F(TownBeliefTest, InitAndCtorOrder)
{
	Town town {};
	auto& b = town.belief;
	b.belief.fill(5.0f);
	b.cap.fill(3.0f);
	b.pending.fill(1.0f);
	b.reduceAccumulator.fill(1.0f);
	b.lastAddedTurn.fill(7);
	b.boredom.fill(0.2f);
	b.recent.at(2) = 0.3f;
	b.addedThisPeriod.at(2) = 0.4f;
	b.beliefInNeutralPlayer = 0.7f;
	tb::Init(town);
	for (size_t i = 0; i < tb::k_Players; ++i)
	{
		EXPECT_EQ(b.belief.at(i), i == k_Neutral ? 0.7f : 0.0f) << i;
		EXPECT_EQ(b.cap.at(i), 10.0f);
		EXPECT_EQ(b.pending.at(i), 0.0f);
		EXPECT_EQ(b.reduceAccumulator.at(i), 0.0f);
		EXPECT_EQ(b.lastAddedTurn.at(i), 0u);
	}
	for (const float boredom : b.boredom)
	{
		EXPECT_EQ(boredom, 1.0f);
	}
	for (size_t d = 0; d < tb::k_Desires; ++d)
	{
		EXPECT_EQ(b.desireThreshold.at(d), d == 14 ? 5.0f : 1.0f) << d;
	}
	EXPECT_EQ(b.recent.at(2), 0.3f);          // not reset
	EXPECT_EQ(b.addedThisPeriod.at(2), 0.4f); // not reset

	// the Town ctor: Init, then beliefInNeutralPlayer from info.dat and beliefScale = 1
	const auto made = MakeTown(1, PlayerNames::NEUTRAL);
	EXPECT_EQ(BeliefOf(made).belief.at(k_Neutral), 0.0f);
	EXPECT_EQ(BeliefOf(made).beliefInNeutralPlayer, 0.5f);
	EXPECT_EQ(BeliefOf(made).beliefScale, 1.0f);
	EXPECT_EQ(BeliefOf(made).desireThreshold.at(0), 1.0f);
	tb::Fold(made); // step 4 pins it
	EXPECT_EQ(BeliefOf(made).belief.at(k_Neutral), 0.5f);
}

// 3. The fold's pending -> belief (step 1)
TEST_F(TownBeliefTest, FoldPending)
{
	const auto town = MakeTown(1, PlayerNames::NEUTRAL); // neutral: no desire cost, the thresholds only reset
	auto& b = BeliefOf(town);
	b.desireThreshold.at(3) = 0.4f;
	b.pending.at(k_One) = 0.1f;
	tb::Fold(town);
	EXPECT_EQ(b.belief.at(k_One), 0.1f);
	EXPECT_EQ(b.addedThisPeriod.at(k_One), 0.1f);
	EXPECT_EQ(b.pending.at(k_One), 0.0f);
	EXPECT_EQ(b.desireThreshold.at(3), 1.0f);
	EXPECT_EQ(TownOf(town).owner, PlayerNames::NEUTRAL);
	EXPECT_EQ(tb::BeliefSpriteCount(), 0u); // no town centre: no sprite

	// the belief scale at 0: d == 0, pending kept, nothing else
	const auto other = MakeTown(2, PlayerNames::NEUTRAL);
	auto& o = BeliefOf(other);
	o.beliefScale = 0.0f;
	o.desireThreshold.at(3) = 0.4f;
	o.pending.at(k_One) = 0.1f;
	tb::Fold(other);
	EXPECT_EQ(o.pending.at(k_One), 0.1f);
	EXPECT_EQ(o.belief.at(k_One), 0.0f);
	EXPECT_EQ(o.addedThisPeriod.at(k_One), 0.0f);
	EXPECT_EQ(o.desireThreshold.at(3), 0.4f);

	// the cap applies to the folded value
	b.cap.at(k_One) = 0.15f;
	b.pending.at(k_One) = 0.1f;
	tb::Fold(town);
	EXPECT_EQ(b.belief.at(k_One), 0.15f);
	EXPECT_EQ(b.addedThisPeriod.at(k_One), 0.2f);
}

// 4. recent x 0.997 for the 8 slots every fold
TEST_F(TownBeliefTest, FoldRecentDecay)
{
	const auto town = MakeTown(1, PlayerNames::NEUTRAL);
	auto& b = BeliefOf(town);
	b.recent.fill(1.0f);
	tb::Fold(town);
	for (const float recent : b.recent)
	{
		EXPECT_EQ(recent, 0.997f);
	}
	tb::Fold(town);
	for (const float recent : b.recent)
	{
		EXPECT_EQ(recent, 0.997f * 0.997f);
	}
}

// 5. The boredom (step 2): stored only below 1
TEST_F(TownBeliefTest, FoldBoredom)
{
	const auto town = MakeTown(1, PlayerNames::NEUTRAL);
	auto& b = BeliefOf(town);
	b.boredom.at(1) = 1.0f - 0.0003f; // + 0.00056 >= 1: unchanged
	b.boredom.at(2) = 0.5f;
	b.boredom.at(3) = 0.5f; // 0.00028
	tb::Fold(town);
	EXPECT_EQ(b.boredom.at(1), 1.0f - 0.0003f);
	EXPECT_FLOAT_EQ(b.boredom.at(2), 0.50056f);
	EXPECT_FLOAT_EQ(b.boredom.at(3), 0.50028f);
	EXPECT_EQ(b.boredom.at(4), 1.0f); // from Init: 1 + 0.00056 >= 1

	// x the lost-town scale
	land_balance::SetLostTownScale(0.5f);
	b.boredom.at(2) = 0.5f;
	tb::Fold(town);
	EXPECT_FLOAT_EQ(b.boredom.at(2), 0.50028f);

	// AddToBoredomMultiplier: not below 0
	tb::AddToBoredomMultiplier(b, 2, -1.0f);
	EXPECT_EQ(b.boredom.at(2), 0.0f);
	EXPECT_EQ(tb::GetBoredomMultiplier(town, 2), 0.0f);
	EXPECT_EQ(tb::GetBoredomMultiplier(entt::null, 2), 1.0f);
}

// 6. The desires that cost the owner belief (step 3)
TEST_F(TownBeliefTest, FoldDesireCost)
{
	const auto town = MakeTown(1, PlayerNames::PLAYER_TWO);
	auto& t = TownOf(town);
	auto& b = t.belief;
	b.belief.at(k_Two) = 2.0f;
	t.desire.desire.at(0) = 1.5f; // GetDesire = the sum of its three parts = 1.5 > threshold 1
	b.desireThreshold.at(1) = 0.25f;
	tb::Fold(town);
	EXPECT_NEAR(b.belief.at(k_Two), 2.0f - 3e-5f, 1e-6f); // (1.5 - 1) x 6e-5
	EXPECT_EQ(b.desireThreshold.at(0), 1.0f - 2.1e-5f);
	EXPECT_EQ(b.desireThreshold.at(1), 0.25f); // at the minimum 0.25: no more decay
	EXPECT_EQ(b.desireThreshold.at(14), 5.0f - 2.1e-5f);
	EXPECT_EQ(b.reduceAccumulator.at(k_Two), 0.0f); // no town centre: not accumulated
	EXPECT_EQ(t.owner, PlayerNames::PLAYER_TWO);

	// the neutral owner: nothing
	const auto neutral = MakeTown(2, PlayerNames::NEUTRAL);
	auto& n = TownOf(neutral);
	n.belief.belief.at(k_Neutral) = 0.5f;
	n.desire.desire.at(0) = 1.5f;
	tb::Fold(neutral);
	EXPECT_EQ(n.belief.belief.at(k_Neutral), 0.5f);
	EXPECT_EQ(n.belief.desireThreshold.at(0), 1.0f);
}

// 7. The conversion (steps 5 and 6)
TEST_F(TownBeliefTest, FoldConversion)
{
	// a neutral town, 0.5 against player one's 0.5: the tie goes to the higher slot, the neutral 7
	const auto tie = MakeTown(1, PlayerNames::NEUTRAL);
	BeliefOf(tie).belief.at(k_One) = 0.5f;
	tb::Fold(tie);
	EXPECT_EQ(TownOf(tie).owner, PlayerNames::NEUTRAL);
	EXPECT_EQ(BeliefOf(tie).belief.at(k_One), 0.5f);
	EXPECT_TRUE(_help.empty());

	// 0.51: player one takes it with 1.5 x 0.51
	const auto taken = MakeTown(2, PlayerNames::NEUTRAL);
	BeliefOf(taken).belief.at(k_One) = 0.51f;
	tb::Fold(taken);
	EXPECT_EQ(TownOf(taken).owner, PlayerNames::PLAYER_ONE);
	EXPECT_FLOAT_EQ(BeliefOf(taken).belief.at(k_One), 0.765f);
	ASSERT_EQ(_help.size(), 1u);
	EXPECT_EQ(_help.at(0).first, tb::detail::HelpSprite::GeneralGood); // the new owner is the local player
	EXPECT_EQ(_help.at(0).second, taken);
	_help.clear();

	// a town of player one with 0.4 against the neutral 0.5: back to the neutral player; every town of player one
	// (this one included) x 0.9 x the lost-town scale
	const auto lost = MakeTown(3, PlayerNames::PLAYER_ONE);
	const auto kept = MakeTown(4, PlayerNames::PLAYER_ONE);
	BeliefOf(lost).belief.at(k_One) = 0.4f;
	BeliefOf(kept).belief.at(k_One) = 2.0f;
	tb::Fold(lost);
	EXPECT_EQ(TownOf(lost).owner, PlayerNames::NEUTRAL);
	EXPECT_FLOAT_EQ(BeliefOf(lost).belief.at(k_Neutral), 0.75f); // 1.5 x the pinned 0.5
	EXPECT_FLOAT_EQ(BeliefOf(lost).belief.at(k_One), 0.36f);
	EXPECT_FLOAT_EQ(BeliefOf(kept).belief.at(k_One), 1.8f);
	EXPECT_FLOAT_EQ(BeliefOf(taken).belief.at(k_One), 0.765f * 0.9f); // the town taken above is player one's too
	EXPECT_EQ(TownOf(kept).owner, PlayerNames::PLAYER_ONE);
	ASSERT_EQ(_help.size(), 1u);
	EXPECT_EQ(_help.at(0).first, tb::detail::HelpSprite::GeneralBad); // the old owner is the local player

	// with SET_LOST_TOWN_SCALE 0.5
	land_balance::SetLostTownScale(0.5f);
	const auto second = MakeTown(5, PlayerNames::PLAYER_ONE);
	BeliefOf(second).belief.at(k_One) = 0.1f;
	tb::Fold(second);
	EXPECT_EQ(TownOf(second).owner, PlayerNames::NEUTRAL);
	EXPECT_FLOAT_EQ(BeliefOf(kept).belief.at(k_One), 1.8f * 0.9f * 0.5f);
}

// 8. ProcessOncePerTurn
TEST_F(TownBeliefTest, ProcessOncePerTurn)
{
	const auto first = MakeTown(1, PlayerNames::PLAYER_TWO);
	const auto second = MakeTown(2, PlayerNames::PLAYER_TWO);
	BeliefOf(first).addedThisPeriod.at(k_One) = 0.002f;
	BeliefOf(second).addedThisPeriod.at(k_One) = 0.003f;
	BeliefOf(second).addedThisPeriod.at(k_Two) = 0.5f;

	game_clock::SetTurn(21); // not a multiple of 10: nothing
	tb::ProcessOncePerTurn();
	EXPECT_EQ(BeliefOf(first).addedThisPeriod.at(k_One), 0.002f);
	EXPECT_TRUE(_tips.empty());

	game_clock::SetTurn(30);
	tb::ProcessOncePerTurn();
	for (const auto town : {first, second})
	{
		for (const float added : BeliefOf(town).addedThisPeriod)
		{
			EXPECT_EQ(added, 0.0f);
		}
	}
	ASSERT_EQ(_tips.size(), 1u);
	EXPECT_EQ(_tips.at(0).first, 0xEE1u);
	EXPECT_EQ(_tips.at(0).second, (0.0f + 0.003f + 0.002f) * 1000.0f); // newest first, the local player's only
	_tips.clear();

	// not over 0.001: no tooltip
	BeliefOf(first).addedThisPeriod.at(k_One) = 0.0005f;
	game_clock::SetTurn(40);
	tb::ProcessOncePerTurn();
	EXPECT_TRUE(_tips.empty());
	EXPECT_EQ(BeliefOf(first).addedThisPeriod.at(k_One), 0.0f);
	EXPECT_TRUE(_help.empty()); // the local player has no town
}

// 8b. ProcessOncePerTurn: the believers gained float up, in yellow and four wide, from the land under the cursor
TEST_F(TownBeliefTest, BelieversGainedFloatUpFromTheLandUnderTheCursor)
{
	const auto town = MakeTown(1, PlayerNames::PLAYER_TWO);
	auto& hand = static_cast<test::creature_fakes::FakeHand&>(Locator::handSystem::emplace<test::creature_fakes::FakeHand>());
	hand.hoverLandPoint = glm::vec3(123.456f, 9.0f, 210.789f);
	Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2 xz) { return xz.x * 0.5f + 3.0f; });
	auto& registry = Locator::entitiesRegistry::value();
	const auto numbers = [&registry]() {
		std::vector<FloatingNumber> all;
		registry.Each<const FloatingNumber>(
		    [&all](entt::entity /*unused*/, const FloatingNumber& number) { all.push_back(number); });
		return all;
	};

	BeliefOf(town).addedThisPeriod.at(k_One) = 0.012f;
	game_clock::SetTurn(30);
	tb::ProcessOncePerTurn();
	auto made = numbers();
	ASSERT_EQ(made.size(), 1u);
	EXPECT_EQ(made.at(0).text, "  12");
	EXPECT_EQ(made.at(0).colour, 0xFFFFFF00u);
	EXPECT_EQ(made.at(0).life, 5.0f);
	// on the ground at the point as map coordinates hold it
	const float x = map_coords::ToMetres(map_coords::ToFixed(123.456f));
	const float z = map_coords::ToMetres(map_coords::ToFixed(210.789f));
	EXPECT_EQ(made.at(0).position, glm::vec3(x, x * 0.5f + 3.0f, z));

	// no land under the cursor: the map's corner, on the ground there
	hand.hoverLandPoint.reset();
	BeliefOf(town).addedThisPeriod.at(k_One) = 0.4f;
	game_clock::SetTurn(40);
	tb::ProcessOncePerTurn();
	made = numbers();
	ASSERT_EQ(made.size(), 2u);
	const auto corner = std::ranges::find(made, std::string(" 400"), &FloatingNumber::text);
	ASSERT_NE(corner, made.end());
	EXPECT_EQ(corner->position, glm::vec3(0.0f, 3.0f, 0.0f));

	// nothing gained over 0.001: no number
	BeliefOf(town).addedThisPeriod.at(k_One) = 0.0005f;
	game_clock::SetTurn(50);
	tb::ProcessOncePerTurn();
	EXPECT_EQ(numbers().size(), 2u);
}

// 9. CheckLosingBelief: the "losing belief" help sprite
TEST_F(TownBeliefTest, LosingBelief)
{
	const auto town = MakeTown(1, PlayerNames::PLAYER_ONE);
	const auto other = MakeTown(2, PlayerNames::PLAYER_ONE);
	for (const auto t : {town, other})
	{
		BeliefOf(t).belief.at(k_One) = 0.54f;
		BeliefOf(t).belief.at(k_Neutral) = 0.5f; // 0.54 - 0.05 < 0.5
	}
	tb::CheckLosingBelief(PlayerNames::PLAYER_ONE);
	ASSERT_EQ(_help.size(), 1u); // the first town found only
	EXPECT_EQ(_help.at(0).first, tb::detail::HelpSprite::LosingBelief);
	EXPECT_EQ(_help.at(0).second, town);
	_help.clear();

	// from ProcessOncePerTurn, on a turn % 10 == 0
	game_clock::SetTurn(10);
	tb::ProcessOncePerTurn();
	EXPECT_EQ(_help.size(), 1u);
	_help.clear();

	// on land 1 nothing
	influence::detail::MapGlobals().landNumber = 1;
	tb::CheckLosingBelief(PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(_help.empty());
	influence::detail::MapGlobals().landNumber = 0;

	// 0.56 - 0.05 > 0.5: nothing
	for (const auto t : {town, other})
	{
		BeliefOf(t).belief.at(k_One) = 0.56f;
	}
	tb::CheckLosingBelief(PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(_help.empty());
}

// 10. The land script: SET_TOWN_BELIEF overwrites (and sets beliefInNeutralPlayer for the neutral player), SET_TOWN_BELIEF_CAP
// caps the later sets, SET_TOWN_BALANCE_BELIEF_SCALE, SET_LOST_TOWN_SCALE
TEST_F(TownBeliefTest, Scripts)
{
	using lhscriptx::FeatureScriptCommands;
	const auto town = MakeTown(5, PlayerNames::NEUTRAL);
	FeatureScriptCommands::SetTownBelief(5, "PLAYER_ONE", 0.3f);
	FeatureScriptCommands::SetTownBelief(5, "PLAYER_ONE", 0.8f);
	EXPECT_EQ(BeliefOf(town).belief.at(k_One), 0.8f);
	FeatureScriptCommands::SetTownBelief(5, "NEUTRAL", 0.7f);
	EXPECT_EQ(BeliefOf(town).beliefInNeutralPlayer, 0.7f);
	EXPECT_EQ(BeliefOf(town).belief.at(k_Neutral), 0.7f);
	FeatureScriptCommands::SetTownBelief(5, "PLAYER_ONE", 20.0f);
	EXPECT_EQ(BeliefOf(town).belief.at(k_One), 10.0f); // the cap
	FeatureScriptCommands::SetTownBeliefCap(5, "PLAYER_TWO", 2.0f);
	FeatureScriptCommands::SetTownBelief(5, "PLAYER_TWO", 5.0f);
	EXPECT_EQ(BeliefOf(town).belief.at(k_Two), 2.0f);
	// the player name ignores case, an unknown name is the neutral player
	FeatureScriptCommands::SetTownBelief(5, "player_two", 1.5f);
	EXPECT_EQ(BeliefOf(town).belief.at(k_Two), 1.5f);
	FeatureScriptCommands::SetTownBelief(5, "NOBODY", 0.6f);
	EXPECT_EQ(BeliefOf(town).beliefInNeutralPlayer, 0.6f);
	EXPECT_EQ(BeliefOf(town).belief.at(k_Neutral), 0.6f);
	FeatureScriptCommands::SetTownBelief(99, "PLAYER_ONE", 1.0f); // no such town: nothing
	FeatureScriptCommands::SetTownBalanceBeliefScale(5, 0.5f);
	EXPECT_EQ(BeliefOf(town).beliefScale, 0.5f);
	FeatureScriptCommands::SetLostTownScale(0.25f);
	EXPECT_EQ(tb::LostTownScale(), 0.25f);
	land_balance::Reset();
	EXPECT_EQ(tb::LostTownScale(), 1.0f);
}

// The claimed-town multiplier is capped
TEST_F(TownBeliefTest, FoldConversionCapsTheClaimedBelief)
{
	const auto town = MakeTown(1, PlayerNames::NEUTRAL);
	BeliefOf(town).belief.at(k_Two) = 8.0f;
	tb::Fold(town);
	EXPECT_EQ(TownOf(town).owner, PlayerNames::PLAYER_TWO);
	EXPECT_FLOAT_EQ(BeliefOf(town).belief.at(k_Two), 10.0f); // 1.5 x 8 over the default cap
}

// SET_PLAYER_BELIEF: the belief in a player as a share of the belief in the town's owner
TEST_F(TownBeliefTest, ScriptSetsBeliefAsAShareOfBeliefInTheOwner)
{
	TownBelief belief {};
	tb::SetBelief(belief, PlayerNames::PLAYER_TWO, 2.0f);
	tb::SetBelief(belief, PlayerNames::PLAYER_ONE, 1.0f);
	tb::SetBeliefRelativeToOwner(belief, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, 0.25f);
	EXPECT_FLOAT_EQ(belief.belief.at(k_One), 0.5f);
	tb::SetBeliefRelativeToOwner(belief, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, 0.0f);
	EXPECT_FLOAT_EQ(belief.belief.at(k_One), 0.0f);
}

// OBJECT_RELATIVE_BELIEF: the take-town cheat halves the most believed in and gives the taker more
TEST_F(TownBeliefTest, TakingATownHalvesTheMostBelievedAndGivesTheTakerMore)
{
	constexpr std::array k_InGame {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO, PlayerNames::NEUTRAL};
	TownBelief belief {};
	tb::SetBelief(belief, PlayerNames::NEUTRAL, 0.5f);
	tb::SetBelief(belief, PlayerNames::PLAYER_TWO, 2.0f);
	tb::PlayerTakeTownCheat(belief, PlayerNames::PLAYER_ONE, 1.0f, k_InGame);
	EXPECT_FLOAT_EQ(belief.belief.at(k_Two), 1.0f);
	EXPECT_FLOAT_EQ(belief.belief.at(k_One), 3.0f);
	EXPECT_FLOAT_EQ(belief.belief.at(k_Neutral), 0.5f);
	// taken by the neutral player: nobody is believed in
	tb::PlayerTakeTownCheat(belief, PlayerNames::NEUTRAL, 1.0f, k_InGame);
	EXPECT_FLOAT_EQ(belief.belief.at(k_One), 0.0f);
	EXPECT_FLOAT_EQ(belief.belief.at(k_Two), 0.0f);
	EXPECT_FLOAT_EQ(belief.belief.at(k_Neutral), 0.0f);
}

// Only the local player winning a town from another hears the take-over
TEST(TownBeliefRules, OnlyTheLocalPlayerWinningATownHearsIt)
{
	EXPECT_TRUE(tb::HearsTakeOver(PlayerNames::NEUTRAL, PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(tb::HearsTakeOver(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(tb::HearsTakeOver(PlayerNames::NEUTRAL, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(tb::HearsTakeOver(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE));
}

// A town changing hands: the villagers' celebration of the new owner, the fireworks and the owner's symbol for a
// player, the influence drawn afresh; nothing for the same owner, no visuals for the neutral player
TEST_F(TownBeliefTest, TakeOverTownDoesTheRest)
{
	auto& particles = static_cast<RecordingParticles&>(Locator::particleSystem::emplace<RecordingParticles>());
	const auto town = MakeTown(1, PlayerNames::NEUTRAL);
	influence::detail::Globals().circlesDirty = false;
	tb::TakeOverTown(PlayerNames::PLAYER_ONE, town);
	EXPECT_EQ(TownOf(town).owner, PlayerNames::PLAYER_ONE);
	namespace reactions = ecs::effects::reactions;
	const auto celebration = reactions::GetReactionOfTypeInitiatedBy(town, Reaction::ReactToTownCelebration);
	ASSERT_NE(celebration, 0u); // no town centre: the town itself
	EXPECT_EQ(reactions::Find(celebration)->player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(particles.started, (std::vector {SpotVisualType::TownFireworks, SpotVisualType::PlayerIconFountain}));
	EXPECT_EQ(particles.players, std::vector<int> {0});
	EXPECT_TRUE(influence::detail::Globals().circlesDirty);

	// the same owner again: nothing
	particles.started.clear();
	influence::detail::Globals().circlesDirty = false;
	tb::TakeOverTown(PlayerNames::PLAYER_ONE, town);
	EXPECT_TRUE(particles.started.empty());
	EXPECT_FALSE(influence::detail::Globals().circlesDirty);

	// back to the neutral player: no visuals
	tb::TakeOverTown(PlayerNames::NEUTRAL, town);
	EXPECT_EQ(TownOf(town).owner, PlayerNames::NEUTRAL);
	EXPECT_TRUE(particles.started.empty());
	EXPECT_TRUE(influence::detail::Globals().circlesDirty);
}

// A town changing hands: the spell icons in its town centre's slots are the new owner's, and so are their seed
// graphics; empty slots are skipped
TEST_F(TownBeliefTest, TakeOverTownGivesTheCentreSpellIconsToTheNewOwner)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = MakeTown(1, PlayerNames::PLAYER_ONE);
	const auto centre = registry.Create();
	registry.Assign<Transform>(centre, glm::vec3(100.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<TownCentreIcons>(centre);
	std::array<entt::entity, 2> icons {entt::null, entt::null};
	std::array<entt::entity, 2> graphics {entt::null, entt::null};
	for (size_t i = 0; i < icons.size(); ++i)
	{
		graphics.at(i) = registry.Create();
		registry.Assign<SpellSeedGraphic>(graphics.at(i)).player = PlayerNames::PLAYER_ONE;
		icons.at(i) = registry.Create();
		auto& icon = registry.Assign<SpellIcon>(icons.at(i));
		icon.player = PlayerNames::PLAYER_ONE;
		icon.graphic = graphics.at(i);
	}
	// slots 0 and 3 hold an icon, the others are empty
	registry.Get<TownCentreIcons>(centre).icons.at(0) = icons.at(0);
	registry.Get<TownCentreIcons>(centre).icons.at(3) = icons.at(1);
	TownOf(town).centre = centre;

	tb::TakeOverTown(PlayerNames::PLAYER_TWO, town);
	for (size_t i = 0; i < icons.size(); ++i)
	{
		EXPECT_EQ(registry.Get<const SpellIcon>(icons.at(i)).player, PlayerNames::PLAYER_TWO);
		EXPECT_EQ(registry.Get<const SpellSeedGraphic>(graphics.at(i)).player, PlayerNames::PLAYER_TWO);
	}
	EXPECT_EQ(worship::town_centre::GraphicPlayers(centre),
	          (std::vector<PlayerNames> {PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_TWO}));

	// back to the neutral player: the icons follow
	tb::TakeOverTown(PlayerNames::NEUTRAL, town);
	EXPECT_EQ(registry.Get<const SpellIcon>(icons.at(1)).player, PlayerNames::NEUTRAL);
	EXPECT_EQ(worship::town_centre::GraphicPlayers(centre),
	          (std::vector<PlayerNames> {PlayerNames::NEUTRAL, PlayerNames::NEUTRAL}));
}

// A town centre's spell icon is found by its seed and shows the highest of the three power-up levels its town holds of
// it, none when it holds none; another seed's levels do not reach it
TEST_F(TownBeliefTest, ACentreSpellIconShowsTheHighestPowerUpLevelHeld)
{
	namespace centre_icons = worship::town_centre;
	auto& registry = Locator::entitiesRegistry::value();
	const auto centre = registry.Create();
	registry.Assign<TownCentreIcons>(centre);
	const auto graphic = registry.Create();
	registry.Assign<SpellSeedGraphic>(graphic);
	const auto icon = registry.Create();
	auto& spellIcon = registry.Assign<SpellIcon>(icon);
	spellIcon.infoIndex = 1;
	spellIcon.seedType = SpellSeedType::Fire;
	spellIcon.graphic = graphic;
	const entt::entity noTown = entt::null;
	registry.Assign<TownCentreSpellIcon>(icon, noTown, centre, static_cast<uint8_t>(2));
	registry.Get<TownCentreIcons>(centre).icons.at(2) = icon;

	EXPECT_TRUE(centre_icons::FindSpellIcon(centre, SpellSeedType::Fire) == icon);
	EXPECT_TRUE(centre_icons::FindSpellIcon(centre, SpellSeedType::Food) == entt::null);
	EXPECT_EQ(centre_icons::SpellCount(centre), 1);
	const auto shown = [&registry, graphic]() { return registry.Get<const SpellSeedGraphic>(graphic).powerUp; };
	EXPECT_EQ(shown(), -1);
	centre_icons::AddPowerUp(centre, SpellSeedType::Fire, 1);
	EXPECT_EQ(shown(), 1);
	centre_icons::AddPowerUp(centre, SpellSeedType::Fire, 0);
	EXPECT_EQ(shown(), 1);
	centre_icons::AddPowerUp(centre, SpellSeedType::Fire, 2);
	EXPECT_EQ(shown(), 2);
	centre_icons::AddPowerUp(centre, SpellSeedType::Food, 1);
	EXPECT_EQ(shown(), 2);
	centre_icons::ClearPowerUp(centre, SpellSeedType::Fire, 2);
	EXPECT_EQ(shown(), 1);
	centre_icons::ClearPowerUp(centre, SpellSeedType::Fire, 1);
	EXPECT_EQ(shown(), 0);
	centre_icons::ClearPowerUp(centre, SpellSeedType::Fire, 0);
	EXPECT_EQ(shown(), -1);
	EXPECT_EQ(registry.Get<const TownCentreSpellIcon>(icon).powerUps, (std::array<bool, 3> {false, false, false}));
}

// A town leaving a building site takes back its builders there, in list order: only those of the town that are
// available, the town asked first
TEST(TownTakeOverRules, OnlyTheTownsAvailableBuildersLeaveASite)
{
	const auto town = entt::entity {10};
	const auto other = entt::entity {11};
	const std::array builders {entt::entity {1}, entt::entity {2}, entt::entity {3}, entt::entity {4}};
	// 1 and 4 are the town's and available, 2 is the town's but dying, 3 is another town's
	const auto townOf = [&](entt::entity builder) { return builder == entt::entity {3} ? other : town; };
	std::vector<entt::entity> asked;
	const auto available = [&](entt::entity builder) {
		asked.push_back(builder);
		return builder != entt::entity {2};
	};
	std::vector<entt::entity> left;
	ecs::building_sites::ForEachBuilderOfTown(builders, town, townOf, available,
	                                          [&](entt::entity builder) { left.push_back(builder); });
	EXPECT_EQ(left, (std::vector {entt::entity {1}, entt::entity {4}}));
	// another town's builder is never asked whether it is available
	EXPECT_EQ(asked, (std::vector {entt::entity {1}, entt::entity {2}, entt::entity {4}}));
}

// A town changing hands leaves its old owner's temple heart: the heart's building site goes out of the town's list
// but is not deleted, and the town's other sites stay; a heart without a site, or another player's heart, does nothing
TEST_F(TownBeliefTest, TakeOverTownLeavesTheOldOwnersHeartSite)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = MakeTown(1, PlayerNames::PLAYER_ONE);
	// player one's temple, its heart being built, with a building site and no builders
	const auto heart = registry.Create();
	registry.Assign<Transform>(heart, glm::vec3(300.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Temple>(heart, PlayerNames::PLAYER_ONE);
	registry.Assign<CitadelWorship>(heart);
	registry.Assign<CitadelHeart>(heart).citadel = heart;
	const auto heartSite = registry.Create();
	registry.Assign<BuildingSite>(heartSite).root = heart;
	registry.Assign<CitadelPartBuild>(heart).buildingSite = heartSite;
	// another site of the town, on another building
	const auto house = registry.Create();
	const auto houseSite = registry.Create();
	registry.Assign<BuildingSite>(houseSite).root = house;
	TownOf(town).buildingSites = {heartSite, houseSite};

	tb::TakeOverTown(PlayerNames::PLAYER_TWO, town);
	EXPECT_EQ(TownOf(town).buildingSites, std::vector<entt::entity> {houseSite});
	EXPECT_TRUE(registry.Valid(heartSite));
	EXPECT_TRUE(registry.AllOf<BuildingSite>(heartSite));

	// the town is player two's now, and player one's heart is not its owner's: its site stays in the list
	TownOf(town).buildingSites = {heartSite, houseSite};
	tb::TakeOverTown(PlayerNames::NEUTRAL, town);
	EXPECT_EQ(TownOf(town).buildingSites, (std::vector<entt::entity> {heartSite, houseSite}));
}

// A town changing hands leaves its old site, which stays, and joins its new player's citadel: the citadel's site of the
// town's tribe takes it (docs/wiki/project/progress/worship/worship_sites.md, a town claimed or lost)
TEST_F(TownBeliefTest, TakeOverTownMovesTheTownFromItsSiteToItsNewOwnersSite)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = MakeTown(1, PlayerNames::PLAYER_ONE);
	// it has people, so it may have a site
	const auto person = registry.Create();
	registry.Assign<Villager>(person).town = town;
	registry.Assign<Transform>(person, glm::vec3(100.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto oldSite = registry.Create();
	registry.Assign<WorshipSite>(oldSite).towns = {town};
	registry.Get<TownMagic>(town).worshipSite = oldSite;
	// player two's citadel, with a site of the town's tribe
	const auto citadel = registry.Create();
	registry.Assign<Temple>(citadel, PlayerNames::PLAYER_TWO);
	registry.Assign<CitadelWorship>(citadel);
	registry.Assign<Transform>(citadel, glm::vec3(500.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto newSite = registry.Create();
	auto& celtic = registry.Assign<WorshipSite>(newSite);
	celtic.citadel = citadel;
	celtic.player = PlayerNames::PLAYER_TWO;
	celtic.tribe = Tribe::CELTIC;
	registry.Get<CitadelWorship>(citadel).sites.at(0) = newSite;

	tb::TakeOverTown(PlayerNames::PLAYER_TWO, town);
	EXPECT_TRUE(registry.Get<const WorshipSite>(oldSite).towns.empty());
	EXPECT_TRUE(registry.Valid(oldSite));
	EXPECT_EQ(registry.Get<const WorshipSite>(newSite).towns, std::vector<entt::entity> {town});
	EXPECT_EQ(registry.Get<const TownMagic>(town).worshipSite, newSite);
}
