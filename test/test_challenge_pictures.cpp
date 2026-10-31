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
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <ECS/ChallengePictures.h>
#include <ECS/Components/ChallengePicture.h>
#include <ECS/Registry.h>
#include <Graphics/RenderPass.h>
#include <Graphics/ScreenshotCapture.h>
#include <LHVMTypes.h>
#include <gtest/gtest.h>

namespace
{
namespace pictures = openblack::ecs::challenge_pictures;
using openblack::ecs::components::ChallengePicture;
using openblack::graphics::ScreenView;

/// The pixels of a test screen, and the view of them that the renderer gives
struct TestScreen
{
	uint32_t width {0};
	uint32_t height {0};
	bool yflip {false};
	std::vector<uint8_t> bgra;

	[[nodiscard]] ScreenView View() const
	{
		return {.width = width, .height = height, .pitch = width * 4, .yflip = yflip, .bgra = bgra};
	}
};

/// A screen whose pixels (red, green, blue) come from where they are, top row first
TestScreen Screen(uint32_t width, uint32_t height, const std::function<std::array<uint8_t, 3>(uint32_t, uint32_t)>& colour,
                  bool yflip = false)
{
	TestScreen frame {.width = width, .height = height, .yflip = yflip, .bgra = {}};
	const auto pitch = static_cast<size_t>(width) * 4;
	frame.bgra.resize(pitch * height);
	for (uint32_t y = 0; y < height; ++y)
	{
		const auto row = yflip ? height - 1 - y : y;
		for (uint32_t x = 0; x < width; ++x)
		{
			const auto [r, g, b] = colour(x, y);
			const auto offset = static_cast<size_t>(row) * pitch + static_cast<size_t>(x) * 4;
			frame.bgra[offset] = b;
			frame.bgra[offset + 1] = g;
			frame.bgra[offset + 2] = r;
			frame.bgra[offset + 3] = 255;
		}
	}
	return frame;
}

/// The picture in high detail of a test screen
std::vector<uint8_t> TakeOf(const TestScreen& screen)
{
	return pictures::Take(screen.View());
}

uint8_t Red(const std::vector<uint8_t>& picture, uint32_t width, uint32_t x, uint32_t y)
{
	return picture.at((static_cast<size_t>(y) * width + x) * 3);
}

uint8_t Green(const std::vector<uint8_t>& picture, uint32_t width, uint32_t x, uint32_t y)
{
	return picture.at((static_cast<size_t>(y) * width + x) * 3 + 1);
}
} // namespace

TEST(ChallengePictures, APictureIsOfTheLargestMiddle4By3PartOfTheScreen)
{
	const auto classic = pictures::PictureRegion(640, 480);
	EXPECT_EQ(classic.x, 0u);
	EXPECT_EQ(classic.y, 0u);
	EXPECT_EQ(classic.width, 640u);
	EXPECT_EQ(classic.height, 480u);

	const auto wide = pictures::PictureRegion(1920, 1080);
	EXPECT_EQ(wide.x, 240u);
	EXPECT_EQ(wide.width, 1440u);
	EXPECT_EQ(wide.height, 1080u);

	const auto tall = pictures::PictureRegion(1280, 1024);
	EXPECT_EQ(tall.y, 32u);
	EXPECT_EQ(tall.width, 1280u);
	EXPECT_EQ(tall.height, 960u);
}

TEST(ChallengePictures, AnEvenScreenGivesAnEvenPicture)
{
	const auto picture = TakeOf(Screen(640, 480, [](uint32_t, uint32_t) { return std::array<uint8_t, 3> {10, 20, 30}; }));
	ASSERT_EQ(picture.size(), static_cast<size_t>(pictures::k_Width) * pictures::k_Height * 3);
	for (size_t i = 0; i < picture.size(); i += 3)
	{
		ASSERT_EQ(picture[i], 10);
		ASSERT_EQ(picture[i + 1], 20);
		ASSERT_EQ(picture[i + 2], 30);
	}
}

