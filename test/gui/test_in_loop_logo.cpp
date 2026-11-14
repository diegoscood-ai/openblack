/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Common/MsvcQsort.h"
#include "Graphics/InLoopLogoDraw.h"
#include "Gui/InLoopLogo.h"
#include "Input/InputRecording.h"

using namespace openblack;
using namespace openblack::in_loop_logo;

namespace
{
/// The C runtime's Random(a, b) on a stream of the test's own, from its seed 1
struct CrtStream
{
	uint32_t seed {game_random::k_CrtInitialSeed};
	uint64_t draws {0};
	float operator()(float a, float b)
	{
		++draws;
		const auto r = static_cast<float>(game_random::crt::Next(seed));
		return ((r * game_random::CrtRandomScale()) * (b - a)) + a;
	}
};

/// A point at a pixel: x and y the pixel, z the depth
glm::vec3 PixelAndDepth(glm::ivec2 pixel, float depth)
{
	return {static_cast<float>(pixel.x), static_cast<float>(pixel.y), depth};
}

/// A picture whose pixel (x, y) is (x, y, x + y)
Picture Gradient()
{
	Picture picture {};
	for (size_t y = 0; y < 64; ++y)
	{
		for (size_t x = 0; x < 64; ++x)
		{
			const size_t at = (y * 64 + x) * 3;
			picture.at(at) = static_cast<uint8_t>(x * 4);
			picture.at(at + 1) = static_cast<uint8_t>(y * 4);
			picture.at(at + 2) = static_cast<uint8_t>(x + y);
		}
	}
	return picture;
}
} // namespace

TEST(InLoopLogo, OneStartInFourTriesTheUsersPictureByTheSecond)
{
	EXPECT_TRUE(TriesUserPicture(0));
	EXPECT_TRUE(TriesUserPicture(999));
	EXPECT_FALSE(TriesUserPicture(1000));
	EXPECT_FALSE(TriesUserPicture(3999));
	EXPECT_TRUE(TriesUserPicture(4000));
	EXPECT_TRUE(TriesUserPicture(4999));
	EXPECT_FALSE(TriesUserPicture(5000));
}

TEST(InLoopLogo, TheUsersFileIsShownOnlyWithAName)
{
	std::vector<uint8_t> bytes(4096, 0x40);
	// grey bytes alone: no name, the game's own picture
	EXPECT_FALSE(ReadUserPicture(bytes).has_value());
	// one more byte: the name's first character's low byte
	bytes.push_back('A');
	auto user = ReadUserPicture(bytes);
	ASSERT_TRUE(user.has_value());
	EXPECT_EQ(user->name, u"A");
	EXPECT_EQ(user->picture.at(0), 0x40);
	EXPECT_EQ(user->picture.at(1), 0x40);
	EXPECT_EQ(user->picture.at(2), 0x40);
	// UTF-16 little-endian, up to its first 0
	bytes.resize(4096);
	for (const char16_t c : std::u16string(u"Hi\0x", 4))
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	user = ReadUserPicture(bytes);
	ASSERT_TRUE(user.has_value());
	EXPECT_EQ(user->name, u"Hi");
	// 128 characters at most
	bytes.resize(4096);
	bytes.insert(bytes.end(), 300 * 2, 'z');
	user = ReadUserPicture(bytes);
	ASSERT_TRUE(user.has_value());
	EXPECT_EQ(user->name.size(), k_NameLength);
	// each grey goes into red, green and blue, rows of y
	bytes.at(64 * 2 + 3) = 0x99;
	user = ReadUserPicture(bytes);
	ASSERT_TRUE(user.has_value());
	const size_t at = (2 * 64 + 3) * 3;
	EXPECT_EQ(user->picture.at(at), 0x99);
	EXPECT_EQ(user->picture.at(at + 2), 0x99);
}

