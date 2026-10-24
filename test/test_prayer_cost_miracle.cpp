/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A seed's prayer power pinned to the original game: its power is the share of its miracle's cost to create it holds,
// never above full, and a new power-up level gives a charge above that level's cost back to the worship site of the
// icon that charged it. The rules and the costs (heal 6000, its power-up 9000) are the original's
// (docs/wiki/magic/spell-core.md, the seed accessors; docs/wiki/project/progress/miracles/heal.md)

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/SpellSeed.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::ecs::components::SpellSeed;
using openblack::ecs::components::WorshipSite;
using openblack::ecs::components::WorshipSpellIcon;

namespace
{
/// The heal's cost to create, and its power-up's, in the game's file
constexpr float k_HealCost = 6000.0f;
constexpr float k_HealPowerUpCost = 9000.0f;

/// Each magic record's own magic type set to where the record stands, as the game's file has them
void NumberMagicRecords(InfoConstants& info)
{
	uint32_t next = 0;
	const auto number = [&next](auto& records) {
		for (auto& record : records)
		{
			record.magicType = static_cast<MagicType>(next++);
		}
	};
	number(info.magicGeneral);
	number(info.magicHeal);
	number(info.magicTeleport);
	number(info.magicForest);
	number(info.magicFood);
	number(info.magicStormAndTornado);
	number(info.magicShield);
	number(info.magicWood);
	number(info.magicWater);
	number(info.magicFlockFlying);
	number(info.magicFlockGround);
	number(info.magicCreatureSpell);
}

/// An empty registry and the tables the test sets, each put back when the test ends
class PrayerCost: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::infoConstants::reset();
	}

	[[nodiscard]] ecs::Registry& Reg() const { return *_registry; }

	/// Zeroed tables where the heal seed's plain miracle is the heal and its first power-up the heal's power-up, each
	/// costing what the game's file gives it
	static void UseHealTables(float healCost = k_HealCost)
	{
		auto info = std::make_unique<InfoConstants>();
		NumberMagicRecords(*info);
		info->spellSeed.at(static_cast<size_t>(SpellSeedType::Heal)).magicTypes = {MagicType::Heal, MagicType::HealPowerUpOne,
		                                                                           MagicType::None, MagicType::None};
		info->magicEffect.at(static_cast<size_t>(MagicType::Heal)).costToCreate = healCost;
		info->magicEffect.at(static_cast<size_t>(MagicType::HealPowerUpOne)).costToCreate = k_HealPowerUpCost;
		Locator::infoConstants::reset(info.release());
	}

	/// A heal seed at a level, holding `store` chants, made at the icon when one is given
	[[nodiscard]] entt::entity HealSeed(int powerUp, float store, entt::entity icon = entt::null) const
	{
		const auto entity = Reg().Create();
		auto& seed = Reg().Assign<SpellSeed>(entity);
		seed.seedType = SpellSeedType::Heal;
		seed.powerUp = powerUp;
		seed.icon = icon;
		magic::seed::SetChantStore(seed, store);
		return entity;
	}

	/// A worship site holding `battery` chants and an icon of it; returns the icon
	[[nodiscard]] entt::entity IconAtSite(float battery, entt::entity& site) const
	{
		site = Reg().Create();
		Reg().Assign<WorshipSite>(site).battery = battery;
		const auto icon = Reg().Create();
		Reg().Assign<WorshipSpellIcon>(icon).site = site;
		return icon;
	}

	[[nodiscard]] float PowerOf(entt::entity seed) const { return magic::seed::GetPower(Reg().Get<const SpellSeed>(seed)); }

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	ecs::Registry* _registry {nullptr};
};
} // namespace

// The power is min(store / cost, 1), the cost being that of the seed's level
TEST_F(PrayerCost, ASeedsPowerIsTheShareOfItsLevelsCostItHoldsAtMostFull)
{
	UseHealTables();
	const auto plain = HealSeed(-1, 0.0f);
	auto& seed = Reg().Get<SpellSeed>(plain);
	EXPECT_EQ(PowerOf(plain), 0.0f);
	magic::seed::SetChantStore(seed, 1500.0f);
	EXPECT_EQ(PowerOf(plain), 0.25f);
	magic::seed::SetChantStore(seed, 3000.0f);
	EXPECT_EQ(PowerOf(plain), 0.5f);
	magic::seed::SetChantStore(seed, k_HealCost);
	EXPECT_EQ(PowerOf(plain), 1.0f);
	// more than the cost is still full
	magic::seed::SetChantStore(seed, 7000.0f);
	EXPECT_EQ(PowerOf(plain), 1.0f);

	// at the power-up level the share is of the power-up's cost
	const auto poweredUp = HealSeed(0, 4500.0f);
	EXPECT_EQ(PowerOf(poweredUp), 0.5f);
	Reg().Get<SpellSeed>(plain).powerUp = 0;
	EXPECT_FLOAT_EQ(PowerOf(plain), 7000.0f / k_HealPowerUpCost);
}

