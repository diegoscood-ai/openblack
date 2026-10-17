/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The village totem's rules (ECS/VillageTotem.h) and its ease. The ease tests keep raffclar's names, but are run on
// our ease: the worship percentage's Zoomer (worship::percentage::SetRise, StepRise and TakeRiseArrival), in place of
// his closed-form Ease. Where his expected values hold for ours they are asserted as he wrote them. The hand's slide,
// the grip and the grip's test keep his names where his rules are the original's, with the original's numbers.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <limits>
#include <numbers>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "3D/CreatureBody.h"
#include "Common/GUtilsAngle.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillageTotemSystem.h"
#include "ECS/VillageTotem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/WorshipPercentage.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace vt = openblack::ecs::village_totem;
namespace percentage = openblack::worship::percentage;

// The arrival is compared in double, against the float 0.005 widened
static_assert(static_cast<double>(vt::k_ArrivedWithin) == 0.004999999888241291);

TEST(VillageTotemEase, FromRestItTakesTheTimeOfTheWholeWayAndArrivesStill)
{
	TotemWorship worship;
	percentage::SetRise(worship, 1.0f);
	EXPECT_FLOAT_EQ(worship.rise.duration, 5200.0f);
	// Half way through the time it is past half way: it sets off fast and settles slowly (6u^2 - 8u^3 + 3u^4)
	percentage::StepRise(worship, 2600.0f);
	EXPECT_NEAR(worship.rise.value, 0.6875f, 1e-4f);
	EXPECT_GT(worship.rise.speed, 0.0f);
	percentage::StepRise(worship, 2600.0f);
	EXPECT_FLOAT_EQ(worship.rise.value, 1.0f);
	EXPECT_FLOAT_EQ(worship.rise.speed, 0.0f);
}

TEST(VillageTotemEase, ItSetsOffAsTheMatrixOfTheCurveSays)
{
	// Moving at some speed part of the way, it is sent somewhere new: the curve meets the target at rest with no
	// acceleration. Our Zoomer's coefficients c4, c3 and c2 are his snap, jerk and acceleration
	TotemWorship worship;
	percentage::SetRise(worship, 1.0f);
	percentage::StepRise(worship, 1000.0f);
	const float s0 = worship.rise.value;
	const float v0 = worship.rise.speed;
	percentage::SetRise(worship, 0.25f);
	const float t = worship.rise.duration;
	EXPECT_FLOAT_EQ(t, 0.75f * 5200.0f);
	// Rows as the game lays them out, the unknowns the snap, jerk and acceleration
	const glm::dmat3 m(glm::dvec3(std::pow(t, 4) / 24.0, std::pow(t, 3) / 6.0, t * t / 2.0),
	                   glm::dvec3(std::pow(t, 3) / 6.0, t * t / 2.0, t), glm::dvec3(t * t / 2.0, t, 1.0));
	const glm::dvec3 rhs(0.25 - s0 - v0 * t, -v0, 0.0);
	const auto solved = glm::inverse(m) * rhs;
	EXPECT_NEAR(worship.rise.c4, solved.x, std::abs(solved.x) * 1e-3);
	EXPECT_NEAR(worship.rise.c3, solved.y, std::abs(solved.y) * 1e-3);
	EXPECT_NEAR(worship.rise.c2, solved.z, std::abs(solved.z) * 1e-3 + 1e-12);
	percentage::StepRise(worship, t);
	EXPECT_FLOAT_EQ(worship.rise.value, 0.25f);
}

