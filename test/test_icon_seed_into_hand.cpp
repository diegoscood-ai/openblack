/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A globe's miracle given into the hand, with the player's worship sites and their icons: the seed is made at the
// player's icon for that miracle when there is one (the icon whose site has the most prayer power available, the first
// on a tie, never another player's), and loose otherwise. A power-up asked of the icon of the seed in the hand waits
// until the icon has charged the extra, and the scribble's cancel puts the pending level back with the seed unchanged.
// docs/wiki/project/progress/miracles/casting_and_globes.md

// the seeds take their creation index from the real index service the test puts in
#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ObjectCreationIndexSystem.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Worship/PlayerSpellIcons.h"
#include "Worship/WorshipSpellIcon.h"
#include "creature/CreatureSystemFakes.h"
#include "support/RestoreService.h"
#include "support/WorshipIconFakes.h"

using namespace openblack;
using openblack::ecs::components::SpellCreator;
using openblack::ecs::components::SpellSeed;
using openblack::ecs::components::WorshipSite;
using openblack::ecs::components::WorshipSpellIcon;
namespace icons = openblack::test::worship_icons;

namespace
{
constexpr auto k_Me = PlayerNames::PLAYER_ONE;
constexpr auto k_Rival = PlayerNames::PLAYER_TWO;
/// The fire seed's plain miracle and its first power-up, and what each costs to create
constexpr float k_FireballCost = 3500.0f;
constexpr float k_PowerUpCost = 5000.0f;

/// An empty registry and creation index, the player human with every miracle, the hand a fake standing over the land, and
/// tables where the fire seed is a fireball with one power-up; each put back when the test ends
class IconSeedIntoHand: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the seed's creation logs to the game's log
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::objectCreationIndexSystem::emplace<ecs::systems::ObjectCreationIndexSystem>();
		_hand = &static_cast<test::creature_fakes::FakeHand&>(Locator::handSystem::emplace<test::creature_fakes::FakeHand>());
		_hand->position = glm::vec3(24.0f, 5.0f, 40.0f);
		Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
		icons::UseSeedLevels(SpellSeedType::Fire, icons::SeedLevels {
		                                              .types = {MagicType::Fireball, MagicType::FireballPowerUpOne,
		                                                        MagicType::None, MagicType::None},
		                                              .costs = {k_FireballCost, k_PowerUpCost, 0.0f, 0.0f},
		                                          });
		icons::AddHumanPlayer(*_registry, k_Me);
		_citadel = icons::AddCitadel(*_registry, k_Me);
	}

	[[nodiscard]] ecs::Registry& Reg() const { return *_registry; }
	[[nodiscard]] const SpellSeed& Seed(entt::entity seed) const { return Reg().Get<const SpellSeed>(seed); }
	[[nodiscard]] WorshipSpellIcon& Icon(entt::entity icon) const { return Reg().Get<WorshipSpellIcon>(icon); }

	/// A globe of the seed tapped into the empty hand, its miracle plain and at full size; the fake hand then holds
	/// the seed, as the hand's place does
	[[nodiscard]] entt::entity Globe(SpellSeedType seed) const
	{
		const auto entity = magic::one_off::CreateSpellIntoHand(k_Me, seed, -1, 1.0f);
		if (entity != entt::null)
		{
			_hand->held = entity;
		}
		return entity;
	}

	ecs::Registry* _registry {nullptr};
	test::creature_fakes::FakeHand* _hand {nullptr};
	entt::entity _citadel {entt::null};

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::objectCreationIndexSystem> _restoreIndex;
	test::RestoreService<Locator::handSystem> _restoreHand;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
};
} // namespace

// With an icon of the globe's miracle at one of the player's sites, the seed in the hand is that icon's: the icon made
// it and charges it, and the icon finds it in the hand; the seed is fully charged from the globe and made where the
// hand is, on the land
TEST_F(IconSeedIntoHand, AGlobesSeedIsMadeAtThePlayersIconOfThatMiracle)
{
	const auto [site, icon] = icons::AddSiteWithIcon(Reg(), _citadel, 0, SpellSeedType::Fire, 100.0f);

	const auto seed = Globe(SpellSeedType::Fire);
	ASSERT_NE(seed, entt::entity {entt::null});
	EXPECT_EQ(Seed(seed).icon, icon);
	EXPECT_EQ(Seed(seed).creator.kind, SpellCreator::Kind::WorshipSpellIcon);
	EXPECT_EQ(Seed(seed).creator.entity, icon);
	EXPECT_EQ(Seed(seed).creator.player, k_Me);
	EXPECT_EQ(worship::icon::GetHeldSpellSeed(icon), seed);
	EXPECT_TRUE(Seed(seed).fromOneShot);
	EXPECT_TRUE(Seed(seed).ready);
	EXPECT_FLOAT_EQ(Seed(seed).chantStore, k_FireballCost);
	EXPECT_EQ(Seed(seed).madeAt.x, map_coords::ToFixed(24.0f));
	EXPECT_EQ(Seed(seed).madeAt.z, map_coords::ToFixed(40.0f));
	EXPECT_EQ(Seed(seed).madeAt.altitude, 0.0f);
	// the globe's charge is its own: the icon and its site keep theirs
	EXPECT_FLOAT_EQ(Icon(icon).chantStore, 0.0f);
	EXPECT_FLOAT_EQ(Reg().Get<const WorshipSite>(site).battery, 0.0f);
}

