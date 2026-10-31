/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The worship site's icons pinned to the original game: tapping an icon that is charging cancels the charge of the
// hand that started it, back at the plain miracle, and gives everything it holds back to its worship site's battery;
// without a site what it held is lost. The rule is the original's
// (docs/wiki/magic/worship.md#cancelling-and-returning-the-seed)

#include <memory>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/WorshipSpellIcon.h"
#include "creature/CreatureSystemFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::ecs::components::WorshipSite;
using openblack::ecs::components::WorshipSpellIcon;

namespace
{
/// An empty registry, put back when the test ends
class DispenserSeedsIcons: public ::testing::Test
{
protected:
	void SetUp() override { _registry = &Locator::entitiesRegistry::emplace<ecs::Registry>(); }

	[[nodiscard]] ecs::Registry& Reg() const { return *_registry; }

	/// A worship site holding `battery` chants
	[[nodiscard]] entt::entity Site(float battery) const
	{
		const auto site = Reg().Create();
		Reg().Assign<WorshipSite>(site).battery = battery;
		return site;
	}

	/// An icon of the site holding `stored` chants, charging the level for the player's hand
	[[nodiscard]] entt::entity ChargingIcon(entt::entity site, float stored, PlayerNames player, int powerUp) const
	{
		const auto icon = Reg().Create();
		Reg().Assign<WorshipSpellIcon>(icon, WorshipSpellIcon {
		                                         .site = site,
		                                         .charging = true,
		                                         .powerUp = powerUp,
		                                         .chargingFor = player,
		                                         .hasChargingInterface = true,
		                                         .chantStore = stored,
		                                     });
		return icon;
	}

	[[nodiscard]] const WorshipSpellIcon& Icon(entt::entity icon) const { return Reg().Get<const WorshipSpellIcon>(icon); }
	[[nodiscard]] float Battery(entt::entity site) const { return Reg().Get<const WorshipSite>(site).battery; }

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	ecs::Registry* _registry {nullptr};
};
} // namespace