TEST(InLoopLogo, TheRuntimesQuicksortLeavesEqualKeysAsTheGamesDoes)
{
	// 230 sprites keyed by their blue byte (i 37 + i / 7) % 11, each tagged with its index: the order the game's own
	// quicksort leaves them in with the logo's comparator (the exe's qsort, run on the oracle, 2026-11-10)
	constexpr std::array<int, 230> k_GameOrder {
	    162, 125, 186, 85,  93,  154, 109, 0,   8,   32,  117, 101, 202, 48,  77,  194, 16,  170, 40,  178, 24,  80,  173,
	    210, 11,  218, 165, 27,  19,  181, 64,  157, 3,   226, 96,  72,  149, 88,  56,  141, 133, 104, 43,  128, 189, 136,
	    152, 229, 197, 205, 213, 51,  112, 6,   221, 144, 120, 160, 35,  59,  83,  67,  75,  30,  46,  22,  38,  14,  62,
	    54,  115, 91,  192, 123, 107, 176, 216, 99,  168, 208, 200, 131, 184, 139, 78,  86,  195, 9,   224, 187, 179, 70,
	    1,   171, 94,  25,  17,  118, 163, 147, 155, 102, 33,  110, 41,  57,  97,  65,  81,  73,  89,  150, 203, 12,  227,
	    4,   219, 49,  211, 142, 134, 20,  174, 166, 158, 126, 113, 121, 44,  129, 60,  153, 206, 222, 145, 76,  214, 137,
	    198, 105, 182, 28,  68,  190, 52,  36,  7,   47,  39,  209, 116, 201, 55,  92,  132, 100, 177, 31,  124, 15,  193,
	    161, 185, 84,  169, 108, 23,  34,  164, 26,  148, 172, 156, 95,  140, 71,  103, 180, 79,  10,  2,   225, 217, 111,
	    63,  18,  87,  188, 90,  58,  167, 204, 127, 74,  159, 82,  196, 13,  228, 42,  66,  151, 212, 119, 50,  220, 135,
	    143, 5,   199, 61,  207, 29,  37,  106, 215, 191, 53,  21,  130, 146, 45,  138, 223, 69,  122, 175, 114, 98,  183};
	std::vector<int> items(k_GameOrder.size());
	std::iota(items.begin(), items.end(), 0);
	const auto key = [](int i) { return (i * 37 + i / 7) % 11; };
	msvc_qsort::Sort(std::span<int>(items), [&key](int a, int b) { return key(a) - key(b); });
	EXPECT_TRUE(std::equal(items.begin(), items.end(), k_GameOrder.begin()));
}

TEST(InLoopLogo, TheStarsArePlacedRowByRowAndSortedByTheirBlue)
{
	CrtStream random;
	const auto state = Start(Gradient(), true, std::u16string(k_DefaultName), {1024, 768}, PixelAndDepth, std::ref(random));
	ASSERT_EQ(state.stars.size(), k_Stars);
	// three draws a star
	EXPECT_EQ(random.draws, k_Stars * 3);
	EXPECT_EQ(random.draws, RandomDraws(0));
	// in blue order, each opaque, at home, still
	for (size_t i = 0; i + 1 < state.stars.size(); ++i)
	{
		EXPECT_LE(state.stars[i].argb & 0xFFu, state.stars[i + 1].argb & 0xFFu);
	}
	for (size_t i = 0; i < state.stars.size(); ++i)
	{
		const auto& star = state.stars[i];
		EXPECT_EQ(star.argb >> 24u, 0xFFu);
		EXPECT_EQ(star.position, state.home[i]);
		EXPECT_EQ(state.velocity[i], glm::vec3(0.0f));
		EXPECT_EQ(state.sizes[i], star.size);
		// the pixel's green decides the material
		EXPECT_EQ(star.bright, ((star.argb >> 8u) & 0xFFu) >= k_BrightGreen);
		// a depth of 70 to 130 over 200 to 320
		EXPECT_GE(star.size, 70.0f / 320.0f);
		EXPECT_LE(star.size, 130.0f / 200.0f + 1e-6f);
	}
	// the first star drawn: the darkest blue, picture pixel (0, 0) at (512 + (-32 x 1024) / 240, 384 + (-32 x 1024) /
	// 240), the divisions cut towards zero; the first draw is its depth
	EXPECT_EQ(state.stars.front().argb, 0xFF000000u);
	CrtStream again;
	const float depth = again(70.0f, 130.0f);
	EXPECT_EQ(state.stars.front().position, glm::vec3(376.0f, 248.0f, depth));
}

