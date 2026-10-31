/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <chrono>
#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/AudioManagerNoOp.h"
#include "ECS/AbodeKnock.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AbodeKnockSystem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "creature/CreatureSystemFakes.h"
#include "support/RestoreService.h"

// The rules of the hand knocking on a building, pure (raffclar's tests, over our values), and the knock system's
// read-out on a fake clock

using namespace openblack;
using namespace openblack::ecs::abode_knock;
using namespace std::chrono_literals;

TEST(AbodeKnock, AnyBuildingButTheVillageCentreTakesAKnock)
{
	EXPECT_TRUE(IsTappable(AbodeType::LivingQuarters));
	EXPECT_TRUE(IsTappable(AbodeType::Windmill));
	EXPECT_TRUE(IsTappable(AbodeType::StoragePit));
	EXPECT_TRUE(IsTappable(AbodeType::Field));
	EXPECT_FALSE(IsTappable(AbodeType::TownCentre));
}

TEST(AbodeKnock, OnlyHousesAndWindmillsKnockBack)
{
	EXPECT_TRUE(KnocksBack(AbodeType::LivingQuarters));
	EXPECT_TRUE(KnocksBack(AbodeType::Windmill));
	EXPECT_FALSE(KnocksBack(AbodeType::StoragePit));
	EXPECT_FALSE(KnocksBack(AbodeType::Wonder));
	EXPECT_FALSE(KnocksBack(AbodeType::Field));
	EXPECT_FALSE(KnocksBack(AbodeType::TownCentre));
}

TEST(AbodeKnock, TheNineSoundsComeInTurn)
{
	EXPECT_EQ(NextKnockSound(0), 1u);
	EXPECT_EQ(NextKnockSound(8), 0u);
}

TEST(AbodeKnock, AKnockStartsOrHoldsTheReadout)
{
	// Nothing showing: it starts, growing in
	EXPECT_EQ(Knock(0), 8000);
	// Still growing in: unchanged
	EXPECT_EQ(Knock(7500), 7500);
	// Showing: held for its full six seconds again
	EXPECT_EQ(Knock(7000), 7000);
	EXPECT_EQ(Knock(3000), 7000);
	EXPECT_EQ(Knock(1000), 7000);
	// Shrinking: it grows back from the size it had shrunk to
	EXPECT_EQ(Knock(400), 7600);
	EXPECT_EQ(ReadoutSteps(400), ReadoutSteps(7600));
}

TEST(AbodeKnock, TheReadoutRunsDownByTheFrame)
{
	EXPECT_EQ(Tick(8000, 16), 7984);
	EXPECT_EQ(Tick(10, 16), 0);
	// a paused frame has no game time
	EXPECT_EQ(Tick(5000, 0), 5000);
}

TEST(AbodeKnock, TheMarkersGrowInHoldAndShrinkAway)
{
	EXPECT_EQ(ReadoutSteps(8000), 0);
	EXPECT_EQ(ReadoutSteps(7000), 255);
	EXPECT_EQ(ReadoutSteps(4000), 255);
	EXPECT_EQ(ReadoutSteps(0), 0);
	// Half way through growing or shrinking: 127 of 255 steps, the fraction cut off
	EXPECT_EQ(ReadoutSteps(500), 127);
	EXPECT_EQ(ReadoutSteps(7500), 127);
	EXPECT_FLOAT_EQ(ReadoutScale(8000), 0.0f);
	EXPECT_FLOAT_EQ(ReadoutScale(4000), 1.0f);
}

// The game's step per millisecond, 0.255 as a float, is a little under 0.255, but each product is a float, which
// rounds up to the whole step: worked out at a higher precision these five would each be a step down (50, 101, 152,
// 203 and 254)
TEST(AbodeKnock, TheShrinkingStepsAreFloatProducts)
{
	EXPECT_EQ(ReadoutSteps(200), 51);
	EXPECT_EQ(ReadoutSteps(400), 102);
	EXPECT_EQ(ReadoutSteps(600), 153);
	EXPECT_EQ(ReadoutSteps(800), 204);
	EXPECT_EQ(ReadoutSteps(1000), 255);
	constexpr float k_Step = 1.0f / 255.0f;
	EXPECT_EQ(ReadoutScale(200), 51.0f * k_Step);
	EXPECT_EQ(ReadoutScale(400), 102.0f * k_Step);
	EXPECT_EQ(ReadoutScale(600), 153.0f * k_Step);
	EXPECT_EQ(ReadoutScale(800), 204.0f * k_Step);
	// the last second starts at full size
	EXPECT_EQ(ReadoutScale(1000), 1.0f);
}