TEST(VillageTotemEase, ANewShareSoNearIsTakenAtOnceAndStillRingsTheBell)
{
	TotemWorship worship;
	percentage::SetRise(worship, 0.0f);
	EXPECT_FLOAT_EQ(worship.rise.duration, 0.0f);
	EXPECT_TRUE(percentage::TakeRiseArrival(worship));
	EXPECT_FALSE(percentage::TakeRiseArrival(worship));
	// (ours) a share whose ease would be shorter than the Zoomer's shortest move is put there at once; one just longer
	// eases
	TotemWorship nearly;
	EXPECT_LT(1e-7f * vt::k_EaseMsPerShare, vt::k_SnapMs);
	percentage::SetRise(nearly, 1e-7f);
	EXPECT_EQ(nearly.rise.duration, 0.0f);
	EXPECT_EQ(nearly.rise.value, 1e-7f);
	TotemWorship justOver;
	EXPECT_GT(2e-7f * vt::k_EaseMsPerShare, vt::k_SnapMs);
	percentage::SetRise(justOver, 2e-7f);
	EXPECT_GT(justOver.rise.duration, 0.0f);
	EXPECT_EQ(justOver.rise.value, 0.0f);
}

TEST(VillageTotemEase, ItArrivesOnceWithinTheBellsReach)
{
	TotemWorship worship;
	percentage::SetRise(worship, 1.0f);
	percentage::StepRise(worship, 100.0f);
	EXPECT_FALSE(percentage::TakeRiseArrival(worship));
	percentage::StepRise(worship, 5200.0f);
	EXPECT_TRUE(percentage::TakeRiseArrival(worship));
	EXPECT_FALSE(percentage::TakeRiseArrival(worship));
}

TEST(VillageTotemRules, TheStatueRisesEightUnitsAtFullShare)
{
	EXPECT_FLOAT_EQ(vt::RiseOf(0.5f), 4.0f);
	EXPECT_EQ(vt::RiseOf(0.0f), 0.0f);
	EXPECT_EQ(vt::RiseOf(1.0f), 8.0f);
	// The share times eight, the same float as eight times the share
	for (const float share : {0.1f, 0.2f, 0.3f, 0.6875f, 0.99f})
	{
		EXPECT_EQ(vt::RiseOf(share), 8.0f * share) << share;
	}
}

TEST(VillageTotemRules, TheShareShowsAsAPercentage)
{
	EXPECT_FLOAT_EQ(vt::AsPercentage(0.4f), 40.0f);
	EXPECT_EQ(vt::AsPercentage(0.0f), 0.0f);
	EXPECT_EQ(vt::AsPercentage(1.0f), 100.0f);
}

TEST(VillageTotemRules, TheHandSlidesThreeIconHeightsAScreenAndStopsAtTheLand)
{
	EXPECT_FLOAT_EQ(vt::HandSlide(100.0f, 600.0f, 4.0f, 10.0f), 2.0f);
	EXPECT_FLOAT_EQ(vt::HandSlide(-100.0f, 600.0f, 4.0f, 10.0f), -2.0f);
	EXPECT_FLOAT_EQ(vt::HandSlide(-100.0f, 600.0f, 4.0f, 0.5f), -0.5f);
	EXPECT_FLOAT_EQ(vt::SlideShare(0.5f, 2.0f), 0.7f);
	EXPECT_FLOAT_EQ(vt::SlideShare(0.95f, 2.0f), 1.0f);
	EXPECT_FLOAT_EQ(vt::SlideShare(0.05f, -2.0f), 0.0f);
	// the pixels times the icon's height times 3, over the screen's height: ten pixels up a 768-pixel screen with an
	// icon 2 high, a tenth of that for the share
	EXPECT_EQ(vt::HandSlide(10.0f, 768.0f, 2.0f, 10.0f), 10.0f * 2.0f * 3.0f / 768.0f);
	EXPECT_EQ(vt::SlideShare(0.25f, 0.078125f), 0.078125f * 0.1f + 0.25f);
	// (ours) no screen, no slide
	EXPECT_EQ(vt::HandSlide(10.0f, 0.0f, 2.0f, 10.0f), 0.0f);
}

