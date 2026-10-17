/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <optional>

#include <gtest/gtest.h>

#include "ECS/VillagerSpeed.h"

using namespace openblack::ecs::villager_speed;

namespace
{
Inputs Fleeing()
{
	Inputs in;
	in.speeds = {1000, 2000, 0, 0, 500, 0};
	in.speedIndex = 1;
	in.lifeWhenWalksWounded = 0.3f;
	in.lifeWhenCrawlsWounded = 0.1f;
	return in;
}
float Zero(float /*max*/)
{
	return 0.0f;
}
} // namespace

TEST(VillagerSpeed, AStatesSpeedScaledByThePlayerAndItsTown)
{
	auto in = Fleeing();
	EXPECT_EQ(StateSpeed(in, Zero), 2000);
	in.landSpeedBalance = 1.5f;
	EXPECT_EQ(StateSpeed(in, Zero), 3000);
	// Believed beyond the neutral belief by 2: a tenth of it, times the story land's scale
	in.landSpeedBalance = 1.0f;
	in.belief = Inputs::Belief {.player = 3.0f, .neutral = 1.0f};
	in.landNumber = 2;
	in.beliefSpeedScaleStory = {0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f};
	EXPECT_EQ(StateSpeed(in, Zero), static_cast<int32_t>(2000.0f * (0.5f * 0.2f + 1.0f)));
	// Not beyond it: the whole belief counts
	in.belief = Inputs::Belief {.player = 0.5f, .neutral = 1.0f};
	EXPECT_EQ(StateSpeed(in, Zero), static_cast<int32_t>(2000.0f * (0.5f * 0.5f + 1.0f)));
}

TEST(VillagerSpeed, ThePlayersScaleOfEverySpeed)
{
	auto in = Fleeing();
	in.landSpeedBalance = 1.5f;
	EXPECT_EQ(Scale(in), 1.5f);
	// At least a tenth of the Indian tribal power
	in.indianPower = 0.05f;
	EXPECT_EQ(Scale(in), 1.5f * 0.1f);
	in.indianPower = 2.0f;
	EXPECT_EQ(Scale(in), 3.0f);
	// The land's own belief scale when it is not 1, else the multiplayer one before the story land's
	in.indianPower.reset();
	in.landSpeedBalance = 1.0f;
	in.belief = Inputs::Belief {.player = 3.0f, .neutral = 1.0f};
	in.beliefSpeedScaleMultiPlayer = 4.0f;
	in.beliefSpeedScaleStory = {1.0f, 1.0f, 1.5f, 2.0f, 3.0f, 3.0f};
	in.landNumber = 2;
	EXPECT_EQ(Scale(in), 1.5f * 0.2f + 1.0f);
	in.multiplayer = true;
	EXPECT_EQ(Scale(in), 4.0f * 0.2f + 1.0f);
	in.landBeliefSpeedScale = 2.0f;
	EXPECT_EQ(Scale(in), 2.0f * 0.2f + 1.0f);
	// Past the story lands the scale stays 1
	in.multiplayer = false;
	in.landBeliefSpeedScale = 1.0f;
	in.landNumber = 6;
	EXPECT_EQ(Scale(in), 0.2f + 1.0f);
}

TEST(VillagerSpeed, TheTownsPlayerGivesThePowerAndTheBelief)
{
	// No town: neither term
	const auto none = PlayerTermsOf(std::nullopt);
	EXPECT_FALSE(none.indianPower.has_value());
	EXPECT_FALSE(none.belief.has_value());
	// A town of the neutral player: the power only
	const auto neutral = PlayerTermsOf(TownPlayer {.neutralPlayer = true, .indianPower = 0.05f, .beliefInPlayer = 3.0f});
	ASSERT_TRUE(neutral.indianPower.has_value());
	EXPECT_EQ(*neutral.indianPower, 0.05f);
	EXPECT_FALSE(neutral.belief.has_value());
	// A town of another player: the power, and its belief in the player and its own neutral belief
	const auto owned = PlayerTermsOf(
	    TownPlayer {.neutralPlayer = false, .indianPower = 1.0f, .beliefInPlayer = 3.0f, .beliefInNeutral = 0.5f});
	ASSERT_TRUE(owned.indianPower.has_value());
	EXPECT_EQ(*owned.indianPower, 1.0f);
	ASSERT_TRUE(owned.belief.has_value());
	EXPECT_EQ(owned.belief->player, 3.0f);
	EXPECT_EQ(owned.belief->neutral, 0.5f);
	// On land 2 its fleeing speed goes up by the story land's scale times a tenth of the belief beyond the neutral one
	auto in = Fleeing();
	in.indianPower = owned.indianPower;
	in.belief = owned.belief;
	in.beliefSpeedScaleStory = {1.0f, 1.0f, 1.5f, 2.0f, 3.0f, 3.0f};
	in.landNumber = 2;
	EXPECT_EQ(StateSpeed(in, Zero), static_cast<int32_t>(2000.0f * (1.5f * (2.5f * 0.1f) + 1.0f)));
}

