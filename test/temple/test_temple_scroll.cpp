/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The temple's scrolls against a made up font: a room's text as the game breaks it into lines, and the scroll's texture
// of it

#include <cstring>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <3D/OrientedText.h>
#include <3D/TempleScroll.h>
#include <3D/TempleScrollPictures.h>
#include <3D/TempleScrolls.h>
#include <3D/TempleSigns.h>
#include <3D/TempleSounds.h>
#include <Audio/Device/Sound.h>
#include <Graphics/GameFont.h>
#include <Gui/GameFont.h>
#include <Gui/TextDatabase.h>
#include <Help/DialogueText.h>
#include <Help/StoryHistory.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

using namespace openblack;

namespace
{
template <typename T>
void Append(std::vector<uint8_t>& data, T value)
{
	const auto offset = data.size();
	data.resize(offset + sizeof(T));
	std::memcpy(data.data() + offset, &value, sizeof(T));
}

/// bgfx's renderer of nothing, which our fonts make their texture with as they load
class NoopRenderer
{
public:
	NoopRenderer()
	{
		// The font reports what it loads to the graphics log
		if (spdlog::get("graphics") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("graphics");
		}
		bgfx::renderFrame(); // single threaded
		bgfx::Init init {};
		init.type = bgfx::RendererType::Noop;
		_running = bgfx::init(init);
		EXPECT_TRUE(_running);
	}
	~NoopRenderer()
	{
		if (_running)
		{
			bgfx::shutdown();
		}
	}
	NoopRenderer(const NoopRenderer&) = delete;
	NoopRenderer& operator=(const NoopRenderer&) = delete;
	NoopRenderer(NoopRenderer&&) = delete;
	NoopRenderer& operator=(NoopRenderer&&) = delete;

private:
	bool _running {false};
};

/// A font in the format of data/j0.met and data/j0.fnt, 80 pixels high as j0 is: a solid A 16 pixels wide, its
/// metrics as wide, and a space as wide again. The renderer comes first and goes last, after the font's texture.
class TestFont: private NoopRenderer, public gui::GameFont
{
public:
	TestFont()
	    : gui::GameFont(Load())
	{
	}

private:
	static gui::GameFont Load()
	{
		constexpr uint32_t k_Height = 80;
		std::vector<uint8_t> met;
		std::vector<uint8_t> fnt;
		Append(met, k_Height);
		met.resize(met.size() + 0x100, 0);
		Append(met, uint32_t {2});
		struct Glyph
		{
			char16_t character;
			uint16_t width;
		};
		for (const auto& glyph : {Glyph {u'A', 16}, Glyph {u' ', 0}})
		{
			const auto offset = static_cast<uint32_t>(fnt.size());
			// Every pixel set: no clear pixels, then all of them
			const uint32_t pixels = glyph.width * k_Height;
			fnt.push_back(0);
			if (pixels > 0)
			{
				fnt.push_back(0xFF);
				Append(fnt, static_cast<uint16_t>(pixels));
			}
			Append(met, static_cast<uint16_t>(glyph.character));
			Append(met, glyph.width);
			Append(met, uint32_t {0});
			Append(met, 0.0f);
			Append(met, 16.0f);
			Append(met, 0.0f);
			Append(met, offset);
			Append(met, static_cast<uint32_t>(fnt.size() - offset));
		}
		auto loaded = std::make_shared<graphics::GameFont>();
		EXPECT_TRUE(loaded->Load(met, fnt, "test font"));
		auto font = gui::GameFont::From(std::move(loaded));
		EXPECT_TRUE(font.has_value());
		return std::move(*font);
	}
};

/// Each row of the parchment its own red
std::vector<uint8_t> Parchment()
{
	std::vector<uint8_t> parchment(256 * 256 * 3);
	for (size_t y = 0; y < 256; ++y)
	{
		for (size_t x = 0; x < 256; ++x)
		{
			parchment[((y * 256) + x) * 3] = static_cast<uint8_t>(y);
			parchment[(((y * 256) + x) * 3) + 1] = 0x80;
			parchment[(((y * 256) + x) * 3) + 2] = 0x40;
		}
	}
	return parchment;
}

uint16_t ParchmentAt(uint32_t row)
{
	return static_cast<uint16_t>(0xF000 | ((row & 0xF0) << 4) | 0x80 | 0x4);
}
} // namespace

TEST(TempleScrollText, EachTextIsALineThenABlankLine)
{
	const TestFont font;
	TempleScrollText text(font);
	text.Add(u"AA A");
	text.AddNewLine();
	text.Add(u"A");
	text.End();
	EXPECT_EQ(text.GetText(), u"AA A<N><N><N>A<N><N><E>");
}

TEST(TempleScrollText, LongTextBreaksAtTheLastSpaceThatFits)
{
	const TestFont font;
	// An A is 16 units of 80 across: 2 texels at a size of 10, 3.5 stretched. 230 texels take 65 and a bit.
	std::u16string words;
	for (int i = 0; i < 20; ++i)
	{
		words += u"AAAA ";
	}
	TempleScrollText text(font);
	text.Add(words);
	const auto& result = text.GetText();
	size_t start = 0;
	size_t lines = 0;
	while (true)
	{
		const auto end = result.find(u"<N>", start);
		if (end == std::u16string::npos)
		{
			break;
		}
		const auto line = std::u16string_view(result).substr(start, end - start);
		EXPECT_LT(font.GetWidth(line, 10.0f) * 1.75f, 230.0f) << lines;
		if (!line.empty())
		{
			EXPECT_NE(line.front(), u' ');
		}
		start = end + 3;
		++lines;
	}
	// Two lines of words and the blank line after them
	EXPECT_EQ(lines, 3);
	EXPECT_EQ(result.substr(0, result.find(u"<N>")).size() % 5, 4);
}