TEST(ChallengePictures, EachPixelIsMeanedWithTheOneBeforeItThenSoftened)
{
	// Red on the right half of the screen only; one screen pixel per picture column
	const auto picture = TakeOf(Screen(256, 192, [](uint32_t x, uint32_t) {
		return std::array<uint8_t, 3> {static_cast<uint8_t>(x >= 128 ? 255 : 0), 0, 0};
	}));
	ASSERT_FALSE(picture.empty());
	// The last row stays as picked: the pixel at the edge is the mean of black and red
	EXPECT_EQ(Red(picture, pictures::k_Width, 127, 127), 0);
	EXPECT_EQ(Red(picture, pictures::k_Width, 128, 127), 127);
	EXPECT_EQ(Red(picture, pictures::k_Width, 129, 127), 255);
	// Softened, each pixel is blended with its neighbour to the right (keeping 128/255 of itself)
	EXPECT_EQ(Red(picture, pictures::k_Width, 126, 0), 0);
	EXPECT_EQ(Red(picture, pictures::k_Width, 127, 0), 63);
	EXPECT_EQ(Red(picture, pictures::k_Width, 128, 0), 190);
	EXPECT_EQ(Red(picture, pictures::k_Width, 129, 0), 255);
}

TEST(ChallengePictures, APictureReadBottomUpIsTheRightWayUp)
{
	const auto top = [](uint32_t, uint32_t y) {
		return std::array<uint8_t, 3> {0, static_cast<uint8_t>(y < 240 ? 255 : 0), 0};
	};
	const auto upright = TakeOf(Screen(640, 480, top));
	const auto flipped = TakeOf(Screen(640, 480, top, true));
	EXPECT_EQ(upright, flipped);
	EXPECT_EQ(Green(flipped, pictures::k_Width, 10, 0), 255);
	EXPECT_EQ(Green(flipped, pictures::k_Width, 10, 127), 0);
}

TEST(ChallengePictures, AWideScreenLeavesItsSidesOut)
{
	const auto picture = TakeOf(Screen(1920, 1080, [](uint32_t x, uint32_t) {
		return std::array<uint8_t, 3> {static_cast<uint8_t>(x < 240 || x >= 1680 ? 255 : 0), 0, 0};
	}));
	ASSERT_FALSE(picture.empty());
	for (size_t i = 0; i < picture.size(); i += 3)
	{
		ASSERT_EQ(picture[i], 0);
	}
}

TEST(ChallengePictures, TheSmallPictureMeansPairsOfEveryOtherRow)
{
	std::vector<uint8_t> full(static_cast<size_t>(pictures::k_Width) * pictures::k_Height * 3, 0);
	// Row 0: red 100 then 201 in the first pair; row 1 is skipped
	full[0] = 100;
	full[3] = 201;
	full[static_cast<size_t>(pictures::k_Width) * 3] = 255;
	const auto small = pictures::Shrink(full);
	ASSERT_EQ(small.size(), static_cast<size_t>(pictures::k_SmallWidth) * pictures::k_SmallHeight * 3);
	EXPECT_EQ(small[0], 150);
	EXPECT_EQ(small[static_cast<size_t>(pictures::k_SmallWidth) * 3], 0);
}

TEST(ChallengePictures, ARecordsPictureComesBackOnceTheScreenIsRead)
{
	openblack::ecs::Registry registry;
	const auto record = registry.Create();
	auto& picture = registry.Assign<ChallengePicture>(record);
	const auto name = pictures::Ask(picture, record);
	EXPECT_TRUE(name.starts_with(openblack::graphics::ScreenPictures::k_Prefix));
	EXPECT_NE(pictures::Ask(picture, record), name);
	const auto asked = *picture.waiting;

	pictures::Collect(registry,
	                  {.take = [](const std::string&) { return std::optional<std::vector<uint8_t>> {}; }, .failed = {}});
	EXPECT_TRUE(registry.Get<ChallengePicture>(record).waiting.has_value());

	pictures::Collect(registry, {.take =
	                                 [&asked](const std::string& wanted) {
		                                 return wanted == asked
		                                            ? std::optional(TakeOf(Screen(640, 480,
		                                                                          [](uint32_t, uint32_t) {
			                                                                          return std::array<uint8_t, 3> {1, 2, 3};
		                                                                          })))
		                                            : std::nullopt;
	                                 },
	                             .failed = {}});
	const auto& taken = registry.Get<ChallengePicture>(record);
	EXPECT_FALSE(taken.waiting.has_value());
	EXPECT_EQ(taken.taken, 1u);
	EXPECT_EQ(taken.full.size(), static_cast<size_t>(pictures::k_Width) * pictures::k_Height * 3);
	EXPECT_EQ(taken.small.size(), static_cast<size_t>(pictures::k_SmallWidth) * pictures::k_SmallHeight * 3);
}