TEST(VillageTotemRules, AHandBelowTheLandIsBroughtUpToItWhicheverWayItSlides)
{
	// the stop is the original's test as it reads: further down than the hand is above the land, which a hand under
	// the land always is, so its slide is to the land
	EXPECT_FLOAT_EQ(vt::HandSlide(10.0f, 600.0f, 4.0f, -1.0f), 1.0f);
	EXPECT_FLOAT_EQ(vt::HandSlide(-10.0f, 600.0f, 4.0f, -1.0f), 1.0f);
	// at the land, going up is free and going down is no slide
	EXPECT_FLOAT_EQ(vt::HandSlide(100.0f, 600.0f, 4.0f, 0.0f), 2.0f);
	EXPECT_EQ(vt::HandSlide(-100.0f, 600.0f, 4.0f, 0.0f), 0.0f);
}

TEST(VillageTotemRules, TheHandHoldsTheIconSevenTenthsOfItsHeightUp)
{
	EXPECT_FLOAT_EQ(vt::GripY(10.0f, 2.0f), 11.4f);
	EXPECT_EQ(vt::GripY(0.0f, 1.0f), 0.7f);
}

TEST(VillageTotemRules, OnlyTheTownsPlayerWithTheirTempleAndAWorshipSiteMayTakeIt)
{
	constexpr vt::GripFacts k_All {.samePlayer = true, .functional = true, .heartBuilt = true, .siteBuilt = true};
	EXPECT_TRUE(vt::ValidForLockedSelect(k_All));
	// another player's town, a totem that is not functional, a temple with no heart built, no worship site built
	auto other = k_All;
	other.samePlayer = false;
	EXPECT_FALSE(vt::ValidForLockedSelect(other));
	auto broken = k_All;
	broken.functional = false;
	EXPECT_FALSE(vt::ValidForLockedSelect(broken));
	auto noTemple = k_All;
	noTemple.heartBuilt = false;
	EXPECT_FALSE(vt::ValidForLockedSelect(noTemple));
	auto noSite = k_All;
	noSite.siteBuilt = false;
	EXPECT_FALSE(vt::ValidForLockedSelect(noSite));
	EXPECT_FALSE(vt::ValidForLockedSelect({}));
}

TEST(VillageTotemRules, TheSecondTotemShowsTheOtherShareWhileTheyStandApart)
{
	// Easing, it stands where it has got to; the second stands where it is going
	auto shown = vt::ShownShares(0.25f, 0.5f, false);
	EXPECT_FLOAT_EQ(shown.solid, 0.25f);
	ASSERT_TRUE(shown.ghost.has_value());
	EXPECT_FLOAT_EQ(*shown.ghost, 0.5f);
	// Held, it stands where the hand holds it, the second at the town's share
	shown = vt::ShownShares(0.25f, 0.5f, true);
	EXPECT_FLOAT_EQ(shown.solid, 0.5f);
	EXPECT_FLOAT_EQ(*shown.ghost, 0.25f);
	// Together, there is no second
	EXPECT_FALSE(vt::ShownShares(0.4f, 0.4f, true).ghost.has_value());
	EXPECT_FLOAT_EQ(vt::AsPercentage(0.4f), 40.0f);
}

TEST(VillageTotemRules, ItIsPlacedAgainWhenItsRiseChangesOrTheSecondShows)
{
	EXPECT_FALSE(vt::Replaces(4.0f, 4.0f, false));
	EXPECT_TRUE(vt::Replaces(4.5f, 4.0f, false));
	// held still with the second showing, it is placed again every frame
	EXPECT_TRUE(vt::Replaces(4.0f, 4.0f, true));
	// once its icon changed, the next frame places it wherever it stands
	EXPECT_TRUE(vt::Replaces(0.0f, vt::k_NeverPlaced, false));
	EXPECT_TRUE(vt::Replaces(vt::RiseOf(1.0f), vt::k_NeverPlaced, false));
}

