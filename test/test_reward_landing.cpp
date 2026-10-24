/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A reward chest from the sky thumping down: once, as it lands, it plays the in-game bank's thump (sample 174, not
// placed in the world) and shakes the camera within 200 of the world's origin, wherever the chest lands, at strength 1
// for 400 ms along every axis. docs/wiki/camera/script-camera.md#shake

#define LOCATOR_IMPLEMENTATIONS

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/Audio.h"
#include "Audio/AudioManagerNoOp.h"
#include "Audio/Game/Banks.h"
#include "Camera/CameraShake.h"
#include "ECS/Components/Reward.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ExplosionSystem.h"
#include "ECS/Systems/Implementations/RewardSystem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The sounds played with their options
class RecordingAudio final: public audio::AudioManagerNoOp
{
public:
	audio::Channel PlaySoundEffect(const audio::PlayOptions& options) override
	{
		played.push_back(options);
		return audio::k_NoChannel;
	}
	std::vector<audio::PlayOptions> played;
};

class RewardLanding: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::explosionSystem::emplace<ecs::systems::ExplosionSystem>();
		_audio = &static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());
		camera_shake::Reset();
	}
	void TearDown() override { camera_shake::Reset(); }

	[[nodiscard]] static Reward::State StateOf(entt::entity chest)
	{
		return Locator::entitiesRegistry::value().Get<Reward>(chest).state;
	}

	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::explosionSystem> _restoreExplosions;
	test::RestoreService<Locator::audio> _restoreAudio;
	ecs::systems::RewardSystem _rewards;
	RecordingAudio* _audio {nullptr};
};
} // namespace

TEST_F(RewardLanding, AChestFromTheSkyThumpsAndShakesTheCameraAboutTheWorldsOriginOnceAsItLands)
{
	// Far from the world's origin: the shake is still about the origin
	const glm::vec3 where {1800.0f, 30.0f, 2400.0f};
	const auto chest = _rewards.Create(where, RewardObjectInfo::SmallFood, PlayerNames::PLAYER_ONE, entt::null, true);
	ASSERT_EQ(StateOf(chest), Reward::State::Falling);

	// Frame by frame until it lands: nothing before
	int frames = 0;
	while (StateOf(chest) == Reward::State::Falling && frames < 100)
	{
		EXPECT_TRUE(camera_shake::Checkers().empty());
		EXPECT_TRUE(_audio->played.empty());
		_rewards.Update(100.0f);
		++frames;
	}
	ASSERT_EQ(StateOf(chest), Reward::State::Landed);

	// The thump: the in-game bank's sample 174, not placed in the world, not following anything
	ASSERT_EQ(_audio->played.size(), 1u);
	const auto& thump = _audio->played.front();
	EXPECT_EQ(thump.sample.bank, audio::Bank(audio::SfxBank::InGame));
	EXPECT_EQ(thump.sample.number, 174);
	EXPECT_FALSE(thump.is3D);
	EXPECT_FALSE(thump.track);

	// The shake: within 200 of the world's origin, strength 1, 400 ms, along every axis
	ASSERT_EQ(camera_shake::Checkers().size(), 1u);
	const auto& shake = camera_shake::Checkers().front();
	EXPECT_EQ(shake.maxDistance, 200.0f);
	EXPECT_EQ(shake.point, glm::vec3(0.0f));
	EXPECT_EQ(shake.amplitude, 1.0f);
	EXPECT_EQ(shake.totalMs, 400);
	EXPECT_EQ(shake.remainingMs, 400);
	EXPECT_FALSE(shake.yOnly);

	// Lying on the land it neither thumps nor shakes again
	for (int i = 0; i < 10; ++i)
	{
		_rewards.Update(100.0f);
	}
	EXPECT_EQ(_audio->played.size(), 1u);
	EXPECT_EQ(camera_shake::Checkers().size(), 1u);
}
