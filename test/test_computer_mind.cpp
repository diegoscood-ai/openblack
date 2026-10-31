/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/ComputerHand.h"
#include "ECS/ComputerPlayerMind.h"
#include "ECS/ComputerPlayerMindTree.h"
#include "ECS/ComputerPlayerSeen.h"

using namespace openblack;
using namespace openblack::computer_player;

namespace
{

/// The names of the mind's nodes as the computer hands first listed them, each where the tree first meets it
constexpr std::array<std::string_view, 117> k_ListedNames {
    "Master Decision Tree",
    "PlayerInteract",
    "TalktoPlayer",
    "SendNiceTextToPlayer(Notdone)",
    "SendNastyTextToPlayer(Notdone)",
    "TauntPlayer(Notdone)",
    "DIRECTPickupAndDropObject",
    "ScarePlayer(Notdone)",
    "GiveToPlayer",
    "GiveCreatureThingToPlayer(Notdone)",
    "GiveToPlayer(Notdone)",
    "ThrowThingAtPlayer",
    "CreatureInteract",
    "Lookaftercreature",
    "GiveCreatureFood",
    "GetFoodFromField",
    "GetFoodFromFishFarm",
    "CastFoodSpellOn",
    "CastFoodForMyCreature",
    "MakeCreatureDrink",
    "RestoreCreatureHealth",
    "RestoreMyCreatureHealth",
    "MakeCreatureSleep",
    "SendCreatureHome",
    "MoveCreaturetoWarmerArea",
    "MoveCreaturetoCoollerArea",
    "TeachCreature",
    "TeachCreatureNormalAction",
    "TeachCreatureAggressiveMagic",
    "TeachCreatureCompassionateMagic",
    "TeachCreatureImpressiveMagic",
    "PlayWithCreature",
    "ThrowFoodatCreature",
    "SlapCreature",
    "StrokeCreature",
    "AttachLeashtoMovingObject",
    "FindFreeVillagerInArea",
    "CastSpellOnMyCreatureForALaugh",
    "ReactToCreatureEmergency",
    "ReacttoCreatureStarvingEmergency",
    "GetFoodFromOtherTownForObject",
    "ReacttoCreatureDamagedEmergency",
    "ReacttoCreatureIllEmergency",
    "TownInteract",
    "AttendTownDesires",
    "TownWithHighestFoodDesire",
    "CreateDiscipleToFulfillTownDesire",
    "GetCreaturetoHelp",
    "ShallCastFoodSpell",
    "GetFoodFromOtherTownForStoragePit",
    "TownWithHighestWoodDesire",
    "ShallCastWoodSpell",
    "GetWoodFromForest",
    "GetWoodFromOtherTown",
    "GiveBuildingSiteWood",
    "GiveBuildingSiteWorkers",
    "GetCreaturetoHelpBuild",
    "FindFreeVillagerBuilderInTown",
    "ExpandTown",
    "CombineScaffolds",
    "FindScaffoldToCombineWith",
    "FindPlaceToUseScaffoldForBuilding",
    "FindLocationToUseScaffold",
    "FillWorkshop",
    "CastWoodSpellOn",
    "AttendToWorshippersWorshippers",
    "GiveWorshippersFood",
    "CastFoodSpellOnWorshipSite",
    "GiveWorshippersRest",
    "CastHealSpellAtWorshipSite",
    "TakeVillagerFromWorshipSiteToTown",
    "WorshipSiteThatNeedsVillagers",
    "RaiseTotemInTown",
    "TakeVillagerFromTownToWorshipSite",
    "ChargeUpSpell",
    "IncreaseTownPopulation",
    "MakeDiscipleBreader",
    "DefeatPlayer",
    "ExpandInfluence",
    "TakeOverTownByImpressing",
    "UseCreatureToImpressTown",
    "ThrowThingsToImpress",
    "CastSpellToImpress",
    "TakeOverTownByFulfillingFoodDesire",
    "TakeOverTown",
    "CastSpellToFulfilTownNeeds",
    "DestroyTown",
    "DestroytownWithSpell",
    "DestroytownWithCreature",
    "DestroytownWithObject",
    "PlayerWithMoreSpells",
    "PlayerWithBetterCreature",
    "AttackCreature",
    "AttackCreatureWithSpells",
    "AttackCreatureWithCreature",
    "ThrowrockatCreature",
    "ReactToPlayer",
    "ReactToAggressiveCreature",
    "ReactToPlayerCastingSpells",
    "UseCreatureAsDiverseionAgainstPlayer",
    "ReactToPlayerThrowingStuff",
    "CastPhysicalShieldToProtect",
    "DefendTown",
    "DefendTownWithCreature",
    "DefendtownWithSpells",
    "PlayerWithMoreWorshippers",
    "CheckTownForDamage",
    "FindBurningAbode",
    "CastWaterSpell",
    "FindDamagedAbode",
    "CheckTownsForBeliefAttack",
    "DefendTownAgainstBeliefAttack",
    "CastSpellToDefendAgainstBelief",
    "CheckTownForProtection",
    "FindUnprotectedAbode",
    "CastShieldToProtect",
    "WorldInteract",
};

/// The place of every node whose row has the name, in the order of the tree
std::vector<uint16_t> NodesNamed(std::string_view rowName)
{
	std::vector<uint16_t> found;
	for (uint16_t node = 0; node < k_MindNodeCount; ++node)
	{
		if (RowOf(node).name == rowName)
		{
			found.push_back(node);
		}
	}
	return found;
}

} // namespace

TEST(ComputerMindTree, HasTheRowsAndNodesOfTheGame)
{
	EXPECT_EQ(k_MindRows.size(), 126u);
	EXPECT_EQ(MindNodes().size(), 198u);
	std::set<ScoreKind> scores;
	std::set<ActionKind> actions;
	size_t scored = 0;
	size_t acting = 0;
	for (const auto& row : k_MindRows)
	{
		if (row.score != ScoreKind::None)
		{
			++scored;
			scores.insert(row.score);
		}
		if (row.action != ActionKind::None)
		{
			++acting;
			actions.insert(row.action);
		}
	}
	EXPECT_EQ(scored, 102u);
	EXPECT_EQ(scores.size(), 86u);
	EXPECT_EQ(acting, 67u);
	EXPECT_EQ(actions.size(), 58u);
}

TEST(ComputerMindTree, TheRootHasTheSixAimsWithTheirPeriods)
{
	EXPECT_EQ(RowOf(k_RootNode).name, "Master Decision Tree");
	EXPECT_FALSE(MindNodes().at(k_RootNode).parent.has_value());
	const auto aims = ChildrenOf(k_RootNode);
	ASSERT_EQ(aims.size(), k_TopNodeCount);
	constexpr std::array<std::string_view, k_TopNodeCount> k_Names {"PlayerInteract: 13 ", "CreatureInteract: 14 ",
	                                                                "TownInteract: 15 ",   "DefeatPlayer: 16 ",
	                                                                "ReactToPlayer: 17 ",  "WorldInteract: 18 "};
	constexpr std::array<uint32_t, k_TopNodeCount> k_Periods {5000, 100, 300, 200, 50, 5000};
	constexpr std::array<uint16_t, k_TopNodeCount> k_Places {1, 12, 53, 105, 171, 197};
	for (size_t index = 0; index < k_TopNodeCount; ++index)
	{
		EXPECT_EQ(aims[index], k_Places.at(index));
		EXPECT_EQ(RowOf(aims[index]).name, k_Names.at(index));
		EXPECT_EQ(RowOf(aims[index]).period, k_Periods.at(index));
		EXPECT_EQ(MindNodes().at(aims[index]).parent, std::optional<uint16_t>(k_RootNode));
	}
	// the last aim has nothing under it
	EXPECT_TRUE(ChildrenOf(197).empty());
}

TEST(ComputerMindTree, EachNodeComesBeforeItsChildrenInTheirOrder)
{
	for (uint16_t node = 0; node < k_MindNodeCount; ++node)
	{
		const auto& row = RowOf(node);
		const auto children = ChildrenOf(node);
		ASSERT_EQ(children.size(), row.childCount);
		for (size_t index = 0; index < children.size(); ++index)
		{
			EXPECT_GT(children[index], node);
			EXPECT_EQ(MindNodes().at(children[index]).row, row.children.at(index));
			EXPECT_EQ(MindNodes().at(children[index]).parent, std::optional<uint16_t>(node));
			if (index > 0)
			{
				EXPECT_GT(children[index], children[index - 1]);
			}
		}
	}
}