TEST(VillageTotemRules, TheShareTooltipOnlyForTheLocalPlayerAndNotOnceAfterTheIconChanged)
{
	EXPECT_TRUE(vt::ShowsShareToolTip(true, false, true));
	// not placed again: no tooltip
	EXPECT_FALSE(vt::ShowsShareToolTip(false, false, true));
	// the first re-place after a new icon is quiet
	EXPECT_FALSE(vt::ShowsShareToolTip(true, true, true));
	// another player's totem, or a neutral one, never
	EXPECT_FALSE(vt::ShowsShareToolTip(true, false, false));
	// the second totem is drawn at half its alpha
	EXPECT_EQ(vt::k_GhostAlpha, 0x80);
}

TEST(VillageTotemRules, ItWearsTheIconOfItsPlayersCreatureOrTheHand)
{
	// The hand is the pack's BuildingSpellHand
	static_assert(vt::k_HandIcon == static_cast<int32_t>(MeshId::BuildingSpellHand));
	constexpr auto k_Pack = static_cast<uint32_t>(MeshId::_COUNT);
	EXPECT_EQ(vt::IconMesh(std::nullopt, k_Pack), 250u);
	EXPECT_EQ(vt::IconMesh(static_cast<int32_t>(MeshId::BuildingPlayerIconCow), k_Pack),
	          static_cast<uint32_t>(MeshId::BuildingPlayerIconCow));
	// A place outside the pack is its first mesh, either way
	EXPECT_EQ(vt::IconMesh(-1, k_Pack), 0u);
	EXPECT_EQ(vt::IconMesh(static_cast<int32_t>(k_Pack), k_Pack), 0u);
	EXPECT_EQ(vt::IconMesh(std::nullopt, 250), 0u);
}

TEST(VillageTotemRules, ACreaturesIconIsItsSpeciesInfos)
{
	std::array<GCreatureInfo, 17> infos {};
	const auto row = creature::InfoRow(CreatureType::Cow);
	infos.at(row).totemIcon = MeshId::BuildingPlayerIconCow;
	EXPECT_EQ(vt::CreatureIcon(CreatureType::Cow, infos), static_cast<int32_t>(MeshId::BuildingPlayerIconCow));
	// no creature: no icon (the totem then wears the hand); a table too short for the species: none either
	EXPECT_FALSE(vt::CreatureIcon(std::nullopt, infos).has_value());
	EXPECT_FALSE(vt::CreatureIcon(CreatureType::Cow, std::span<const GCreatureInfo>(infos).first(row)).has_value());
}

TEST(VillageTotemRules, ItTurnsSideOnToItsWorshipSite)
{
	// A site due east of it is the game's angle 0, due south (+z) a quarter turn: the totem faces a quarter turn on
	constexpr map_coords::MapCoords k_Totem {.x = 100 << 16, .z = 200 << 16};
	constexpr int32_t k_Away = 30 << 16;
	EXPECT_EQ(vt::FacingSite(k_Totem, {.x = k_Totem.x + k_Away, .z = k_Totem.z}), 1.5707964f);
	EXPECT_EQ(vt::FacingSite(k_Totem, {.x = k_Totem.x, .z = k_Totem.z + k_Away}), 3.1415927f);
	// It is the game's angle, quantised to 2048ths of a turn, not the exact one
	const float diagonal = vt::FacingSite(k_Totem, {.x = k_Totem.x + k_Away, .z = k_Totem.z + k_Away});
	EXPECT_EQ(diagonal, gutils::ConvertGameAngleTo3D(gutils::GetAngleFromDXDZ(k_Away, k_Away)) + gutils::k_ScawenOffset);
}

TEST(VillageTotemRules, AScriptsShareIsKeptAsGivenPastAll)
{
	// A script's HEIGHT set is not held to 0..1: the statue eases there in the time it is away and stands higher than
	// with all its town at worship
	TotemWorship worship;
	percentage::SetRise(worship, 1.25f);
	EXPECT_EQ(worship.percentage, 1.25f);
	EXPECT_FLOAT_EQ(worship.rise.duration, 1.25f * vt::k_EaseMsPerShare);
	percentage::StepRise(worship, worship.rise.duration);
	EXPECT_EQ(worship.rise.value, 1.25f);
	EXPECT_FLOAT_EQ(vt::RiseOf(worship.rise.value), 10.0f);
}

