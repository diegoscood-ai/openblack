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
#include "Audio/Services/Guidance.h"
#include "Common/GUtilsAngle.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureTownHelp.h"
#include "ECS/Abodes.h"
#include "ECS/Archetypes/PuzzleTotemArchetype.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/PuzzleTotem.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Life.h"
#include "ECS/PuzzleTotem.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/CreatureMindTownHelp.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/Implementations/VillageTotemSystem.h"
#include "ECS/VillageTotem.h"
#include "InfoConstants.h"
#include "Input/GamePackets.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Worship/WorshipPercentage.h"
#include "support/MapFakes.h"
#include "support/RestoreService.h"

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

TEST(VillageTotemRules, AHandBelowTheLandSlidesUpFreelyAndIsBroughtUpToTheLandGoingDown)
{
	// only a slide down is held to the land: further down than the hand is above the land, which a hand under the
	// land always is, so a slide down takes it up to the land
	EXPECT_FLOAT_EQ(vt::HandSlide(-10.0f, 600.0f, 4.0f, -1.0f), 1.0f);
	// a slide up is the mouse's own, however far under the land the hand is
	EXPECT_EQ(vt::HandSlide(10.0f, 600.0f, 4.0f, -1.0f), 10.0f * 4.0f * 3.0f / 600.0f);
	EXPECT_EQ(vt::HandSlide(10.0f, 600.0f, 4.0f, -100.0f), 10.0f * 4.0f * 3.0f / 600.0f);
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

TEST(VillageTotemHandHold, TheCursorsImageIsPinnedOnTheHandKeptOnTheScreen)
{
	constexpr glm::ivec2 k_Screen {640, 480};
	// To the nearest pixel, a half going to the even one
	EXPECT_EQ(vt::CursorPin({100.4f, 200.6f}, k_Screen), glm::ivec2(100, 201));
	EXPECT_EQ(vt::CursorPin({10.5f, 11.5f}, k_Screen), glm::ivec2(10, 12));
	// Kept from the first pixel to the last
	EXPECT_EQ(vt::CursorPin({-5.0f, 600.0f}, k_Screen), glm::ivec2(0, 479));
	EXPECT_EQ(vt::CursorPin({639.6f, -0.4f}, k_Screen), glm::ivec2(639, 0));
	EXPECT_EQ(vt::CursorPin({638.5f, 478.5f}, k_Screen), glm::ivec2(638, 478));
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

// Gripping the totem takes its share into the hand and starts the raise sound's watch from it; letting go sends the
// hand's share as a packet and changes nothing itself: only the packet's let go applies the share
TEST_F(VillageTotemScriptTest, TheGripStartsTheRaiseSoundAndOnlyTheLetGoAppliesTheShare)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Assign<TotemWorship>(_totem, TotemWorship {.percentage = 0.25f});
	_system.StartLockedSelect(_totem);
	EXPECT_EQ(registry.Get<const VillageTotem>(_totem).held, 0.25f);
	EXPECT_EQ(audio::guidance::GetState().totemHeight, 0.25f);
	// the hand slides it down
	registry.Get<VillageTotem>(_totem).held = 0.1f;
	const auto pending = game_packets::Pending();
	_system.EndLockedSelect(_totem);
	EXPECT_EQ(game_packets::Pending(), pending + 1);
	EXPECT_EQ(worship::percentage::GetStatuePercentage(_totem), 0.25f);
	// the packet's let go
	_system.LetGo(_totem, 0.1f);
	EXPECT_EQ(worship::percentage::GetStatuePercentage(_totem), 0.1f);
	EXPECT_EQ(registry.Get<const VillageTotem>(_totem).held, 0.1f);
}

// The hand holding the totem is placed at the icon as the icon is when the hand asks, every frame: over the totem's
// place, as high as it holds the icon (here an icon with no model, so at the icon's own height)
TEST_F(VillageTotemScriptTest, TheHandIsHeldWhereTheIconIsEachTimeItAsks)
{
	auto& registry = Locator::entitiesRegistry::value();
	// no icon on the plinth yet
	EXPECT_FALSE(_system.GetHandHold(_totem).has_value());
	registry.Assign<Transform>(_totem, glm::vec3(100.0f, 0.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto icon = registry.Create();
	registry.Assign<Transform>(icon, glm::vec3(100.0f, 5.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Get<VillageTotem>(_totem).icon = icon;
	auto hold = _system.GetHandHold(_totem);
	ASSERT_TRUE(hold.has_value());
	EXPECT_FLOAT_EQ(hold->position.x, 100.0f);
	EXPECT_FLOAT_EQ(hold->position.z, 200.0f);
	EXPECT_EQ(hold->position.y, 5.0f);
	// the totem rises: the next ask follows the icon
	registry.Get<Transform>(icon).position.y = 9.5f;
	hold = _system.GetHandHold(_totem);
	ASSERT_TRUE(hold.has_value());
	EXPECT_EQ(hold->position.y, 9.5f);
	// and sinks
	registry.Get<Transform>(icon).position.y = 1.25f;
	EXPECT_EQ(_system.GetHandHold(_totem)->position.y, 1.25f);
}

// A saved game's objects all made, every totem, on a town centre or not, wears its player's creature's icon again
// (here no player has one, so the hand) and is placed again with no share tooltip
TEST_F(VillageTotemScriptTest, ALoadedWorldPutsEveryTotemsIconBack)
{
	// a totem whose icon changes leaves its map cells and goes back in, so the map cells are there
	const test::RestoreService<Locator::mapCellsSystem> restoreCells;
	Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
	auto& registry = Locator::entitiesRegistry::value();
	const auto other = registry.Create();
	registry.Assign<VillageTotem>(other).onCentre = false;
	for (const auto totem : {_totem, other})
	{
		const auto icon = registry.Create();
		registry.Assign<Mesh>(icon, resources::HashIdentifier(MeshId::BuildingPlayerIconCow), static_cast<int8_t>(0),
		                      static_cast<int8_t>(0));
		registry.Get<VillageTotem>(totem).icon = icon;
	}
	_system.WorldLoaded();
	for (const auto totem : {_totem, other})
	{
		const auto& statue = registry.Get<const VillageTotem>(totem);
		EXPECT_EQ(registry.Get<const Mesh>(statue.icon).id, resources::HashIdentifier(MeshId::BuildingSpellHand));
		EXPECT_TRUE(statue.quietOnce);
		EXPECT_EQ(statue.rise, vt::k_NeverPlaced);
	}
}

TEST_F(VillageTotemScriptTest, AnythingElseIsLeftAlone)
{
	EXPECT_FALSE(_system.GetBuiltPercentage(_other).has_value());
	EXPECT_FALSE(_system.SetBuiltPercentageFromScript(_other, 0.5f));
	EXPECT_FALSE(_system.SetLifeFromScript(_other, 0.0f));
	EXPECT_FALSE(Locator::entitiesRegistry::value().AllOf<openblack::ecs::components::Life>(_other));
	EXPECT_FALSE(_system.SetLifeFromScript(entt::null, 0.0f));
}

// The puzzle totem (ECS/PuzzleTotem.h): a totem of no town whose height, in whole steps, scripts set and read. Its
// numbers are the original's arithmetic, step by step in floats.
namespace pt = openblack::ecs::puzzle_totem;

TEST(PuzzleTotemRules, ItTravelsItsWholeHeightLessWhatItKeepsAboveTheGround)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(pt::Travel(10.0f, 2.233f)), 0x41E83B65u);
	EXPECT_EQ(std::bit_cast<uint32_t>(pt::Travel(4.5f, 1.0f)), 0x40BB3333u);
	EXPECT_FLOAT_EQ(pt::AltitudeAt(0.5f, 0.0f, 5.85f), 0.5f - 5.85f);
	EXPECT_FLOAT_EQ(pt::AltitudeAt(0.5f, 1.0f, 5.85f), 0.5f);
}

TEST(PuzzleTotemRules, ANewTotemStandsSunkAtNoneOfAHundredSteps)
{
	PuzzleTotem totem;
	EXPECT_EQ(totem.maxHeight, pt::k_DefaultMaxHeight);
	EXPECT_EQ(totem.height, 0);
	EXPECT_FLOAT_EQ(pt::Share(totem), 0.0f);
	EXPECT_FLOAT_EQ(pt::MaxHeightProperty(totem), 100.0f);
}

TEST(PuzzleTotemRules, AScriptsHeightIsTakenInWholeStepsAndNeverAboveTheHighest)
{
	PuzzleTotem totem;
	pt::SetHeightProperty(totem, pt::MaxHeightProperty(totem));
	EXPECT_EQ(totem.height, 100);
	// the undead village's totems sink by taking a tiny amount off their height: one whole step
	pt::SetHeightProperty(totem, pt::HeightProperty(totem) - 5e-05f);
	EXPECT_EQ(totem.height, 99);
	pt::SetHeightProperty(totem, 0.0f);
	pt::SetHeightProperty(totem, pt::HeightProperty(totem) - 5e-05f);
	EXPECT_EQ(totem.height, 0);
	pt::SetHeightProperty(totem, 150.0f);
	EXPECT_EQ(totem.height, 100);
	pt::SetHeightProperty(totem, std::numeric_limits<float>::quiet_NaN());
	EXPECT_EQ(totem.height, 100);
	// there is no lowest
	pt::SetHeightProperty(totem, -3.7f);
	EXPECT_EQ(totem.height, -3);
	EXPECT_FLOAT_EQ(pt::HeightProperty(totem), -3.0f);
}

TEST(PuzzleTotemRules, ANewHighestLeavesTheHeightAsItIs)
{
	PuzzleTotem totem;
	pt::SetHeightProperty(totem, 100.0f);
	pt::SetMaxHeightProperty(totem, 50.9f);
	EXPECT_EQ(totem.maxHeight, 50);
	EXPECT_EQ(totem.height, 100);
	EXPECT_FLOAT_EQ(pt::Share(totem), 2.0f);
}

TEST(PuzzleTotemRules, AShareIsTakenInWholeStepsOfItsHighest)
{
	PuzzleTotem totem;
	pt::SetShare(totem, 0.02f);
	EXPECT_EQ(totem.height, 2);
	pt::SetShare(totem, 0.999f);
	EXPECT_EQ(totem.height, 99);
	pt::SetShare(totem, 3.0f);
	EXPECT_EQ(totem.height, 100);
	pt::SetShare(totem, -0.5f);
	EXPECT_EQ(totem.height, -50);
}

TEST(PuzzleTotemRules, ATapOnlyTotemAlwaysStandsAtAllItsHeight)
{
	PuzzleTotem totem {.maxHeight = pt::k_TapOnly, .height = 3};
	EXPECT_FLOAT_EQ(pt::Share(totem), 1.0f);
	pt::SetShare(totem, 0.25f);
	EXPECT_EQ(totem.height, pt::k_TapOnly);
}

TEST(PuzzleTotemRules, APuzzleGamesMoveBlendsInOverFiveReads)
{
	PuzzleTotem totem {.maxHeight = 100, .height = 60, .previousHeight = 10, .blendCountdown = pt::k_BlendReads};
	const std::array<float, 6> expected {0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.6f};
	for (const float share : expected)
	{
		EXPECT_FLOAT_EQ(pt::Share(totem), share);
	}
	EXPECT_EQ(totem.blendCountdown, 0);
}

TEST(PuzzleTotemRules, WhatHasNoWholeNumberGivesTheLowest)
{
	EXPECT_EQ(pt::Truncate(-2.9f), -2);
	EXPECT_EQ(pt::Truncate(std::numeric_limits<float>::quiet_NaN()), std::numeric_limits<int32_t>::min());
	EXPECT_EQ(pt::Truncate(3e9f), std::numeric_limits<int32_t>::min());
}

/// A puzzle totem standing on no land (0 under it), its mesh 9 high (a half height of 4.5) at scale 1: it travels
/// 5.85 between sunk and standing
class PuzzleTotemPlaceTest: public ::testing::Test
{
protected:
	static constexpr entt::id_type k_MeshId = 4242;

	void SetUp() override
	{
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::FakeMeshBoxProvider boxes;
		boxes.meshBox = [](entt::id_type id) -> std::optional<AxisAlignedBoundingBox> {
			if (id == k_MeshId)
			{
				return AxisAlignedBoundingBox {{-1.0f, 0.0f, -1.0f}, {1.0f, 9.0f, 1.0f}};
			}
			return std::nullopt;
		};
		Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>(boxes);
		_totem = registry.Create();
		registry.Assign<Transform>(_totem, glm::vec3(3.0f, 0.0f, 4.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Mesh>(_totem, k_MeshId, static_cast<int8_t>(0), static_cast<int8_t>(0));
		registry.Assign<PuzzleTotem>(_totem);
	}

	void TearDown() override
	{
		Locator::meshBoxProvider::reset();
		Locator::entitiesRegistry::reset();
	}

	PuzzleTotem& Totem() { return Locator::entitiesRegistry::value().Get<PuzzleTotem>(_totem); }
	float Y() { return Locator::entitiesRegistry::value().Get<Transform>(_totem).position.y; }

	entt::entity _totem {entt::null};
};

TEST_F(PuzzleTotemPlaceTest, ItStandsAsHighAsItsHeightSays)
{
	using Archetype = ecs::archetypes::PuzzleTotemArchetype;
	EXPECT_EQ(std::bit_cast<uint32_t>(Archetype::TravelOf(_totem)), 0x40BB3333u);
	// made with no height: sunk by its whole travel
	Archetype::Place(_totem);
	EXPECT_FLOAT_EQ(Totem().altitude, -5.85f);
	EXPECT_FLOAT_EQ(Y(), -5.85f);
	// a script raises it to its highest: it stands on its rest altitude
	pt::SetHeightProperty(Totem(), pt::MaxHeightProperty(Totem()));
	Archetype::Place(_totem);
	EXPECT_FLOAT_EQ(Y(), 0.0f);
	// half way
	pt::SetHeightProperty(Totem(), 50.0f);
	Archetype::Place(_totem);
	EXPECT_FLOAT_EQ(Y(), 0.5f * 5.85f - 5.85f);
	// a highest lowered under its height lifts it past its rest altitude, as the original draws it
	pt::SetHeightProperty(Totem(), 100.0f);
	pt::SetMaxHeightProperty(Totem(), 50.0f);
	Archetype::Place(_totem);
	EXPECT_FLOAT_EQ(Y(), 5.85f);
}

// The creature raises a puzzle totem by the steps it raises a village totem's share by, on the totem's own share
TEST_F(PuzzleTotemPlaceTest, TheCreatureRaisesItByItsOwnShare)
{
	namespace town_help = ecs::systems::mind_town_help;
	creature_mind::IdleMind mind;
	mind.agenda = creature_town_help::MoveTotem(entt::to_integral(_totem), true);
	mind.step = mind.agenda.size() - 1;
	const auto& registry = Locator::entitiesRegistry::value();
	ASSERT_EQ(mind.agenda[mind.step].kind, creature_mind::Step::Kind::RaiseTotem);
	pt::SetHeightProperty(Totem(), 40.0f);
	const auto share = town_help::TotemShareOf(registry, mind);
	ASSERT_TRUE(share.has_value());
	EXPECT_FLOAT_EQ(*share, 0.4f);
	const auto turn = creature_town_help::MoveTotemTurn(*share, true);
	creature_mind::Commands commands;
	commands.totem = creature_mind::Commands::TotemShare {.totem = entt::to_integral(_totem), .share = turn.share};
	town_help::Apply(registry, commands);
	EXPECT_EQ(Totem().height, 42);
	EXPECT_FLOAT_EQ(Y(), 0.42f * 5.85f - 5.85f);
	// near the top it is put at its highest
	pt::SetHeightProperty(Totem(), 99.0f);
	const auto last = creature_town_help::MoveTotemTurn(*town_help::TotemShareOf(registry, mind), true);
	EXPECT_TRUE(last.done);
	commands.totem->share = last.share;
	town_help::Apply(registry, commands);
	EXPECT_EQ(Totem().height, 100);
}

namespace
{
/// The original's drag, run on its own code (the drag, the altitude packet and the height setter of a puzzle totem):
/// the travel, the highest, the rest altitude, the drag's offset, the altitude the hand holds it at and the height the
/// packet leaves, as bits
struct DragCase
{
	uint32_t travel;
	int32_t maxHeight;
	uint32_t rest;
	uint32_t offset;
	uint32_t altitude;
	int32_t height;
};
constexpr std::array<DragCase, 11> k_DragCases {{
    {0x41EB3333u, 100, 0x00000000u, 0xBE7C9630u, 0xBE968721u, 98},
    {0x41EB3333u, 100, 0x00000000u, 0xC10E147Bu, 0xC111D2F2u, 69},
    {0x41EB3333u, 100, 0x00000000u, 0xC1EB3333u, 0xC1EB3333u, 0},
    {0x41EB3333u, 100, 0x00000000u, 0xC19C0DA7u, 0xC19D9580u, 33},
    {0x40F8A3D7u, 100, 0x40500000u, 0xBE7C9630u, 0x403C1BDAu, 96},
    {0x40F8A3D7u, 100, 0x40500000u, 0xC0F8A3D7u, 0xC090A3D7u, 0},
    {0x40F8A3D7u, 100, 0x40500000u, 0x00000000u, 0x40500000u, 100},
    {0x41595C29u, 37, 0x00000000u, 0xC0DD036Au, 0xC0DF3C0Eu, 18},
    {0x41595C29u, 37, 0x00000000u, 0xC1048F5Cu, 0xC1071D9Du, 13},
    {0x40A00000u, 100, 0xBFC00000u, 0xBFBD70A4u, 0xC0400000u, 70},
    {0x40A00000u, 100, 0xBFC00000u, 0xBFBBBBB9u, 0xC0400000u, 70},
}};
} // namespace

TEST(PuzzleTotemRules, TheHandsDragGivesTheOriginalsAltitudeAndHeight)
{
	for (const auto& c : k_DragCases)
	{
		const float travel = std::bit_cast<float>(c.travel);
		const float rest = std::bit_cast<float>(c.rest);
		const float altitude = pt::DragAltitude(std::bit_cast<float>(c.offset), rest, travel, c.maxHeight);
		EXPECT_EQ(std::bit_cast<uint32_t>(altitude), c.altitude) << c.travel << " " << c.offset;
		PuzzleTotem totem {.maxHeight = c.maxHeight};
		pt::SetShare(totem, pt::ShareFromAltitude(altitude, rest, travel));
		EXPECT_EQ(totem.height, c.height) << c.travel << " " << c.offset;
	}
}

TEST(PuzzleTotemRules, TheDragCanLeaveItAStepBelowTheStepTheHandChose)
{
	// the hand's step 99 of 100 over a travel of 29.4 comes back as height 98, as in the original
	const float travel = std::bit_cast<float>(0x41EB3333u);
	const float offset = std::bit_cast<float>(0xBE7C9630u);
	EXPECT_EQ(pt::Truncate(((travel - std::fabs(offset)) / travel) * 100.0f), 99);
	PuzzleTotem totem;
	pt::SetShare(totem, pt::ShareFromAltitude(pt::DragAltitude(offset, 0.0f, travel, 100), 0.0f, travel));
	EXPECT_EQ(totem.height, 98);
}

TEST(PuzzleTotemRules, TheDragStaysBetweenItsLowestAndItsRestAndSendsOnlyNewSteps)
{
	EXPECT_FLOAT_EQ(pt::DragOffset(0.0f, 3.0f, 5.85f), 0.0f);
	EXPECT_FLOAT_EQ(pt::DragOffset(0.0f, -30.0f, 5.85f), -5.85f);
	EXPECT_FLOAT_EQ(pt::DragOffset(-1.0f, -1.5f, 5.85f), -2.0f);
	EXPECT_FLOAT_EQ(pt::ShareFromAltitude(-9.0f, 0.0f, 5.85f), 0.0f);
	EXPECT_FLOAT_EQ(pt::ShareFromAltitude(0.0f, 0.0f, 5.85f), 1.0f);
	PuzzleTotem totem {.height = 100, .restAltitude = 0.0f, .altitude = 0.0f};
	pt::StartDrag(totem);
	EXPECT_FLOAT_EQ(totem.dragOffset, 0.0f);
	// a slide too small to leave its highest step sends nothing
	EXPECT_FALSE(pt::Drag(totem, 0.001f, 5.85f).has_value());
	const auto sent = pt::Drag(totem, -1.5f, 5.85f);
	ASSERT_TRUE(sent.has_value());
	EXPECT_FLOAT_EQ(*sent, totem.handAltitude);
	EXPECT_FALSE(pt::Drag(totem, 0.0f, 5.85f).has_value());
	// a tap-only totem is not dragged
	PuzzleTotem tapOnly {.maxHeight = pt::k_TapOnly};
	EXPECT_FALSE(pt::Drag(tapOnly, -1.5f, 5.85f).has_value());
	EXPECT_EQ(tapOnly.height, pt::k_TapOnly);
}
