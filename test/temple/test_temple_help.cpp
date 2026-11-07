/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Temple/TempleHelp.h"
#include "Temple/TempleTurn.h"

using namespace openblack;
using namespace openblack::temple_help;

namespace
{
/// Records what the temple asks of the scripts, in order
class FakeScripts final: public Scripts
{
public:
	void Start(std::string_view name) override { calls.push_back("start " + std::string(name)); }
	void StopHelp() override { calls.emplace_back("stop help"); }
	void Stop(std::string_view name) override { calls.push_back("stop " + std::string(name)); }

	std::vector<std::string> calls;
};
} // namespace

TEST(TempleHelp, EnteringARoomStopsTheHelpThenStartsTheRoomsOwn)
{
	FakeScripts scripts;
	EnterRoom(scripts, TempleRoom::CreatureCave, true);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {"stop help", "start CitadelCreatureRoomHelp"}));
}

TEST(TempleHelp, WithTheHelpSystemOffARoomOnlyStopsTheHelp)
{
	FakeScripts scripts;
	EnterRoom(scripts, TempleRoom::Main, false);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {"stop help"}));
}

TEST(TempleHelp, EachRoomHasItsHelpAndTheOptionsRoomSharesTheMainRooms)
{
	EXPECT_EQ(RoomHelp(TempleRoom::Main), "CitadelWorldRoomHelp");
	EXPECT_EQ(RoomHelp(TempleRoom::Options), "CitadelWorldRoomHelp");
	EXPECT_EQ(RoomHelp(TempleRoom::CreatureCave), "CitadelCreatureRoomHelp");
	EXPECT_EQ(RoomHelp(TempleRoom::Challenge), "CitadelChallengeRoomHelp");
	EXPECT_EQ(RoomHelp(TempleRoom::SaveGame), "CitadelSaveGameRoomHelp");
	EXPECT_EQ(RoomHelp(TempleRoom::Credits), "CitadelCreditsRoomHelp");
	EXPECT_FALSE(RoomHelp(TempleRoom::Multi).has_value());

	FakeScripts scripts;
	EnterRoom(scripts, TempleRoom::Multi, true);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {"stop help"}));
}

TEST(TempleHelp, LookingAtAScrollStartsItsHelpWhetherOrNotTheHelpSystemIsOn)
{
	using Content = TempleScrolls::Content;
	FakeScripts scripts;
	LookAtScroll(scripts, Content::World);
	LookAtScroll(scripts, Content::CreatureAttributes);
	LookAtScroll(scripts, Content::CreatureActions);
	LookAtScroll(scripts, Content::CreatureMind);
	LookAtScroll(scripts, Content::CreatureMiracles);
	LookAtScroll(scripts, Content::Challenge);
	LookAtScroll(scripts, Content::SaveGame);
	LookAtScroll(scripts, Content::LibraryStaff);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {
	                             "start CitadelWorldRoomScrollHelp",
	                             "start CitadelCreatureRoomAttributesScroll",
	                             "start CitadelCreatureRoomActionsLearntScroll",
	                             "start CitadelCreatureRoomLikesScroll",
	                             "start CitadelCreatureRoomMagicScroll",
	                             "start CitadelChallengeRoomScrollHelp",
	                             "start CitadelSaveGameRoomScrollHelp",
	                         }));
}

TEST(TempleHelp, LeavingAScrollStopsTheScrollHelpOfTheRoomsCamera)
{
	FakeScripts scripts;
	LeaveScroll(scripts, TempleRoom::Main);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {"stop CitadelWorldRoomScrollHelp"}));

	scripts.calls.clear();
	LeaveScroll(scripts, TempleRoom::SaveGame);
	EXPECT_EQ(scripts.calls,
	          (std::vector<std::string> {"stop CitadelChallengeRoomScrollHelp", "stop CitadelSaveGameRoomScrollHelp"}));

	scripts.calls.clear();
	LeaveScroll(scripts, TempleRoom::CreatureCave);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {
	                             "stop CitadelCreatureRoomAttributesScroll",
	                             "stop CitadelCreatureRoomLikesScroll",
	                             "stop CitadelCreatureRoomMagicScroll",
	                             "stop CitadelCreatureRoomActionsLearntScroll",
	                         }));

	scripts.calls.clear();
	LeaveScroll(scripts, TempleRoom::Credits);
	EXPECT_TRUE(scripts.calls.empty());
}

TEST(TempleHelp, OnlyTheBeltsAndMedalsOfTheCreaturesRoomStartHelp)
{
	using CreatureCaveTargets::Target;
	FakeScripts scripts;
	for (const auto target : {Target::Creature, Target::Belts, Target::Medals, Target::Fourth, Target::Exit})
	{
		ZoomToCaveTarget(scripts, target);
	}
	EXPECT_EQ(scripts.calls,
	          (std::vector<std::string> {"start CitadelCreatureRoomAttackDummies", "start CitadelCreatureRoomMagicPlinths"}));
}

TEST(TempleHelp, LeavingTheTempleStopsTheHelp)
{
	FakeScripts scripts;
	Leave(scripts);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {"stop help"}));
}