TEST(VillageTotemHandHold, TheHandTipsOverMoreTheNearerTheLand)
{
	// Seven sixteenths of a half turn, as the game keeps it, from 2.5 up in the world
	static_assert(static_cast<double>(vt::k_HandTilt) == 1.3744468688964844);
	EXPECT_EQ(vt::HandTiltAt(30.0f), vt::k_HandTilt);
	EXPECT_EQ(vt::HandTiltAt(vt::k_LowHand), vt::k_HandTilt);
	// A hand's size below that, a further eighth of a half turn: 11/16 of it
	EXPECT_NEAR(vt::HandTiltAt(vt::k_LowHand - vt::k_HandSize), 11.0 * std::numbers::pi / 16.0, 1e-6);
	EXPECT_GT(vt::HandTiltAt(2.0f), vt::k_HandTilt);
	EXPECT_LT(vt::HandTiltAt(2.0f), vt::HandTiltAt(1.0f));
}

TEST(VillageTotemHandHold, TheSideHoldClosesWithTheIconsHoldRadius)
{
	// Half the hand's size closes it all the way, and no more
	EXPECT_EQ(vt::GripClosure(0.8f), 0.5f);
	EXPECT_EQ(vt::GripClosure(1.6f), 1.0f);
	EXPECT_EQ(vt::GripClosure(5.0f), 1.0f);
	// Half the clip's half at most, in whole milliseconds, truncated
	EXPECT_EQ(vt::GripTimeMs(1.0f, 1000), 250);
	EXPECT_EQ(vt::GripTimeMs(0.5f, 1001), 125);
	EXPECT_EQ(vt::GripTimeMs(0.999f, 1000), 249);
	EXPECT_EQ(vt::GripTimeMs(0.0f, 1000), 0);
}

TEST(VillageTotemHandHold, TheHandIsTippedAboutItsSideThenTurned)
{
	constexpr float k_Pi = std::numbers::pi_v<float>;
	// No tilt, and a heading of half a turn back, which the half turn on cancels: as the model is
	const auto still = vt::HoldRotation(0.0f, -k_Pi);
	EXPECT_EQ(still, glm::mat3(1.0f));
	// Tipped a quarter turn: its y goes along the turn's z, its z down
	const auto tipped = vt::HoldRotation(k_Pi / 2.0f, -k_Pi);
	EXPECT_NEAR(tipped[1].z, 1.0f, 1e-6f);
	EXPECT_NEAR(tipped[2].y, -1.0f, 1e-6f);
	// Turned a quarter turn without a tilt: its x goes to z, its z to -x
	const auto turned = vt::HoldRotation(0.0f, -k_Pi / 2.0f);
	EXPECT_NEAR(glm::distance(turned[0], glm::vec3(0.0f, 0.0f, 1.0f)), 0.0f, 1e-6f);
	EXPECT_NEAR(glm::distance(turned[2], glm::vec3(-1.0f, 0.0f, 0.0f)), 0.0f, 1e-6f);
	// Any tilt and turn keep it a rotation
	const auto any = vt::HoldRotation(vt::HandTiltAt(1.0f), 0.7f);
	EXPECT_NEAR(glm::determinant(any), 1.0f, 1e-5f);
	EXPECT_NEAR(glm::dot(any[0], any[1]), 0.0f, 1e-6f);
	EXPECT_NEAR(glm::dot(any[1], any[2]), 0.0f, 1e-6f);
}

