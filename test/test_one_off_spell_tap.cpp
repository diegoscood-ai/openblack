/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A one-shot miracle's bubble tapped by the hand, and a one-shot miracle given straight into the hand: the bubble's
// miracle goes into the hand only when the hand can take it, and only then does the bubble pop, with its sound, and go.
// docs/wiki/project/progress/hand/hand_sounds.md, docs/wiki/project/progress/hand/clicking_and_activating.md,
// docs/wiki/project/progress/hand/hand_in_scripts.md

#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/AudioManagerNoOp.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Worship/Worship.h"
#include "creature/CreatureSystemFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::ecs::components::OneOffSpellSeed;

namespace
{
/// Every sound played, by its sample
class TapSounds final: public audio::AudioManagerNoOp
{
public:
	using AudioManagerNoOp::PlaySoundEffect;
	audio::Channel PlaySoundEffect(const audio::PlayOptions& options) override
	{
		played.push_back(options.sample.number);
		return audio::k_NoChannel;
	}

	std::vector<int> played;
};

/// An empty registry, the hand a fake and the sounds recorded, put back when the test ends
class OneOffSpellTap: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		_hand = &static_cast<test::creature_fakes::FakeHand&>(Locator::handSystem::emplace<test::creature_fakes::FakeHand>());
		_sounds = &static_cast<TapSounds&>(Locator::audio::emplace<TapSounds>());
		Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
	}

	/// A one-shot bubble of the seed
	[[nodiscard]] entt::entity Bubble(SpellSeedType seed) const
	{
		const auto orb = _registry->Create();
		_registry->Assign<OneOffSpellSeed>(orb, OneOffSpellSeed {.seedType = seed});
		return orb;
	}

	ecs::Registry* _registry {nullptr};
	test::creature_fakes::FakeHand* _hand {nullptr};
	TapSounds* _sounds {nullptr};

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::handSystem> _restoreHand;
	test::RestoreService<Locator::audio> _restoreAudio;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
};
} // namespace

// The hand holding something can't take the bubble's miracle: the tap is refused, the bubble stays and nothing pops
TEST_F(OneOffSpellTap, ABubbleTappedWhileTheHandHoldsSomethingStaysAndDoesNotPop)
{
	const auto orb = Bubble(SpellSeedType::Fire);
	_hand->held = _registry->Create();
	EXPECT_EQ(magic::one_off::InterfaceTap(orb, PlayerNames::PLAYER_ONE), 0);
	EXPECT_TRUE(_registry->Valid(orb));
	EXPECT_TRUE(_sounds->played.empty());
}

// The temple's tap reaches a one-shot bubble through its miracle; a thing that is no bubble nor icon takes no tap
TEST_F(OneOffSpellTap, TheHandsTapReachesAOneShotBubbleAndNothingElse)
{
	const auto orb = Bubble(SpellSeedType::Fire);
	const auto other = _registry->Create();
	EXPECT_TRUE(worship::InterfaceValidToTap(orb, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(worship::InterfaceValidToTap(other, PlayerNames::PLAYER_ONE));
	EXPECT_EQ(worship::InterfaceTap(other, PlayerNames::PLAYER_ONE), 0);
	// the bubble's tap is the bubble's own: with the hand busy it is refused the same way, and the bubble stays
	_hand->held = other;
	EXPECT_EQ(worship::InterfaceTap(orb, PlayerNames::PLAYER_ONE), 0);
	EXPECT_TRUE(_registry->Valid(orb));
	EXPECT_TRUE(_sounds->played.empty());
}

// A one-shot miracle is given into the hand only when there is a hand, it holds nothing, and the seed is one of the
// thirty
TEST_F(OneOffSpellTap, AOneShotMiracleGoesIntoTheHandOnlyWhenTheHandIsFreeAndTheSeedIsReal)
{
	EXPECT_TRUE(magic::one_off::CreateSpellIntoHand(PlayerNames::PLAYER_ONE, SpellSeedType::None, -1, 1.0f) == entt::null);
	EXPECT_TRUE(magic::one_off::CreateSpellIntoHand(PlayerNames::PLAYER_ONE, static_cast<SpellSeedType>(30), -1, 1.0f) ==
	            entt::null);
	_hand->held = _registry->Create();
	EXPECT_TRUE(magic::one_off::CreateSpellIntoHand(PlayerNames::PLAYER_ONE, SpellSeedType::Fire, -1, 1.0f) == entt::null);
	Locator::handSystem::reset();
	EXPECT_TRUE(magic::one_off::CreateSpellIntoHand(PlayerNames::PLAYER_ONE, SpellSeedType::Fire, -1, 1.0f) == entt::null);
}
