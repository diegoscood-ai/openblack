/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// raffclar's cases (0222f9f5, test_dialogue_text.cpp) on our help::DialogueText: the scripts' texts, their reading
// time, the wait for a click, the voices and the draw gate, with the game's clock, texts and voices faked by the test.
// His settings are our Info (8 and 5, as info.dat), SetReadSpeed and the TEXT_DRAW value the display's gate is asked
// with; his "important" text is our entry's first argument; his advisor voice is a sample in the advisors' bank; his
// key is our skip key query, read by ProcessClick. His continue cue case is not here: ours is the input prompt icon
// the dialogue makes as it draws.

#include <cmath>
#include <cstdint>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Help/DialogueText.h"

using namespace openblack;
using namespace openblack::help;

namespace
{
/// A voice in the advisors' bank, and one in another
constexpr audio::TextVoice k_AdvisorVoice {.bank = audio::SfxBank::HelpSprites, .sample = 1};
constexpr audio::TextVoice k_OtherVoice {.bank = audio::SfxBank::Villagers, .sample = 1};

struct Fixture
{
	struct FakeText
	{
		int32_t narrator {0};
		bool important {false};
		std::u16string text;
		audio::TextVoice voice {};
	};

	std::map<uint32_t, FakeText> texts;
	uint32_t turn {100};
	int32_t nowMs {10000};
	bool inTemple {false};
	bool advisorsTalking {false};
	bool narrationSounding {false};
	bool wideScreen {false};
	bool keyDown {false};
	std::vector<std::pair<VoiceRoute, uint32_t>> advisorSaid;
	std::vector<uint32_t> narrated;
	int interrupts {0};
	int narrationStops {0};

	[[nodiscard]] DialogueText Make(float readSpeed = 0.5f)
	{
		DialogueText::Queries queries;
		queries.textEntry = [this](uint32_t number) {
			const auto it = texts.find(number);
			if (it == texts.end())
			{
				return helptext::Entry {};
			}
			return helptext::Entry {
			    .arg0 = it->second.important ? 1 : 0, .narrator = it->second.narrator, .name = {}, .text = it->second.text};
		};
		queries.textVoice = [this](uint32_t number) {
			const auto it = texts.find(number);
			return it == texts.end() ? audio::TextVoice {} : it->second.voice;
		};
		queries.voiceBankLoaded = [](audio::SfxBank) { return true; };
		queries.turn = [this]() { return turn; };
		queries.nowMs = [this]() { return nowMs; };
		queries.citadelClock = [this]() { return inTemple; };
		queries.skipKey = [this]() { return keyDown; };
		queries.advisorsTalking = [this]() { return advisorsTalking; };
		queries.isPlaying = [this](audio::SfxBank, audio::VoiceOwner owner, uint32_t) {
			return narrationSounding && owner == audio::VoiceOwner::Narration;
		};
		queries.scriptWideScreen = [this]() { return wideScreen; };
		DialogueText::Hooks hooks;
		hooks.sayVoice = [this](uint32_t number, VoiceRoute route, audio::TextVoice) {
			if (route == VoiceRoute::Narration)
			{
				narrated.push_back(number);
			}
			else
			{
				advisorSaid.emplace_back(route, number);
			}
		};
		hooks.interruptAdvisors = [this]() { ++interrupts; };
		hooks.stopVoicesOnClick = [this]() { ++narrationStops; };
		DialogueText dialogue({.readDefaultAdjustGTTime = 8, .readDefaultWordGTTime = 5}, std::move(queries), std::move(hooks));
		dialogue.SetReadSpeed(readSpeed);
		return dialogue;
	}

	/// The player's click, the key, or both
	void Click(DialogueText& dialogue, bool click, bool key)
	{
		keyDown = key;
		dialogue.ProcessClick(click);
		keyDown = false;
	}
};
} // namespace