TEST(VillageTotemHandHold, TheTippedRowsTakeTheTurnsSineWiderRoundedOnce)
{
	// The tipped rows multiply the tilt's float sine and cosine by the turn's sine before it is rounded, each product
	// rounded once to a float. At a heading of 0.1 that differs from the product with the float sine by one unit
	const float heading = 0.1f;
	const float tilt = vt::k_HandTilt;
	const auto sinTilt = static_cast<float>(std::sin(static_cast<double>(tilt)));
	const auto cosTilt = static_cast<float>(std::cos(static_cast<double>(tilt)));
	const float turn = heading + 3.14159274f;
	const double sinTurn = std::sin(static_cast<double>(turn));
	const auto cosTurn = static_cast<float>(std::cos(static_cast<double>(turn)));
	const auto wide = [sinTurn](float value) { return static_cast<float>(static_cast<double>(value) * sinTurn); };
	const auto narrow = [sinTurn](float value) { return value * static_cast<float>(sinTurn); };
	ASSERT_NE(wide(sinTilt), narrow(sinTilt));
	ASSERT_NE(wide(cosTilt), narrow(cosTilt));

	const auto m = vt::HoldRotation(tilt, heading);
	const auto bits = [](const glm::vec3& row) {
		return std::array<uint32_t, 3> {std::bit_cast<uint32_t>(row.x), std::bit_cast<uint32_t>(row.y),
		                                std::bit_cast<uint32_t>(row.z)};
	};
	// Against the reference worked above
	EXPECT_EQ(bits(m[0]), bits(glm::vec3(cosTurn, 0.0f, static_cast<float>(sinTurn))));
	EXPECT_EQ(bits(m[1]), bits(glm::vec3(0.0f - wide(sinTilt), cosTilt, sinTilt * cosTurn)));
	EXPECT_EQ(bits(m[2]), bits(glm::vec3(0.0f - wide(cosTilt), -sinTilt, cosTilt * cosTurn)));
	// And against the bits worked in double outside the game
	EXPECT_EQ(bits(m[0]), (std::array<uint32_t, 3> {0xBF7EB898, 0x00000000, 0xBDCC7575}));
	EXPECT_EQ(bits(m[1]), (std::array<uint32_t, 3> {0x3DC887BC, 0x3E47C5BC, 0xBF79D3A1}));
	EXPECT_EQ(bits(m[2]), (std::array<uint32_t, 3> {0x3C9F8D3B, 0xBF7B14BF, 0xBE46C63D}));
}

TEST(VillageTotemRules, AScriptsBuiltPercentageIsKeptFromNoneToTheWhole)
{
	// As given in between, none below none, the whole from the whole up
	EXPECT_EQ(vt::PercentBuiltSetTo(0.5f), 0.5f);
	EXPECT_EQ(vt::PercentBuiltSetTo(0.999f), 0.999f);
	EXPECT_EQ(vt::PercentBuiltSetTo(0.0f), 0.0f);
	EXPECT_EQ(vt::PercentBuiltSetTo(-0.25f), 0.0f);
	EXPECT_EQ(vt::PercentBuiltSetTo(1.0f), 1.0f);
	EXPECT_EQ(vt::PercentBuiltSetTo(1.5f), 1.0f);
	EXPECT_EQ(vt::PercentBuiltSetTo(std::numeric_limits<float>::infinity()), 1.0f);
	// No number at all is none (the sign of a zero is not pinned: the compiler may give either, and no script can
	// tell them apart)
	EXPECT_EQ(std::bit_cast<uint32_t>(vt::PercentBuiltSetTo(std::numeric_limits<float>::quiet_NaN())), 0u);
}

TEST(VillageTotemRules, ItIsBuiltOnlyWhenAllOfItIs)
{
	EXPECT_TRUE(vt::IsBuilt(1.0f));
	EXPECT_TRUE(vt::IsBuilt(1.5f));
	EXPECT_FALSE(vt::IsBuilt(0.999f));
	EXPECT_FALSE(vt::IsBuilt(0.0f));
	EXPECT_FALSE(vt::IsBuilt(std::numeric_limits<float>::quiet_NaN()));
}