TEST(TempleScrollText, WordsItCantShowAreHidden)
{
	const TestFont font;
	TempleScrollText text(font);
	text.Add(u"A $x A");
	EXPECT_EQ(text.GetText(), u"A \xF8FE\xF8FE A<N><N>");
}

TEST(TempleScrollText, AGestureEndsTheTextAndGoesOnALineOfItsOwn)
{
	const TestFont font;
	TempleScrollText text(font);
	text.Add(u"A $g3 A");
	const std::u16string gesture {TempleScrollText::k_Gesture, static_cast<char16_t>(TempleScrollText::k_Hidden + 3)};
	EXPECT_EQ(text.GetText(), u"A <N>" + gesture + u"<N><N><N>");
}

TEST(TempleScrollText, AControlEndsTheTextWithItsKeyAndMouseInput)
{
	const TestFont font;
	TempleScrollText text(font);
	text.SetControlWords({u"()", TempleScrollText::ControlWords(u"CTRL + S", u"Left Mouse Button")});
	text.Add(u"Save $m1 now");
	const std::u16string control {TempleScrollText::k_Control, static_cast<char16_t>(TempleScrollText::k_Hidden + 1)};
	EXPECT_EQ(text.GetText(), u"Save (CTRL + S Left Mouse Button)<N>" + control + u"<N><N><N>");
}

TEST(TempleScrollText, AControlOutsideTheGamesControlsHasNoWords)
{
	const TestFont font;
	TempleScrollText text(font);
	text.SetControlWords({u"()"});
	text.Add(u"A $m40 A");
	const std::u16string control {TempleScrollText::k_Control, static_cast<char16_t>(TempleScrollText::k_Hidden + 40)};
	EXPECT_EQ(text.GetText(), u"A <N>" + control + u"<N><N><N>");
}

TEST(TempleScrollText, AControlsWordsAreItsKeyAndMouseInputInBrackets)
{
	EXPECT_EQ(TempleScrollText::ControlWords(u"F5", u""), u"(F5)");
	EXPECT_EQ(TempleScrollText::ControlWords(u"", u"Right Mouse Button"), u"(Right Mouse Button)");
	EXPECT_EQ(TempleScrollText::ControlWords(u"SHIFT + A", u"Wheel Up"), u"(SHIFT + A Wheel Up)");
	EXPECT_EQ(TempleScrollText::ControlWords(u"", u""), u"()");
}

TEST(TempleScrolls, TheLibrarysNavigationTipNamesItsControlsButton)
{
	const TestFont font;
	gui::TextDatabase texts;
	const std::u16string script = u"ADD_TEXT(1, NONE, \"HELP_TEXT_NONE\", \"None\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_LIBRARY_ROOM_02\", \"Nav\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_DYK_46\", \"Hold it.$m2\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_ACTIONS_RIGHTMOUSEBUTTON\", \"BDR\")\n";
	std::vector<uint8_t> bytes;
	for (const auto c : script)
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	texts.AddHelpScript(bytes);
	auto facts = TempleScrolls::Facts::Mock();
	facts.libraryHelp.at(0) = {2};
	// The third control, the right mouse button by default, with no key
	const std::u16string control {TempleScrollText::k_Control, static_cast<char16_t>(TempleScrollText::k_Hidden + 2)};
	EXPECT_EQ(TempleScrolls::Write(TempleScrolls::Content::LibraryControl, texts, font, facts),
	          u"Nav<N><N><N>Hold it.(BDR)<N>" + control + u"<N><N><N><N><E>");
}

TEST(TempleScrollPictures, AControlsPictureIsTwoLinesHighWithinTheScroll)
{
	using temple_scroll_pictures::Place;
	using temple_scroll_pictures::PlaceAt;
	// The original's own (oracle): a line 17 pixels high on the screen, the scroll from (300.25, 100.5) to (700.75,
	// 650.125), the picture put from 351.3
	EXPECT_EQ(PlaceAt(17.0f, 300.25f, 100.5f, 700.75f, 650.125f, 351.3f), (Place {.x = 517, .y = 327, .size = 34}));
	EXPECT_EQ(PlaceAt(12.0f, -20.6f, 40.4f, 610.2f, 500.9f, 233.7f), (Place {.x = 306, .y = 216, .size = 24}));
	EXPECT_EQ(PlaceAt(12.0f, -20.6f, 40.4f, 610.2f, 500.9f, 500.8f), (Place {.x = 306, .y = 484, .size = 24}));
	// Its top on the scroll's top, or its bottom on the scroll's bottom: not drawn
	EXPECT_FALSE(PlaceAt(12.0f, -20.6f, 40.4f, 610.2f, 500.9f, 64.4f).has_value());
	EXPECT_FALSE(PlaceAt(12.0f, -20.6f, 40.4f, 610.2f, 500.9f, 500.9f).has_value());
}