TEST(DialogueText, TheTextsTheTempleKeptAreShownAgain)
{
	Fixture f;
	f.texts[30] = {.narrator = 2, .text = u"a b c d"};
	f.texts[40] = {.narrator = 1, .text = u"one two"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 30, 0);
	dialogue.RunText(false, 40, 1);
	const auto held = dialogue.Hold();
	EXPECT_EQ(held.texts[0], 40u);
	EXPECT_EQ(held.texts[1], 30u);
	EXPECT_TRUE(held.waitClick);
	EXPECT_FALSE(held.noClick);

	// Emptied while the player is inside, the texts and the click wait come back, but show only once shown again
	dialogue.ClearAllText();
	dialogue.Restore(held);
	EXPECT_EQ(dialogue.GetCurrentText(), 40u);
	EXPECT_TRUE(dialogue.IsWaitingForClick());
	EXPECT_FALSE(dialogue.GetDisplay().HasNewestText());
	f.turn = 500;
	dialogue.ShowAgain();
	EXPECT_TRUE(dialogue.GetDisplay().HasNewestText());
	EXPECT_FALSE(dialogue.GetDisplay().IsHidden());
	// The newest's reading time starts again
	EXPECT_EQ(dialogue.GetStartTurn(), 500u);
	EXPECT_TRUE(dialogue.IsWaitingForClick());
}

TEST(DialogueText, ReadSpeedFactor)
{
	EXPECT_FLOAT_EQ(ReadSpeedFactor(0.0f), 3.0f);
	EXPECT_FLOAT_EQ(ReadSpeedFactor(0.25f), 2.0f);
	EXPECT_FLOAT_EQ(ReadSpeedFactor(0.5f), 1.0f);
	EXPECT_FLOAT_EQ(ReadSpeedFactor(0.75f), 0.6f);
	EXPECT_FLOAT_EQ(ReadSpeedFactor(1.0f), 0.2f);
	// Not a number counts as slow
	EXPECT_TRUE(std::isnan(ReadSpeedFactor(std::nanf(""))));
}

TEST(DialogueText, ReadingTimeIsFiveTurnsAWordAndEightMore)
{
	Fixture f;
	f.texts[10] = {.text = u"one two three"};
	auto dialogue = f.Make();
	EXPECT_TRUE(dialogue.RunText(false, 10, 0));
	EXPECT_EQ(dialogue.GetCurrentText(), 10u);
	// 23 turns of 100 ms at the normal reading speed
	EXPECT_EQ(dialogue.GetEndTurn(), 123u);
	EXPECT_EQ(dialogue.GetEndMs(), 12300);
	EXPECT_FALSE(dialogue.IsTextRead());
	f.turn = 123;
	EXPECT_FALSE(dialogue.IsTextRead());
	f.turn = 124;
	EXPECT_TRUE(dialogue.IsTextRead());
}

TEST(DialogueText, SlowReadersGetLonger)
{
	Fixture f;
	f.texts[10] = {.text = u"one two three"};
	auto dialogue = f.Make(0.0f);
	dialogue.RunText(false, 10, 0);
	EXPECT_EQ(dialogue.GetEndTurn(), 169u);
	EXPECT_EQ(dialogue.GetEndMs(), 16900);
}

TEST(DialogueText, InTheTempleTheTimeIsInMilliseconds)
{
	Fixture f;
	f.inTemple = true;
	f.texts[10] = {.text = u"one two three"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 0);
	f.nowMs = 12300;
	EXPECT_FALSE(dialogue.IsTextRead());
	f.nowMs = 12301;
	EXPECT_TRUE(dialogue.IsTextRead());
}

TEST(DialogueText, InvalidTextShowsTheFirst)
{
	Fixture f;
	auto dialogue = f.Make();
	EXPECT_FALSE(dialogue.RunText(false, k_HelpTextCount, 0));
	EXPECT_EQ(dialogue.GetCurrentText(), 0u);
	EXPECT_TRUE(dialogue.RunText(false, k_HelpTextCount - 1, 0));
}

