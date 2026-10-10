/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// CloudSystem: Reset lays out the sky's clouds from the C runtime's stream, as a new Clouds does, in place of the
// last layout; Update moves them with the wind as Clouds::Update does. Clouds::Layout makes six draws per cloud
// (x, y, z, the mist's start, size, k), checked with a fake stream. A land's open (Clouds::OnLandscapeOpened, with a
// fake sky of the frame) counts the landscape and lays out the sky at once, at its place in the map script: a drawn
// frame afterwards makes no layout draws, and every later land open lays out a new sky.

#define LOCATOR_IMPLEMENTATIONS

#include <chrono>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/Clouds.h"
#include "Common/GameRandom.h"
#include "ECS/Systems/Implementations/CloudSystem.h"
#include "ECS/Systems/SkyFrameSystemInterface.h"

using namespace openblack;
using openblack::ecs::systems::CloudSystem;
using Milliseconds = std::chrono::duration<float, std::milli>;

namespace
{
void ExpectSameClouds(const Clouds& actual, const Clouds& expected)
{
	ASSERT_EQ(actual.GetClouds().size(), expected.GetClouds().size());
	for (size_t i = 0; i < expected.GetClouds().size(); ++i)
	{
		const auto& a = actual.GetClouds()[i];
		const auto& e = expected.GetClouds()[i];
		EXPECT_EQ(a.local, e.local) << "cloud " << i;
		EXPECT_EQ(a.size, e.size) << "cloud " << i;
		EXPECT_EQ(a.k, e.k) << "cloud " << i;
		EXPECT_EQ(a.pinned, e.pinned) << "cloud " << i;
		EXPECT_EQ(a.counter, e.counter) << "cloud " << i;
	}
}
} // namespace

TEST(CloudSystem, NoCloudsBeforeTheFirstReset)
{
	CloudSystem clouds;
	EXPECT_EQ(clouds.GetLayout(), nullptr);
	// nothing to move yet
	clouds.Update(Milliseconds(16.0f));
	EXPECT_EQ(clouds.GetLayout(), nullptr);
}

TEST(CloudSystem, ResetLaysOutTheCloudsAsANewSkyDoes)
{
	CloudSystem clouds;
	game_random::crt::Srand(1);
	clouds.Reset();

	game_random::crt::Srand(1);
	const Clouds expected;

	ASSERT_NE(clouds.GetLayout(), nullptr);
	ExpectSameClouds(*clouds.GetLayout(), expected);
}

TEST(CloudSystem, EachResetTakesTheNextNumbersOfTheStream)
{
	CloudSystem clouds;
	game_random::crt::Srand(1);
	clouds.Reset();
	clouds.Reset();

	game_random::crt::Srand(1);
	const Clouds first;
	const Clouds second;

	ExpectSameClouds(*clouds.GetLayout(), second);
	EXPECT_NE(clouds.GetLayout()->GetClouds()[2].local, first.GetClouds()[2].local);
}

TEST(CloudSystem, UpdateMovesTheCloudsWithTheWind)
{
	CloudSystem clouds;
	game_random::crt::Srand(1);
	clouds.Reset();
	game_random::crt::Srand(1);
	Clouds expected;

	clouds.Update(Milliseconds(500.0f));
	expected.Update(500.0f);

	ExpectSameClouds(*clouds.GetLayout(), expected);
	// the two pinned at the ends of the track stay
	EXPECT_EQ(clouds.GetLayout()->GetClouds()[0].local.x, 8000.0f);
}

namespace
{
/// The ranges of one cloud's six draws, in their order: x, y, z, the mist's start, size, k
const std::vector<std::pair<float, float>> k_CloudDraws = {
    {-8000.0f, 8000.0f}, {300.0f, 500.0f}, {-5000.0f, 5000.0f}, {0.0f, 16.0f}, {13.0f, 50.0f}, {2.5f, 5.0f},
};
} // namespace

TEST(CloudSystem, LayoutDrawsSixNumbersPerCloudInTheOriginalsOrder)
{
	std::vector<std::pair<float, float>> draws;
	const auto clouds = Clouds::Layout([&draws](float a, float b) {
		draws.emplace_back(a, b);
		return a;
	});

	ASSERT_EQ(clouds.size(), 70u);
	// 70 clouds x 6 = 420 draws, clouds 0 and 1 too
	ASSERT_EQ(draws.size(), 420u);
	for (size_t i = 0; i < draws.size(); ++i)
	{
		EXPECT_EQ(draws[i], k_CloudDraws[i % k_CloudDraws.size()]) << "draw " << i;
	}
}