TEST(TempleScrollPictures, AControlsPictureIsPutByItsLineOnTheScreen)
{
	// The scroll's frame as it is, a texel a unit, and a camera that puts a point at its depth across and its height down
	// from the middle of an 800 by 600 screen
	const TextFrame frame;
	const temple_scroll_pictures::Project screen =
	    [](const glm::vec3& point) -> std::optional<temple_scroll_pictures::ScreenPoint> {
		const float x = point.z + 400.0f;
		const float y = point.y + 300.0f;
		return temple_scroll_pictures::ScreenPoint {
		    .x = x, .y = y, .onScreen = x >= 0.0f && y >= 0.0f && x < 800.0f && y < 600.0f};
	};
	// The scroll from (272, 172) to (528, 428); the line's top at (372, 222), the line below 10 lower: two lines high, a
	// line below its line's top
	const auto place = temple_scroll_pictures::ControlPicture(frame, 1.0f, 1.0f, 100.0f, 50.0f, screen);
	EXPECT_EQ(place, (temple_scroll_pictures::Place {.x = 410, .y = 238, .size = 20}));
	// Nothing nearer than the camera's near plane, nothing off the screen
	const temple_scroll_pictures::Project behind = [](const glm::vec3&) {
		return std::optional<temple_scroll_pictures::ScreenPoint>();
	};
	EXPECT_FALSE(temple_scroll_pictures::ControlPicture(frame, 1.0f, 1.0f, 100.0f, 50.0f, behind).has_value());
	EXPECT_FALSE(temple_scroll_pictures::ControlPicture(frame, 1.0f, 1.0f, 100.0f, 400.0f, screen).has_value());
}

TEST(TempleScrollTexture, TheLinesPassedOverGiveTheirPlaceToAControlsPicture)
{
	const TestFont font;
	const std::u16string control {TempleScrollText::k_Control, static_cast<char16_t>(TempleScrollText::k_Hidden + 2)};
	const std::u16string text = u"AA<N>" + control + u"<N><E>";
	std::vector<std::u16string> lines;
	std::vector<std::array<float, 3>> pictures;
	const auto picture = [&pictures](int32_t number, float lineX, float y) {
		pictures.push_back({static_cast<float>(number), lineX, y});
	};
	// The text line, centred: two A's 3.5 texels wide each; the control's line below it takes its place across
	TempleScrollTexture::LayOut(
	    text, 0, font, [&lines](std::u16string_view line, float, float) { lines.emplace_back(line); }, picture);
	ASSERT_EQ(pictures.size(), 1u);
	EXPECT_EQ(lines, (std::vector<std::u16string> {u"AA"}));
	EXPECT_FLOAT_EQ(pictures[0][0], 2.0f);
	EXPECT_FLOAT_EQ(pictures[0][1], (256.0f - 7.0f) * 0.5f);
	EXPECT_FLOAT_EQ(pictures[0][2], 10.0f);
	// Turned past the text line, the picture still takes its place
	pictures.clear();
	lines.clear();
	TempleScrollTexture::LayOut(
	    text, 10, font, [&lines](std::u16string_view line, float, float) { lines.emplace_back(line); }, picture);
	ASSERT_EQ(pictures.size(), 1u);
	EXPECT_TRUE(lines.empty());
	EXPECT_FLOAT_EQ(pictures[0][1], (256.0f - 7.0f) * 0.5f);
	EXPECT_FLOAT_EQ(pictures[0][2], 0.0f);
}

TEST(TempleScrollText, ValuesTakeThePlaceOfTheirMarks)
{
	EXPECT_EQ(TempleScrollText::WithNumber(u"Deaths: $I", 179), u"Deaths: 179");
	EXPECT_EQ(TempleScrollText::WithNumber(u"Health: $I%", -5), u"Health: -5%");
	EXPECT_EQ(TempleScrollText::WithString(u"I'm $s greedy.", u"slightly"), u"I'm slightly greedy.");
}

TEST(TempleScrollTexture, ClampsToTheText)
{
	EXPECT_EQ(TempleScrollTexture::ClampPosition(-4, 500), 0);
	EXPECT_EQ(TempleScrollTexture::ClampPosition(100, 500), 100);
	// 24 lines of 10 texels show at once
	EXPECT_EQ(TempleScrollTexture::ClampPosition(400, 500), 260);
	EXPECT_EQ(TempleScrollTexture::ClampPosition(50, 100), 0);
}

TEST(TempleScrolls, TurnByTheMouseMoveEachFrameWithinTheText)
{
	// Up the screen the text comes up, a texel a pixel; down it goes back
	EXPECT_EQ(TempleScrolls::Dragged(100, 500, 300, 290), 110);
	EXPECT_EQ(TempleScrolls::Dragged(100, 500, 300, 310), 90);
	// Within its text: 24 lines of 10 texels show at once
	EXPECT_EQ(TempleScrolls::Dragged(250, 500, 300, 270), 260);
	EXPECT_EQ(TempleScrolls::Dragged(5, 500, 0, 40), 0);
	EXPECT_EQ(TempleScrolls::Dragged(30, 100, 0, -20), 0);
}