TEST(DialogueText, WaitsForAClick)
{
	Fixture f;
	f.texts[10] = {.text = u"click me"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 1);
	EXPECT_TRUE(dialogue.IsWaitingForClick());
	f.turn += 1000;
	EXPECT_FALSE(dialogue.IsTextRead());
	// Shown a second: the click only ends the wait outside the cut scenes
	f.Click(dialogue, true, false);
	EXPECT_FALSE(dialogue.IsWaitingForClick());
	EXPECT_TRUE(dialogue.IsTextRead());
	EXPECT_EQ(f.interrupts, 0);
}

TEST(DialogueText, AClickTooSoonDoesNothing)
{
	Fixture f;
	f.texts[10] = {.text = u"click me"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 1);
	// Waiting for a click it must be shown a second: nine turns are not enough
	f.turn += 9;
	f.Click(dialogue, true, false);
	EXPECT_TRUE(dialogue.IsWaitingForClick());
	f.turn += 1;
	f.Click(dialogue, true, false);
	EXPECT_FALSE(dialogue.IsWaitingForClick());
}

TEST(DialogueText, TheKeyNeedsNoWait)
{
	Fixture f;
	f.texts[10] = {.text = u"click me"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 1);
	f.Click(dialogue, false, true);
	EXPECT_FALSE(dialogue.IsWaitingForClick());
	// The key cuts the text short
	EXPECT_EQ(f.interrupts, 1);
	EXPECT_EQ(f.narrationStops, 1);
	EXPECT_EQ(dialogue.GetEndTurn(), 0u);
}

TEST(DialogueText, NoClickTextsIgnoreClicks)
{
	Fixture f;
	f.texts[10] = {.text = u"one two three"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 2);
	f.turn += 10;
	f.Click(dialogue, true, true);
	EXPECT_EQ(f.interrupts, 0);
	EXPECT_EQ(dialogue.GetEndTurn(), 123u);
}

TEST(DialogueText, AClickInACutSceneCutsTheTextShort)
{
	Fixture f;
	f.wideScreen = true;
	f.texts[10] = {.text = u"one two three"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 0);
	// Half a second must pass first
	f.turn += 4;
	f.Click(dialogue, true, false);
	EXPECT_EQ(f.interrupts, 0);
	f.turn += 1;
	f.Click(dialogue, true, false);
	EXPECT_EQ(f.interrupts, 1);
	EXPECT_EQ(f.narrationStops, 1);
	EXPECT_TRUE(dialogue.IsTextRead());
	// The text stays on screen
	EXPECT_EQ(dialogue.GetCurrentText(), 10u);
}

TEST(DialogueText, AReadTextIgnoresClicks)
{
	Fixture f;
	f.wideScreen = true;
	f.texts[10] = {.text = u"one"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 0);
	f.turn += 100;
	f.Click(dialogue, true, false);
	EXPECT_EQ(f.interrupts, 0);
}

TEST(DialogueText, AdvisorTextsAreSaidByTheAdvisor)
{
	Fixture f;
	f.texts[4431] = {.narrator = k_NarratorGoodAdvisor, .text = u"Greetings.", .voice = k_AdvisorVoice};
	f.texts[4432] = {.narrator = k_NarratorEvilAdvisor, .text = u"Hello.", .voice = k_AdvisorVoice};
	auto dialogue = f.Make();
	dialogue.RunText(false, 4431, 0);
	dialogue.RunText(false, 4432, 0);
	ASSERT_EQ(f.advisorSaid.size(), 2u);
	EXPECT_EQ(f.advisorSaid[0], std::make_pair(VoiceRoute::GoodSpirit, 4431u));
	EXPECT_EQ(f.advisorSaid[1], std::make_pair(VoiceRoute::EvilSpirit, 4432u));
	EXPECT_TRUE(f.narrated.empty());
	// Read once the advisors stop talking, whatever the time
	f.advisorsTalking = true;
	EXPECT_FALSE(dialogue.IsTextRead());
	f.advisorsTalking = false;
	EXPECT_TRUE(dialogue.IsTextRead());
}