// Only the hand that charges the icon cancels it; then it stops charging, goes back to the plain miracle, no hand
// charges it, and all it held goes into its site's battery
TEST_F(DispenserSeedsIcons, TappingAChargingIconCancelsItAndGivesThePrayerPowerBackToTheSite)
{
	const auto site = Site(100.0f);
	const auto icon = ChargingIcon(site, 2500.0f, PlayerNames::PLAYER_ONE, 0);

	// another player's hand does not cancel it
	EXPECT_FALSE(worship::icon::CancelCharge(icon, PlayerNames::PLAYER_TWO));
	EXPECT_TRUE(Icon(icon).charging);
	EXPECT_EQ(Icon(icon).chantStore, 2500.0f);
	EXPECT_EQ(Battery(site), 100.0f);

	EXPECT_TRUE(worship::icon::CancelCharge(icon, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(Icon(icon).charging);
	EXPECT_EQ(Icon(icon).powerUp, -1);
	EXPECT_FALSE(Icon(icon).hasChargingInterface);
	EXPECT_EQ(Icon(icon).chantStore, 0.0f);
	EXPECT_EQ(Battery(site), 2600.0f);

	// nothing left to cancel
	EXPECT_FALSE(worship::icon::CancelCharge(icon, PlayerNames::PLAYER_ONE));
	EXPECT_EQ(Battery(site), 2600.0f);
}

// A charge just started, with nothing in it yet, is cancelled all the same
TEST_F(DispenserSeedsIcons, AChargeWithNothingInItYetIsCancelled)
{
	const auto site = Site(40.0f);
	const auto icon = ChargingIcon(site, 0.0f, PlayerNames::PLAYER_ONE, -1);
	EXPECT_TRUE(worship::icon::CancelCharge(icon, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(Icon(icon).charging);
	EXPECT_EQ(Battery(site), 40.0f);
}

// An icon that holds chants but that no hand charges is not cancelled, and keeps them
TEST_F(DispenserSeedsIcons, AnIconNoHandChargesKeepsWhatItHolds)
{
	const auto site = Site(0.0f);
	const auto icon = ChargingIcon(site, 300.0f, PlayerNames::PLAYER_ONE, -1);
	auto& charge = Reg().Get<WorshipSpellIcon>(icon);
	charge.charging = false;
	charge.hasChargingInterface = false;
	EXPECT_FALSE(worship::icon::CancelCharge(icon, PlayerNames::PLAYER_ONE));
	EXPECT_EQ(Icon(icon).chantStore, 300.0f);
	EXPECT_EQ(Battery(site), 0.0f);
}

// Without a worship site the chants an icon gives back have nowhere to go: its store is emptied all the same
TEST_F(DispenserSeedsIcons, AnIconWithoutASiteLosesWhatItHeldWhenCancelled)
{
	const auto icon = ChargingIcon(entt::null, 700.0f, PlayerNames::PLAYER_ONE, 1);
	EXPECT_TRUE(worship::icon::CancelCharge(icon, PlayerNames::PLAYER_ONE));
	EXPECT_EQ(Icon(icon).chantStore, 0.0f);
	EXPECT_EQ(Icon(icon).powerUp, -1);
}

// The click of a tap rises with the icon's place: the first six places, and every place past them as the sixth
TEST(DispenserSeedsIconTap, TheTapsClickRisesWithTheIconsPlaceUpToTheSixth)
{
	EXPECT_EQ(worship::icon::TapSoundPitch(0), 100);
	EXPECT_EQ(worship::icon::TapSoundPitch(1), 115);
	EXPECT_EQ(worship::icon::TapSoundPitch(2), 130);
	EXPECT_EQ(worship::icon::TapSoundPitch(3), 145);
	EXPECT_EQ(worship::icon::TapSoundPitch(4), 155);
	EXPECT_EQ(worship::icon::TapSoundPitch(5), 175);
	EXPECT_EQ(worship::icon::TapSoundPitch(6), 175);
	EXPECT_EQ(worship::icon::TapSoundPitch(-1), 100);
}

// A tap on a site's icon with the hand empty starts it charging for the tapping player, when its miracle is one the
// player has and nothing is held; the charge waits for its prayer power (here 1000 to make)
TEST_F(DispenserSeedsIcons, TappingAnIconWithAnEmptyHandStartsItChargingForThatPlayer)
{
	test::RestoreService<Locator::handSystem> restoreHand;
	test::RestoreService<Locator::infoConstants> restoreInfo;
	auto& hand = static_cast<test::creature_fakes::FakeHand&>(Locator::handSystem::emplace<test::creature_fakes::FakeHand>());
	auto info = std::make_unique<InfoConstants>();
	for (auto& row : info->magicEffect)
	{
		row.costToCreate = 1000.0f;
	}
	Locator::infoConstants::reset(info.release());
	// the human player, who has every miracle
	const auto player = Reg().Create();
	Reg().Assign<ecs::components::Player>(player, PlayerNames::PLAYER_ONE);
	auto& magic = Reg().Assign<ecs::components::PlayerMagic>(player);
	magic.playerType = 1;
	magic.allMagicCheat = true;
	// an idle icon of a site with some prayer power
	const auto site = Site(500.0f);
	const auto icon = Reg().Create();
	Reg().Assign<ecs::components::SpellIcon>(icon).seedType = SpellSeedType::Storm;
	Reg().Assign<WorshipSpellIcon>(icon, WorshipSpellIcon {.site = site, .slot = 10});
	ASSERT_FALSE(Icon(icon).charging);
	// a tap while the hand holds something does not start it
	hand.held = Reg().Create();
	EXPECT_EQ(worship::icon::InterfaceTap(icon, PlayerNames::PLAYER_ONE), 1);
	EXPECT_FALSE(Icon(icon).charging);
	// with the hand empty it does
	hand.held.reset();
	EXPECT_EQ(worship::icon::InterfaceTap(icon, PlayerNames::PLAYER_ONE), 1);
	EXPECT_TRUE(Icon(icon).charging);
	EXPECT_EQ(Icon(icon).chargingFor, PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(Icon(icon).hasChargingInterface);
	EXPECT_EQ(Icon(icon).powerUp, -1);
	EXPECT_EQ(Icon(icon).chantStore, 0.0f);
}