TEST(InLoopLogo, TheTimeStartsAtAThousandthAndRunsBySpeed)
{
	EXPECT_EQ(NextTime(0.0f, 500, 1.0f), 0.001f);
	EXPECT_EQ(NextTime(1.0f, 20, 1.0f), 1.0f + 20.0f * 0.0011f);
	EXPECT_EQ(NextTime(1.0f, 20, 0.5f), 1.0f + (20.0f * 0.5f) * 0.0011f);
}

TEST(InLoopLogo, TheFadesFollowTheLogosTime)
{
	// black lifting over the first 12, falling from 31 to 33
	EXPECT_EQ(FrameAt(0.001f).fade, 254);
	EXPECT_EQ(FrameAt(6.0f).fade, 127);
	EXPECT_EQ(FrameAt(12.0f).fade, 0);
	EXPECT_EQ(FrameAt(31.0f).fade, 0);
	EXPECT_EQ(FrameAt(32.0f).fade, 127);
	EXPECT_EQ(FrameAt(33.0f).fade, 255);
	// the sky: white at 160 until 22, then darker and more opaque, held from 25.8
	EXPECT_EQ(FrameAt(10.0f).skyArgb, 0xA0FFFFFFu);
	EXPECT_EQ(FrameAt(23.0f).skyArgb, 0xB2CDCDCDu);
	EXPECT_EQ(FrameAt(30.0f).skyArgb, 0xE6414141u);
	// the stars' alpha and the end
	EXPECT_EQ(FrameAt(32.0f).starAlpha, 255);
	EXPECT_EQ(FrameAt(33.0f).starAlpha, 127);
	EXPECT_EQ(FrameAt(34.0f).starAlpha, 0);
	EXPECT_FALSE(FrameAt(35.9f).ends);
	EXPECT_TRUE(FrameAt(36.0f).ends);
	// the name fades in from 22 to 23 and out from 33 to 34
	EXPECT_EQ(FrameAt(21.0f).nameFade, 0.0f);
	EXPECT_EQ(FrameAt(22.5f).nameFade, 0.5f);
	EXPECT_EQ(FrameAt(30.0f).nameFade, 1.0f);
	EXPECT_EQ(FrameAt(33.5f).nameFade, 0.5f);
	EXPECT_EQ(FrameAt(34.0f).nameFade, 0.0f);
}

TEST(InLoopLogo, TheCameraFliesIntoTheLogoAndFlashesThroughIt)
{
	// before 15 no zoom: at 10 the camera is 10 + 15 (1/3) away along its waves
	const auto early = FrameAt(10.0f);
	EXPECT_EQ(early.zoom, 0.0f);
	EXPECT_EQ(early.sizeScale, 1.0f);
	EXPECT_EQ(early.focus, glm::vec3(0.0f));
	EXPECT_NEAR(early.eye.x, std::sin(10.0 * 0.352532 + 2.0) * 15.0, 1e-4);
	EXPECT_NEAR(early.eye.y, std::cos(3.0 - 10.0 * 0.26793) * 15.0, 1e-4);
	EXPECT_NEAR(early.eye.z, std::sin(10.0 * 0.15161 + 7.0) * 15.0, 1e-4);
	// from 26 at the start camera, the stars at their largest
	const auto in = FrameAt(27.0f);
	EXPECT_EQ(in.zoom, 1.0f);
	EXPECT_NEAR(in.eye.z, 100.0f, 1e-4f);
	EXPECT_FLOAT_EQ(in.sizeScale, 2.54f);
	// the flash: at 31 the camera half way in, its focus out past the middle by as much
	const auto flash = FrameAt(31.0f);
	EXPECT_EQ(flash.flash, 0.5f);
	EXPECT_NEAR(flash.eye.z, 50.0f, 1e-3f);
	EXPECT_NEAR(flash.focus.z, -50.0f, 1e-3f);
}