TEST(DialogueText, OtherVoicesAreNarration)
{
	Fixture f;
	// A villager's line, and an advisor's line in another bank
	f.texts[5187] = {.narrator = 5, .text = u"Help!", .voice = k_OtherVoice};
	f.texts[20] = {.narrator = k_NarratorGoodAdvisor, .text = u"Hi.", .voice = k_OtherVoice};
	auto dialogue = f.Make();
	dialogue.RunText(true, 5187, 0);
	dialogue.RunText(true, 20, 0);
	EXPECT_EQ(f.narrated, (std::vector<uint32_t> {5187, 20}));
	EXPECT_TRUE(f.advisorSaid.empty());
}

TEST(DialogueText, NarrationIsReadAShortWhileAfterItStops)
{
	Fixture f;
	f.texts[5187] = {.narrator = 5, .text = u"Help!", .voice = k_OtherVoice};
	auto dialogue = f.Make();
	dialogue.RunText(true, 5187, 0);
	f.nowMs = 30000;
	f.narrationSounding = true;
	EXPECT_FALSE(dialogue.IsTextRead());
	EXPECT_EQ(dialogue.GetEndMs(), 30450);
	f.narrationSounding = false;
	f.nowMs = 30449;
	EXPECT_FALSE(dialogue.IsTextRead());
	f.nowMs = 30450;
	EXPECT_TRUE(dialogue.IsTextRead());
}

TEST(DialogueText, WithoutAVoiceTheReadingTimeCounts)
{
	Fixture f;
	f.texts[10] = {.narrator = k_NarratorGoodAdvisor, .text = u"one two three"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 0);
	EXPECT_TRUE(f.advisorSaid.empty());
	EXPECT_TRUE(f.narrated.empty());
	f.advisorsTalking = true;
	f.turn = 124;
	EXPECT_TRUE(dialogue.IsTextRead());
}

TEST(DialogueText, ClearingTakesEveryTextAway)
{
	Fixture f;
	f.texts[10] = {.text = u"one"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 1);
	dialogue.ClearAllText();
	EXPECT_EQ(dialogue.GetCurrentText(), 0u);
	EXPECT_FALSE(dialogue.IsWaitingForClick());
	EXPECT_EQ(dialogue.GetEndTurn(), 0u);
	EXPECT_EQ(dialogue.GetEndMs(), 0);
	dialogue.RunText(false, 10, 0);
	dialogue.CloseDialogue();
	EXPECT_EQ(dialogue.GetCurrentText(), 0u);
	EXPECT_FALSE(dialogue.GetDisplay().IsBoxShown());
}

TEST(DialogueText, DrawGate)
{
	// TEXT_DRAW 1 draws only the important texts, 0 none and 2 all
	Fixture f;
	f.texts[10] = {.text = u"plain"};
	f.texts[11] = {.important = true, .text = u"important"};
	auto onlyImportant = f.Make();
	// Nothing shown yet is drawn
	EXPECT_TRUE(onlyImportant.GetDisplay().IsTextDrawn(1));
	onlyImportant.RunText(false, 10, 0);
	EXPECT_FALSE(onlyImportant.GetDisplay().IsTextDrawn(1));
	onlyImportant.RunText(false, 11, 0);
	EXPECT_TRUE(onlyImportant.GetDisplay().IsTextDrawn(1));

	auto never = f.Make();
	never.RunText(false, 11, 0);
	EXPECT_FALSE(never.GetDisplay().IsTextDrawn(0));

	auto always = f.Make();
	always.RunText(false, 10, 0);
	EXPECT_TRUE(always.GetDisplay().IsTextDrawn(2));
}
