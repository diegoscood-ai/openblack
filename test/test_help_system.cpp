/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <PackFile.h>
#include <gtest/gtest.h>

#include "Common/HelpText.h"
#include "Help/HelpSystem.h"
#include "InfoConstants.h"

// Milestone A11 of dev\tmp_dis\audio\PLAN.md: HelpSystem without voices (runblack.exe W120 0x5C5550..0x5C6E00, the
// splitter 0x5CB0F0..0x5CBF00 and GScript 0x6F7C70..0x6F8280; voices.md §2.3-2.4). The installation tests need
// OPENBLACK_TEST_BW_ROOT; without it they skip.

using namespace openblack;
using namespace openblack::help;

namespace
{
std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

std::optional<std::filesystem::path> GameRoot()
{
	const char* root = std::getenv("OPENBLACK_TEST_BW_ROOT");
	if (root == nullptr || *root == '\0' || !std::filesystem::is_directory(root))
	{
		return std::nullopt;
	}
	return std::filesystem::path(root);
}

std::optional<std::filesystem::path> FindNoCase(const std::filesystem::path& root, std::string_view relative)
{
	auto current = root;
	for (const auto& part : std::filesystem::path(relative))
	{
		const auto wanted = Lower(part.string());
		std::optional<std::filesystem::path> found;
		std::error_code ec;
		for (const auto& entry : std::filesystem::directory_iterator(current, ec))
		{
			if (Lower(entry.path().filename().string()) == wanted)
			{
				found = entry.path();
				break;
			}
		}
		if (!found)
		{
			return std::nullopt;
		}
		current = *found;
	}
	return current;
}

/// A help system over a fixed text list and a clock the test moves
struct Fixture
{
	uint32_t turn {100};
	int32_t now {5000};
	bool bankLoaded {false};
	bool playing {false};
	bool advisors {false};
	bool wideScreen {false};
	std::vector<helptext::Entry> texts;
	std::vector<audio::TextVoice> voices;
	std::vector<std::pair<uint32_t, VoiceRoute>> said;
	int stops {0};
	std::unique_ptr<HelpSystem> help;

	Fixture()
	{
		// id 0: HELP_TEXT_NONE; 1: two words; 2: a spirit's text; 3: a villager's text
		texts = {{0, 0, "HELP_TEXT_NONE", u"Cadena de texto no v\u00E1lida"},
		         {1, 1, "HELP_TEXT_TWO", u"Dos palabras"},
		         {1, helptext::k_NarratorGoodSpirit, "HELP_TEXT_SPIRIT", u"Hola"},
		         {1, 5, "HELP_TEXT_MAN", u"Un hombre habla"}};
		voices = {{}, {}, {audio::SfxBank::HelpSprites, 7}, {audio::SfxBank::Villagers, 9}};
		HelpSystem::Queries queries;
		queries.textEntry = [this](uint32_t id) { return id < texts.size() ? texts[id] : texts[0]; };
		queries.textVoice = [this](uint32_t id) { return id < voices.size() ? voices[id] : audio::TextVoice {}; };
		queries.turn = [this]() { return turn; };
		queries.nowMs = [this]() { return now; };
		queries.voiceBankLoaded = [this](audio::SfxBank) { return bankLoaded; };
		queries.isPlaying = [this](audio::SfxBank, audio::VoiceOwner owner, uint32_t) {
			return playing && owner == audio::VoiceOwner::Narration;
		};
		queries.advisorsTalking = [this]() { return advisors; };
		queries.scriptWideScreen = [this]() { return wideScreen; };
		HelpSystem::Hooks hooks;
		hooks.sayVoice = [this](uint32_t id, VoiceRoute route, audio::TextVoice) { said.emplace_back(id, route); };
		hooks.stopVoicesOnClick = [this]() { ++stops; };
		// HelpSystemInfo of info.dat: readDefaultAdjustGTTime 8, readDefaultWordGTTime 5
		help = std::make_unique<HelpSystem>(HelpSystem::Info {8, 5}, std::move(queries), std::move(hooks));
	}
};
} // namespace