TEST(ComputerMindTree, ARowNamedTwiceUnderOneParentMakesTwoNodes)
{
	// the town's desires list the building site's wood twice: the second is weighed again, as a node of its own
	const auto desires = NodesNamed("AttendTownDesires: 32 ");
	ASSERT_EQ(desires.size(), 1u);
	const auto children = ChildrenOf(desires.front());
	ASSERT_EQ(children.size(), 5u);
	EXPECT_EQ(RowOf(children[2]).name, "GiveBuildingSiteWood: 86 ");
	EXPECT_EQ(RowOf(children[4]).name, "GiveBuildingSiteWood: 86 ");
	EXPECT_NE(children[2], children[4]);
}

TEST(ComputerMindTree, TheNamesTheHandsListedComeFromTheTree)
{
	const auto names = MindNodeNames();
	ASSERT_EQ(names.size(), k_ListedNames.size());
	for (size_t index = 0; index < names.size(); ++index)
	{
		EXPECT_EQ(names[index], k_ListedNames.at(index)) << index;
	}
	// so a script's name finds the same actions as before
	EXPECT_EQ(computer_hand::FindMindAction("FindPlaceToUseScaffoldForBuilding"),
	          computer_hand::MindAction::FindPlaceToUseScaffoldForBuilding);
	EXPECT_EQ(computer_hand::FindMindAction("ShallCastWoodSpell"), computer_hand::MindAction::NotPorted);
	EXPECT_EQ(computer_hand::FindMindAction("NoSuchAction"), std::nullopt);
}

TEST(ComputerMindTree, ANameFindsEveryNodeWhoseNameItBeginsWithInTheOrderOfTheTree)
{
	// the name up to the last ':' must begin the name asked for: the same row met under three parents is three nodes
	EXPECT_EQ(FindNodes("TakeOverTown"), (std::vector<uint16_t> {122, 133, 162}));
	EXPECT_EQ(FindNodes("TakeOverTownByImpressing"), (std::vector<uint16_t> {107, 122, 133, 162}));
	// a need of the attack on a player is met before the aim of the same name
	EXPECT_EQ(FindNodes("ReactToPlayer"), (std::vector<uint16_t> {148, 171, 172}));
	EXPECT_EQ(RowOf(148).name, "ReactToPlayer: 42 ");
	EXPECT_EQ(RowOf(171).name, "ReactToPlayer: 17 ");
	EXPECT_EQ(FindNodes("ExpandInfluence"), (std::vector<uint16_t> {106}));
	EXPECT_EQ(FindNodes("ReactToAggressiveCreature"), (std::vector<uint16_t> {149, 173}));
	EXPECT_EQ(FindNodes("ShallCastWoodSpell"), (std::vector<uint16_t> {65, 69, 77}));
	EXPECT_EQ(RowOf(65).name, "ShallCastWoodSpell: 132 ");
	EXPECT_EQ(FindNodes("Master Decision Tree"), (std::vector<uint16_t> {0}));
	EXPECT_TRUE(FindNodes("TakeOver").empty());
	EXPECT_TRUE(FindNodes("").empty());
	EXPECT_EQ(FindNode("ShallCastWoodSpell"), std::optional<uint16_t>(65));
	EXPECT_EQ(FindNode("NoSuchNode"), std::nullopt);
}

TEST(ComputerMindTree, ANewMindTakesEachRowsWeightAndHoldsNothingBack)
{
	const auto mind = MakeMind();
	ASSERT_EQ(mind.nodes.size(), k_MindNodeCount);
	for (uint16_t node = 0; node < k_MindNodeCount; ++node)
	{
		EXPECT_EQ(mind.nodes.at(node).personality, RowOf(node).personality);
		EXPECT_EQ(mind.nodes.at(node).suppression, 1.0f);
		EXPECT_EQ(mind.nodes.at(node).score, 0.0f);
		EXPECT_FALSE(mind.nodes.at(node).chosen.has_value());
	}
	EXPECT_EQ(mind.nodes.at(106).personality, 0.75f);
	EXPECT_EQ(mind.nodes.at(6).personality, 0.0f);
	EXPECT_EQ(mind.planScore, 0.0f);
	EXPECT_EQ(mind.emptyWeighingTurn, 0u);
	EXPECT_TRUE(mind.restAfterSteps);
	for (const auto attitude : mind.attitude)
	{
		EXPECT_EQ(attitude, 1.0f);
	}
}

TEST(ComputerMindTree, TheRowsHoldBackAfterSuccessAsTheGameDoes)
{
	// every row comes back to 1 after its action was done, but three
	EXPECT_EQ(RowOf(6).suppressionAfterSuccess, 0.0f);
	EXPECT_EQ(RowOf(FindNodes("FindFreeVillagerInArea").front()).suppressionAfterSuccess, 0.5f);
	// the game's lookup matches by prefix, so its first node is another row's; take the row named exactly
	const auto spells = FindNodes("AttackCreatureWithSpells");
	const auto own =
	    std::ranges::find_if(spells, [](uint16_t node) { return NamePrefix(RowOf(node).name) == "AttackCreatureWithSpells"; });
	ASSERT_NE(own, spells.end());
	EXPECT_EQ(RowOf(*own).suppressionAfterSuccess, 0.9f);
	size_t others = 0;
	for (const auto& row : k_MindRows)
	{
		others += row.suppressionAfterSuccess == 1.0f ? 1 : 0;
	}
	EXPECT_EQ(others, 123u);
}

