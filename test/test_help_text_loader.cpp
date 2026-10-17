/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Common/HelpText.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "Gui/TextDatabase.h"
#include "Help/AdvisorVoices.h"
#include "Help/DialogueText.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using openblack::helptext::LanguageIndex;
using openblack::resources::HelpTextLoader;

namespace
{
/// A fake script in UTF-16 little endian with its byte order mark
std::vector<uint8_t> Utf16(const std::u16string& text)
{
	std::vector<uint8_t> bytes = {0xFF, 0xFE};
	for (const char16_t c : text)
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	return bytes;
}

/// A help texts' slot that only holds the text database
class FakeHelpTexts final: public openblack::ecs::systems::HelpTextSystemInterface
{
public:
	[[nodiscard]] bool IsStarted() const final { return false; }
	void SetTexts(const openblack::gui::TextDatabase* texts) final { _texts = texts; }
	[[nodiscard]] const openblack::gui::TextDatabase* GetTexts() const final { return _texts; }
	void Start(std::unique_ptr<openblack::help::DialogueText> /*dialogue*/) final {}
	void Shutdown() final {}
	[[nodiscard]] openblack::help::DialogueText* GetDialogue() final { return nullptr; }
	[[nodiscard]] const openblack::help::DialogueText* GetDialogue() const final { return nullptr; }
	[[nodiscard]] openblack::help::AdvisorVoices& GetVoices() final { return _voices; }
	[[nodiscard]] const openblack::help::AdvisorVoices& GetVoices() const final { return _voices; }
	void InitVoices(uint16_t /*bank*/) final {}
	bool RunText(bool /*singleLine*/, uint32_t /*text*/, int32_t /*withInteraction*/) final { return false; }
	[[nodiscard]] bool IsTextRead() const final { return false; }
	void ClearAllText() final {}
	void CloseDialogue() final {}
	void InterruptAdvisors() final {}
	[[nodiscard]] int32_t GetNarrator(uint32_t /*text*/) const final { return 0; }
	[[nodiscard]] const openblack::help::TextFrame* Layout(glm::ivec2 /*screen*/, int /*barPixels*/,
	                                                       const openblack::help::WidthFn& /*widthFn*/) const final
	{
		return nullptr;
	}

private:
	const openblack::gui::TextDatabase* _texts {nullptr};
	/// No sound, no clock
	openblack::help::AdvisorVoices _voices {openblack::help::AdvisorVoices::Audio {}};
};
} // namespace

TEST(HelpTextLoader, ParsesTheTextsOfAScript)
{
	const auto bytes = Utf16(u"HELP_TEXT_NARRATOR_GOOD_SPIRIT = 2\n"
	                         u"ADD_TEXT(0, HELP_TEXT_NARRATOR_GOOD_SPIRIT, \"HELP_TEXT_ONE\", \"Hello\")\n"
	                         u"ADD_TEXT(1, HELP_TEXT_NARRATOR_GOOD_SPIRIT, \"HELP_TEXT_TWO\", \"World\")\n");
	const auto texts = HelpTextLoader {}(HelpTextLoader::FromBufferTag {}, bytes);
	ASSERT_NE(texts, nullptr);
	ASSERT_EQ(texts->size(), 2u);
	EXPECT_EQ((*texts)[1].name, "HELP_TEXT_TWO");
	EXPECT_EQ((*texts)[1].text, u"World");
	EXPECT_EQ((*texts)[1].narrator, openblack::helptext::k_NarratorGoodSpirit);
}

TEST(HelpTextLoader, NoBytesIsAnEmptyDatabase)
{
	const auto texts = HelpTextLoader {}(HelpTextLoader::FromBufferTag {}, {});
	ASSERT_NE(texts, nullptr);
	EXPECT_TRUE(texts->empty());
}

TEST(HelpText, ReadsTheInterfacesTextDatabase)
{
	using openblack::Locator;
	if (Locator::helpTextSystem::has_value())
	{
		GTEST_SKIP() << "the help texts' slot is already taken";
	}
	openblack::gui::TextDatabase database;
	database.AddHelpScript(Utf16(u"HELP_TEXT_NARRATOR_GOOD_SPIRIT = 2\n"
	                             u"ADD_TEXT(0, HELP_TEXT_NARRATOR_NONE, \"HELP_TEXT_NONE\", \"Nothing\")\n"
	                             u"ADD_TEXT(1, HELP_TEXT_NARRATOR_GOOD_SPIRIT, \"HELP_TEXT_ONE\", \"Hello~there\\n\")\n"));
	Locator::helpTextSystem::emplace<FakeHelpTexts>().SetTexts(&database);

	EXPECT_EQ(openblack::helptext::Count(), 2u);
	EXPECT_EQ(openblack::helptext::Get(1), u"Hello\xF8FE"
	                                       u"there\n");
	const auto entry = openblack::helptext::GetEntry(1);
	EXPECT_EQ(entry.arg0, 1);
	EXPECT_EQ(entry.narrator, openblack::helptext::k_NarratorGoodSpirit);
	EXPECT_EQ(entry.name, "HELP_TEXT_ONE");
	// out of range, the first; an undeclared narrator as the help text cache gives it
	EXPECT_EQ(openblack::helptext::Get(2), u"Nothing");
	EXPECT_EQ(openblack::helptext::GetEntry(0).narrator, openblack::helptext::k_NarratorUndeclared);
	EXPECT_EQ(openblack::helptext::GetEntry(0).arg0, 0);

	Locator::helpTextSystem::reset();
}

TEST(HelpTextLanguage, TheFirstTwoCharactersInTheTable)
{
	const auto index = [](const std::string& text) { return LanguageIndex(std::vector<uint8_t>(text.begin(), text.end())); };
	EXPECT_EQ(index("uk"), 0u);
	EXPECT_EQ(index("es"), 5u);
	EXPECT_EQ(index("jp"), 6u);
	EXPECT_EQ(index("th"), 14u);
	EXPECT_EQ(index("FR\r\n"), 2u); // case insensitive, the rest ignored
	EXPECT_EQ(index("dex"), 3u);
}

TEST(HelpTextLanguage, NoMatchIsZero)
{
	const auto index = [](const std::vector<uint8_t>& bytes) { return LanguageIndex(bytes); };
	EXPECT_EQ(index({}), 0u);
	EXPECT_EQ(index({'z', 'z'}), 0u);
	EXPECT_EQ(index({'u'}), 0u);
	EXPECT_EQ(index({'u', 0, 's'}), 0u); // stops at a zero
}
