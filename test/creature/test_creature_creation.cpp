/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A new creature's draws: their order and count, the personality's clamp, the ranges that make no draw, and the values
// the draws make. On a fake stream and synthetic tables, never the game's data

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureCreation.h"
#include "Creature/CreatureDesires.h"
#include "Enums.h"
#include "InfoConstants.h"

using namespace openblack;
using creature_creation::Make;
using creature_creation::Setup;
using creature_desires::k_DesireCount;

namespace
{
/// One call to the stream: a whole number below the range, or a float towards it
struct Call
{
	bool whole {false};
	float range {0.0f};

	bool operator==(const Call&) const = default;
};

/// A stream that records every call. Floats come from a script while it lasts, then half the range; whole numbers
/// are half the range
class FakeRandom
{
public:
	FakeRandom() = default;
	FakeRandom(const FakeRandom&) = delete;
	FakeRandom& operator=(const FakeRandom&) = delete;

	std::vector<Call> calls;
	std::deque<float> scripted;

	[[nodiscard]] creature_creation::Random Random()
	{
		return {
		    .rand =
		        [this](uint32_t range) {
			        calls.push_back({.whole = true, .range = static_cast<float>(range)});
			        return range / 2;
		        },
		    .floatRand =
		        [this](float range) {
			        calls.push_back({.whole = false, .range = range});
			        if (scripted.empty())
			        {
				        return range * 0.5f;
			        }
			        const float value = scripted.front();
			        scripted.pop_front();
			        return value;
		        },
		};
	}
};

/// The sources of each desire laid out as the game's tables have them: anger's in the order 6, 9, 7, and 8, 10 and 11
/// in none. 58 in all
std::array<std::vector<uint32_t>, k_DesireCount> GameLikeSources()
{
	std::array<std::vector<uint32_t>, k_DesireCount> ids {{
	    {0, 1},
	    {2, 3, 4, 5},
	    {6, 9, 7},
	    {12, 13},
	    {14, 15, 16},
	    {17, 18, 19},
	    {20},
	    {21},
	    {22, 23, 24, 25, 26},
	}};
	uint32_t next = 27;
	for (size_t d = 9; d < 16; ++d)
	{
		ids.at(d) = {next++};
	}
	ids.at(16) = {34, 35};
	ids.at(17) = {36, 37};
	ids.at(18) = {38, 39};
	next = 40;
	for (size_t d = 19; d < k_DesireCount; ++d)
	{
		ids.at(d) = {next++};
	}
	return ids;
}

/// A setup like the game's: every desire with a decay range and a growing time, the sources above with spreads of 0.1
/// and 0.2 (0.1 and 0.1 for source 22), every personality value 0.5, and 328 actions
Setup GameLikeSetup()
{
	Setup setup;
	setup.strength = 0.6f;
	setup.startFatness = 0.5f;
	const auto ids = GameLikeSources();
	for (size_t d = 0; d < k_DesireCount; ++d)
	{
		auto& desire = setup.desires.at(d);
		desire.decayMin = 0.9f;
		desire.decayMax = 0.9f + (0.001f * static_cast<float>(d + 1));
		desire.increaseSeconds = static_cast<float>(d + 1);
		for (const auto id : ids.at(d))
		{
			desire.sources.push_back({.type = id, .value = 0.5f, .threshold = 0.5f});
			setup.spreads.at(d).push_back({.value = 0.1f, .threshold = id == 22 ? 0.1f : 0.2f});
		}
	}
	setup.personality.fill(0.5f);
	setup.actionCount = 328;
	return setup;
}

/// A setup whose desires draw nothing: no decay range, no growing time, no source
Setup QuietSetup()
{
	Setup setup;
	for (auto& desire : setup.desires)
	{
		desire.decayMin = 0.5f;
		desire.decayMax = 0.5f;
		desire.increaseSeconds = 0.0f;
	}
	setup.personality.fill(0.5f);
	return setup;
}
} // namespace