TEST(VillagerSpeed, WoundedLoadedNeededAndSpedUp)
{
	auto in = Fleeing();
	in.life = 0.05f;
	EXPECT_EQ(StateSpeed(in, Zero), 200);
	in.life = 0.2f;
	EXPECT_EQ(StateSpeed(in, Zero), 500);
	in.life = 1.0f;
	in.townInEmergency = true;
	EXPECT_EQ(StateSpeed(in, Zero), 1500);
	in.townInEmergency = false;
	in.townNeeds = 1.0f;
	in.divisorForTownNeedsSpeedMod = 1.0f;
	in.baseForTownNeedsSpeedMod = 1.0f;
	EXPECT_EQ(StateSpeed(in, Zero), 3000);
	in.townNeeds.reset();
	in.woodHeld = 1.0f;
	in.speedModWhenFullLoadOfWood = 0.5f;
	EXPECT_EQ(StateSpeed(in, Zero), static_cast<int32_t>(2000.0f * 0.75f));
	in.woodHeld = 0.0f;
	in.foodSpeedUp = true;
	in.foodPowerupIncrease = 1.5f;
	EXPECT_EQ(StateSpeed(in, Zero), 3000);
}

TEST(VillagerSpeed, EachVillagersOwnWayOfGoing)
{
	// Made first: 16 hundredths slow
	EXPECT_NEAR(PersonalFactor({.creationIndex = 0, .age = 30, .grownUpAge = 18, .oldAge = 60, .life = 0.0f}), 0.84f, 1e-5f);
	EXPECT_NEAR(PersonalFactor({.creationIndex = 1, .age = 30, .grownUpAge = 18, .oldAge = 60, .life = 1.0f, .female = true}),
	            (47 % 31 - 16) * 0.01f + 1.0f - 0.1f - 0.2f, 1e-5f);
	// Young: slower by a fiftieth a year short of grown, at most four tenths
	EXPECT_NEAR(PersonalFactor({.creationIndex = 0, .age = 8, .grownUpAge = 18, .oldAge = 60}), 0.84f - 0.2f, 1e-5f);
	EXPECT_NEAR(PersonalFactor({.creationIndex = 0, .age = 0, .grownUpAge = 40, .oldAge = 60}), 0.84f - 0.4f, 1e-5f);
	EXPECT_EQ(FinalSpeed(1000, 0.84f), 840);
	EXPECT_EQ(FinalSpeed(-5, 1.0f), 0);
	EXPECT_EQ(FinalSpeed(70000, 1.0f), 0xFFFF);
}

// Ours: a capacity or a town needs divisor of 0 (only made-up villagers have one) gives no load or needs term, as
// villager::LoadFactors and villager::TownNeedsFactor have it. His version would divide by 0 and clamp, slowing it to
// 0.75 and quickening it by the most needs, 2250 here
TEST(VillagerSpeed, NoCapacityOrNeedsDivisorGivesNoTerm)
{
	auto in = Fleeing();
	in.maxWood = 0.0f;
	in.woodHeld = 5.0f;
	in.townNeeds = 1.0f;
	in.divisorForTownNeedsSpeedMod = 0.0f;
	in.baseForTownNeedsSpeedMod = 1.0f;
	EXPECT_EQ(StateSpeed(in, Zero), 2000);
}

TEST(VillagerSpeed, ScriptedAndDancingVillagersKeepTheirSpeed)
{
	EXPECT_TRUE(StateChangeSetsSpeed(false, false));
	// A script set its speed: a new state doesn't change it
	EXPECT_FALSE(StateChangeSetsSpeed(true, false));
	EXPECT_FALSE(StateChangeSetsSpeed(false, true));
	EXPECT_FALSE(StateChangeSetsSpeed(true, true));
}

