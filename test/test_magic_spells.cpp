/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <memory>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <gtest/gtest.h>

#include "Enums.h"
#include "InfoConstants.h"
#include "Magic/DispenserRules.h"

using namespace openblack;
using namespace openblack::magic;

// Dispensers: the pure rules of a dispenser's timer and bubble, with our counting (the tick goes back to 0 only when
// a bubble is made)

TEST(DispenserRules, ADispenserCountsToItsPeriodWhileItsBubbleIsGoneAndAsksAgainUntilOneIsMade)
{
	DispenserTimer timer {.tick = 0, .period = 3, .active = false};
	// inactive, or without a magic: nothing counts
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::Wait);
	EXPECT_EQ(timer.tick, 0u);
	timer.active = true;
	EXPECT_EQ(StepDispenser(timer, false, false, false), DispenserStep::Wait);
	EXPECT_EQ(timer.tick, 0u);
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::Wait);
	EXPECT_EQ(timer.tick, 1u);
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::Wait);
	EXPECT_EQ(timer.tick, 2u);
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::MakeOrb);
	EXPECT_EQ(timer.tick, 3u);
	// with no orb made the tick goes on, and every turn asks again
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::MakeOrb);
	EXPECT_EQ(timer.tick, 4u);
	// an orb made (the tick back to 0): it waits while the orb is on it
	timer.tick = 0;
	EXPECT_EQ(StepDispenser(timer, true, true, true), DispenserStep::Wait);
	EXPECT_EQ(StepDispenser(timer, true, true, true), DispenserStep::Wait);
	EXPECT_EQ(timer.tick, 0u);
	// taken: that turn only forgets it, the count starts on the next
	timer.tick = 2;
	EXPECT_EQ(StepDispenser(timer, true, false, true), DispenserStep::OrbTaken);
	EXPECT_EQ(timer.tick, 0u);
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::Wait);
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::Wait);
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::MakeOrb);
	// inactive again: its orb is still forgotten when taken, but nothing counts
	timer.active = false;
	EXPECT_EQ(StepDispenser(timer, true, false, true), DispenserStep::OrbTaken);
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::Wait);
	EXPECT_EQ(timer.tick, 0u);
}

TEST(DispenserRules, TheOrbFloatsAboveItsDispenserAndStaysThereWithinHalfAMetreAcrossTheLand)
{
	const auto orb = OrbPosition({10.0f, 2.0f, 20.0f}, 5.0f);
	EXPECT_FLOAT_EQ(orb.x, 10.0f);
	EXPECT_FLOAT_EQ(orb.y, 2.0f + 5.0f * 1.2f);
	EXPECT_FLOAT_EQ(orb.z, 20.0f);
	// the height does not count, only the distance across the land, up to half a metre and the touch tolerance
	EXPECT_TRUE(OrbStillThere({10.3f, 99.0f, 20.0f}, orb));
	EXPECT_TRUE(OrbStillThere({10.5f, 8.0f, 20.0f}, orb));
	EXPECT_TRUE(OrbStillThere({10.5009f, 8.0f, 20.0f}, orb));
	EXPECT_FALSE(OrbStillThere({10.502f, 8.0f, 20.0f}, orb));
	EXPECT_FALSE(OrbStillThere({11.0f, 8.0f, 20.0f}, orb));
}

TEST(DispenserRules, ABubbleFacesAlongAnyDirection)
{
	for (const auto direction : {glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 2.0f, -3.0f), glm::vec3(0.0f, -1.0f, 0.0f)})
	{
		const auto turn = FaceTowards(direction);
		const auto up = turn * glm::vec3(0.0f, 1.0f, 0.0f);
		EXPECT_NEAR(glm::dot(up, glm::normalize(direction)), 1.0f, 1e-5f);
		EXPECT_NEAR(glm::determinant(turn), 1.0f, 1e-5f);
	}
	EXPECT_TRUE(FaceTowards(glm::vec3(0.0f)) == glm::mat3(1.0f));
}

TEST(DispenserRules, EveryDispensableMiracleIsAMiracleOnce)
{
	const auto miracles = DispensableMiracles();
	EXPECT_EQ(miracles.size(), 34u);
	EXPECT_EQ(miracles.front(), MagicType::Fireball);
	EXPECT_EQ(std::ranges::count(miracles, MagicType::None), 0);
	for (const auto type : miracles)
	{
		EXPECT_EQ(std::ranges::count(miracles, type), 1) << static_cast<int>(type);
	}
}

TEST(DispenserRules, TheDispenserBuildingsAreThoseOfTheirTypeNorseFirst)
{
	// zeroed tables: no building is a dispenser
	const auto info = std::make_unique<InfoConstants>();
	EXPECT_TRUE(DispenserAbodes(*info).empty());
	EXPECT_FALSE(DefaultDispenserAbode({}).has_value());

	info->abode.at(static_cast<size_t>(AbodeInfo::CelticSpellDispenser)).abodeType = AbodeType::SpellDispenser;
	info->abode.at(static_cast<size_t>(AbodeInfo::NorseSpellDispenser)).abodeType = AbodeType::SpellDispenser;
	const std::vector<AbodeInfo> expected {AbodeInfo::CelticSpellDispenser, AbodeInfo::NorseSpellDispenser};
	const auto abodes = DispenserAbodes(*info);
	EXPECT_EQ(abodes, expected);
	EXPECT_EQ(DefaultDispenserAbode(abodes), AbodeInfo::NorseSpellDispenser);
	const std::vector<AbodeInfo> celtic {AbodeInfo::CelticSpellDispenser};
	EXPECT_EQ(DefaultDispenserAbode(celtic), AbodeInfo::CelticSpellDispenser);
}