TEST(TempleScrollTexture, TheParchmentRollsWithThePosition)
{
	const TestFont font;
	std::vector<uint16_t> texels(256 * 256);
	EXPECT_EQ(TempleScrollTexture::Draw(texels, Parchment(), u"<E>", 0, font), 0);
	EXPECT_EQ(texels[0], ParchmentAt(0));
	EXPECT_EQ(texels[255 * 256], ParchmentAt(255));

	TempleScrollTexture::Draw(texels, Parchment(), u"<E>", 0x123, font);
	EXPECT_EQ(texels[0], ParchmentAt(0x23));
	EXPECT_EQ(texels[(0xFF - 0x23 + 1) * 256], ParchmentAt(0));
}

TEST(TempleScrollTexture, LinesAreCentredInYellowOverTheirShadows)
{
	const TestFont font;
	std::vector<uint16_t> texels(256 * 256);
	// The text is its lines' height: two lines here
	EXPECT_EQ(TempleScrollTexture::Draw(texels, Parchment(), u"A<N><N><E>", 0, font), 20);
	// The A is 3.5 texels across from 126.25: texels 126 to 128, 10 high. Where it covers the texel wholly, the
	// texel is the yellow's ramp at full coverage.
	EXPECT_EQ(texels[(5 * 256) + 127], 0xFEE0);
	// Its shadow shows below it
	EXPECT_EQ(texels[(10 * 256) + 128] & 0x0FFF, 0x0000);
	// Away from it the parchment is untouched
	EXPECT_EQ(texels[(5 * 256) + 100], ParchmentAt(5));
}

TEST(TempleScrollTexture, LinesBeforeThePositionArePassedOver)
{
	const TestFont font;
	std::vector<uint16_t> texels(256 * 256);
	// Turned on by a line and 5 texels, the second line is drawn 5 texels above the top
	EXPECT_EQ(TempleScrollTexture::Draw(texels, Parchment(), u"<N>A<N><E>", 15, font), 20);
	EXPECT_EQ(texels[(2 * 256) + 127], 0xFEE0);
	EXPECT_EQ(texels[(6 * 256) + 127] & 0x0FFF, ParchmentAt(6 + 15) & 0x0FFF);
}

TEST(TempleScrollTexture, ALineTooLongForItsBufferEndsTheText)
{
	const TestFont font;
	std::vector<uint16_t> texels(256 * 256);
	const std::u16string longLine(64, u' ');
	EXPECT_EQ(TempleScrollTexture::Draw(texels, Parchment(), u"A<N>" + longLine + u"<N>A<N><E>", 0, font), 10);
}

TEST(OrientedText, StandsOffThePageAlongTheAxisLeft)
{
	// The scrolls run along z and down y, off x; the main room's signs along y and down z, off x; the creature room's
	// along x and down z, off y
	EXPECT_EQ(OrientedTextDepthAxis(2, 1), 0);
	EXPECT_EQ(OrientedTextDepthAxis(1, 2), 0);
	EXPECT_EQ(OrientedTextDepthAxis(0, 2), 1);
}

TEST(OrientedText, EachGlyphIsAQuadDownItsLetters)
{
	const TestFont font;
	std::vector<OrientedTextVertex> vertices;
	// Two As and a space, 10 high: an A starts where the pen is, 16 units and one of 80 across, its height down
	AppendOrientedText(vertices, font, TextFrame {}, 0, 1, u"A A", glm::vec3(1.0f, 2.0f, 3.0f), 10.0f, 1.0f,
	                   {255, 255, 0, 255});
	ASSERT_EQ(vertices.size(), 12);
	EXPECT_FLOAT_EQ(vertices[0].position.x, 1.0f);
	EXPECT_FLOAT_EQ(vertices[0].position.y, 2.0f);
	EXPECT_FLOAT_EQ(vertices[0].position.z, 3.0f);
	EXPECT_FLOAT_EQ(vertices[1].position.y, 12.0f);
	EXPECT_FLOAT_EQ(vertices[2].position.x, 1.0f + (17.0f * 10.0f / 80.0f));
	// The second A comes after the first's 16 units and the space's 16
	EXPECT_FLOAT_EQ(vertices[6].position.x, 1.0f + (32.0f * 10.0f / 80.0f));
	EXPECT_EQ(vertices[0].colour, 0xFF00FFFFu);
}

TEST(TempleSigns, TheLabelsHaveTheOriginalsSizeAndPulse)
{
	EXPECT_FLOAT_EQ(TempleSigns::k_LabelSize, 2.1f);
	EXPECT_FLOAT_EQ(TempleSigns::k_LabelStretch, 0.8f);
	// The original's greys, at its single precision, through a round of 1024 milliseconds
	EXPECT_EQ(TempleSigns::HighlightGrey(0), 230);
	EXPECT_EQ(TempleSigns::HighlightGrey(128), 246);
	EXPECT_EQ(TempleSigns::HighlightGrey(256), 254);
	EXPECT_EQ(TempleSigns::HighlightGrey(512), 230);
	EXPECT_EQ(TempleSigns::HighlightGrey(768), 206);
	EXPECT_EQ(TempleSigns::HighlightGrey(1023), 229);
	EXPECT_EQ(TempleSigns::HighlightGrey(1024 + 128), 246);
}

TEST(TempleSigns, TheMainRoomLightsTheSignOfTheDoorUnderTheCursor)
{
	EXPECT_EQ(TempleSigns::MainRoomSignOfDoor(std::nullopt), std::nullopt);
	EXPECT_EQ(TempleSigns::MainRoomSignOfDoor(0), 0);
	EXPECT_EQ(TempleSigns::MainRoomSignOfDoor(5), 5);
	// The challenge room's door, past the wall of scrolls
	EXPECT_EQ(TempleSigns::MainRoomSignOfDoor(7), 6);
}