TEST(CloudSystem, LayoutStartsEachMistCounterFromItsDraw)
{
	// the mist's draw of cloud i gives starts[i % 6]; every other draw gives the bottom of its range
	const std::vector<float> starts = {0.0f, 0.99f, 7.5f, 15.99f, 16.0f, 3.0f};
	int mistDraws = 0;
	const auto clouds = Clouds::Layout([&](float a, float b) {
		if (a == 0.0f && b == 16.0f)
		{
			return starts[static_cast<size_t>(mistDraws++) % starts.size()];
		}
		return a;
	});

	ASSERT_EQ(mistDraws, 70);
	for (size_t i = 0; i < clouds.size(); ++i)
	{
		const float drawn = starts[i % starts.size()];
		EXPECT_EQ(clouds[i].counter, static_cast<int>(drawn) & 15) << "cloud " << i;
		EXPECT_EQ(clouds[i].counterRemainder, 0.0f) << "cloud " << i;
	}
	// 16.0 truncated is 16, & 15 is 0
	EXPECT_EQ(clouds[4].counter, 0);
	EXPECT_EQ(clouds[3].counter, 15);
	// the size and k are the draws after the mist's (the bottoms of their ranges here), clouds 2 onwards
	EXPECT_EQ(clouds[2].local, glm::vec3(-8000.0f, 300.0f, -5000.0f));
	EXPECT_EQ(clouds[2].size, 13.0f);
	EXPECT_EQ(clouds[2].k, 2.5f);
}

TEST(CloudSystem, ANewSkyTakesFourHundredAndTwentyNumbersOfTheCrtStream)
{
	game_random::crt::Srand(1);
	const Clouds clouds;
	const auto next = game_random::crt::Rand();

	game_random::crt::Srand(1);
	const auto layout = Clouds::Layout(game_random::crt::Random);
	ASSERT_EQ(layout.size(), clouds.GetClouds().size());
	for (size_t i = 0; i < layout.size(); ++i)
	{
		EXPECT_EQ(clouds.GetClouds()[i].local, layout[i].local) << "cloud " << i;
		EXPECT_EQ(clouds.GetClouds()[i].counter, layout[i].counter) << "cloud " << i;
	}

	// the number after the sky is the stream's 421st
	game_random::crt::Srand(1);
	for (int i = 0; i < 420; ++i)
	{
		static_cast<void>(game_random::crt::Rand());
	}
	EXPECT_EQ(game_random::crt::Rand(), next);
}

namespace
{
/// The sky of the frame, with only the count of the landscapes opened
class FakeSkyFrame final: public openblack::ecs::systems::SkyFrameSystemInterface
{
public:
	void SetThresholds(const sky_type::Thresholds& thresholds) noexcept override { _thresholds = thresholds; }
	[[nodiscard]] const sky_type::Thresholds& GetThresholds() const noexcept override { return _thresholds; }
	void SampleFrame(float) noexcept override {}
	[[nodiscard]] float GetCurrentSkyType() const noexcept override { return 0.0f; }
	[[nodiscard]] float GetCurrentHour() const noexcept override { return 0.0f; }
	void Jump(float) noexcept override {}
	[[nodiscard]] sky_type::DomeBlend& GetDome() noexcept override { return _dome; }
	void OnLandscapeOpened() noexcept override { ++_generation; }
	[[nodiscard]] uint32_t GetLandscapeGeneration() const noexcept override { return _generation; }

private:
	sky_type::Thresholds _thresholds {};
	sky_type::DomeBlend _dome {};
	uint32_t _generation {0};
};

/// A cloud system that records its layouts and the landscape count each one saw
class RecordingClouds final: public openblack::ecs::systems::CloudSystemInterface
{
public:
	explicit RecordingClouds(const FakeSkyFrame& sky)
	    : _sky(sky)
	{
	}
	void Reset() override { resets.push_back(_sky.GetLandscapeGeneration()); }
	void Update(Milliseconds) override { ++updates; }
	[[nodiscard]] Clouds* GetLayout() noexcept override { return nullptr; }

	std::vector<uint32_t> resets; ///< the landscape count at each layout
	int updates {0};

private:
	const FakeSkyFrame& _sky;
};

/// The stream's number after `count` numbers from the seed 1
uint32_t CrtNumberAfter(int count)
{
	game_random::crt::Srand(1);
	for (int i = 0; i < count; ++i)
	{
		static_cast<void>(game_random::crt::Rand());
	}
	return game_random::crt::Rand();
}

/// One line of a fake map script: its command and what it does
struct FakeCommand
{
	std::string name;
	std::function<void()> run;
};
} // namespace