// The original divides without a test: a store over a cost of 0 is infinite or not a number, and the min against 1
// then gives 1
TEST_F(PrayerCost, ASeedOfAMiracleThatCostsNothingIsAtFullPower)
{
	UseHealTables(0.0f);
	const auto entity = HealSeed(-1, 0.0f);
	EXPECT_EQ(PowerOf(entity), 1.0f);
	magic::seed::SetChantStore(Reg().Get<SpellSeed>(entity), 250.0f);
	EXPECT_EQ(PowerOf(entity), 1.0f);
}

// A new level: a charge above that level's cost goes back to the battery of the worship site of the seed's icon, and
// the seed keeps the cost; a charge below it stays in the seed for the icon to charge the rest
TEST_F(PrayerCost, APowerUpGivesAChargeAboveItsCostBackToTheIconsSite)
{
	UseHealTables();
	entt::entity site = entt::null;
	const auto icon = IconAtSite(100.0f, site);

	// a plain seed holding 9500 powered up: the 500 above the power-up's 9000 goes back
	const auto entity = HealSeed(-1, 9500.0f, icon);
	magic::seed::SetPowerUp(entity, 0);
	EXPECT_EQ(Reg().Get<const SpellSeed>(entity).powerUp, 0);
	EXPECT_EQ(Reg().Get<const SpellSeed>(entity).chantStore, k_HealPowerUpCost);
	EXPECT_EQ(Reg().Get<const SpellSeed>(entity).chantStoreCopy, k_HealPowerUpCost);
	EXPECT_EQ(Reg().Get<const WorshipSite>(site).battery, 600.0f);

	// back down to the plain level: the 3000 above the heal's 6000 goes back
	magic::seed::SetPowerUp(entity, -1);
	EXPECT_EQ(Reg().Get<const SpellSeed>(entity).powerUp, -1);
	EXPECT_EQ(Reg().Get<const SpellSeed>(entity).chantStore, k_HealCost);
	EXPECT_EQ(Reg().Get<const WorshipSite>(site).battery, 3600.0f);

	// a charge below the new level's cost stays in the seed, and the site gets nothing
	const auto shortSeed = HealSeed(-1, 6000.0f, icon);
	magic::seed::SetPowerUp(shortSeed, 0);
	EXPECT_EQ(Reg().Get<const SpellSeed>(shortSeed).chantStore, 6000.0f);
	EXPECT_EQ(Reg().Get<const WorshipSite>(site).battery, 3600.0f);
	EXPECT_EQ(magic::seed::GetChantNeeded(Reg().Get<const SpellSeed>(shortSeed), 0), 3000.0f);
}

// Only a seed made at an icon refunds: a loose seed keeps all its charge
TEST_F(PrayerCost, ALooseSeedKeepsItsChargeAboveTheNewLevelsCost)
{
	UseHealTables();
	const auto entity = HealSeed(0, 9500.0f);
	magic::seed::SetPowerUp(entity, -1);
	EXPECT_EQ(Reg().Get<const SpellSeed>(entity).powerUp, -1);
	EXPECT_EQ(Reg().Get<const SpellSeed>(entity).chantStore, 9500.0f);
}

/// With OPENBLACK_GAME_PATH set to the install: the heal seed of the game's file, at its two levels
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST_F(PrayerCost, realInfoDatHealSeedPower)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	// Scripts/info.dat: a 0x2C-byte pack header, then the InfoConstants block
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	Locator::infoConstants::reset(info.release());
	const auto plain = HealSeed(-1, 3000.0f);
	EXPECT_EQ(PowerOf(plain), 0.5f);
	const auto poweredUp = HealSeed(0, 4500.0f);
	EXPECT_EQ(PowerOf(poweredUp), 0.5f);
	EXPECT_EQ(magic::seed::GetChantNeeded(Reg().Get<const SpellSeed>(plain), 0), k_HealPowerUpCost - 3000.0f);
}

/// The same reads on the synthetic tables
TEST_F(PrayerCost, realInfoDatHealSeedPowerSynthetic)
{
	UseHealTables();
	const auto plain = HealSeed(-1, 3000.0f);
	EXPECT_EQ(PowerOf(plain), 0.5f);
	const auto poweredUp = HealSeed(0, 4500.0f);
	EXPECT_EQ(PowerOf(poweredUp), 0.5f);
	EXPECT_EQ(magic::seed::GetChantNeeded(Reg().Get<const SpellSeed>(plain), 0), k_HealPowerUpCost - 3000.0f);
}