TEST(TempleSigns, TheLibraryLabelsItsSevenScrollsAsTheOriginalLaysThemOut)
{
	const auto library = TempleSigns::LayoutOf(TempleRoom::Credits);
	ASSERT_TRUE(library.has_value());
	// Smaller than the main room's, unstretched, along the signs' x axis
	EXPECT_FLOAT_EQ(library->size, 0.8f);
	EXPECT_FLOAT_EQ(library->stretch, 1.0f);
	EXPECT_TRUE(library->alongX);
	// The signs in the order the game lists their names, each with its text
	const std::vector<std::pair<std::string_view, std::string_view>> expected {
	    {"SIGN CONTROL", "HELP_TEXT_TEMPLE_SCROLLS_02"}, {"SIGN CREATURE", "HELP_TEXT_TEMPLE_SCROLLS_03"},
	    {"SIGN CREDITS", "HELP_TEXT_TEMPLE_SCROLLS_01"}, {"SIGN DIDYOUKNOW", "HELP_TEXT_TEMPLE_SCROLLS_06"},
	    {"SIGN HISTORY", "HELP_TEXT_TEMPLE_SCROLLS_07"}, {"SIGN MIRACLES", "HELP_TEXT_TEMPLE_SCROLLS_05"},
	    {"SIGN VILLAGE", "HELP_TEXT_TEMPLE_SCROLLS_04"},
	};
	EXPECT_EQ(library->signs, expected);
	// The main room's run against the signs' y axis, at a size and stretch of one
	const auto main = TempleSigns::LayoutOf(TempleRoom::Main);
	ASSERT_TRUE(main.has_value());
	EXPECT_FLOAT_EQ(main->size, 1.0f);
	EXPECT_FLOAT_EQ(main->stretch, 1.0f);
	EXPECT_FALSE(main->alongX);
	EXPECT_FALSE(TempleSigns::LayoutOf(TempleRoom::Options).has_value());
}

TEST(TempleScrolls, TheCreditsListThePeopleWhoMadeTheGameInTheOriginalsOrder)
{
	const TestFont font;
	const gui::TextDatabase texts;
	const auto facts = TempleScrolls::Facts::Mock();
	const auto credits = TempleScrolls::Write(TempleScrolls::Content::LibraryStaff, texts, font, facts);
	ASSERT_TRUE(credits.has_value());
	// The original's 36 names, each a line and a blank line, with no title
	constexpr std::array<std::u16string_view, 36> k_Names {
	    u"Aaron Ludlow",    u"Alex Evans",          u"Andy Bass",         u"Andy Robson",      u"Catherine Tutton",
	    u"Cathy Campos",    u"Christian Bravery",   u"Claire Hedley",     u"Daniel Deptford",  u"Eric Bailey",
	    u"Georg Backer",    u"Giles Jermy",         u"James Leach",       u"Jamie Durrant",    u"Janice Nussey",
	    u"Jason Hutchens",  u"Jean-Claude Cottier", u"Jeremy Chatelaine", u"Joe Borthwick",    u"Jonty Barnes",
	    u"Ken Malcolm",     u"Mark Healey",         u"Mark Webley",       u"Nathan Smethurst", u"Oliver Purkiss",
	    u"Paul McLaughlin", u"Paul Nettleton",      u"Pete Hawley",       u"Peter Molyneux",   u"Richard Evans",
	    u"Russell Shaw",    u"Scawen Roberts",      u"Steve Jackson",     u"Steve Lawrie",     u"Thomas Barnet-Lamb",
	    u"Tim Rance",
	};
	std::u16string expected;
	for (const auto name : k_Names)
	{
		expected += name;
		expected += u"<N><N>";
	}
	expected += u"<E>";
	EXPECT_EQ(*credits, expected);
}

TEST(TempleScrolls, TheCreaturesMindListsItsDesires)
{
	const TestFont font;
	gui::TextDatabase texts;
	const std::u16string script = u"ADD_TEXT(1, NONE, \"HELP_TEXT_ROOM_LIKES_SCROLL_TITLE\", \"Mind\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_ROOM_PERSONALITY_GREEDY\", \"I'm $s greedy.\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_CREATURE_ATTITUDE_07\", \"slightly\")\n";
	std::vector<uint8_t> bytes;
	for (const auto c : script)
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	texts.AddScript(bytes);
	auto facts = TempleScrolls::Facts::Mock();
	const auto mind = TempleScrolls::Write(TempleScrolls::Content::CreatureMind, texts, font, facts);
	ASSERT_TRUE(mind.has_value());
	const std::u16string_view expected = u"Mind<N><N><N>I'm slightly greedy.<N><N>";
	EXPECT_EQ(mind->substr(0, expected.size()), expected);

	// Without the player's creature the room leaves the scroll unwritten
	facts.creature.reset();
	EXPECT_FALSE(TempleScrolls::Write(TempleScrolls::Content::CreatureMind, texts, font, facts).has_value());
}