TEST(ChallengePictures, APictureGivenUpLeavesTheOldOne)
{
	openblack::ecs::Registry registry;
	const auto record = registry.Create();
	auto& picture = registry.Assign<ChallengePicture>(record);
	picture.full = {1, 2, 3};
	[[maybe_unused]] const auto name = pictures::Ask(picture, record);
	pictures::Collect(registry, {.take = {}, .failed = [](const std::string&) { return true; }});
	const auto& kept = registry.Get<ChallengePicture>(record);
	EXPECT_FALSE(kept.waiting.has_value());
	EXPECT_EQ(kept.full, (std::vector<uint8_t> {1, 2, 3}));
	EXPECT_EQ(kept.taken, 0u);
}

TEST(ChallengePictures, ARequestIsSentOnce)
{
	openblack::ecs::Registry registry;
	const auto record = registry.Create();
	auto& picture = registry.Assign<ChallengePicture>(record);
	EXPECT_TRUE(pictures::TakeRequests(registry).empty());

	const auto first = pictures::Ask(picture, record);
	EXPECT_EQ(pictures::TakeRequests(registry), (std::vector<std::string> {first}));
	EXPECT_TRUE(pictures::TakeRequests(registry).empty());

	// Asked again before the first is back: the new name is sent
	const auto second = pictures::Ask(registry.Get<ChallengePicture>(record), record);
	EXPECT_NE(second, first);
	EXPECT_EQ(pictures::TakeRequests(registry), (std::vector<std::string> {second}));
	EXPECT_TRUE(pictures::TakeRequests(registry).empty());
}

TEST(ChallengePictures, PicturesAskedTogetherAreOfOneFrame)
{
	// The game reads the screen at once for each picture: two asked before a frame's draw are of that frame
	openblack::ecs::Registry registry;
	const auto first = registry.Create();
	const auto second = registry.Create();
	const auto firstName = pictures::Ask(registry.Assign<ChallengePicture>(first), first);
	const auto secondName = pictures::Ask(registry.Assign<ChallengePicture>(second), second);
	const auto oneFrame = pictures::TakeRequests(registry);
	ASSERT_EQ(oneFrame.size(), 1u);
	EXPECT_TRUE(oneFrame[0] == firstName || oneFrame[0] == secondName);
	EXPECT_TRUE(pictures::TakeRequests(registry).empty());
	// The renderer gives that frame once; both records take it
	int takes = 0;
	const std::vector<uint8_t> screen(static_cast<size_t>(pictures::k_Width) * pictures::k_Height * 3, 7);
	pictures::Collect(registry, {.take = [&](const std::string& name) -> std::optional<std::vector<uint8_t>> {
		                             ++takes;
		                             return name == oneFrame[0] ? std::optional(screen) : std::nullopt;
	                             },
	                             .failed = [](const std::string&) { return false; }});
	EXPECT_EQ(takes, 1);
	for (const auto entity : {first, second})
	{
		const auto& picture = registry.Get<ChallengePicture>(entity);
		EXPECT_EQ(picture.taken, 1u);
		EXPECT_FALSE(picture.waiting.has_value());
		EXPECT_EQ(picture.full, screen);
	}
}

TEST(ChallengePictures, TheTexelsKeepFiveBitsAColour)
{
	// The top bit set, then red, green and blue, each its five highest bits
	const std::array<uint8_t, 6> rgb {255, 0, 8, 7, 128, 249};
	EXPECT_EQ(pictures::Texels(rgb), (std::vector<uint16_t> {0xFC01, 0x821F}));
}