TEST(HelpSystem, CountWords)
{
	// fn_005CBEC0 over the splitter fn_005CB590
	EXPECT_EQ(CountWords(u""), 0u);
	EXPECT_EQ(CountWords(u"   "), 0u);
	EXPECT_EQ(CountWords(u"Cadena de texto no v\u00E1lida"), 5u);
	EXPECT_EQ(CountWords(u"  dos\tpalabras \r\n"), 2u);
	EXPECT_EQ(CountWords(u"una\n\notra"), 2u);
	EXPECT_EQ(CountWords(u"a\xF8FE"
	                     u"b"),
	          2u);
	// codes are not words: $m2 (fn_005CB2A0 M), \n written as two characters (N), $1 (a number), $C12x (C and one more)
	EXPECT_EQ(CountWords(u"Haz clic en la Criatura para que podamos acariciarla. $m2"), 9u);
	EXPECT_EQ(CountWords(u"uno\\ndos"), 2u);
	EXPECT_EQ(CountWords(u"$1 uno"), 1u);
	EXPECT_EQ(CountWords(u"$C12xhola"), 1u);
	EXPECT_EQ(CountWords(u"$C"), 0u);
	// not a code: only the '$' or '\' goes, the rest is a word
	EXPECT_EQ(CountWords(u"Descubiertos: $I Retos"), 3u);
	EXPECT_EQ(CountWords(u"Nombre: $s"), 2u);
	EXPECT_EQ(CountWords(u"v\u00E1lido.n\\Int\u00E9ntalo con otro"), 4u);
	// "$$" is a literal '$' that starts a word; a '$' ends the word before it
	EXPECT_EQ(CountWords(u"a$$b"), 2u);
	EXPECT_EQ(CountWords(u"$$"), 1u);
	// a word stops after 47 characters (0x5CB6E3) and the rest is another one
	EXPECT_EQ(CountWords(std::u16string(47, u'x')), 1u);
	EXPECT_EQ(CountWords(std::u16string(48, u'x')), 2u);
}

TEST(HelpSystem, ReadSpeedFactor)
{
	// fn_005C6CB0
	EXPECT_DOUBLE_EQ(ReadSpeedFactor(0.5f), 1.0);
	EXPECT_DOUBLE_EQ(ReadSpeedFactor(0.0f), 3.0);
	EXPECT_DOUBLE_EQ(ReadSpeedFactor(0.25f), 2.0);
	EXPECT_NEAR(ReadSpeedFactor(1.0f), 0.2, 1e-12);
	EXPECT_NEAR(ReadSpeedFactor(0.75f), 0.6, 1e-12);
}

TEST(HelpSystem, ReadingTimeWithoutVoice)
{
	Fixture f;
	// 2 words with READ_SPEED 0.5: (2 * 5 + 8) turns * 1 = 18 (fn_005C61B0)
	f.help->RunText(false, 1, 0);
	EXPECT_EQ(f.help->GetStartTurn(), 100u);
	EXPECT_EQ(f.help->GetEndTurn(), 118u);
	EXPECT_EQ(f.help->GetTexts()[0], 1u);
	f.turn = 118;
	EXPECT_FALSE(f.help->IsTextRead()); // +0x45DC < turn (0x5C64C3)
	f.turn = 119;
	EXPECT_TRUE(f.help->IsTextRead());

	// READ_SPEED 0: x3 = 54 turns; 1: x0.2, 18 * 100 * 0.001f * 0.2 = 0.36 s -> ftol(3.6) = 3 turns
	f.turn = 200;
	f.help->SetReadSpeed(0.0f);
	f.help->RunText(false, 1, 0);
	EXPECT_EQ(f.help->GetEndTurn(), 254u);
	f.help->SetReadSpeed(1.0f);
	f.help->RunText(false, 1, 0);
	EXPECT_EQ(f.help->GetEndTurn(), 203u);

	// 5 words: 33 turns
	f.help->SetReadSpeed(0.5f);
	f.help->RunText(false, 0, 0);
	EXPECT_EQ(f.help->GetEndTurn(), 233u);
	// out of range: text 0 (0x6F7CE8)
	f.help->RunText(false, 7000, 0);
	EXPECT_EQ(f.help->GetTexts()[0], 0u);
}

TEST(HelpSystem, QueueSingleLineAndClear)
{
	Fixture f;
	f.help->RunText(false, 1, 0);
	f.help->RunText(false, 3, 0);
	EXPECT_EQ(f.help->GetTexts()[0], 3u);
	EXPECT_EQ(f.help->GetTexts()[1], 1u);
	// singleLine clears first; the next text clears again because the flag stays (HelpText +0xB0)
	f.help->RunText(true, 2, 0);
	EXPECT_EQ(f.help->GetTexts()[0], 2u);
	EXPECT_EQ(f.help->GetTexts()[1], 0u);
	f.help->RunText(false, 1, 0);
	EXPECT_EQ(f.help->GetTexts()[0], 1u);
	EXPECT_EQ(f.help->GetTexts()[1], 0u);
	f.help->RunText(false, 3, 0);
	EXPECT_EQ(f.help->GetTexts()[1], 1u);
	// six slots: the oldest goes
	for (uint32_t i = 0; i < 6; ++i)
	{
		f.help->RunText(false, 1, 0);
	}
	EXPECT_EQ(std::count(f.help->GetTexts().begin(), f.help->GetTexts().end(), 1u), 6);

	// GAME_CLEAR_DIALOGUE: no current text, read at once
	f.help->ClearDialogue();
	EXPECT_EQ(f.help->GetTexts()[0], 0u);
	EXPECT_EQ(f.help->GetEndTurn(), 0u);
	EXPECT_TRUE(f.help->IsTextRead());
	f.help->RunText(false, 1, 1);
	f.help->CloseDialogue();
	EXPECT_FALSE(f.help->IsWaitingForClick());
	EXPECT_TRUE(f.help->IsTextRead());
}