TEST(AbodeKnock, AHouseShowsGoldForItsAdultsAndGreenForItsFreePlaces)
{
	const auto markers = Readout(3, 1, 1.0f);
	ASSERT_EQ(markers.size(), 3u);
	EXPECT_EQ(markers[0].colour, k_LivedInColour);
	EXPECT_EQ(markers[1].colour, k_FreePlaceColour);
	EXPECT_EQ(markers[2].colour, k_FreePlaceColour);
	// the markers stand a 1.3th apart, each place divided by 1.3
	EXPECT_EQ(markers[0].along, 0.0f);
	EXPECT_EQ(markers[2].along, 2.0f / 1.3f);
	EXPECT_EQ(markers[0].size, 1.0f / 1.3f);
	// the row starts half a place to the left for each place
	const auto start = RowStart(glm::vec3(10.0f, 20.0f, 30.0f), glm::vec3(1.0f, 0.0f, 0.0f), 3);
	EXPECT_EQ(start, glm::vec3(8.5f, 20.0f, 30.0f));
	EXPECT_EQ(MarkerPoint(start, glm::vec3(1.0f, 0.0f, 0.0f), markers[2].along).x, 8.5f + 2.0f / 1.3f);
}

TEST(AbodeKnock, TheRowStandsOverTheModel)
{
	// twice the half height and 1.5 over the model's position
	EXPECT_EQ(RowPoint(glm::vec3(1.0f, 2.0f, 3.0f), 4.0f), glm::vec3(1.0f, 11.5f, 3.0f));
}

// The read-out runs down by the frame's game time only: a paused frame (0 ms) leaves it where it is
TEST(AbodeKnockSystem, TheReadoutRunsOnGameTimeAndStandsStillWhilePaused)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	// a storage pit with nobody inside and no town: the knock moves nobody and plays no sound
	auto info = std::make_unique<InfoConstants>();
	info->abode.at(0).abodeNumber = AbodeNumber::StoragePit;
	info->abode.at(0).abodeType = AbodeType::StoragePit;
	Locator::infoConstants::reset(info.release());
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& registry = Locator::entitiesRegistry::value();
	const auto pit = registry.Create();
	registry.Assign<ecs::components::Abode>(pit, AbodeNumber::StoragePit, ecs::components::Abode::k_NoTown, 0u, 0u);

	ecs::systems::AbodeKnockSystem knocks;
	const glm::vec3 right {1.0f, 0.0f, 0.0f};
	EXPECT_TRUE(knocks.Tap(pit, glm::vec3(0.0f), false));
	EXPECT_TRUE(knocks.GetReadoutTown() == entt::null);
	// until the first frame the steps are the game's starting ones, full
	EXPECT_EQ(knocks.GetReadoutScale(), 1.0f);
	knocks.Update(16ms, right);
	const float grown = knocks.GetReadoutScale();
	EXPECT_EQ(grown, ReadoutScale(7984));
	knocks.Update(0ms, right);
	knocks.Update(0ms, right);
	EXPECT_EQ(knocks.GetReadoutScale(), grown);
	knocks.Update(984ms, right);
	EXPECT_EQ(knocks.GetReadoutScale(), 1.0f);
	knocks.Update(7000ms, right);
	EXPECT_EQ(knocks.GetReadoutScale(), 0.0f);
	// run down: nothing changes any more until the next knock
	knocks.Update(16ms, right);
	EXPECT_EQ(knocks.GetReadoutScale(), 0.0f);
	EXPECT_TRUE(knocks.PeopleOver(pit).empty());
}

TEST(AbodeKnock, AHeldVillagersHomeShowsOneTealPersonAtFullSize)
{
	// one place, whatever the scale, and drawn even with the read-out run down
	const auto markers = Readout(1, k_HeldVillagersHome, 0.0f);
	ASSERT_EQ(markers.size(), 1u);
	EXPECT_EQ(markers[0].colour, k_HeldVillagersHomeColour);
	EXPECT_EQ(markers[0].size, 1.0f / 1.3f);
	EXPECT_EQ(markers[0].along, 0.0f);
	EXPECT_TRUE(RowDrawn(0, k_HeldVillagersHome));
	EXPECT_FALSE(RowDrawn(0, 2));
	EXPECT_TRUE(RowDrawn(10, 2));
	// shown over any building drawn, whatever the read-out's steps or town
	EXPECT_TRUE(ShowsRow(0, false, true));
}

TEST(AbodeKnock, ShrunkMarkersKeepASmallestSize)
{
	EXPECT_EQ(Readout(1, 0, 0.0f)[0].size, k_SmallestMarker);
	EXPECT_TRUE(Readout(0, 0, 1.0f).empty());
}

TEST(AbodeKnock, OnlyTheKnockedTownsBuildingsShowARowWhileItHasSteps)
{
	EXPECT_TRUE(ShowsRow(1, true, false));
	EXPECT_FALSE(ShowsRow(0, true, false));
	EXPECT_FALSE(ShowsRow(255, false, false));
	EXPECT_TRUE(DrawsReadout(AbodeType::LivingQuarters));
	EXPECT_TRUE(DrawsReadout(AbodeType::TownCentre));
	EXPECT_TRUE(DrawsReadout(AbodeType::StoragePit));
	EXPECT_FALSE(DrawsReadout(AbodeType::Field));
	EXPECT_FALSE(DrawsReadout(AbodeType::FootballPitch));
	EXPECT_FALSE(DrawsReadout(AbodeType::Totem));
	EXPECT_FALSE(DrawsReadout(AbodeType::SpellDispenser));
}