// With no icon of the globe's miracle the seed is loose: no icon made it and it is the player's own
TEST_F(IconSeedIntoHand, AGlobeWithoutAnIconOfItsMiracleGivesALooseSeed)
{
	const auto fire = icons::AddSiteWithIcon(Reg(), _citadel, 0, SpellSeedType::Fire, 100.0f).icon;

	const auto seed = Globe(SpellSeedType::Storm);
	ASSERT_NE(seed, entt::entity {entt::null});
	EXPECT_EQ(Seed(seed).icon, entt::entity {entt::null});
	EXPECT_EQ(Seed(seed).creator.kind, SpellCreator::Kind::Player);
	EXPECT_EQ(Seed(seed).creator.player, k_Me);
	EXPECT_EQ(worship::icon::GetHeldSpellSeed(fire), entt::entity {entt::null});
}

// Another player's icon of the miracle is never the one: the seed is loose
TEST_F(IconSeedIntoHand, ARivalsIconOfTheMiracleIsNotUsed)
{
	icons::AddHumanPlayer(Reg(), k_Rival);
	const auto rivalCitadel = icons::AddCitadel(Reg(), k_Rival);
	icons::AddSiteWithIcon(Reg(), rivalCitadel, 0, SpellSeedType::Fire, 100.0f);

	const auto seed = Globe(SpellSeedType::Fire);
	ASSERT_NE(seed, entt::entity {entt::null});
	EXPECT_EQ(Seed(seed).icon, entt::entity {entt::null});
}

// Of two sites with an icon of the miracle, the one with more prayer power available for its icons wins; on a tie the
// first of the citadel's slots does
TEST_F(IconSeedIntoHand, TheIconOfTheSiteWithTheMostPrayerPowerAvailableWinsTheFirstOnATie)
{
	const auto first = icons::AddSiteWithIcon(Reg(), _citadel, 0, SpellSeedType::Fire, 100.0f);
	const auto second = icons::AddSiteWithIcon(Reg(), _citadel, 1, SpellSeedType::Fire, 400.0f);
	EXPECT_EQ(worship::player::FindBestSpellIconForSpellSeed(k_Me, SpellSeedType::Fire), second.icon);
	const auto seed = Globe(SpellSeedType::Fire);
	ASSERT_NE(seed, entt::entity {entt::null});
	EXPECT_EQ(Seed(seed).icon, second.icon);

	// the same power at both: the first slot's
	Reg().Get<WorshipSite>(first.site).available = 400.0f;
	EXPECT_EQ(worship::player::FindBestSpellIconForSpellSeed(k_Me, SpellSeedType::Fire), first.icon);
}

// A power-up asked of the icon whose seed is in the hand starts the icon charging for that level; the seed stays at its
// level while the icon has not charged the extra, and takes the level the turn it has, the icon then charging nothing
TEST_F(IconSeedIntoHand, APowerUpWaitsForTheIconToChargeTheExtra)
{
	const auto icon = icons::AddSiteWithIcon(Reg(), _citadel, 0, SpellSeedType::Fire, 100.0f).icon;
	const auto seed = Globe(SpellSeedType::Fire);
	ASSERT_EQ(Seed(seed).icon, icon);

	ASSERT_TRUE(worship::player::SetChargingPowerUp(icon, k_Me, 0));
	EXPECT_TRUE(worship::icon::IsCharging(icon, k_Me, false));
	EXPECT_EQ(Icon(icon).powerUp, 0);
	// the extra is the power-up's cost over what the seed holds
	EXPECT_FLOAT_EQ(worship::icon::GetChantNeeded(icon), k_PowerUpCost - k_FireballCost);

	// part of the extra charged: the icon gives it to the seed, which keeps its level
	Icon(icon).chantStore = 1000.0f;
	worship::icon::Process(icon);
	EXPECT_EQ(Seed(seed).powerUp, -1);
	EXPECT_FLOAT_EQ(Seed(seed).chantStore, k_FireballCost + 1000.0f);
	EXPECT_FLOAT_EQ(Icon(icon).chantStore, 0.0f);
	EXPECT_TRUE(Icon(icon).charging);

	// the rest charged: the seed takes the level and the icon stops charging
	Icon(icon).chantStore = k_PowerUpCost - k_FireballCost - 1000.0f;
	worship::icon::Process(icon);
	EXPECT_EQ(Seed(seed).powerUp, 0);
	EXPECT_FLOAT_EQ(Seed(seed).chantStore, k_PowerUpCost);
	EXPECT_FALSE(Icon(icon).charging);
	EXPECT_EQ(Icon(icon).powerUp, -1);
}

// The scribble's cancel of a pending power-up stops the icon charging and puts its level back; the seed in the hand
// keeps its level and what it holds, and what the icon had gathered goes back to its site
TEST_F(IconSeedIntoHand, CancellingAPendingPowerUpLeavesTheSeedAtItsLevel)
{
	const auto [site, icon] = icons::AddSiteWithIcon(Reg(), _citadel, 0, SpellSeedType::Fire, 100.0f);
	const auto seed = Globe(SpellSeedType::Fire);
	ASSERT_TRUE(worship::player::SetChargingPowerUp(icon, k_Me, 0));
	// a charge started on turn 0 is never the most recent one, so the charge is taken as started later
	Icon(icon).chargeStartTurn = 5;
	Icon(icon).chantStore = 700.0f;

	EXPECT_TRUE(worship::player::CancelMostRecentCharge(k_Me));
	EXPECT_FALSE(Icon(icon).charging);
	EXPECT_EQ(Icon(icon).powerUp, -1);
	EXPECT_FLOAT_EQ(Icon(icon).chantStore, 0.0f);
	EXPECT_FLOAT_EQ(Reg().Get<const WorshipSite>(site).battery, 700.0f);
	EXPECT_EQ(Seed(seed).powerUp, -1);
	EXPECT_FLOAT_EQ(Seed(seed).chantStore, k_FireballCost);
	// nothing is charging any more
	EXPECT_FALSE(worship::player::CancelMostRecentCharge(k_Me));
}