TEST(HelpSystem, TempText)
{
	std::u16string shown;
	HelpSystem::Hooks hooks;
	hooks.showText = [&shown](const std::u16string& text, float, int32_t narrator) {
		shown = text;
		EXPECT_EQ(narrator, 1);
	};
	HelpSystem help({8, 5}, {}, std::move(hooks));
	help.TempText(false, u"dev text", 0);
	EXPECT_EQ(shown, u"*dev text");
	EXPECT_EQ(help.GetTexts()[0], 0u);
	EXPECT_EQ(help.GetHistoryCount(), 0); // no fn_005C5EE0
	EXPECT_EQ(help.GetEndTurn(), 18u);    // "*dev" "text"
}

TEST(HelpSystem, WithInteraction)
{
	Fixture f;
	f.help->RunText(false, 1, 1);
	EXPECT_TRUE(f.help->IsWaitingForClick());
	f.turn = 500;
	EXPECT_FALSE(f.help->IsTextRead()); // 0x5C64E0
	EXPECT_EQ(f.help->ProcessInterface(false), 1);
	EXPECT_TRUE(f.help->IsWaitingForClick());
	// a click counts after 1 s while it waits (fn_005C68C0): 10 turns of 100 ms
	f.turn = 100 + 9;
	f.help->RunText(false, 1, 1);
	f.help->ProcessInterface(true);
	EXPECT_TRUE(f.help->IsWaitingForClick());
	f.turn += 9;
	f.help->ProcessInterface(true);
	EXPECT_TRUE(f.help->IsWaitingForClick());
	f.turn += 1;
	EXPECT_EQ(f.help->ProcessInterface(true), 1);
	EXPECT_FALSE(f.help->IsWaitingForClick());
	EXPECT_FALSE(f.help->IsTextRead()); // its reading time still runs
	f.turn += 9;
	EXPECT_TRUE(f.help->IsTextRead());

	// withInteraction 2: a click does nothing
	f.help->RunText(false, 0, 2);
	f.turn += 20;
	EXPECT_EQ(f.help->ProcessInterface(true), 1);
	EXPECT_FALSE(f.help->IsTextRead());

	// with the script's wide screen a click after 0.5 s cuts the text
	f.wideScreen = true;
	f.help->RunText(false, 0, 0);
	f.turn += 4;
	EXPECT_EQ(f.help->ProcessInterface(true), 1);
	f.turn += 1;
	EXPECT_EQ(f.help->ProcessInterface(true), k_ClickTaken);
	EXPECT_EQ(f.stops, 0); // text 0: no voice to stop (0x5C6A84)
	EXPECT_TRUE(f.help->IsTextRead());
	f.help->RunText(false, 3, 0);
	f.turn += 5;
	EXPECT_EQ(f.help->ProcessInterface(true), k_ClickTaken);
	EXPECT_EQ(f.stops, 1);
}

TEST(HelpSystem, VoiceHooks)
{
	Fixture f;
	f.help->RunText(false, 1, 0);
	f.help->RunText(false, 2, 0);
	f.help->RunText(false, 3, 0);
	ASSERT_EQ(f.said.size(), 2u);
	EXPECT_EQ(f.said[0], std::make_pair(2u, VoiceRoute::GoodSpirit));
	EXPECT_EQ(f.said[1], std::make_pair(3u, VoiceRoute::Narration));
	EXPECT_EQ(RouteOf(3, {audio::SfxBank::HelpSprites, 0}), VoiceRoute::EvilSpirit);
	EXPECT_EQ(RouteOf(5, {audio::SfxBank::HelpSprites, 4}), VoiceRoute::Narration);
	EXPECT_EQ(RouteOf(2, {audio::SfxBank::Villagers, 4}), VoiceRoute::Narration);
	EXPECT_EQ(RouteOf(2, {}), VoiceRoute::None);

	// history (fn_005C5EE0 / fn_005C5F50): the last first
	EXPECT_EQ(f.help->GetHistoryCount(), 3);
	EXPECT_EQ(f.help->GetHistory(0)->textId, 3u);
	EXPECT_EQ(f.help->GetHistory(0)->narrator, 5);
	EXPECT_EQ(f.help->GetHistory(2)->textId, 1u);
	EXPECT_EQ(f.help->GetHistory(3), nullptr);
	for (int i = 0; i < 1100; ++i)
	{
		f.help->RunText(false, 1, 0);
	}
	EXPECT_EQ(f.help->GetHistoryCount(), 1024);
	EXPECT_EQ(f.help->GetHistory(1023)->textId, 1u);
}