TEST(CloudSystem, ALandOpenCountsTheLandscapeAndLaysOutTheSkyOnce)
{
	FakeSkyFrame sky;
	RecordingClouds clouds(sky);

	Clouds::OnLandscapeOpened(sky, clouds);

	EXPECT_EQ(sky.GetLandscapeGeneration(), 1u);
	// one layout, made after the landscape was counted
	ASSERT_EQ(clouds.resets.size(), 1u);
	EXPECT_EQ(clouds.resets[0], 1u);
	EXPECT_EQ(clouds.updates, 0);
}

TEST(CloudSystem, ALandOpenDrawsFourHundredAndTwentyNumbersAtOnce)
{
	FakeSkyFrame sky;
	CloudSystem clouds;
	game_random::crt::Srand(1);

	Clouds::OnLandscapeOpened(sky, clouds);
	const auto next = game_random::crt::Rand();

	ASSERT_NE(clouds.GetLayout(), nullptr);
	game_random::crt::Srand(1);
	const Clouds expected;
	ExpectSameClouds(*clouds.GetLayout(), expected);
	// 70 clouds x 6 draws, all made by the open itself
	EXPECT_EQ(next, CrtNumberAfter(420));
}

TEST(CloudSystem, TheSkyIsLaidOutAtTheMapScriptsLoadLandscapeLine)
{
	// The land's script: its first lines draw nothing, LOAD_LANDSCAPE opens the land (and its sky), and a street
	// lantern made by a later line takes its first draw (its jitter clock) after the sky's 420
	FakeSkyFrame sky;
	CloudSystem clouds;
	std::vector<std::string> ran;
	float lanternClock = -1.0f;
	const std::vector<FakeCommand> script = {
	    {"VERSION", [] {}},
	    {"SET_LAND_NUMBER", [] {}},
	    {"LOAD_LANDSCAPE", [&] { Clouds::OnLandscapeOpened(sky, clouds); }},
	    {"CREATE_STREET_LANTERN", [&] { lanternClock = game_random::crt::Random(0.0f, 30.0f); }},
	};
	game_random::crt::Srand(1);
	for (const auto& command : script)
	{
		ran.push_back(command.name);
		command.run();
		if (command.name != "LOAD_LANDSCAPE")
		{
			continue;
		}
		// the layout is there as soon as the line has run
		ASSERT_NE(clouds.GetLayout(), nullptr);
		EXPECT_EQ(sky.GetLandscapeGeneration(), 1u);
	}

	ASSERT_EQ(ran.size(), script.size());
	// the lantern's draw is the stream's 421st number
	game_random::crt::Srand(1);
	for (int i = 0; i < 420; ++i)
	{
		static_cast<void>(game_random::crt::Rand());
	}
	EXPECT_EQ(lanternClock, game_random::crt::Random(0.0f, 30.0f));
}

TEST(CloudSystem, TheFirstDrawnFrameMakesNoLayoutDraws)
{
	FakeSkyFrame sky;
	CloudSystem clouds;
	game_random::crt::Srand(1);
	Clouds::OnLandscapeOpened(sky, clouds);
	const auto* layout = clouds.GetLayout();
	ASSERT_NE(layout, nullptr);

	// the frame's cloud work: the wind, the edge alphas, the colour and the animation of every cloud
	clouds.Update(Milliseconds(33.0f));
	for (size_t i = 0; i < clouds.GetLayout()->GetClouds().size(); ++i)
	{
		static_cast<void>(Clouds::EdgeAlpha(clouds.GetLayout()->GetClouds()[i]));
		clouds.GetLayout()->AdvanceAnimation(i, 33.0f);
	}
	static_cast<void>(Clouds::Colour(0.0f, 0xFFFFFFFFu));

	// the same layout, and the stream still where the land's open left it
	const auto next = game_random::crt::Rand();
	EXPECT_EQ(clouds.GetLayout(), layout);
	EXPECT_EQ(next, CrtNumberAfter(420));
}

TEST(CloudSystem, ASecondLandOpenLaysOutANewSky)
{
	FakeSkyFrame sky;
	CloudSystem clouds;
	game_random::crt::Srand(1);
	Clouds::OnLandscapeOpened(sky, clouds);
	const auto firstLocal = clouds.GetLayout()->GetClouds()[2].local;

	Clouds::OnLandscapeOpened(sky, clouds);
	const auto next = game_random::crt::Rand();

	EXPECT_EQ(sky.GetLandscapeGeneration(), 2u);
	game_random::crt::Srand(1);
	const Clouds first;
	const Clouds second;
	ExpectSameClouds(*clouds.GetLayout(), second);
	EXPECT_NE(clouds.GetLayout()->GetClouds()[2].local, firstLocal);
	EXPECT_EQ(next, CrtNumberAfter(840));
}