TEST(InLoopLogo, TheMouseIsSmoothedByATenthAndPushesTheStarsNearIt)
{
	CrtStream random;
	auto state = Start(Gradient(), true, std::u16string(k_DefaultName), {1024, 768}, PixelAndDepth, std::ref(random));
	const auto frame = FrameAt(1.0f);
	// every star on screen at its own x and y, at its depth
	const auto project = [](const glm::vec3& p) -> std::optional<OnScreen> {
		return OnScreen {.pixel = {static_cast<int32_t>(p.x), static_cast<int32_t>(p.y)}, .depth = p.z};
	};
	// a move of 5: a tenth is cut to nothing, nothing is pushed
	Step(state, frame, {.move = {5, 0}, .mouse = {512, 384}, .deltaMs = 16, .screenWidth = 1024}, project, PixelAndDepth,
	     std::ref(random));
	EXPECT_EQ(state.mouseMove, glm::ivec2(0));
	EXPECT_EQ(random.draws, RandomDraws(1));
	EXPECT_EQ(state.stars[0].position, state.home[0]);
	// a move of 30: 3, then pushed along by up to 4 x 3 pixels within a tenth of the width
	const auto before = state.stars;
	Step(state, frame, {.move = {30, 0}, .mouse = {512, 384}, .deltaMs = 16, .screenWidth = 1024}, project, PixelAndDepth,
	     std::ref(random));
	EXPECT_EQ(state.mouseMove, glm::ivec2(3, 0));
	size_t pushed = 0;
	for (size_t i = 0; i < state.stars.size(); ++i)
	{
		const auto dx = static_cast<int32_t>(before[i].position.x) - 512;
		const auto dy = static_cast<int32_t>(before[i].position.y) - 384;
		const int32_t distanceSquared = dx * dx + dy * dy;
		// pushed a whole pixel or more within 93 pixels (3 x 4 (1 - 93 / 102) is just over 1)
		if (distanceSquared < 93 * 93)
		{
			++pushed;
			EXPECT_GT(state.velocity[i].x, 0.0f) << i;
		}
		else if (distanceSquared >= 102 * 102)
		{
			EXPECT_EQ(state.velocity[i], glm::vec3(0.0f)) << i;
		}
	}
	EXPECT_GT(pushed, 0u);
	// the stars take the frame's alpha and size and a new angle
	EXPECT_EQ(state.stars[0].argb >> 24u, frame.starAlpha);
	EXPECT_EQ(state.stars[0].size, frame.sizeScale * state.sizes[0]);
}

TEST(InLoopLogo, TheWheelSlowsItDownToATenthAndBackToFull)
{
	CrtStream random;
	auto state = Start(Gradient(), true, std::u16string(k_DefaultName), {640, 480}, PixelAndDepth, std::ref(random));
	const auto project = [](const glm::vec3&) -> std::optional<OnScreen> { return std::nullopt; };
	const auto frame = FrameAt(1.0f);
	Step(state, frame, {.wheel = -120, .deltaMs = 16, .screenWidth = 640}, project, PixelAndDepth, std::ref(random));
	EXPECT_NEAR(state.speed, std::exp(-120.0 / 1500.0), 1e-6);
	// past 240 a frame counts as 240
	for (int i = 0; i < 20; ++i)
	{
		Step(state, frame, {.wheel = -1200, .deltaMs = 16, .screenWidth = 640}, project, PixelAndDepth, std::ref(random));
	}
	EXPECT_EQ(state.speed, 0.1f);
	for (int i = 0; i < 20; ++i)
	{
		Step(state, frame, {.wheel = 240, .deltaMs = 16, .screenWidth = 640}, project, PixelAndDepth, std::ref(random));
	}
	EXPECT_EQ(state.speed, 1.0f);
}