namespace
{

using openblack::ecs::components::ComputerMind;
using openblack::ecs::components::ComputerMindNode;

/// The float of the bits the original gave
float Bits(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

uint32_t BitsOf(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// A made-up tree as the oracle ran the original's weighing on: a root without a way of scoring over A, B and C, which
/// score; B may have two children D and E of its own
struct SmallTree
{
	static constexpr uint16_t k_Root = 0;
	static constexpr uint16_t k_A = 1;
	static constexpr uint16_t k_B = 2;
	static constexpr uint16_t k_C = 3;
	static constexpr uint16_t k_D = 4;
	static constexpr uint16_t k_E = 5;

	explicit SmallTree(bool grandchildren)
	{
		rows.at(0) = MindRow {"Root", ScoreKind::None, ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 0, {}};
		rows.at(1) = MindRow {"Leaf", ScoreKind::TalktoPlayer, ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 0, {}};
		nodes.at(k_Root) = MindNode {.row = 0, .parent = std::nullopt, .childCount = 3, .children = {k_A, k_B, k_C}};
		for (const auto child : {k_A, k_B, k_C})
		{
			nodes.at(child) = MindNode {.row = 1, .parent = k_Root, .childCount = 0, .children = {}};
		}
		for (const auto child : {k_D, k_E})
		{
			nodes.at(child) = MindNode {.row = 1, .parent = k_B, .childCount = 0, .children = {}};
		}
		if (grandchildren)
		{
			nodes.at(k_B).childCount = 2;
			nodes.at(k_B).children = {k_D, k_E};
		}
		mind.nodes.resize(nodes.size());
		constexpr std::array<float, 6> k_Personalities {0.25f, 0.3f, 0.6f, 0.9f, 0.5f, 0.8f};
		for (size_t index = 0; index < nodes.size(); ++index)
		{
			mind.nodes.at(index).personality = k_Personalities.at(index);
		}
	}

	[[nodiscard]] MindTree Tree() const { return MindTree {nodes, rows}; }

	std::array<MindRow, 2> rows {};
	std::array<MindNode, 6> nodes {};
	ComputerMind mind;
};

/// Scores and actions the test lines up, and a record of what the mind asked for
class FakeMindWorld final: public MindWorld
{
public:
	float Score(ScoreKind /*kind*/, ComputerMind& mind, uint16_t node) override
	{
		scored.push_back(node);
		if (const auto found = things.find(node); found != things.end())
		{
			mind.nodes.at(node).thing = found->second;
		}
		const auto found = scores.find(node);
		return found != scores.end() ? found->second : everyScore;
	}
	[[nodiscard]] bool HasScore(ScoreKind kind) const override { return !missingScores.contains(kind) && !noScores; }
	[[nodiscard]] bool HasAction(ActionKind kind) const override { return !missing.contains(kind); }
	void GiveUpSteps() override { calls.push_back("give up"); }
	bool RunAction(ActionKind /*kind*/, ComputerMind& mind, uint16_t node) override
	{
		calls.push_back("run");
		ran.push_back(node);
		seenThings.push_back(mind.nodes.at(node).thing);
		return done;
	}
	void TimeSteps() override { calls.push_back("time"); }
	void ActionMissing(ActionKind kind, uint16_t node) override { missingRun.emplace_back(kind, node); }

	float everyScore {0.0f};
	std::map<uint16_t, float> scores;
	std::map<uint16_t, entt::entity> things;
	std::set<ActionKind> missing;
	std::set<ScoreKind> missingScores;
	bool noScores {false};
	bool done {true};
	std::vector<uint16_t> scored;
	std::vector<uint16_t> ran;
	std::vector<entt::entity> seenThings;
	std::vector<std::string> calls;
	std::vector<std::pair<ActionKind, uint16_t>> missingRun;
};

/// A hand whose turn the test lines up, writing what the mind's turn asked of it into the world's calls
class FakeTurnHand final: public TurnHand
{
public:
	explicit FakeTurnHand(FakeMindWorld& world)
	    : _world(world)
	{
	}
	[[nodiscard]] bool Busy() const override { return busy || held; }
	[[nodiscard]] bool HeldByScript() const override { return held; }
	void Step() override { _world.calls.push_back("step"); }
	bool ForceNext() override
	{
		_world.calls.push_back("force");
		return forced;
	}

	bool busy {false};
	bool held {false};
	bool forced {false};

private:
	FakeMindWorld& _world;
};

constexpr uint32_t k_Turn = 1234;

} // namespace

TEST(ComputerMindWeighing, EveryChildScoringAHalfGivesTheOriginalsNumbers)
{
	// the oracle: every score 0.5, the root 0x3DE66666 and its choice C
	SmallTree small(false);
	FakeMindWorld world;
	world.everyScore = 0.5f;
	const auto tree = small.Tree();
	EXPECT_EQ(BitsOf(WeighAll(tree, small.mind, k_Turn, world)), 0x3DE66666u);
	const auto& nodes = small.mind.nodes;
	EXPECT_EQ(BitsOf(nodes.at(SmallTree::k_Root).score), 0x3DE66666u);
	EXPECT_EQ(nodes.at(SmallTree::k_Root).chosen, std::optional<uint16_t>(SmallTree::k_C));
	EXPECT_EQ(BitsOf(nodes.at(SmallTree::k_A).score), 0x3E19999Au);
	EXPECT_EQ(BitsOf(nodes.at(SmallTree::k_B).score), 0x3E99999Au);
	EXPECT_EQ(BitsOf(nodes.at(SmallTree::k_C).score), 0x3EE66666u);
	for (const auto node : {SmallTree::k_A, SmallTree::k_B, SmallTree::k_C})
	{
		EXPECT_EQ(nodes.at(node).chosen, std::optional<uint16_t>(node));
		EXPECT_EQ(nodes.at(node).lastWeighedTurn, k_Turn);
	}
	EXPECT_EQ(nodes.at(SmallTree::k_Root).lastWeighedTurn, k_Turn);
	EXPECT_EQ(world.scored, (std::vector<uint16_t> {SmallTree::k_A, SmallTree::k_B, SmallTree::k_C}));
}

TEST(ComputerMindWeighing, OnceTheBestIsAboveTheParentsWeightNoLaterChildIsWeighed)
{
	// the oracle: every score 5.0, A gives 0.375 > the root's 0.25, so B and C are never weighed
	SmallTree small(false);
	FakeMindWorld world;
	world.everyScore = 5.0f;
	EXPECT_EQ(WeighAll(small.Tree(), small.mind, k_Turn, world), 0.375f);
	const auto& nodes = small.mind.nodes;
	EXPECT_EQ(nodes.at(SmallTree::k_Root).chosen, std::optional<uint16_t>(SmallTree::k_A));
	EXPECT_EQ(nodes.at(SmallTree::k_A).score, 1.5f);
	for (const auto node : {SmallTree::k_B, SmallTree::k_C})
	{
		EXPECT_EQ(nodes.at(node).lastWeighedTurn, 0u);
		EXPECT_EQ(nodes.at(node).score, 0.0f);
		EXPECT_FALSE(nodes.at(node).chosen.has_value());
	}
	EXPECT_EQ(world.scored, (std::vector<uint16_t> {SmallTree::k_A}));
}

TEST(ComputerMindWeighing, ChildrenScoringNothingLeaveTheRootWithNoChoice)
{
	SmallTree small(false);
	FakeMindWorld world;
	EXPECT_EQ(WeighAll(small.Tree(), small.mind, k_Turn, world), 0.0f);
	const auto& nodes = small.mind.nodes;
	EXPECT_FALSE(nodes.at(SmallTree::k_Root).chosen.has_value());
	// each leaf still chose itself
	for (const auto node : {SmallTree::k_A, SmallTree::k_B, SmallTree::k_C})
	{
		EXPECT_EQ(nodes.at(node).chosen, std::optional<uint16_t>(node));
	}
	EXPECT_EQ(world.scored.size(), 3u);
}

TEST(ComputerMindWeighing, ANodeThatScoresIsWorthItsScoreTimesItsBestChild)
{
	// the oracle: B with children D and E, scores 0.5: B 0x3D9374BD (0.3 x 0.24) and its choice E
	SmallTree small(true);
	FakeMindWorld world;
	world.everyScore = 0.5f;
	EXPECT_EQ(BitsOf(WeighAll(small.Tree(), small.mind, k_Turn, world)), 0x3DE66666u);
	const auto& nodes = small.mind.nodes;
	EXPECT_EQ(nodes.at(SmallTree::k_Root).chosen, std::optional<uint16_t>(SmallTree::k_C));
	EXPECT_EQ(BitsOf(nodes.at(SmallTree::k_B).score), 0x3D9374BDu);
	EXPECT_EQ(nodes.at(SmallTree::k_B).chosen, std::optional<uint16_t>(SmallTree::k_E));
	EXPECT_EQ(BitsOf(nodes.at(SmallTree::k_D).score), 0x3E800000u);
	EXPECT_EQ(BitsOf(nodes.at(SmallTree::k_E).score), 0x3ECCCCCDu);
	EXPECT_EQ(world.scored,
	          (std::vector<uint16_t> {SmallTree::k_A, SmallTree::k_B, SmallTree::k_D, SmallTree::k_E, SmallTree::k_C}));
}

TEST(ComputerMindWeighing, ANodeScoringNothingDoesNotWeighItsChildren)
{
	SmallTree small(true);
	FakeMindWorld world;
	world.everyScore = 0.5f;
	world.scores[SmallTree::k_B] = 0.0f;
	WeighAll(small.Tree(), small.mind, k_Turn, world);
	EXPECT_EQ(small.mind.nodes.at(SmallTree::k_B).chosen, std::optional<uint16_t>(SmallTree::k_B));
	EXPECT_EQ(small.mind.nodes.at(SmallTree::k_D).lastWeighedTurn, 0u);
	EXPECT_EQ(world.scored, (std::vector<uint16_t> {SmallTree::k_A, SmallTree::k_B, SmallTree::k_C}));
}

TEST(ComputerMindWeighing, OfTwoEqualChildrenTheFirstStays)
{
	SmallTree small(false);
	small.mind.nodes.at(SmallTree::k_A).personality = 0.6f;
	small.mind.nodes.at(SmallTree::k_C).personality = 0.6f;
	FakeMindWorld world;
	world.everyScore = 0.5f;
	WeighAll(small.Tree(), small.mind, k_Turn, world);
	EXPECT_EQ(small.mind.nodes.at(SmallTree::k_Root).chosen, std::optional<uint16_t>(SmallTree::k_A));
}

TEST(ComputerMindWeighing, ANodeWithoutAScoreActsOnTheThingNearestAboveIt)
{
	// B scores and chooses a town; D, which has no way of scoring, acts on B's town
	SmallTree small(true);
	small.nodes.at(SmallTree::k_D).row = 0;
	FakeMindWorld world;
	world.everyScore = 0.5f;
	const auto town = static_cast<entt::entity>(7);
	world.things[SmallTree::k_B] = town;
	WeighAll(small.Tree(), small.mind, k_Turn, world);
	EXPECT_EQ(small.mind.nodes.at(SmallTree::k_D).thing, town);
	// the root has none above it
	EXPECT_EQ(small.mind.nodes.at(SmallTree::k_Root).thing, entt::entity {entt::null});
}

TEST(ComputerMindWeighing, AHeldBackNodeComesBackBeforeItIsWeighed)
{
	// the oracle: 0.01 weighed last at turn 1000 and now 1234 gives 0x3C4A2E2B; 0.5 after 1000 turns is back to 1
	ComputerMindNode node {.suppression = 0.01f, .lastWeighedTurn = 1000};
	Recover(node, 1.001f, k_Turn);
	EXPECT_EQ(BitsOf(node.suppression), 0x3C4A2E2Bu);
	ComputerMindNode later {.suppression = 0.5f, .lastWeighedTurn = 234};
	Recover(later, 1.001f, k_Turn);
	EXPECT_EQ(later.suppression, 1.0f);
	// a rate of 1 never brings it back
	ComputerMindNode never {.suppression = 0.25f, .lastWeighedTurn = 0};
	Recover(never, 1.0f, k_Turn);
	EXPECT_EQ(never.suppression, 0.25f);
	EXPECT_EQ(Bits(0x3C4A2E2Bu), node.suppression);
}

TEST(ComputerMindWeighing, AnAimIsDueOnTheMultiplesOfItsPeriod)
{
	EXPECT_TRUE(IsDue(0, 17));
	EXPECT_TRUE(IsDue(50, 0));
	EXPECT_TRUE(IsDue(50, 150));
	EXPECT_FALSE(IsDue(50, 151));
	EXPECT_TRUE(IsDue(5000, 10000));
}

TEST(ComputerMindWeighing, WhileTheHandIsBusyOnlyTheDueAimsAreWeighed)
{
	SmallTree small(false);
	small.rows.at(1).period = 100;
	FakeMindWorld world;
	world.everyScore = 0.5f;
	EXPECT_EQ(WeighDueAims(small.Tree(), small.mind, 150, world), 0.0f);
	EXPECT_TRUE(world.scored.empty());
	EXPECT_FALSE(small.mind.nodes.at(SmallTree::k_Root).chosen.has_value());
	// due: each aim is weighed on its own, not times the root's weight, and the best gives the root its choice
	const float best = WeighDueAims(small.Tree(), small.mind, 200, world);
	EXPECT_EQ(best, small.mind.nodes.at(SmallTree::k_C).score);
	EXPECT_EQ(small.mind.nodes.at(SmallTree::k_Root).score, best);
	EXPECT_EQ(small.mind.nodes.at(SmallTree::k_Root).chosen, std::optional<uint16_t>(SmallTree::k_C));
	EXPECT_EQ(world.scored.size(), 3u);
}

TEST(ComputerMindRunning, AChosenNodeWithoutAnActionRunsItsParents)
{
	SmallTree small(true);
	small.rows.at(0).action = ActionKind::GiveCreatureFood;
	small.rows.at(0).suppressionAfterSuccess = 0.5f;
	// B runs as the root's row, with its action; E has none
	small.nodes.at(SmallTree::k_B).row = 0;
	small.mind.nodes.at(SmallTree::k_Root).chosen = SmallTree::k_E;
	FakeMindWorld world;
	EXPECT_TRUE(RunPlan(small.Tree(), small.mind, k_Turn, world));
	EXPECT_EQ(world.ran, (std::vector<uint16_t> {SmallTree::k_B}));
	EXPECT_EQ(world.calls, (std::vector<std::string> {"give up", "run", "time"}));
	EXPECT_EQ(small.mind.nodes.at(SmallTree::k_B).suppression, 0.5f);
	EXPECT_EQ(small.mind.nodes.at(SmallTree::k_E).lastRunTurn, k_Turn);
	EXPECT_EQ(small.mind.nodes.at(SmallTree::k_B).lastRunTurn, k_Turn);
	EXPECT_EQ(small.mind.nodes.at(SmallTree::k_Root).lastRunTurn, 0u);
	EXPECT_EQ(small.mind.planScore, small.mind.nodes.at(SmallTree::k_Root).score);
}

TEST(ComputerMindRunning, AFailedActionHoldsTheNodeBackToAHundredth)
{
	SmallTree small(false);
	small.rows.at(1).action = ActionKind::GiveCreatureFood;
	small.mind.planScore = 0.75f;
	small.mind.nodes.at(SmallTree::k_Root).chosen = SmallTree::k_A;
	FakeMindWorld world;
	world.done = false;
	EXPECT_FALSE(RunPlan(small.Tree(), small.mind, k_Turn, world));
	EXPECT_EQ(BitsOf(small.mind.nodes.at(SmallTree::k_A).suppression), 0x3C23D70Au);
	EXPECT_EQ(world.calls, (std::vector<std::string> {"give up", "run"}));
	// the plan under way stays what it was
	EXPECT_EQ(small.mind.planScore, 0.75f);
}

TEST(ComputerMindRunning, AnActionOpenblackLacksIsNotRunAndChangesNothing)
{
	SmallTree small(false);
	small.rows.at(1).action = ActionKind::GiveCreatureFood;
	small.mind.nodes.at(SmallTree::k_A).suppression = 0.7f;
	small.mind.nodes.at(SmallTree::k_Root).chosen = SmallTree::k_A;
	FakeMindWorld world;
	world.missing.insert(ActionKind::GiveCreatureFood);
	EXPECT_FALSE(RunPlan(small.Tree(), small.mind, k_Turn, world));
	EXPECT_TRUE(world.calls.empty());
	EXPECT_EQ(small.mind.nodes.at(SmallTree::k_A).suppression, 0.7f);
	// the world hears of it once, the node with it
	EXPECT_EQ(world.missingRun,
	          (std::vector<std::pair<ActionKind, uint16_t>> {{ActionKind::GiveCreatureFood, SmallTree::k_A}}));
	// nothing chosen: nothing runs
	small.mind.nodes.at(SmallTree::k_Root).chosen.reset();
	EXPECT_FALSE(RunChosen(small.Tree(), small.mind, SmallTree::k_Root, k_Turn, world));
}

TEST(ComputerMindTurns, ANewBestRunsOnlyWhenWorthMoreThanTwiceThePlan)
{
	ComputerMind mind;
	mind.planScore = 0.2f;
	EXPECT_FALSE(OutbidsPlan(mind, 0.4f));
	EXPECT_TRUE(OutbidsPlan(mind, 0.41f));
	mind.planScore = 0.0f;
	EXPECT_TRUE(OutbidsPlan(mind, 0.01f));
	EXPECT_FALSE(OutbidsPlan(mind, 0.0f));
}

TEST(ComputerMindTurns, AFreeHandRestsAfterStepsAndAfterAnEmptyWeighing)
{
	ComputerMind mind;
	// a new mind rests its first free turn
	EXPECT_EQ(NextFreeTurn(mind, 10), FreeTurn::Rest);
	EXPECT_EQ(NextFreeTurn(mind, 11), FreeTurn::Weigh);
	mind.emptyWeighingTurn = 100;
	EXPECT_EQ(NextFreeTurn(mind, 129), FreeTurn::Rest);
	EXPECT_EQ(NextFreeTurn(mind, 130), FreeTurn::Weigh);
	// the 30 turns end the rest after steps as well
	mind.restAfterSteps = true;
	EXPECT_EQ(NextFreeTurn(mind, 110), FreeTurn::Rest);
	EXPECT_FALSE(mind.restAfterSteps);
}

TEST(ComputerMindTurns, AWeighingThatFindsNothingKeepsItsTurn)
{
	SmallTree small(false);
	small.rows.at(1).action = ActionKind::GiveCreatureFood;
	FakeMindWorld world;
	EXPECT_FALSE(WeighAndRun(small.Tree(), small.mind, k_Turn, world));
	EXPECT_EQ(small.mind.emptyWeighingTurn, k_Turn);
	world.everyScore = 0.5f;
	EXPECT_TRUE(WeighAndRun(small.Tree(), small.mind, k_Turn + 1, world));
	EXPECT_EQ(small.mind.emptyWeighingTurn, 0u);
	EXPECT_EQ(world.ran, (std::vector<uint16_t> {SmallTree::k_C}));
	EXPECT_EQ(BitsOf(small.mind.planScore), 0x3DE66666u);
}

TEST(ComputerMindTurns, ABusyHandHeldByAScriptOnlyRestsTheNextFreeTurn)
{
	SmallTree small(false);
	FakeMindWorld world;
	world.everyScore = 0.5f;
	small.mind.restAfterSteps = false;
	BusyTurn(small.Tree(), small.mind, true, 0, world);
	EXPECT_TRUE(small.mind.restAfterSteps);
	EXPECT_TRUE(world.scored.empty());
	small.rows.at(1).action = ActionKind::GiveCreatureFood;
	BusyTurn(small.Tree(), small.mind, false, 0, world);
	EXPECT_EQ(world.ran, (std::vector<uint16_t> {SmallTree::k_C}));
}

// A mind's turn in the original's order: a busy hand goes through its steps and then weighs its due aims; a free hand
// forces the next queued action first, and only when that gave it nothing rests or weighs the whole tree in the same
// turn (docs/wiki/hand/computer-minds.md#a-turn-of-the-mind)
TEST(ComputerMindTurns, ATurnStepsTheBusyHandOrForcesTheQueueBeforeTheWeighing)
{
	SmallTree small(false);
	small.rows.at(1).action = ActionKind::GiveCreatureFood;
	FakeMindWorld world;
	world.everyScore = 0.5f;
	FakeTurnHand hand(world);
	// busy: its steps, then its due aims; the next free turn rests
	hand.busy = true;
	small.mind.restAfterSteps = false;
	EXPECT_EQ(TakeTurn(small.Tree(), small.mind, hand, k_Turn, world), TurnDone::Steps);
	EXPECT_EQ(world.calls, (std::vector<std::string> {"step", "give up", "run", "time"}));
	EXPECT_TRUE(small.mind.restAfterSteps);
	// held by a script: only its steps
	world.calls.clear();
	hand.busy = false;
	hand.held = true;
	EXPECT_EQ(TakeTurn(small.Tree(), small.mind, hand, k_Turn, world), TurnDone::Steps);
	EXPECT_EQ(world.calls, (std::vector<std::string> {"step"}));
	// free: the queue first; a forced action that gave the hand something ends the turn
	world.calls.clear();
	hand.held = false;
	hand.forced = true;
	EXPECT_EQ(TakeTurn(small.Tree(), small.mind, hand, k_Turn, world), TurnDone::Forced);
	EXPECT_EQ(world.calls, (std::vector<std::string> {"force"}));
	// nothing given: the rest after steps, then the whole tree weighed in the same turn as the force
	world.calls.clear();
	hand.forced = false;
	EXPECT_EQ(TakeTurn(small.Tree(), small.mind, hand, k_Turn + 1, world), TurnDone::Rested);
	EXPECT_EQ(world.calls, (std::vector<std::string> {"force"}));
	world.calls.clear();
	world.scored.clear();
	EXPECT_EQ(TakeTurn(small.Tree(), small.mind, hand, k_Turn + 2, world), TurnDone::Weighed);
	EXPECT_EQ(world.calls, (std::vector<std::string> {"force", "give up", "run", "time"}));
	EXPECT_FALSE(world.scored.empty());
}

TEST(ComputerMindTurns, TheGamesTreeWeighsWithEveryScoreAtNothing)
{
	auto mind = MakeMind();
	FakeMindWorld world;
	EXPECT_EQ(WeighAll(GameTree(), mind, 5, world), 0.0f);
	// every scoring aim and need reached was asked once, none of their children
	EXPECT_FALSE(world.scored.empty());
	for (const auto node : world.scored)
	{
		EXPECT_NE(RowOf(node).score, ScoreKind::None);
		EXPECT_EQ(mind.nodes.at(node).chosen, std::optional<uint16_t>(node));
	}
}

TEST(ComputerMindScripts, APersonalityGoesToEveryNodeTheNameNames)
{
	auto mind = MakeMind();
	// Land 2's start: ExpandInfluence from 0.75 to 0.2
	EXPECT_TRUE(SetPersonality(mind, "ExpandInfluence", 0.2f));
	EXPECT_EQ(mind.nodes.at(106).personality, 0.2f);
	// the name of a node that begins another's sets both
	EXPECT_TRUE(SetPersonality(mind, "TakeOverTownByImpressing", 0.5f));
	for (const auto node : {107, 122, 133, 162})
	{
		EXPECT_EQ(mind.nodes.at(node).personality, 0.5f) << node;
	}
	EXPECT_EQ(mind.nodes.at(92).personality, RowOf(92).personality);
	EXPECT_FALSE(SetPersonality(mind, "NoSuchNode", 1.0f));
}

TEST(ComputerMindScripts, ASuppressionGoesToEveryNodeTheNameNames)
{
	auto mind = MakeMind();
	EXPECT_TRUE(SetSuppression(mind, "ReactToAggressiveCreature", 0.0f));
	EXPECT_EQ(mind.nodes.at(149).suppression, 0.0f);
	EXPECT_EQ(mind.nodes.at(173).suppression, 0.0f);
	EXPECT_EQ(mind.nodes.at(148).suppression, 1.0f);
	EXPECT_EQ(mind.nodes.at(149).personality, RowOf(149).personality);
}

TEST(ComputerMindScripts, AnAttitudeIsKeptAsTwiceTheValueAndReadAsHalf)
{
	auto mind = MakeMind();
	// a mind no script set reads a half
	EXPECT_EQ(Attitude(mind, PlayerNames::PLAYER_ONE), 0.5f);
	// Nemesis toward the player: 2, kept as 4
	SetAttitude(mind, PlayerNames::PLAYER_ONE, 2.0f);
	EXPECT_EQ(mind.attitude.at(0), 4.0f);
	EXPECT_EQ(Attitude(mind, PlayerNames::PLAYER_ONE), 2.0f);
	SetAttitude(mind, PlayerNames::PLAYER_THREE, 0.25f);
	EXPECT_EQ(mind.attitude.at(2), 0.5f);
	EXPECT_EQ(Attitude(mind, PlayerNames::PLAYER_THREE), 0.25f);
	SetAttitude(mind, PlayerNames::NEUTRAL, -1.0f);
	EXPECT_EQ(Attitude(mind, PlayerNames::NEUTRAL), -1.0f);
	EXPECT_EQ(mind.attitude.at(1), 1.0f);
}

TEST(ComputerMindScripts, ANewLandKeepsTheTreeAndTheAttitudesAndRestsTheMind)
{
	std::optional<ComputerMind> kept = MakeMind();
	SetPersonality(*kept, "ExpandInfluence", 0.0f);
	SetAttitude(*kept, PlayerNames::PLAYER_ONE, 2.0f);
	kept->planScore = 0.3f;
	kept->emptyWeighingTurn = 400;
	kept->restAfterSteps = false;
	kept->seen.push_back({.thing = static_cast<entt::entity>(4), .turn = 9});
	StartNewLand(*kept);
	const auto mind = TakeUp(kept);
	EXPECT_FALSE(kept.has_value());
	EXPECT_EQ(mind.nodes.at(106).personality, 0.0f);
	EXPECT_EQ(Attitude(mind, PlayerNames::PLAYER_ONE), 2.0f);
	EXPECT_EQ(mind.planScore, 0.3f);
	EXPECT_EQ(mind.emptyWeighingTurn, 0u);
	EXPECT_TRUE(mind.restAfterSteps);
	// what it saw was the land's
	EXPECT_TRUE(mind.seen.empty());
	// with none kept, the player takes up a new mind
	const auto fresh = TakeUp(kept);
	EXPECT_EQ(fresh.nodes.size(), k_MindNodeCount);
	EXPECT_EQ(fresh.nodes.at(106).personality, 0.75f);
}

TEST(ComputerMindForcing, ADirectActionRunsAtOnceOnBothThings)
{
	auto mind = MakeMind();
	FakeMindWorld world;
	const auto villager = static_cast<entt::entity>(9);
	const auto marker = static_cast<entt::entity>(10);
	EXPECT_TRUE(Force(mind, "DIRECTPickupAndDropObject",
	                  {.first = villager, .second = marker, .secondWithPlace = marker, .marker = glm::vec3(1.0f)}, k_Turn,
	                  world));
	EXPECT_EQ(world.ran, (std::vector<uint16_t> {6}));
	EXPECT_EQ(world.calls, (std::vector<std::string> {"give up", "run", "time"}));
	EXPECT_EQ(mind.nodes.at(6).thing, villager);
	EXPECT_EQ(mind.nodes.at(6).secondThing, marker);
	// its row holds it back fully once done
	EXPECT_EQ(mind.nodes.at(6).suppression, 0.0f);
	// the direct run is not a run of the node: its turn is not kept
	EXPECT_EQ(mind.nodes.at(6).lastRunTurn, 0u);
}

TEST(ComputerMindForcing, ADirectActionNotDoneGoesOnAsAnyForcedAction)
{
	auto mind = MakeMind();
	FakeMindWorld world;
	world.done = false;
	const auto villager = static_cast<entt::entity>(9);
	EXPECT_FALSE(Force(mind, "DIRECTPickupAndDropObject", {.first = villager, .marker = glm::vec3(4.0f)}, k_Turn, world));
	// the action ran once and failed; then the marker's point went to the node, which had chosen nothing
	EXPECT_EQ(world.calls, (std::vector<std::string> {"give up", "run"}));
	EXPECT_EQ(BitsOf(mind.nodes.at(6).suppression), 0x3C23D70Au);
	EXPECT_EQ(mind.nodes.at(6).point, glm::vec3(4.0f));
	EXPECT_EQ(mind.nodes.at(6).score, k_ForcedScore);
}

TEST(ComputerMindForcing, WithAMarkerTheFirstChildTakesItsPointAndTheNodeRunsThroughIt)
{
	auto mind = MakeMind();
	FakeMindWorld world;
	const auto scaffold = static_cast<entt::entity>(5);
	EXPECT_TRUE(Force(mind, "FindPlaceToUseScaffoldForBuilding",
	                  {.first = scaffold, .second = static_cast<entt::entity>(11), .marker = glm::vec3(7.0f)}, k_Turn, world));
	EXPECT_EQ(mind.nodes.at(83).thing, scaffold);
	EXPECT_EQ(mind.nodes.at(84).point, glm::vec3(7.0f));
	EXPECT_EQ(mind.nodes.at(83).chosen, std::optional<uint16_t>(84));
	// the child has no action: its parent's runs
	EXPECT_EQ(world.ran, (std::vector<uint16_t> {83}));
	EXPECT_EQ(mind.nodes.at(84).lastRunTurn, k_Turn);
	EXPECT_EQ(mind.nodes.at(83).lastRunTurn, k_Turn);
}

TEST(ComputerMindForcing, AQueuedLeafRunsOnlyOnceAWeighingChoseIt)
{
	// LearnGestures: ShallCastWoodSpell on the town and the wood icon
	auto mind = MakeMind();
	FakeMindWorld world;
	const auto town = static_cast<entt::entity>(20);
	const auto icon = static_cast<entt::entity>(21);
	EXPECT_FALSE(Force(mind, "ShallCastWoodSpell", {.first = town, .second = icon}, k_Turn, world));
	EXPECT_TRUE(world.calls.empty());
	EXPECT_EQ(mind.nodes.at(62).thing, town);
	EXPECT_EQ(mind.nodes.at(65).thing, icon);
	EXPECT_EQ(mind.nodes.at(65).score, k_ForcedScore);
	// once a weighing reached it, it chose itself, and the same force runs its action
	mind.nodes.at(65).chosen = 65;
	EXPECT_TRUE(Force(mind, "ShallCastWoodSpell", {.first = town, .second = icon}, k_Turn, world));
	EXPECT_EQ(world.ran, (std::vector<uint16_t> {65}));
}

TEST(ComputerMindForcing, ANodeWithAnActionAndChildrenButNoSecondThingWeighsItsChildren)
{
	auto mind = MakeMind();
	FakeMindWorld world;
	world.everyScore = 0.5f;
	EXPECT_TRUE(Force(mind, "FindPlaceToUseScaffoldForBuilding", {.first = static_cast<entt::entity>(5)}, k_Turn, world));
	EXPECT_EQ(world.scored, (std::vector<uint16_t> {84}));
	EXPECT_EQ(mind.nodes.at(83).chosen, std::optional<uint16_t>(84));
	EXPECT_EQ(world.ran, (std::vector<uint16_t> {83}));
}

TEST(ComputerMindForcing, ANodeWithoutAnActionThatStillChoosesItselfRefuses)
{
	auto mind = MakeMind();
	FakeMindWorld world;
	// as after a weighing in which nothing under it was worth more
	mind.nodes.at(106).chosen = 106;
	EXPECT_FALSE(Force(mind, "ExpandInfluence", {}, k_Turn, world));
	EXPECT_TRUE(world.ran.empty());
	EXPECT_FALSE(world.scored.empty());
}

TEST(ComputerMindForcing, AForceThatWouldWeighAWayOfScoringOpenblackLacksChangesNothing)
{
	auto mind = MakeMind();
	FakeMindWorld world;
	world.noScores = true;
	EXPECT_FALSE(Force(mind, "ExpandInfluence", {.first = static_cast<entt::entity>(3)}, k_Turn, world));
	EXPECT_FALSE(Force(mind, "FindPlaceToUseScaffoldForBuilding", {.first = static_cast<entt::entity>(5)}, k_Turn, world));
	EXPECT_TRUE(world.scored.empty());
	EXPECT_EQ(mind.nodes.at(106).thing, entt::entity {entt::null});
	EXPECT_EQ(mind.nodes.at(83).score, 0.0f);
	// a name no node has does nothing
	EXPECT_FALSE(Force(mind, "NoSuchAction", {}, k_Turn, world));
	EXPECT_TRUE(world.calls.empty());
}

TEST(ComputerMindForcing, AnActionActsOnTheThingNearestAboveAndThePointOfTheNodeItChose)
{
	auto mind = MakeMind();
	const auto town = static_cast<entt::entity>(30);
	mind.nodes.at(80).thing = town;
	EXPECT_EQ(ThingAbove(GameTree(), mind, 84), town);
	mind.nodes.at(83).thing = static_cast<entt::entity>(5);
	EXPECT_EQ(ThingAbove(GameTree(), mind, 84), static_cast<entt::entity>(5));
	EXPECT_FALSE(ChosenBelow(mind, 83).has_value());
	mind.nodes.at(83).chosen = 83;
	EXPECT_FALSE(ChosenBelow(mind, 83).has_value());
	mind.nodes.at(83).chosen = 84;
	EXPECT_EQ(ChosenBelow(mind, 83), std::optional<uint16_t>(84));
	// a node's point is 0 until something gives it one
	EXPECT_EQ(mind.nodes.at(84).point, glm::vec3(0.0f));
}

TEST(ComputerMindForcing, DirectInAnyCase)
{
	EXPECT_TRUE(IsDirectName("DIRECTPickupAndDropObject"));
	EXPECT_TRUE(IsDirectName("directSomething"));
	EXPECT_FALSE(IsDirectName("DIREC"));
	EXPECT_FALSE(IsDirectName("FindPlaceToUseScaffoldForBuilding"));
}

namespace
{
using openblack::ecs::components::ComputerMindSeen;

/// A 16 x 16 land whose cells hold the things the test puts there, and what the mind remembers each as
class FakeSeenWorld final: public SeenWorld
{
public:
	[[nodiscard]] std::optional<std::vector<entt::entity>> CellObjects(uint16_t x, uint16_t z) const override
	{
		if (x >= k_Cells || z >= k_Cells)
		{
			return std::nullopt;
		}
		const auto found = cells.find({x, z});
		return found != cells.end() ? found->second : std::vector<entt::entity> {};
	}
	[[nodiscard]] std::optional<ComputerMindSeen> SeenAs(entt::entity thing) const override
	{
		if (gone.contains(thing))
		{
			return std::nullopt;
		}
		if (const auto found = forestOf.find(thing); found != forestOf.end())
		{
			return ComputerMindSeen {.forest = found->second};
		}
		if (others.contains(thing))
		{
			return std::nullopt;
		}
		return ComputerMindSeen {.thing = thing};
	}
	[[nodiscard]] bool StillThere(const ComputerMindSeen& seen) const override
	{
		return seen.forest.has_value() ? !goneForests.contains(*seen.forest) : !gone.contains(seen.thing);
	}

	static constexpr uint16_t k_Cells = 16;
	std::map<std::pair<uint16_t, uint16_t>, std::vector<entt::entity>> cells;
	std::map<entt::entity, uint32_t> forestOf;
	std::set<entt::entity> others;
	std::set<entt::entity> gone;
	std::set<uint32_t> goneForests;
};

/// The cells the look goes over from the middle of a cell in the middle of the land, in its order: the spiral as far as
/// the corner three cells down and left, which is not nearer than 40 metres
constexpr std::array<std::pair<int, int>, 31> k_SpiralCells {{
    {0, 0},   {-1, 0},  {-1, -1}, {0, -1}, {1, -1}, {1, 0},  {1, 1},   {0, 1},   {-1, 1},  {-2, 1}, {-2, 0},
    {-2, -1}, {-2, -2}, {-1, -2}, {0, -2}, {1, -2}, {2, -2}, {2, -1},  {2, 0},   {2, 1},   {2, 2},  {1, 2},
    {0, 2},   {-1, 2},  {-2, 2},  {-3, 2}, {-3, 1}, {-3, 0}, {-3, -1}, {-3, -2}, {-3, -3},
}};

/// The middle of the cell (x, z)
map_coords::MapCoords CellMiddle(int x, int z)
{
	return {(x << 16) | 0x8000, (z << 16) | 0x8000, 0.0f};
}

std::vector<entt::entity> ThingsSeen(const ComputerMind& mind)
{
	std::vector<entt::entity> things;
	for (const auto& seen : mind.seen)
	{
		things.push_back(seen.thing);
	}
	return things;
}
} // namespace

TEST(ComputerMindSeen, TheMindLooksOverTheCellsInASpiralFromItsHandAsFarAsItsReach)
{
	// a tree in each cell the spiral meets from the hand at cell (8, 8), and five more past where it stops
	FakeSeenWorld world;
	std::vector<entt::entity> expected;
	for (size_t k = 0; k < k_SpiralCells.size(); ++k)
	{
		const entt::entity tree {static_cast<uint32_t>(100 + k)};
		const auto [dx, dz] = k_SpiralCells.at(k);
		world.cells[{static_cast<uint16_t>(8 + dx), static_cast<uint16_t>(8 + dz)}].push_back(tree);
		if (k + 1 < k_SpiralCells.size())
		{
			expected.insert(expected.begin(), tree);
		}
	}
	for (const auto& [dx, dz] : std::array<std::pair<int, int>, 4> {{{-2, -3}, {3, -1}, {3, 0}, {0, 3}}})
	{
		world.cells[{static_cast<uint16_t>(8 + dx), static_cast<uint16_t>(8 + dz)}].push_back(entt::entity {99});
	}
	auto mind = MakeMind();
	ScanAroundHand(mind, world, CellMiddle(8, 8), 1234);
	// the 30 cells before the corner, the newest first, each at the turn it was seen
	EXPECT_EQ(ThingsSeen(mind), expected);
	EXPECT_TRUE(std::ranges::all_of(mind.seen, [](const ComputerMindSeen& seen) { return seen.turn == 1234u; }));
	// what it has seen already is not seen again
	ScanAroundHand(mind, world, CellMiddle(8, 8), 1235);
	EXPECT_EQ(ThingsSeen(mind), expected);
}

TEST(ComputerMindSeen, CellsOffTheLandArePassedOverAndTheLookGoesOn)
{
	// the hand in the corner cell (0, 0): the cells below 0 are passed over, and the look stops at (3, 3)
	FakeSeenWorld world;
	const auto put = [&world](uint16_t x, uint16_t z, uint32_t thing) { world.cells[{x, z}].push_back(entt::entity {thing}); };
	put(3, 0, 1);
	put(3, 1, 2);
	put(3, 2, 3);
	put(2, 3, 4);
	put(0, 3, 5);
	put(1, 1, 6);
	auto mind = MakeMind();
	ScanAroundHand(mind, world, CellMiddle(0, 0), 7);
	EXPECT_EQ(ThingsSeen(mind),
	          (std::vector<entt::entity> {entt::entity {3}, entt::entity {2}, entt::entity {1}, entt::entity {6}}));
}

TEST(ComputerMindSeen, ATreeIsSeenAsItsForestOnceAndOnlyWhatIsThereIsSeen)
{
	FakeSeenWorld world;
	const entt::entity inForestA {1};
	const entt::entity inForestB {2};
	const entt::entity loose {3};
	const entt::entity unavailable {4};
	const entt::entity seed {5};
	const entt::entity alsoInForestA {6};
	const entt::entity looseSeen {7};
	const entt::entity house {8};
	world.forestOf = {{inForestA, 11}, {inForestB, 12}, {alsoInForestA, 11}};
	world.gone = {unavailable};
	world.others = {house};
	world.cells[{8, 8}] = {inForestA, house};
	world.cells[{7, 8}] = {inForestB};
	world.cells[{7, 7}] = {loose};
	world.cells[{8, 7}] = {unavailable};
	world.cells[{9, 7}] = {seed};
	world.cells[{9, 8}] = {alsoInForestA};
	world.cells[{9, 9}] = {looseSeen};
	auto mind = MakeMind();
	mind.seen = {ComputerMindSeen {.forest = 12, .turn = 3}, ComputerMindSeen {.thing = looseSeen, .turn = 4}};
	ScanAroundHand(mind, world, CellMiddle(8, 8), 10);
	ASSERT_EQ(mind.seen.size(), 5u);
	EXPECT_EQ(mind.seen.at(0).thing, seed);
	EXPECT_EQ(mind.seen.at(1).thing, loose);
	EXPECT_EQ(mind.seen.at(2).forest, std::optional<uint32_t> {11});
	EXPECT_EQ(mind.seen.at(2).thing, entt::entity {entt::null});
	EXPECT_EQ(mind.seen.at(2).turn, 10u);
	EXPECT_EQ(mind.seen.at(3).turn, 3u);
	EXPECT_EQ(mind.seen.at(4).thing, looseSeen);
}

TEST(ComputerMindSeen, TheThingsOfACellAreSeenInTheOrderTheGameWalksThem)
{
	FakeSeenWorld world;
	world.cells[{8, 8}] = {entt::entity {2}, entt::entity {1}};
	world.cells[{7, 8}] = {entt::entity {3}};
	auto mind = MakeMind();
	ScanAroundHand(mind, world, CellMiddle(8, 8), 1);
	EXPECT_EQ(ThingsSeen(mind), (std::vector<entt::entity> {entt::entity {3}, entt::entity {1}, entt::entity {2}}));
}

TEST(ComputerMindSeen, WhatWasSeenLongAgoIsForgottenEvery500TurnsWhenMoreThan50AreKept)
{
	// the oracle's cases: over 8000 turns, a turn after the game's counting as long ago
	std::vector<ComputerMindSeen> seen;
	for (const uint32_t turn : {1999u, 2000u, 2001u, 9000u})
	{
		seen.push_back({.thing = entt::entity {turn}, .turn = turn});
	}
	ForgetSeenBefore(seen, 10000, k_SeenOld);
	ASSERT_EQ(seen.size(), 3u);
	EXPECT_EQ(seen.front().turn, 2000u);
	std::vector<ComputerMindSeen> early {{.thing = entt::entity {1}, .turn = 6}, {.thing = entt::entity {2}, .turn = 0}};
	ForgetSeenBefore(early, 5, k_SeenOld);
	ASSERT_EQ(early.size(), 1u);
	EXPECT_EQ(early.front().turn, 0u);
	// the look forgets only at every 500th turn, and only with more than 50 kept
	FakeSeenWorld world;
	const auto filled = [](size_t count) {
		auto mind = MakeMind();
		for (size_t k = 0; k < count; ++k)
		{
			mind.seen.push_back({.thing = entt::entity {static_cast<uint32_t>(1000 + k)}, .turn = 1});
		}
		return mind;
	};
	auto many = filled(51);
	ScanAroundHand(many, world, CellMiddle(8, 8), 9500);
	EXPECT_TRUE(many.seen.empty());
	auto offTurn = filled(51);
	ScanAroundHand(offTurn, world, CellMiddle(8, 8), 9501);
	EXPECT_EQ(offTurn.seen.size(), 51u);
	auto fifty = filled(50);
	ScanAroundHand(fifty, world, CellMiddle(8, 8), 9500);
	EXPECT_EQ(fifty.seen.size(), 50u);
}

TEST(ComputerMindSeen, WhatIsGoneIsForgotten)
{
	FakeSeenWorld world;
	world.gone = {entt::entity {2}};
	world.goneForests = {7};
	auto mind = MakeMind();
	mind.seen = {{.thing = entt::entity {1}}, {.thing = entt::entity {2}}, {.forest = 7}, {.forest = 8}};
	ForgetSeenGone(mind, world);
	ASSERT_EQ(mind.seen.size(), 2u);
	EXPECT_EQ(mind.seen.at(0).thing, entt::entity {1});
	EXPECT_EQ(mind.seen.at(1).forest, std::optional<uint32_t> {8});
}

namespace
{

/// A personality file's entry for a node: the bytes over its row's name, then its weight lowest byte first
void AppendEntry(std::vector<uint8_t>& file, std::string_view nameBytes, float weight)
{
	file.insert(file.end(), nameBytes.begin(), nameBytes.end());
	const auto bits = std::bit_cast<uint32_t>(weight);
	for (uint32_t index = 0; index < 4; ++index)
	{
		file.push_back(static_cast<uint8_t>(bits >> (8 * index)));
	}
}

/// The row's own name and its nul, as a file holds it
std::string WithNul(std::string_view name)
{
	return std::string(name) + '\0';
}

} // namespace

TEST(ComputerMindPersonalityFile, HoldsEachNodesNameAndWeightInTheOrderOfTheTree)
{
	auto mind = MakeMind();
	mind.nodes.at(106).personality = 0.2f;
	const auto file = WritePersonality(mind, OwnRowNames());
	std::vector<uint8_t> expected;
	for (uint16_t node = 0; node < k_MindNodeCount; ++node)
	{
		AppendEntry(expected, WithNul(RowOf(node).name), mind.nodes.at(node).personality);
	}
	EXPECT_EQ(file, expected);
	// the root first: its name, its nul and 1 as a float, lowest byte first
	ASSERT_GE(file.size(), 25u);
	EXPECT_EQ(std::string(file.begin(), file.begin() + 21), WithNul("Master Decision Tree"));
	EXPECT_EQ(std::vector<uint8_t>(file.begin() + 21, file.begin() + 25), (std::vector<uint8_t> {0x00, 0x00, 0x80, 0x3F}));
}

TEST(ComputerMindPersonalityFile, AWrittenFileReadsBackTheWeightsAndLeavesTheNames)
{
	auto saved = MakeMind();
	SetPersonality(saved, "ExpandInfluence", 0.2f);
	SetPersonality(saved, "TakeOverTownByImpressing", 0.5f);
	auto file = WritePersonality(saved, OwnRowNames());
	// what comes after the last node is not read
	file.push_back(0x7F);
	MindRowNames names;
	auto mind = MakeMind();
	ReadPersonality(file, mind, names);
	for (uint16_t node = 0; node < k_MindNodeCount; ++node)
	{
		EXPECT_EQ(mind.nodes.at(node).personality, saved.nodes.at(node).personality) << node;
		EXPECT_EQ(names.Name(MindNodes().at(node).row), RowOf(node).name) << node;
	}
}

TEST(ComputerMindPersonalityFile, ANameReadIsNotComparedButWrittenOverTheRowForEveryMind)
{
	std::vector<uint8_t> file;
	for (uint16_t node = 0; node < k_MindNodeCount; ++node)
	{
		AppendEntry(file, WithNul(node == 1 ? "PlayerInteracX: 13 " : RowOf(node).name), 0.25f);
	}
	MindRowNames names;
	auto mind = MakeMind();
	ReadPersonality(file, mind, names);
	EXPECT_EQ(mind.nodes.at(1).personality, 0.25f);
	EXPECT_EQ(mind.nodes.at(k_MindNodeCount - 1).personality, 0.25f);
	EXPECT_EQ(names.Name(1), "PlayerInteracX: 13 ");
	// the row now answers to the name read, for this mind and any other, and no longer to its own
	EXPECT_TRUE(FindNodes("PlayerInteract", names).empty());
	EXPECT_EQ(FindNodes("PlayerInteracX", names), (std::vector<uint16_t> {1}));
	auto other = MakeMind();
	EXPECT_TRUE(SetPersonality(other, "PlayerInteracX", 0.5f, names));
	EXPECT_EQ(other.nodes.at(1).personality, 0.5f);
	EXPECT_FALSE(SetPersonality(other, "PlayerInteract", 0.5f, names));
	// the rows' own names are untouched
	EXPECT_EQ(OwnRowNames().Name(1), "PlayerInteract: 13 ");
	EXPECT_EQ(FindNodes("PlayerInteract"), (std::vector<uint16_t> {1}));
}

TEST(ComputerMindPersonalityFile, ANameCutShortByANulIsReadShortWhereItsRowIsMetAgain)
{
	// the building site's wood is met more than once: its first entry ends its name early, so each later one reads
	// only the shorter name and its nul
	const auto wood = NodesNamed("GiveBuildingSiteWood: 86 ");
	ASSERT_GE(wood.size(), 2u);
	const std::string shortName("Give\0", 5);
	std::vector<uint8_t> file;
	for (uint16_t node = 0; node < k_MindNodeCount; ++node)
	{
		if (node == wood.front())
		{
			auto padded = shortName;
			padded.resize(RowOf(node).name.size() + 1, '\0');
			AppendEntry(file, padded, 0.5f);
		}
		else if (std::ranges::find(wood, node) != wood.end())
		{
			AppendEntry(file, shortName, 0.5f);
		}
		else
		{
			AppendEntry(file, WithNul(RowOf(node).name), 0.25f);
		}
	}
	MindRowNames names;
	auto mind = MakeMind();
	ReadPersonality(file, mind, names);
	for (uint16_t node = 0; node < k_MindNodeCount; ++node)
	{
		const bool isWood = std::ranges::find(wood, node) != wood.end();
		EXPECT_EQ(mind.nodes.at(node).personality, isWood ? 0.5f : 0.25f) << node;
	}
	EXPECT_EQ(names.Name(MindNodes().at(wood.front()).row), "Give");
	// a script's name is found by the name each row has now, up to its ':', at the name's start: the short "Give"
	// still begins the old name, so the old name finds the same nodes, and a name that does not begin with it does not
	EXPECT_EQ(FindNodes("GiveBuildingSiteWood", names), wood);
	EXPECT_TRUE(FindNodes("Giv", names).empty());
	EXPECT_EQ(FindNodes("Give", names), wood);
}

TEST(ComputerMindPersonalityFile, AShortFileLeavesWhatItDoesNotReach)
{
	MindRowNames names;
	auto mind = MakeMind();
	// an empty file changes nothing
	ReadPersonality({}, mind, names);
	EXPECT_EQ(mind.nodes.at(0).personality, 1.0f);
	EXPECT_EQ(names.Name(0), "Master Decision Tree");
	// the root's name, then two bytes of its weight: only the weight's two lowest bytes are written
	std::vector<uint8_t> file;
	AppendEntry(file, WithNul("Master Decision Tree"), 0.0f);
	file.resize(file.size() - 2);
	file.at(file.size() - 2) = 0x11;
	file.at(file.size() - 1) = 0x22;
	ReadPersonality(file, mind, names);
	EXPECT_EQ(std::bit_cast<uint32_t>(mind.nodes.at(0).personality), 0x3F802211u);
	EXPECT_EQ(mind.nodes.at(1).personality, RowOf(1).personality);
	EXPECT_EQ(names.Name(1), "PlayerInteract: 13 ");
	// a file that ends inside a name writes only the bytes it has over it
	ReadPersonality(std::vector<uint8_t> {'X', 'X'}, mind, names);
	EXPECT_EQ(names.Name(0), "XXster Decision Tree");
}