TEST(CreatureCreation, TheDrawsComeInTheGamesOrder)
{
	const auto setup = GameLikeSetup();
	FakeRandom random;
	const auto made = Make(setup, random.Random());

	std::vector<Call> expected;
	// the strength, then the fatness
	expected.push_back({.whole = false, .range = 0.2f});
	expected.push_back({.whole = false, .range = 0.2f});
	// each desire's decay in its range, then its growing time moved by a fifth either way
	for (const auto& desire : setup.desires)
	{
		expected.push_back({.whole = false, .range = desire.decayMax - desire.decayMin});
		const float fifth = 0.2f * desire.increaseSeconds;
		expected.push_back({.whole = false, .range = fifth + fifth});
	}
	// each desire's sources in their order: the value, then the threshold
	size_t sources = 0;
	for (size_t d = 0; d < k_DesireCount; ++d)
	{
		for (const auto& source : setup.desires.at(d).sources)
		{
			expected.push_back({.whole = false, .range = 0.2f});
			expected.push_back({.whole = false, .range = source.type == 22 ? 0.2f : 0.4f});
			++sources;
		}
	}
	EXPECT_EQ(sources, 58u);
	// the personality: four draws each about 0.5, the game's widths
	for (const float range : {0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.6f, 0.8f, 0.8f, 0.8f})
	{
		for (int i = 0; i < 4; ++i)
		{
			expected.push_back({.whole = false, .range = range});
		}
	}
	// a whole number below 120 for each action
	for (size_t i = 0; i < 328; ++i)
	{
		expected.push_back({.whole = true, .range = 120.0f});
	}

	ASSERT_EQ(expected.size(), 2u + 80u + 116u + 36u + 328u);
	ASSERT_EQ(random.calls.size(), expected.size());
	for (size_t i = 0; i < expected.size(); ++i)
	{
		EXPECT_EQ(random.calls.at(i), expected.at(i)) << i;
	}
	// anger's sources keep the tables' order
	const auto& anger = made.desires[creature_desires::Desire::Anger].sources;
	ASSERT_EQ(anger.size(), 3u);
	EXPECT_EQ(anger.at(0).type, 6u);
	EXPECT_EQ(anger.at(1).type, 9u);
	EXPECT_EQ(anger.at(2).type, 7u);
	EXPECT_EQ(made.previousActions, std::vector<uint32_t>(328, 60u));
}

namespace
{
/// How many draws the first personality value takes, and what it comes to, about a value with the script's draws for
/// it. The strength and fatness come first, and the other eight values draw four times each
std::pair<size_t, float> FirstPersonality(float innate, std::deque<float> draws)
{
	auto setup = QuietSetup();
	setup.personality.at(0) = innate;
	FakeRandom random;
	random.scripted = {0.1f, 0.1f};
	random.scripted.insert(random.scripted.end(), draws.begin(), draws.end());
	const auto made = Make(setup, random.Random());
	EXPECT_TRUE(random.scripted.empty());
	return {random.calls.size() - 2 - (8 * 4), made.personality.at(0)};
}
} // namespace

TEST(CreatureCreation, EachTestOfThePersonalitysClampDrawsAgain)
{
	// above 0, then 1 or more: 1, after two draws
	EXPECT_EQ(FirstPersonality(0.5f, {0.15f, 0.7f}), std::make_pair(size_t {2}, 1.0f));
	// 0 or less, then 0 or less again: 0, after two draws
	EXPECT_EQ(FirstPersonality(0.05f, {0.0f, 0.0f}), std::make_pair(size_t {2}, 0.0f));
	// 0 or less, then above 0: a fourth value, after three draws
	EXPECT_EQ(FirstPersonality(0.05f, {0.0f, 0.2f, 0.1f}), std::make_pair(size_t {3}, (0.1f + 0.05f) - 0.1f));
	// above 0, below 1, then 0 or less: 0, after three draws
	EXPECT_EQ(FirstPersonality(0.05f, {0.2f, 0.1f, 0.0f}), std::make_pair(size_t {3}, 0.0f));
	// above 0, below 1, above 0: a fourth value, after four draws
	EXPECT_EQ(FirstPersonality(0.05f, {0.2f, 0.1f, 0.2f, 0.15f}), std::make_pair(size_t {4}, (0.15f + 0.05f) - 0.1f));
}

TEST(CreatureCreation, ARangeOf0MakesNoDraw)
{
	auto setup = QuietSetup();
	// a source with no spread
	setup.desires.at(0).sources.push_back({.type = 3, .value = 0.4f, .threshold = 0.6f});
	setup.spreads.at(0).push_back({});
	FakeRandom random;
	const auto made = Make(setup, random.Random());
	// the strength and fatness, and the personality's 36: nothing for the desires or the source
	EXPECT_EQ(random.calls.size(), 2u + 36u);
	EXPECT_TRUE(std::ranges::none_of(random.calls, [](const Call& call) { return call.range == 0.0f; }));
	EXPECT_FLOAT_EQ(made.desires.desires.at(0).decay, 0.5f);
	EXPECT_EQ(made.desires.desires.at(0).increaseSeconds, 0.0f);
	ASSERT_EQ(made.desires.desires.at(0).sources.size(), 1u);
	EXPECT_EQ(made.desires.desires.at(0).sources.at(0).value, 0.4f);
	EXPECT_EQ(made.desires.desires.at(0).sources.at(0).threshold, 0.6f);
	EXPECT_TRUE(made.previousActions.empty());
}