TEST(TempleScrolls, TheAlignmentTextIsPickedAsTheGamesOtherAlignmentReadersPickIt)
{
	const TestFont font;
	gui::TextDatabase texts;
	const std::u16string script = u"ADD_TEXT(1, NONE, \"HELP_TEXT_ROOM_CREATURE_SCROLL_ALIGNMENT\", \"Alignment $s\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_ALIGNMENT_06\", \"good\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_ALIGNMENT_07\", \"angelic\")\n";
	std::vector<uint8_t> bytes;
	for (const auto c : script)
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	texts.AddScript(bytes);
	auto facts = TempleScrolls::Facts::Mock();
	ASSERT_TRUE(facts.creature.has_value());
	// A hair under the edge of the top text: (a + 1) / 2 times the game's factor, the float just below 7, stays below
	// 6 here (the original's answer is 5, from the exe oracle), where a factor of 7 would reach it
	facts.creature->alignment = 0x1.6db6dcp-1f;
	const auto scroll = TempleScrolls::Write(TempleScrolls::Content::CreatureAttributes, texts, font, facts);
	ASSERT_TRUE(scroll.has_value());
	EXPECT_NE(scroll->find(u"Alignment good"), std::u16string::npos);
	EXPECT_EQ(scroll->find(u"Alignment angelic"), std::u16string::npos);
	// The most good alignment reaches the top text
	facts.creature->alignment = 1.0f;
	const auto top = TempleScrolls::Write(TempleScrolls::Content::CreatureAttributes, texts, font, facts);
	ASSERT_TRUE(top.has_value());
	EXPECT_NE(top->find(u"Alignment angelic"), std::u16string::npos);
}

TEST(TempleScrolls, TheMindScrollAsksForItsGodsDesireAndOnlyEndsALineForIt)
{
	const TestFont font;
	gui::TextDatabase texts;
	const std::u16string script =
	    u"ADD_TEXT(1, NONE, \"HELP_TEXT_ROOM_PERSONALITY_HOW_MUCH_ATTENTION_PLAYER_HAS_GIVEN_ME\", \"Attention.\") "
	    u"ADD_TEXT(1, NONE, \"HELP_TEXT_ROOM_PERSONALITY_PERCEIVED_PLAYER_DESIRE\", \"You want\") "
	    u"ADD_TEXT(1, NONE, \"HELP_TEXT_ROOM_PERSONALITY_BELIEFS_ABOUT_CREATURES\", \"Creatures.\") ";
	std::vector<uint8_t> bytes;
	for (const auto c : script)
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	texts.AddScript(bytes);
	auto facts = TempleScrolls::Facts::Mock();
	const auto write = [&] { return TempleScrolls::Write(TempleScrolls::Content::CreatureMind, texts, font, facts); };

	// Without a desire it thinks its god wants, the attention's line, then two line ends before the creatures it knows
	const auto unset = write();
	ASSERT_TRUE(unset.has_value());
	EXPECT_NE(unset->find(u"Attention.<N><N><N><N>Creatures."), std::u16string::npos);
	int asked = 0;
	bool wants = false;
	facts.creature->takeGodsDesire = [&] {
		++asked;
		return wants;
	};
	EXPECT_EQ(write(), unset);
	EXPECT_EQ(asked, 1);

	// With one, a line's end more, and never the desire's text: each writing asks once, which forgets it
	wants = true;
	const auto known = write();
	ASSERT_TRUE(known.has_value());
	EXPECT_EQ(asked, 2);
	EXPECT_NE(known->find(u"Attention.<N><N><N><N><N>Creatures."), std::u16string::npos);
	EXPECT_EQ(known->find(u"You want"), std::u16string::npos);
	EXPECT_EQ(known->size(), unset->size() + 3);
}

TEST(TempleScrolls, TheMindScrollsAttentionIsOneOfFiveWords)
{
	const TestFont font;
	gui::TextDatabase texts;
	const std::u16string script =
	    u"ADD_TEXT(1, NONE, \"HELP_TEXT_ROOM_PERSONALITY_HOW_MUCH_ATTENTION_PLAYER_HAS_GIVEN_ME\", \"Attention\") "
	    u"ADD_TEXT(1, NONE, \"HELP_TEXT_HOW_MUCH_01\", \"none\") "
	    u"ADD_TEXT(1, NONE, \"HELP_TEXT_HOW_MUCH_04\", \"a lot\") "
	    u"ADD_TEXT(1, NONE, \"HELP_TEXT_HOW_MUCH_05\", \"all\") ";
	std::vector<uint8_t> bytes;
	for (const auto c : script)
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	texts.AddScript(bytes);
	auto facts = TempleScrolls::Facts::Mock();
	const auto write = [&] { return TempleScrolls::Write(TempleScrolls::Content::CreatureMind, texts, font, facts); };

	const auto least = write();
	ASSERT_TRUE(least.has_value());
	EXPECT_NE(least->find(u"none"), std::u16string::npos);
	facts.creature->attention = 3;
	const auto fourth = write();
	ASSERT_TRUE(fourth.has_value());
	EXPECT_NE(fourth->find(u"a lot"), std::u16string::npos);
	EXPECT_EQ(fourth->find(u"all"), std::u16string::npos);
	EXPECT_EQ(fourth->find(u"none"), std::u16string::npos);
}