TEST(InLoopLogo, TheExponentialIsTheFpusInlineOne)
{
	EXPECT_EQ(FpuExp(0.0f), 1.0);
	for (const float x : {-0.016f, -0.001f, -0.1f, 0.16f, -0.16f})
	{
		const double value = FpuExp(x);
		EXPECT_NEAR(value, std::exp(static_cast<double>(x)), 1e-6) << x;
		// a float's mantissa
		EXPECT_EQ(value, static_cast<double>(static_cast<float>(value))) << x;
	}
}

TEST(InLoopLogo, AKeyAButtonOrItsEndStopsIt)
{
	EXPECT_TRUE(GoesOn(false, false, false));
	EXPECT_FALSE(GoesOn(true, false, false));
	EXPECT_FALSE(GoesOn(false, true, false));
	EXPECT_FALSE(GoesOn(false, false, true));
	EXPECT_EQ(RandomDraws(10), 4096u * 13u);
}

TEST(InLoopLogo, TheTrademarkAndTheNameArePlacedOnTheScreen)
{
	const auto trademark = TrademarkPlace({1024, 768});
	EXPECT_EQ(trademark.position, glm::vec2(655.0f, 245.0f));
	EXPECT_EQ(trademark.size, 14.0f);
	const auto name = NamePlace({1024, 768}, 100.0f);
	EXPECT_EQ(name.position.x, 462.0f);
	EXPECT_EQ(name.position.y, 768.0f * 0.71428573f);
	EXPECT_EQ(name.size, 38.4f);
}

TEST(InLoopLogo, TheSkysTexelsAreFiveBitsAColourAndOpaque)
{
	const std::array<uint16_t, 4> texels {0x7FFF, 0x0000, 0x7C00, 0x8000 | 0x0001};
	const auto rgba = graphics::InLoopLogoDraw::SkyTexels(texels);
	ASSERT_EQ(rgba.size(), 16u);
	EXPECT_EQ((std::array<uint8_t, 4> {rgba[0], rgba[1], rgba[2], rgba[3]}), (std::array<uint8_t, 4> {255, 255, 255, 255}));
	EXPECT_EQ((std::array<uint8_t, 4> {rgba[4], rgba[5], rgba[6], rgba[7]}), (std::array<uint8_t, 4> {0, 0, 0, 255}));
	EXPECT_EQ((std::array<uint8_t, 4> {rgba[8], rgba[9], rgba[10], rgba[11]}), (std::array<uint8_t, 4> {255, 0, 0, 255}));
	// the top bit is not alpha
	EXPECT_EQ((std::array<uint8_t, 4> {rgba[12], rgba[13], rgba[14], rgba[15]}), (std::array<uint8_t, 4> {0, 0, 8, 255}));
}

TEST(InLoopLogo, ARecordingKeepsTheLogosFrames)
{
	using namespace openblack::input::recording;
	const Recording recording {.window = {1024, 768},
	                           .events = {Event {.frame = 0, .kind = Kind::Logo, .values = {1234, 0, 0, 0, 0, 0}}}};
	const auto read = Read(Write(recording));
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, recording);
	EXPECT_TRUE(ReleasesOfHeld(read->events, 1).empty());
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it. Its synthetic twin is
// TheStarsArePlacedRowByRowAndSortedByTheirBlue
TEST(InLoopLogo, TheGamesPictureMakesItsStars)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Data" / "lhlogo.raw", std::ios::binary);
	ASSERT_TRUE(file.good());
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(bytes.size(), k_PictureBytes);
	Picture picture {};
	std::copy(bytes.begin(), bytes.end(), picture.begin());
	CrtStream random;
	const auto state = Start(picture, true, std::u16string(k_DefaultName), {1024, 768}, PixelAndDepth, std::ref(random));
	ASSERT_EQ(state.stars.size(), k_Stars);
	const auto bright = std::count_if(state.stars.begin(), state.stars.end(), [](const Star& s) { return s.bright; });
	EXPECT_GT(bright, 0);
	EXPECT_LT(bright, static_cast<std::ptrdiff_t>(k_Stars));
	for (size_t i = 0; i + 1 < state.stars.size(); ++i)
	{
		EXPECT_LE(state.stars[i].argb & 0xFFu, state.stars[i + 1].argb & 0xFFu);
	}
}