TEST(VillageTotemRules, ItIsDrawnOnlyBuiltAndAlive)
{
	EXPECT_TRUE(vt::IsDrawn(true, 1.0f));
	EXPECT_TRUE(vt::IsDrawn(true, 0.001f));
	EXPECT_FALSE(vt::IsDrawn(true, 0.0f));
	EXPECT_FALSE(vt::IsDrawn(true, -0.1f));
	EXPECT_FALSE(vt::IsDrawn(true, std::numeric_limits<float>::quiet_NaN()));
	EXPECT_FALSE(vt::IsDrawn(false, 1.0f));
}

/// A totem and something else in a registry of their own, with the totems' system on its own: what a script's
/// HEALTH and BUILT_PERCENTAGE set and read
class VillageTotemScriptTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		_totem = registry.Create();
		registry.Assign<VillageTotem>(_totem);
		_other = registry.Create();
	}

	void TearDown() override { Locator::entitiesRegistry::reset(); }

	ecs::systems::VillageTotemSystem _system;
	entt::entity _totem {entt::null};
	entt::entity _other {entt::null};
};

TEST_F(VillageTotemScriptTest, ATotemIsMadeBuiltWithAllItsLife)
{
	ASSERT_TRUE(_system.GetBuiltPercentage(_totem).has_value());
	EXPECT_EQ(*_system.GetBuiltPercentage(_totem), 1.0f);
	EXPECT_TRUE(ecs::abodes::IsBuilt(_totem));
	EXPECT_EQ(ecs::life::LifeOf(_totem), 1.0f);
}

TEST_F(VillageTotemScriptTest, ItsBuiltPercentageIsSetAndReadBack)
{
	EXPECT_TRUE(_system.SetBuiltPercentageFromScript(_totem, 0.5f));
	EXPECT_EQ(*_system.GetBuiltPercentage(_totem), 0.5f);
	EXPECT_FALSE(ecs::abodes::IsBuilt(_totem));
	EXPECT_TRUE(_system.SetBuiltPercentageFromScript(_totem, -1.0f));
	EXPECT_EQ(*_system.GetBuiltPercentage(_totem), 0.0f);
	// built again, at the whole, from a value past it
	EXPECT_TRUE(_system.SetBuiltPercentageFromScript(_totem, 2.0f));
	EXPECT_EQ(*_system.GetBuiltPercentage(_totem), 1.0f);
	EXPECT_TRUE(ecs::abodes::IsBuilt(_totem));
}

TEST_F(VillageTotemScriptTest, ItsHealthIsItsLifeAsGiven)
{
	EXPECT_TRUE(_system.SetLifeFromScript(_totem, 0.0f));
	EXPECT_EQ(ecs::life::LifeOf(_totem), 0.0f);
	EXPECT_TRUE(_system.SetLifeFromScript(_totem, -0.5f));
	EXPECT_EQ(ecs::life::LifeOf(_totem), -0.5f);
	EXPECT_TRUE(_system.SetLifeFromScript(_totem, 1.5f));
	EXPECT_EQ(ecs::life::LifeOf(_totem), 1.5f);
	// its build is not touched
	EXPECT_EQ(*_system.GetBuiltPercentage(_totem), 1.0f);
}

TEST_F(VillageTotemScriptTest, AnythingElseIsLeftAlone)
{
	EXPECT_FALSE(_system.GetBuiltPercentage(_other).has_value());
	EXPECT_FALSE(_system.SetBuiltPercentageFromScript(_other, 0.5f));
	EXPECT_FALSE(_system.SetLifeFromScript(_other, 0.0f));
	EXPECT_FALSE(Locator::entitiesRegistry::value().AllOf<openblack::ecs::components::Life>(_other));
	EXPECT_FALSE(_system.SetLifeFromScript(entt::null, 0.0f));
}