TEST(TempleScrolls, SqueakAsTheyStartToTurnAndAgainAfterStopping)
{
	bool squeaked = false;
	// The first turn squeaks, and turning on doesn't
	EXPECT_TRUE(TempleScrolls::Squeaks(squeaked, 0, 4));
	EXPECT_TRUE(squeaked);
	EXPECT_FALSE(TempleScrolls::Squeaks(squeaked, 4, 9));
	EXPECT_FALSE(TempleScrolls::Squeaks(squeaked, 9, 2));
	// Held at an end of its text the scroll stops, and squeaks again as it turns back
	EXPECT_FALSE(TempleScrolls::Squeaks(squeaked, 2, 2));
	EXPECT_FALSE(squeaked);
	EXPECT_TRUE(TempleScrolls::Squeaks(squeaked, 2, 7));
}

TEST(TempleSounds, TheScrollsSqueakOneOfSixByTheClock)
{
	const std::array squeaks {
	    audio::SoundId::G_ScrollSqueak_01, audio::SoundId::G_ScrollSqueak_02, audio::SoundId::G_ScrollSqueak_03,
	    audio::SoundId::G_ScrollSqueak_04, audio::SoundId::G_ScrollSqueak_05, audio::SoundId::G_ScrollSqueak_06,
	};
	for (const uint32_t tick : {0u, 1u, 5u, 6u, 1001u, 0xFFFFFFFFu})
	{
		const auto squeak = temple_sounds::ScrollSqueak(tick);
		EXPECT_EQ(squeak.sound, static_cast<entt::id_type>(squeaks.at(tick % 6))) << tick;
		EXPECT_FALSE(squeak.is3D);
		EXPECT_FALSE(squeak.track);
		EXPECT_EQ(squeak.owner, audio::Owner::None());
		EXPECT_EQ(squeak.loops, 0);
		EXPECT_EQ(squeak.mode, 2);
	}
}

TEST(TempleSounds, TheCameraWooshesOneOfFourByTheClock)
{
	const std::array wooshes {
	    audio::SoundId::G_Woosh_01,
	    audio::SoundId::G_Woosh_02,
	    audio::SoundId::G_Woosh_03,
	    audio::SoundId::G_Woosh_04,
	};
	for (const uint32_t tick : {0u, 1u, 3u, 4u, 1002u, 0xFFFFFFFFu})
	{
		const auto woosh = temple_sounds::Woosh(tick);
		EXPECT_EQ(woosh.sound, static_cast<entt::id_type>(wooshes.at(tick & 3u))) << tick;
		EXPECT_FALSE(woosh.is3D);
		EXPECT_FALSE(woosh.track);
		EXPECT_EQ(woosh.owner, audio::Owner::None());
		EXPECT_EQ(woosh.loops, 0);
		EXPECT_EQ(woosh.mode, 3);
	}
}

TEST(TempleScrolls, WithoutACreatureTheCreatureRoomWritesOnlyItsTitles)
{
	const TestFont font;
	gui::TextDatabase texts;
	const std::u16string script = u"ADD_TEXT(1, NONE, \"HELP_TEXT_ROOM_CREATURE_SCROLL_TITLE\", \"A\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_ROOM_MAGIC_SCROLL_TITLE\", \"AA\")\n";
	std::vector<uint8_t> bytes;
	for (const auto c : script)
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	texts.AddScript(bytes);
	auto facts = TempleScrolls::Facts::Mock();
	facts.creature.reset();

	// The attributes scroll keeps its title, then it has only blank lines in the place of its facts
	std::u16string attributes = u"A<N><N>";
	for (int i = 0; i < 23; ++i)
	{
		attributes += u"<N>";
	}
	attributes += u"<E>";
	EXPECT_EQ(TempleScrolls::Write(TempleScrolls::Content::CreatureAttributes, texts, font, facts), attributes);

	// The actions scroll and the mind scroll stay as the bare parchment
	EXPECT_FALSE(TempleScrolls::Write(TempleScrolls::Content::CreatureActions, texts, font, facts).has_value());
	EXPECT_FALSE(TempleScrolls::Write(TempleScrolls::Content::CreatureMind, texts, font, facts).has_value());

	// The magic scroll keeps only its title
	EXPECT_EQ(TempleScrolls::Write(TempleScrolls::Content::CreatureMiracles, texts, font, facts),
	          std::u16string(u"AA<N><N><N><E>"));
}

TEST(TempleScrolls, TheTextNumberedZeroWritesNothingAndOnePastTheTextsIsTheFirstText)
{
	const TestFont font;
	gui::TextDatabase texts;
	const std::u16string script = u"ADD_TEXT(1, NONE, \"HELP_TEXT_NONE\", \"None\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_ROOM_CHALLENGE_TITLE\", \"C\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_FIRST\", \"First\")\n";
	std::vector<uint8_t> bytes;
	for (const auto c : script)
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	texts.AddHelpScript(bytes);
	auto facts = TempleScrolls::Facts::Mock();
	facts.challenges = {0, 2, 99};
	const auto challenges = TempleScrolls::Write(TempleScrolls::Content::Challenge, texts, font, facts);
	ASSERT_TRUE(challenges.has_value());
	// The titles after the counts: nothing for 0, then the text, then the first text for a number past them
	const auto titles = challenges->substr(challenges->rfind(u"<N><N>First"));
	EXPECT_EQ(titles, u"<N><N>First<N><N>None<N><N><E>");

	// A miracle with no text writes neither it nor how much is learnt
	ASSERT_TRUE(facts.creature.has_value());
	facts.creature->miracles = {{.text = 0, .percent = 10}, {.text = 2, .percent = 20}};
	const auto miracles = TempleScrolls::Write(TempleScrolls::Content::CreatureMiracles, texts, font, facts);
	ASSERT_TRUE(miracles.has_value());
	EXPECT_EQ(miracles->find(u"10%"), std::u16string::npos);
	EXPECT_NE(miracles->find(u"First<N><N>20%"), std::u16string::npos);
}

