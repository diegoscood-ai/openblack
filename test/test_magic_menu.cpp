/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstring>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Debug/MagicModel.h"
#include "InfoConstants.h"
#include "Particles/ParticleTypes.h"

using namespace openblack;
using namespace openblack::debug::magic_window;

namespace
{
void Name(std::array<char, 0x30>& field, std::string_view name)
{
	field.fill('\0');
	std::memcpy(field.data(), name.data(), std::min(name.size(), field.size() - 1));
}
} // namespace

TEST(MagicMenu, SectionNames)
{
	EXPECT_EQ(SectionName(magic::MagicInfoSection::General), "General");
	EXPECT_EQ(SectionName(magic::MagicInfoSection::StormAndTornado), "Storm and tornado");
	EXPECT_EQ(SectionName(magic::MagicInfoSection::CreatureSpell), "Creature spell");
	EXPECT_EQ(SectionName(magic::MagicInfoSection::_COUNT), "?");
}

TEST(MagicMenu, NamesFromTheTables)
{
	auto info = std::make_unique<InfoConstants>();
	Name(info->magicEffect.at(static_cast<size_t>(MagicType::Fireball)).debugString, "MAGIC_TYPE_FIREBALL");
	Name(info->spellSeed.at(static_cast<size_t>(SpellSeedType::Fire)).debugString, "SPELL_SEED_TYPE_FIRE");
	EXPECT_EQ(MagicName(*info, MagicType::Fireball), "MAGIC_TYPE_FIREBALL");
	// Without a name in the tables, the number
	EXPECT_EQ(MagicName(*info, MagicType::Heal), "Magic type 10");
	EXPECT_EQ(SeedName(*info, SpellSeedType::Fire), "SPELL_SEED_TYPE_FIRE");
	EXPECT_EQ(SeedName(*info, SpellSeedType::None), "none");
}

TEST(MagicMenu, Texts)
{
	EXPECT_EQ(Seconds(-1.0f), "no limit");
	EXPECT_EQ(Seconds(0.0f), "0.0 s");
	EXPECT_EQ(Seconds(12.34f), "12.3 s");
	EXPECT_EQ(PowerUpLevelName(-1), "base");
	EXPECT_EQ(PowerUpLevelName(2), "2");
	EXPECT_EQ(DrawPathName(particles::DrawPath::Sorted), "Sorted");
	EXPECT_EQ(DrawPathName(particles::DrawPath::Queued), "Queued");
	EXPECT_EQ(DrawPathName(particles::DrawPath::Immediate), "Immediate");
}

TEST(MagicMenu, ParticleTypeLabels)
{
	// the number, the name and the file
	const auto none = ParticleTypeLabel(ParticleType::None);
	EXPECT_EQ(none.rfind("0 ", 0), 0u);
	EXPECT_NE(none.find("(no file)"), std::string::npos);
	const auto smoke = ParticleTypeLabel(ParticleType::Smoke);
	EXPECT_EQ(smoke.rfind("115 ", 0), 0u);
	EXPECT_NE(smoke.find(particles::ParticleTypeName(ParticleType::Smoke)), std::string::npos);
	EXPECT_EQ(smoke.find("no file"), std::string::npos);
}

TEST(MagicMenu, NearestTargetsAreGivenLast)
{
	const auto a = static_cast<entt::entity>(1);
	const auto b = static_cast<entt::entity>(2);
	const auto c = static_cast<entt::entity>(3);
	const std::vector<TargetCandidate> candidates {
	    {.distance = 30.0f, .entity = a}, {.distance = 10.0f, .entity = b}, {.distance = 20.0f, .entity = c}};
	// the two nearest, the nearest given last
	EXPECT_EQ(TargetsInGivingOrder(candidates, 2), (std::vector {c, b}));
	EXPECT_EQ(TargetsInGivingOrder(candidates, 10), (std::vector {a, c, b}));
	EXPECT_TRUE(TargetsInGivingOrder(candidates, 0).empty());
	EXPECT_TRUE(TargetsInGivingOrder({}, 3).empty());
}