TEST(HelpSystem, Reset)
{
	// HelpSystem::Reset 0x5C5580 (GScript::Reset 0x6EB340): ClearAllText and the history emptied (0x5C55EA, 0x5C55F0)
	Fixture f;
	f.help->RunText(true, 1, 1);
	f.help->RunText(false, 3, 0);
	ASSERT_EQ(f.help->GetHistoryCount(), 2);
	f.help->Reset();
	EXPECT_EQ(f.help->GetHistoryCount(), 0);
	EXPECT_EQ(f.help->GetHistory(0), nullptr);
	EXPECT_EQ(f.help->GetTexts()[0], 0u);
	EXPECT_EQ(f.help->GetTexts()[1], 0u);
	EXPECT_FALSE(f.help->IsWaitingForClick());
	EXPECT_TRUE(f.help->IsTextRead());
	// the single line flag went with HelpText::Reset (0x5CB053): the next text does not clear the one before
	f.help->RunText(false, 1, 0);
	f.help->RunText(false, 3, 0);
	EXPECT_EQ(f.help->GetTexts()[1], 1u);
	f.help->RunText(false, 2, 0);
	EXPECT_EQ(f.help->GetHistory(0)->textId, 2u);
	EXPECT_EQ(f.help->GetHistoryCount(), 3);
}

TEST(HelpSystem, ReadWithVoice)
{
	Fixture f;
	f.bankLoaded = true;
	// the narration (0x270F): +450 ms after it stops (0x5C640A)
	f.help->RunText(false, 3, 0);
	f.playing = true;
	EXPECT_FALSE(f.help->IsTextRead());
	f.playing = false;
	f.now += 449;
	EXPECT_FALSE(f.help->IsTextRead());
	f.now += 1;
	EXPECT_TRUE(f.help->IsTextRead());
	// the advisors (HelpSprites): read when neither talks
	f.help->RunText(false, 2, 0);
	f.advisors = true;
	EXPECT_FALSE(f.help->IsTextRead());
	f.advisors = false;
	EXPECT_TRUE(f.help->IsTextRead());
}

TEST(HelpSystem, InstalledGame)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// HelpSystemInfo in info.dat (voices.md §7.3): readDefaultAdjustGTTime 8, readDefaultWordGTTime 5
	const auto info = FindNoCase(*root, "Scripts/info.dat");
	ASSERT_TRUE(info);
	std::ifstream stream(*info, std::ios::binary);
	pack::PackFile file;
	ASSERT_EQ(file.ReadFile(stream), pack::PackResult::Success);
	const auto& block = file.GetBlock("Info");
	ASSERT_EQ(block.size(), sizeof(InfoConstants));
	auto constants = std::make_unique<InfoConstants>();
	std::memcpy(constants.get(), block.data(), sizeof(InfoConstants));
	EXPECT_EQ(constants->helpSystem.readDefaultAdjustGTTime, 8u);
	EXPECT_EQ(constants->helpSystem.readDefaultWordGTTime, 5u);
	EXPECT_EQ(constants->helpSystem.wideScreenTime, 2.0f);

	// the words of every text of InfoScript2.txt, 52190 (the same splitter ported to Python on its own,
	// dev\tmp_dis\audio\text_words.py)
	const auto script = FindNoCase(*root, "Scripts/InfoScript2.txt");
	ASSERT_TRUE(script);
	std::ifstream text(*script, std::ios::binary);
	const std::vector<uint8_t> bytes {std::istreambuf_iterator<char>(text), std::istreambuf_iterator<char>()};
	const auto entries = helptext::Parse(bytes);
	ASSERT_EQ(entries.size(), helptext::k_TextCount);
	uint64_t words = 0;
	for (const auto& entry : entries)
	{
		words += CountWords(entry.text);
	}
	EXPECT_EQ(words, 52190u);
	EXPECT_EQ(CountWords(entries[1715].text), 11u); // a real line feed ("\n" in the file)
	EXPECT_EQ(CountWords(entries[1153].text), 3u);  // "Descubiertos: $I Retos"
}