TEST(TempleScrolls, WithoutSavedGamesTheSaveRoomWritesItsTitleTimeAndCounts)
{
	const TestFont font;
	gui::TextDatabase texts;
	const std::u16string script = u"ADD_TEXT(1, NONE, \"HELP_TEXT_ROOM_SAVEGAME_TITLE\", \"T\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_DIALOG_ADDITION_136\", \"%.2d:%.2d:%.2d\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_DIALOG_ADDITION_119\", \"S%i\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_DIALOG_ADDITION_84\", \"L%i\")\n";
	std::vector<uint8_t> bytes;
	for (const auto c : script)
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	texts.AddScript(bytes);
	// No saved games and no saves or loads: the title, the time, the counts and the end
	auto facts = TempleScrolls::Facts::Mock();
	EXPECT_TRUE(facts.savedGames.empty());
	facts.timePlayed = std::chrono::seconds(3725);
	EXPECT_EQ(TempleScrolls::Write(TempleScrolls::Content::SaveGame, texts, font, facts),
	          std::u16string(u"T<N><N><N>01:02:05<N><N>S0<N><N>L0<N><N><N><E>"));
	// The hours are not wrapped: a hundred hours show three digits
	facts.timePlayed = std::chrono::seconds(360000);
	EXPECT_EQ(TempleScrolls::Write(TempleScrolls::Content::SaveGame, texts, font, facts),
	          std::u16string(u"T<N><N><N>100:00:00<N><N>S0<N><N>L0<N><N><N><E>"));
}

TEST(TempleScrolls, TheStoryHistoryShowsFiveLinesFromItsFirstLine)
{
	const TestFont font;
	gui::TextDatabase texts;
	const std::u16string script = u"ADD_TEXT(1, NONE, \"HELP_TEXT_NONE\", \"None\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_LIBRARY_ROOM_07\", \"Story\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_DIALOG_ADDITION_06\", \"%d ago.\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_DIALOG_ADDITION_131\", \"Good:\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_DIALOG_ADDITION_135\", \"Said:\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_FIRST\", \"First\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_SECOND\", \"Second\")\n";
	std::vector<uint8_t> bytes;
	for (const auto c : script)
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	texts.AddHelpScript(bytes);
	// The history as the dialogue keeps it, the newest line last
	const std::vector<help::HistoryEntry> lines {
	    {.textId = 5, .withInteraction = 0, .number = 0.0f, .narrator = 1},
	    {.textId = 6, .withInteraction = 0, .number = 0.0f, .narrator = 2},
	};
	auto facts = TempleScrolls::Facts::Mock();
	facts.history = help::StoryHistoryView {
	    .back = [&lines](int32_t linesBack) -> const help::HistoryEntry* {
		    const auto count = static_cast<int32_t>(lines.size());
		    return linesBack >= 0 && linesBack < count ? &lines[static_cast<size_t>(count - 1 - linesBack)] : nullptr;
	    },
	    .count = static_cast<int32_t>(lines.size()),
	};

	// The newest line first, with no count; the line before it tells how many lines ago it was said
	const auto newest = TempleScrolls::Write(TempleScrolls::Content::LibraryHistory, texts, font, facts);
	ASSERT_TRUE(newest.has_value());
	const std::u16string_view expected = u"Story<N><N><N>"
	                                     u"Good:<N><N><N>Second<N><N><N><N>"
	                                     u"1 ago.<N><N>Said:<N><N><N>First<N><N><N><N><E>";
	EXPECT_EQ(*newest, expected);

	// From the line before the newest line, the newest line is not shown
	const auto older = TempleScrolls::Write(TempleScrolls::Content::LibraryHistory, texts, font, facts, 1);
	ASSERT_TRUE(older.has_value());
	EXPECT_EQ(older->find(u"Second"), std::u16string::npos);
	EXPECT_NE(older->find(u"1 ago.<N><N>Said:"), std::u16string::npos);

	// Without a history only the title is written
	facts.history = {};
	EXPECT_EQ(*TempleScrolls::Write(TempleScrolls::Content::LibraryHistory, texts, font, facts), u"Story<N><N><N><E>");
}

TEST(TempleScrolls, TheLibrarysHelpScrollsListTheTipsRead)
{
	const TestFont font;
	gui::TextDatabase texts;
	const std::u16string script = u"ADD_TEXT(1, NONE, \"HELP_TEXT_NONE\", \"None\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_LIBRARY_ROOM_03\", \"Creature\")\n"
	                              u"ADD_TEXT(1, NONE, \"HELP_TEXT_DYK_01\", \"Tip\")\n";
	std::vector<uint8_t> bytes;
	for (const auto c : script)
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	texts.AddHelpScript(bytes);
	auto facts = TempleScrolls::Facts::Mock();
	facts.libraryHelp.at(1) = {2};
	const auto creature = TempleScrolls::Write(TempleScrolls::Content::LibraryCreature, texts, font, facts);
	ASSERT_TRUE(creature.has_value());
	EXPECT_EQ(*creature, u"Creature<N><N><N>Tip<N><N><N><E>");
}