TEST(ChallengePictures, SofteningKeeps128Of255OfEachPixel)
{
	// The original's blend (oracle): (255, 0) gives 128, (0, 255) 127, (200, 100) 150. Four colour columns, one screen
	// pixel a picture column, each picked as the mean with the one before it: 255, 127, 100, 100; every row the same,
	// so the softening blends each with the one to its right
	const auto picture = TakeOf(Screen(256, 192, [](uint32_t x, uint32_t) {
		constexpr std::array<uint8_t, 4> k_Red {255, 0, 200, 1};
		return std::array<uint8_t, 3> {k_Red.at(std::min(x, 3U)), 0, 0};
	}));
	ASSERT_FALSE(picture.empty());
	EXPECT_EQ(Red(picture, pictures::k_Width, 0, 0), 191); // (128 x 255 + 127 x 127) / 255
	EXPECT_EQ(Red(picture, pictures::k_Width, 1, 0), 113); // (128 x 127 + 127 x 100) / 255
	EXPECT_EQ(Red(picture, pictures::k_Width, 2, 0), 100);
}

namespace
{
namespace lhvm = openblack::lhvm;
using openblack::graphics::ShownFrames;

/// The natives a fake program calls, by index: three that take a challenge's picture among others
std::vector<lhvm::NativeFunction> FakeNatives()
{
	std::vector<lhvm::NativeFunction> natives;
	for (const auto* name : {"NONE", "SNAPSHOT", "MOVE_GAME_THING", "UPDATE_SNAPSHOT_PICTURE", "UPDATE_SNAPSHOT"})
	{
		natives.emplace_back(nullptr, 0, 0, name);
	}
	return natives;
}

lhvm::VMInstruction Call(uint32_t native)
{
	return {lhvm::Opcode::Sys, lhvm::VMMode::Immediate, lhvm::DataType::None, lhvm::VMValue(native), 0};
}

lhvm::VMInstruction Start(uint32_t script, lhvm::VMMode mode = lhvm::VMMode::Sync)
{
	return {lhvm::Opcode::Run, mode, lhvm::DataType::None, lhvm::VMValue(script), 0};
}

lhvm::VMInstruction End()
{
	return {lhvm::Opcode::End, lhvm::VMMode::Immediate, lhvm::DataType::None, lhvm::VMValue(), 0};
}

lhvm::VMScript Script(uint32_t id, uint32_t start)
{
	return {"Script" + std::to_string(id), "fake.txt", lhvm::ScriptType::Script, 0, {}, start, 0, id};
}

std::map<uint32_t, lhvm::VMTask> Tasks(std::initializer_list<uint32_t> scripts)
{
	std::map<uint32_t, lhvm::VMTask> tasks;
	uint32_t number = 0;
	for (const auto script : scripts)
	{
		++number;
		tasks.emplace(number, lhvm::VMTask({}, script, number, 0, 0, {}, "", "", lhvm::ScriptType::Script));
	}
	return tasks;
}
} // namespace

TEST(ChallengePictures, AScriptCanTakeAPictureWhenItsCodeCallsASnapshotOrStartsAScriptThatDoes)
{
	const auto natives = FakeNatives();
	// 1 moves a thing; 2 takes a snapshot; 3 starts 2; 4 starts 1 aside; 5 updates a picture; 6 starts 3, which starts
	// 2; 7 updates a snapshot
	const std::vector code = {Call(2), End(),   Call(1), End(),    Start(2), End(),   Start(1, lhvm::VMMode::Async),
	                          End(),   Call(3), End(),   Start(3), End(),    Call(4), End()};
	const std::vector scripts = {Script(1, 0), Script(2, 2),  Script(3, 4), Script(4, 6),
	                             Script(5, 8), Script(6, 10), Script(7, 12)};
	const auto snapshots = pictures::ScriptsThatSnapshot(scripts, code, natives);
	EXPECT_EQ(snapshots, (std::vector<bool> {false, false, true, true, false, true, true, true}));
}