namespace
{
/// The knock's sounds, with the knock counter at the value the test gives
class KnockSounds final: public audio::AudioManagerNoOp
{
public:
	using AudioManagerNoOp::PlaySoundEffect;
	audio::Channel PlaySoundEffect(const audio::PlayOptions& options) override
	{
		played.push_back(options);
		return audio::k_NoChannel;
	}
	[[nodiscard]] int NextCounter(audio::Counter /*counter*/) override { return next; }

	int next {0};
	std::vector<audio::PlayOptions> played;
};
} // namespace

// On a house the local player's own hand plays its tap-house animation where the hand is, then the knock sounds at the
// hand's point: 110 + the knock counter, owned by the house; another player's knock sounds the same with no animation
TEST(AbodeKnockSystem, OnAHouseTheLocalHandPlaysItsTapHouseAnimationAndTheKnockSounds)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	const test::RestoreService<Locator::handSystem> restoreHand;
	const test::RestoreService<Locator::audio> restoreAudio;
	auto info = std::make_unique<InfoConstants>();
	info->abode.at(0).abodeNumber = AbodeNumber::A;
	info->abode.at(0).abodeType = AbodeType::LivingQuarters;
	Locator::infoConstants::reset(info.release());
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& registry = Locator::entitiesRegistry::value();
	auto& hand = static_cast<test::creature_fakes::FakeHand&>(Locator::handSystem::emplace<test::creature_fakes::FakeHand>());
	hand.left = registry.Create();
	const glm::vec3 handAt {30.0f, 12.0f, -8.0f};
	registry.Assign<ecs::components::Transform>(hand.left, handAt, glm::mat3(1.0f), glm::vec3(1.0f));
	auto& sounds = static_cast<KnockSounds&>(Locator::audio::emplace<KnockSounds>());
	sounds.next = 4;
	const auto house = registry.Create();
	registry.Assign<ecs::components::Abode>(house, AbodeNumber::A, ecs::components::Abode::k_NoTown, 0u, 0u);

	ecs::systems::AbodeKnockSystem knocks;
	const glm::vec3 point {31.0f, 10.0f, -7.0f};
	EXPECT_TRUE(knocks.Tap(house, point, true));
	ASSERT_EQ(hand.fixedPosAnimations.size(), 1u);
	EXPECT_EQ(hand.fixedPosAnimations[0].first, "Ctap_house");
	EXPECT_EQ(hand.fixedPosAnimations[0].second, handAt);
	ASSERT_EQ(sounds.played.size(), 1u);
	EXPECT_EQ(sounds.played[0].sample.number, 114);
	EXPECT_TRUE(sounds.played[0].is3D);
	EXPECT_FALSE(sounds.played[0].track);
	EXPECT_EQ(sounds.played[0].position, point);
	EXPECT_TRUE(sounds.played[0].owner == audio::Owner::Thing(house));

	// another player's knock: the sound, but the local hand plays nothing
	EXPECT_TRUE(knocks.Tap(house, point, false));
	EXPECT_EQ(hand.fixedPosAnimations.size(), 1u);
	EXPECT_EQ(sounds.played.size(), 2u);
}

// A building that does not knock back (a storage pit) plays neither the animation nor the sound
TEST(AbodeKnockSystem, ABuildingThatDoesNotKnockBackPlaysNoTapHouseAnimation)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	const test::RestoreService<Locator::handSystem> restoreHand;
	const test::RestoreService<Locator::audio> restoreAudio;
	auto info = std::make_unique<InfoConstants>();
	info->abode.at(0).abodeNumber = AbodeNumber::StoragePit;
	info->abode.at(0).abodeType = AbodeType::StoragePit;
	Locator::infoConstants::reset(info.release());
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& registry = Locator::entitiesRegistry::value();
	auto& hand = static_cast<test::creature_fakes::FakeHand&>(Locator::handSystem::emplace<test::creature_fakes::FakeHand>());
	hand.left = registry.Create();
	registry.Assign<ecs::components::Transform>(hand.left, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& sounds = static_cast<KnockSounds&>(Locator::audio::emplace<KnockSounds>());
	const auto pit = registry.Create();
	registry.Assign<ecs::components::Abode>(pit, AbodeNumber::StoragePit, ecs::components::Abode::k_NoTown, 0u, 0u);

	ecs::systems::AbodeKnockSystem knocks;
	EXPECT_TRUE(knocks.Tap(pit, glm::vec3(0.0f), true));
	EXPECT_TRUE(hand.fixedPosAnimations.empty());
	EXPECT_TRUE(sounds.played.empty());
}