namespace
{
/// Records what the temple does to the dialogue, in order, into the same list as the scripts (raffclar's)
class FakeDialogue final: public Dialogue
{
public:
	explicit FakeDialogue(std::vector<std::string>& calls)
	    : _calls(calls)
	{
	}
	void SendAdvisorsHome() override { _calls.emplace_back("advisors home"); }
	[[nodiscard]] bool IsControlled() const override { return owner != 0 || wideScreen; }
	[[nodiscard]] uint32_t GetOwner() const override { return owner; }
	[[nodiscard]] bool IsTempleScript(uint32_t task) const override { return task == templeTask; }
	void GiveTo(uint32_t task) override
	{
		_calls.push_back("give to " + std::to_string(task));
		owner = task;
		text = {};
	}
	void TakeAway() override
	{
		_calls.emplace_back("take away");
		owner = 0;
	}
	[[nodiscard]] help::DialogueText::Held HoldText() const override { return text; }
	void RestoreText(const help::DialogueText::Held& held) override
	{
		_calls.emplace_back("restore");
		text = held;
	}
	void ShowTextAgain() override { _calls.emplace_back("show again"); }

	uint32_t owner {0};
	bool wideScreen {false};
	uint32_t templeTask {999};
	help::DialogueText::Held text;

private:
	std::vector<std::string>& _calls;
};
} // namespace

TEST(TempleHelp, AScriptsDialogueIsKeptWhileInsideAndGivenBackOnLeaving)
{
	FakeScripts scripts;
	FakeDialogue dialogue(scripts.calls);
	dialogue.owner = 7;
	dialogue.text = {.texts = {40, 30, 0, 0, 0, 0}, .waitClick = true, .noClick = false};

	auto held = PutAwayDialogue(scripts, dialogue);
	ASSERT_TRUE(held.has_value());
	EXPECT_EQ(held->owner, 7u);
	EXPECT_EQ(held->text.texts[0], 40u);
	EXPECT_TRUE(held->text.waitClick);
	EXPECT_EQ(dialogue.owner, 0u);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {"stop help", "advisors home", "take away"}));

	scripts.calls.clear();
	BringBackDialogue(dialogue, held);
	EXPECT_FALSE(held.has_value());
	EXPECT_EQ(dialogue.owner, 7u);
	EXPECT_EQ(dialogue.text.texts[1], 30u);
	EXPECT_TRUE(dialogue.text.waitClick);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {"give to 7", "restore", "show again"}));
}

TEST(TempleHelp, ATempleScriptsDialogueAndNoDialogueAreNotKept)
{
	FakeScripts scripts;
	FakeDialogue dialogue(scripts.calls);
	EXPECT_FALSE(PutAwayDialogue(scripts, dialogue).has_value());

	dialogue.owner = 3;
	dialogue.templeTask = 3;
	EXPECT_FALSE(PutAwayDialogue(scripts, dialogue).has_value());
	EXPECT_EQ(dialogue.owner, 3u);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {"stop help", "advisors home", "stop help", "advisors home"}));

	// The temple kept nothing: when the player leaves, the box shows its texts again
	scripts.calls.clear();
	std::optional<HeldDialogue> none;
	BringBackDialogue(dialogue, none);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {"show again"}));
}

TEST(TempleHelp, TheCinemaBarsAloneAreKeptWithNoOwner)
{
	FakeScripts scripts;
	FakeDialogue dialogue(scripts.calls);
	dialogue.wideScreen = true;
	const auto held = PutAwayDialogue(scripts, dialogue);
	ASSERT_TRUE(held.has_value());
	EXPECT_EQ(held->owner, 0u);
}

TEST(TempleHelp, TheDialogueTakenInsideIsNotGivenBack)
{
	FakeScripts scripts;
	FakeDialogue dialogue(scripts.calls);
	dialogue.owner = 7;
	auto held = PutAwayDialogue(scripts, dialogue);
	ASSERT_TRUE(held.has_value());

	// Another script took the dialogue while the player was inside
	dialogue.owner = 9;
	scripts.calls.clear();
	BringBackDialogue(dialogue, held);
	EXPECT_FALSE(held.has_value());
	EXPECT_EQ(dialogue.owner, 9u);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {"show again"}));
}

namespace
{
/// Records the steps of a temple turn, in order
class FakeTurnSteps final: public temple_turn::Steps
{
public:
	void LookInTempleScripts() override { calls.emplace_back("scripts"); }
	void AudioTurn() override { calls.emplace_back("audio"); }
	void RoomToolTip() override { calls.emplace_back("tooltip"); }
	void HelpTurn() override { calls.emplace_back("help"); }
	std::vector<std::string> calls;
};
} // namespace

TEST(TempleTurn, WhilePausedTheScriptsThenTheAudioThenTheTooltipAndTheHelpTakeTheirTurn)
{
	FakeTurnSteps steps;
	temple_turn::Process(steps, true);
	EXPECT_EQ(steps.calls, (std::vector<std::string> {"scripts", "audio", "tooltip", "help"}));
}

TEST(TempleTurn, NotPausedTheTempleTurnDoesNothing)
{
	FakeTurnSteps steps;
	temple_turn::Process(steps, false);
	EXPECT_TRUE(steps.calls.empty());
}