TEST(ChallengePictures, AScriptsCodeEndsWhereTheNextScriptsBegins)
{
	const auto natives = FakeNatives();
	// The scripts are listed out of their code's order: 1's code is only its first two instructions, the snapshot
	// after them is 2's
	const std::vector code = {Call(2), End(), Call(1), End()};
	const std::vector scripts = {Script(2, 2), Script(1, 0)};
	EXPECT_EQ(pictures::ScriptsThatSnapshot(scripts, code, natives), (std::vector<bool> {false, false, true}));
	// A program without the natives has no script that takes a picture
	EXPECT_EQ(pictures::ScriptsThatSnapshot(scripts, code, {}), (std::vector<bool> {false, false, false}));
}

TEST(ChallengePictures, TheFramesAreKeptWhileARunningTaskCanTakeAPicture)
{
	const std::vector<bool> snapshots = {false, false, true};
	EXPECT_FALSE(pictures::KeepsShownFrames(snapshots, {}));
	EXPECT_FALSE(pictures::KeepsShownFrames(snapshots, Tasks({1, 1})));
	EXPECT_TRUE(pictures::KeepsShownFrames(snapshots, Tasks({1, 2})));
	// A task of a script the program does not have
	EXPECT_FALSE(pictures::KeepsShownFrames(snapshots, Tasks({9})));
	// The task named is the first that can take one, by number
	EXPECT_EQ(pictures::TaskThatSnapshots(snapshots, Tasks({1, 2, 2})), 2u);
	EXPECT_FALSE(pictures::TaskThatSnapshots(snapshots, Tasks({1})).has_value());
}

TEST(ChallengePictures, TheFrameOnTheScreenIsTheLastOneDrawn)
{
	EXPECT_FALSE(ShownFrames::OnScreen(0).has_value());
	EXPECT_EQ(ShownFrames::OnScreen(1), 0u);
	EXPECT_EQ(ShownFrames::OnScreen(42), 41u);
	EXPECT_TRUE(ShownFrames::IsName(ShownFrames::Name(41)));
	EXPECT_TRUE(openblack::graphics::ScreenPictures::InMemory(ShownFrames::Name(41)));
	EXPECT_FALSE(ShownFrames::IsName("memory:challenge-picture/1/0"));
	EXPECT_NE(ShownFrames::Name(41), ShownFrames::Name(42));
}

TEST(ChallengePictures, TheFramesKeptAreDrawnIntoTheTwoTargetsInTurn)
{
	ShownFrames frames;
	EXPECT_FALSE(frames.Last().has_value());
	const auto first = frames.Drawn(10);
	const auto second = frames.Drawn(11);
	EXPECT_NE(first, second);
	EXPECT_EQ(frames.Drawn(12), first);
	EXPECT_FALSE(frames.Holding(10).has_value());
	EXPECT_EQ(frames.Holding(11), second);
	EXPECT_EQ(frames.Holding(12), first);
	EXPECT_EQ(frames.Last(), 12u);
	// A frame drawn after one not kept still goes into the other target than the last kept one's
	EXPECT_EQ(frames.Drawn(20), second);
	EXPECT_EQ(frames.Holding(12), first);
	// The targets let go hold nothing
	frames.Forget();
	EXPECT_FALSE(frames.Holding(20).has_value());
	EXPECT_FALSE(frames.Last().has_value());
}

TEST(ChallengePictures, AFrameIsReadBackOnlyWhenAPictureIsAskedOfIt)
{
	ShownFrames frames;
	// Nothing kept: no picture of the frame on the screen
	EXPECT_FALSE(frames.Ask(5).has_value());
	const auto target = frames.Drawn(4);
	EXPECT_TRUE(frames.TakeReadBacks().empty());
	// The turn of frame 5 runs with frame 4 on the screen
	const auto asked = frames.Ask(5);
	ASSERT_TRUE(asked.has_value());
	EXPECT_EQ(asked->name, ShownFrames::Name(4));
	EXPECT_TRUE(asked->first);
	const auto readBacks = frames.TakeReadBacks();
	ASSERT_EQ(readBacks.size(), 1u);
	EXPECT_EQ(readBacks[0].target, target);
	EXPECT_EQ(readBacks[0].name, ShownFrames::Name(4));
	EXPECT_TRUE(frames.TakeReadBacks().empty());
	// The frame read back is not drawn into by the frame drawn after the turn
	EXPECT_NE(frames.Drawn(5), target);
	// Frame 6 was not kept: a picture asked while it is on the screen has no kept frame
	EXPECT_FALSE(frames.Ask(7).has_value());
	EXPECT_TRUE(frames.TakeReadBacks().empty());
}