namespace
{
// The player's scale of a state's speed as the original game works it out, its exact bits, for a villager whose town
// and player are given and with the land's balance and the town tables' belief scales set
struct OriginalScaleCase
{
	std::optional<float> indianPower;
	std::optional<Inputs::Belief> belief;
	float landSpeedBalance;
	float landBeliefSpeedScale;
	float beliefSpeedScaleMultiPlayer;
	bool skirmish;
	uint32_t landNumber;
	std::array<float, 6> beliefSpeedScaleStory;
	uint32_t scale;
};
// The story lands' belief scales of the game's town tables, and made-up ones that tell the lands apart
constexpr std::array k_GameStory {1.0f, 1.0f, 1.5f, 2.0f, 3.0f, 3.0f};
constexpr std::array k_DistinctStory {0.25f, 1.125f, 1.75f, 2.375f, 3.5f, 4.625f};
// Each case: the town player's Indian tribal power (none without a town), the town's belief in its player and in the
// neutral player (none without a town or for the neutral player's town), the land's speed balance and belief scale, the
// multiplayer scale, a skirmish game, the land number, the story lands' scales, and the bits of the scale the original works
// out
const auto k_OriginalScaleCases = std::to_array<OriginalScaleCase>({
    // No town: neither term
    {std::nullopt, std::nullopt, 1.0f, 1.0f, 4.0f, false, 1u, k_GameStory, 0x3F800000u},
    // A town of the neutral player: its power only
    {2.5f, std::nullopt, 1.0f, 1.0f, 4.0f, false, 1u, k_GameStory, 0x40200000u},
    // The neutral player's power below a tenth counts as a tenth
    {0.05f, std::nullopt, 1.0f, 1.0f, 4.0f, false, 1u, k_GameStory, 0x3DCCCCCDu},
    // A town of another player believing beyond its neutral belief
    {1.0f, Inputs::Belief {3.0f, 0.5f}, 1.0f, 1.0f, 4.0f, false, 1u, k_GameStory, 0x3FA00000u},
    // Believing as much as its neutral belief: the whole belief counts
    {1.0f, Inputs::Belief {0.5f, 0.5f}, 1.0f, 1.0f, 4.0f, false, 1u, k_GameStory, 0x3FC00000u},
    // Believing less than its neutral belief: the whole belief counts
    {1.0f, Inputs::Belief {0.3f, 0.5f}, 1.0f, 1.0f, 4.0f, false, 1u, k_GameStory, 0x3FA66666u},
    // The player's power below a tenth counts as a tenth
    {0.05f, Inputs::Belief {3.0f, 0.5f}, 1.0f, 1.0f, 4.0f, false, 1u, k_GameStory, 0x3E000000u},
    // Just above a tenth it counts as it is
    {0.10000001f, Inputs::Belief {3.0f, 0.5f}, 1.0f, 1.0f, 4.0f, false, 1u, k_GameStory, 0x3E000001u},
    // The land's own belief scale when it is not 1
    {1.0f, Inputs::Belief {3.0f, 0.5f}, 1.0f, 2.75f, 4.0f, false, 4u, k_GameStory, 0x3FD80000u},
    // The land's speed balance times the power
    {1.3f, Inputs::Belief {2.2f, 0.5f}, 0.8f, 1.0f, 4.0f, false, 3u, k_GameStory, 0x3FB2617Cu},
    // A skirmish game's belief scale
    {1.0f, Inputs::Belief {3.0f, 0.5f}, 1.0f, 1.0f, 4.0f, true, 2u, k_GameStory, 0x40000000u},
    // A story land's belief scale
    {1.1f, Inputs::Belief {2.6f, 0.4f}, 1.0f, 1.0f, 4.0f, false, 3u, k_DistinctStory, 0x3FD65E36u},
    // The last story land
    {1.1f, Inputs::Belief {2.6f, 0.4f}, 1.0f, 1.0f, 4.0f, false, 5u, k_DistinctStory, 0x400E0831u},
    // Past the story lands the belief scale is 1
    {1.1f, Inputs::Belief {2.6f, 0.4f}, 1.0f, 1.0f, 4.0f, false, 6u, k_DistinctStory, 0x3FABC6A8u},
    // And for any land number past them
    {1.1f, Inputs::Belief {2.6f, 0.4f}, 1.0f, 1.0f, 4.0f, false, UINT32_MAX, k_DistinctStory, 0x3FABC6A8u},
});
} // namespace

TEST(VillagerSpeed, ThePlayersScaleIsTheOriginalsToTheBit)
{
	for (const auto& c : k_OriginalScaleCases)
	{
		Inputs in;
		in.landSpeedBalance = c.landSpeedBalance;
		in.landBeliefSpeedScale = c.landBeliefSpeedScale;
		in.indianPower = c.indianPower;
		in.belief = c.belief;
		in.beliefSpeedScaleMultiPlayer = c.beliefSpeedScaleMultiPlayer;
		in.beliefSpeedScaleStory = c.beliefSpeedScaleStory;
		in.multiplayer = c.skirmish;
		in.landNumber = c.landNumber;
		EXPECT_EQ(std::bit_cast<uint32_t>(Scale(in)), c.scale) << "land " << c.landNumber;
	}
}