TEST(CreatureCreation, TheDrawsMakeTheValues)
{
	auto setup = QuietSetup();
	setup.strength = 0.6f;
	setup.startFatness = 0.5f;
	auto& desire = setup.desires.at(0);
	desire.max = 2.0f;
	desire.weight = 0.75f;
	desire.decayMin = 0.25f;
	desire.decayMax = 0.75f;
	desire.increaseSeconds = 10.0f;
	desire.sources.push_back({.type = 1, .value = 0.95f, .threshold = 0.05f, .multiplier = 0.5f});
	desire.sources.push_back({.type = 2, .value = 0.05f, .threshold = 0.9f});
	setup.spreads.at(0) = {{.value = 0.1f, .threshold = 0.2f}, {.value = 0.1f, .threshold = 0.2f}};
	FakeRandom random;
	random.scripted = {0.05f, 0.15f, 0.3f, 1.0f, 0.2f, 0.0f, 0.0f, 0.4f};
	const auto made = Make(setup, random.Random());

	// the draw added to the species' value, then 0.1 taken away
	EXPECT_EQ(made.strength, (0.05f + 0.6f) - 0.1f);
	EXPECT_EQ(made.fatness, (0.15f + 0.5f) - 0.1f);
	const auto& state = made.desires.desires.at(0);
	EXPECT_EQ(state.max, 2.0f);
	EXPECT_EQ(state.weight, 0.75f);
	// the draw over the bottom of the range
	EXPECT_EQ(state.decay, 0.3f + 0.25f);
	// the fifth taken from the draw, then the growing time added
	const float fifth = 0.2f * 10.0f;
	EXPECT_EQ(state.increaseSeconds, (1.0f - fifth) + 10.0f);
	ASSERT_EQ(state.sources.size(), 2u);
	// over 1: 1; below 0: 0, for the value and the threshold alike
	EXPECT_EQ(state.sources.at(0).value, 1.0f);
	EXPECT_EQ(state.sources.at(0).threshold, 0.0f);
	EXPECT_EQ(state.sources.at(0).multiplier, 0.5f);
	EXPECT_EQ(state.sources.at(1).value, 0.0f);
	EXPECT_EQ(state.sources.at(1).threshold, 1.0f);
	// a source moved within the limits keeps the moved value
	auto inside = QuietSetup();
	inside.desires.at(0).sources.push_back({.type = 1, .value = 0.5f, .threshold = 0.5f});
	inside.spreads.at(0).push_back({.value = 0.1f, .threshold = 0.2f});
	FakeRandom other;
	other.scripted = {0.0f, 0.0f, 0.15f, 0.1f};
	const auto moved = Make(inside, other.Random()).desires.desires.at(0).sources.at(0);
	EXPECT_EQ(moved.value, (0.15f - 0.1f) + 0.5f);
	EXPECT_EQ(moved.threshold, (0.1f - 0.2f) + 0.5f);
}

TEST(CreatureCreation, TheSetupIsTheSpeciesTables)
{
	auto info = std::make_unique<InfoConstants>();
	auto& ape = info->creature.at(0);
	ape.strength = 0.6f;
	ape.startFatness = 0.5f;
	ape.innateKindness = 0.1f;
	ape.innateCommunicativeness = 0.3f;
	ape.field0x360 = 0.5f;
	auto& anger = info->creatureInitialDesire.at(static_cast<size_t>(creature_desires::Desire::Anger));
	anger.field0x0 = 6;
	anger.field0x4 = creature_desires::k_NoSource;
	anger.field0x8 = 9;
	anger.field0xc = 7;
	anger.field0x10 = creature_desires::k_NoSource;
	anger.field0x14 = creature_desires::k_NoSource;
	anger.field0x18 = creature_desires::k_NoSource;
	anger.field0x1c = creature_desires::k_NoSource;
	info->desireSourceTable.at(9).field0x8 = 0.95f;
	info->desireSourceTable.at(9).field0xc = 0.1f;
	info->desireSourceTable.at(9).field0x10 = 0.2f;
	info->creatureInitialSource1.at(9).field0x0 = 0.25f;
	info->creatureInitialSource2.at(9).field0x0 = 0.75f;

	const auto setup = creature_creation::SetupFor(*info, CreatureType::GiantApe);
	EXPECT_EQ(setup.strength, 0.6f);
	EXPECT_EQ(setup.startFatness, 0.5f);
	EXPECT_EQ(setup.personality.at(0), 0.1f);
	EXPECT_EQ(setup.personality.at(4), 0.3f);
	EXPECT_EQ(setup.personality.at(8), 0.5f);
	EXPECT_EQ(setup.actionCount, 328u);
	// a slot with no source is passed over and the next read; each source has its spreads beside it
	const auto angerAt = static_cast<size_t>(creature_desires::Desire::Anger);
	const auto& sources = setup.desires.at(angerAt).sources;
	ASSERT_EQ(sources.size(), 3u);
	ASSERT_EQ(setup.spreads.at(angerAt).size(), 3u);
	EXPECT_EQ(sources.at(0).type, 6u);
	EXPECT_EQ(sources.at(1).type, 9u);
	EXPECT_EQ(sources.at(2).type, 7u);
	EXPECT_EQ(sources.at(1).value, 0.25f);
	EXPECT_EQ(sources.at(1).threshold, 0.75f);
	EXPECT_EQ(sources.at(1).multiplier, 0.95f);
	EXPECT_EQ(setup.spreads.at(angerAt).at(1).value, 0.1f);
	EXPECT_EQ(setup.spreads.at(angerAt).at(1).threshold, 0.2f);
}