TEST(ChallengePictures, TwoSnapshotsOfOneTurnShareTheFrameDrawnBeforeIt)
{
	ShownFrames frames;
	[[maybe_unused]] const auto target = frames.Drawn(4);
	const auto firstAsked = frames.Ask(5);
	const auto secondAsked = frames.Ask(5);
	ASSERT_TRUE(firstAsked.has_value());
	ASSERT_TRUE(secondAsked.has_value());
	EXPECT_TRUE(firstAsked->first);
	EXPECT_FALSE(secondAsked->first);
	EXPECT_EQ(secondAsked->name, firstAsked->name);
	EXPECT_EQ(frames.TakeReadBacks().size(), 1u);

	openblack::ecs::Registry registry;
	const auto first = registry.Create();
	const auto second = registry.Create();
	EXPECT_EQ(pictures::Ask(registry.Assign<ChallengePicture>(first), first, firstAsked->name), firstAsked->name);
	EXPECT_EQ(pictures::Ask(registry.Assign<ChallengePicture>(second), second, secondAsked->name), secondAsked->name);
	// Nothing more is asked of the renderer: the frame on the screen is read back as it is
	EXPECT_TRUE(pictures::TakeRequests(registry).empty());

	// The one read-back comes back once, under the name both wait for
	const std::vector<uint8_t> screen(static_cast<size_t>(pictures::k_Width) * pictures::k_Height * 3, 4);
	bool given = false;
	pictures::Collect(registry, {.take = [&](const std::string& name) -> std::optional<std::vector<uint8_t>> {
		                             if (given || name != firstAsked->name)
		                             {
			                             return std::nullopt;
		                             }
		                             given = true;
		                             return screen;
	                             },
	                             .failed = [](const std::string&) { return false; }});
	for (const auto entity : {first, second})
	{
		const auto& picture = registry.Get<ChallengePicture>(entity);
		EXPECT_EQ(picture.taken, 1u);
		EXPECT_FALSE(picture.waiting.has_value());
		EXPECT_EQ(picture.full, screen);
	}
}

TEST(ChallengePictures, AFrameReadBackGivenUpLeavesTheOldPicture)
{
	openblack::ecs::Registry registry;
	const auto record = registry.Create();
	auto& picture = registry.Assign<ChallengePicture>(record);
	picture.full = {9};
	const auto shown = ShownFrames::Name(0);
	[[maybe_unused]] const auto name = pictures::Ask(picture, record, shown);
	pictures::Collect(registry, {.take = {}, .failed = [&shown](const std::string& wanted) { return wanted == shown; }});
	const auto& left = registry.Get<ChallengePicture>(record);
	EXPECT_FALSE(left.waiting.has_value());
	EXPECT_EQ(left.full, (std::vector<uint8_t> {9}));
}

TEST(ChallengePictures, TheFrameKeptHasTheGamesPassesAndNotTheDebugWindows)
{
	using openblack::graphics::RenderPass;
	using openblack::graphics::ShownFrameTargets;
	// The copy to the window comes after every pass the frame kept has, and before the debug windows and the pointer
	EXPECT_LT(static_cast<int>(RenderPass::Interface), static_cast<int>(RenderPass::ShownFrameCopy));
	EXPECT_LT(static_cast<int>(RenderPass::ShownFrameCopy), static_cast<int>(RenderPass::ImGui));
	for (const auto pass : ShownFrameTargets::k_Passes)
	{
		EXPECT_LT(static_cast<int>(pass), static_cast<int>(RenderPass::ShownFrameCopy)) << static_cast<int>(pass);
		EXPECT_NE(pass, RenderPass::ImGui);
		EXPECT_NE(pass, RenderPass::Cursor);
	}
}
