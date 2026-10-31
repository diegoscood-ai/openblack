/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The reward system's tap on a chest, without the game's data: with no reward table a chest holds nothing, so only
// the opening itself and the chest's time are seen; with a table of the game's own amounts, its food and wood rows are
// poured at the chest

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <optional>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/AudioManagerNoOp.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Reward.h"
#include "ECS/Registry.h"
#include "ECS/RewardOpening.h"
#include "ECS/Systems/Implementations/RewardSystem.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
class RewardTap: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
	}

	entt::entity Chest(bool fromSky)
	{
		return _rewards.Create(glm::vec3(0.0f), RewardObjectInfo::SmallFood, PlayerNames::PLAYER_ONE, entt::null, fromSky);
	}
	static const Reward& Of(entt::entity chest) { return Locator::entitiesRegistry::value().Get<Reward>(chest); }

	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	ecs::systems::RewardSystem _rewards;
};

/// The sounds a tap plays: the sample, whether it is placed in the world, and its bank
class ChestSounds final: public audio::AudioManagerNoOp
{
public:
	struct Played
	{
		int sample;
		bool is3D;
		audio::BankId bank;
	};
	audio::Channel PlaySoundEffect(const audio::PlayOptions& options) override
	{
		played.push_back({.sample = options.sample.number, .is3D = options.is3D, .bank = options.sample.bank});
		return audio::k_NoChannel;
	}
	audio::Channel PlaySoundEffect(audio::Owner /*owner*/, int sample, int /*mode*/, int /*loops*/, bool /*extra3DFlag*/,
	                               bool is3D, audio::SfxBank bank) override
	{
		played.push_back({.sample = sample, .is3D = is3D, .bank = audio::Bank(bank)});
		return audio::k_NoChannel;
	}
	using AudioManagerNoOp::PlaySoundEffect;

	std::vector<Played> played;
};
} // namespace

TEST_F(RewardTap, OnlyItsOwnPlayerOpensAChestOnTheLandAndOnlyOnce)
{
	const auto chest = Chest(false);
	EXPECT_FALSE(_rewards.Tap(chest, PlayerNames::PLAYER_TWO));
	EXPECT_FALSE(_rewards.IsOpened(chest));
	EXPECT_TRUE(_rewards.Tap(chest, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(_rewards.IsOpened(chest));
	EXPECT_EQ(Of(chest).state, Reward::State::Opened);
	EXPECT_EQ(Of(chest).turnsLeft, ecs::reward::k_OpenedTurns);
	// The closed chest gave way to the opened one
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<Mesh>(chest).id, entt::hashed_string("coffre").value());
	// A second tap does nothing
	EXPECT_FALSE(_rewards.Tap(chest, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(_rewards.IsOpened(chest));
}

TEST_F(RewardTap, AChestStillFallingTakesTheTapButStaysShut)
{
	const auto chest = Chest(true);
	EXPECT_TRUE(_rewards.Tap(chest, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(_rewards.IsOpened(chest));
	EXPECT_EQ(Of(chest).state, Reward::State::Falling);
}

TEST_F(RewardTap, AChestWithNoPlayerOpensForNoOne)
{
	// So what an opening chest gives always goes to the chest's own player
	const auto chest = _rewards.Create(glm::vec3(0.0f), RewardObjectInfo::SmallFood, std::nullopt, entt::null, false);
	EXPECT_FALSE(_rewards.Tap(chest, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(_rewards.Tap(chest, PlayerNames::NEUTRAL));
	EXPECT_FALSE(_rewards.IsOpened(chest));
	EXPECT_EQ(Of(chest).state, Reward::State::Landed);
}

TEST_F(RewardTap, AnOpenedChestGoesTwoHundredTurnsLater)
{
	// Its deletion reaches the dead list and the map cells
	const test::ScopedWorldSystems worldSystems;
	const auto chest = Chest(false);
	ASSERT_TRUE(_rewards.Tap(chest, PlayerNames::PLAYER_ONE));
	for (int turn = 1; turn < ecs::reward::k_OpenedTurns; ++turn)
	{
		_rewards.ProcessTurn();
	}
	EXPECT_TRUE(ecs::IsAvailable(chest));
	_rewards.ProcessTurn();
	EXPECT_FALSE(ecs::IsAvailable(chest));
}

// The tap's own sound, sample 173 of the in-game bank, heard flat by the player at this computer; then, on the land, the
// two opening sounds, 40 and 41, flat
TEST_F(RewardTap, ATapOnAChestSoundsAndItsOpeningPlaysTwoMoreFlat)
{
	const test::RestoreService<Locator::audio> keepAudio;
	auto& sounds = static_cast<ChestSounds&>(Locator::audio::emplace<ChestSounds>());
	const auto inGame = audio::Bank(audio::SfxBank::InGame);
	const auto chest = Chest(false);
	ASSERT_TRUE(_rewards.Tap(chest, PlayerNames::PLAYER_ONE));
	ASSERT_EQ(sounds.played.size(), 3u);
	EXPECT_EQ(sounds.played[0].sample, 173);
	EXPECT_EQ(sounds.played[1].sample, 40);
	EXPECT_EQ(sounds.played[2].sample, 41);
	for (const auto& played : sounds.played)
	{
		EXPECT_FALSE(played.is3D) << played.sample;
		EXPECT_EQ(played.bank, inGame) << played.sample;
	}
	// an opened chest takes no more taps, and is silent
	sounds.played.clear();
	EXPECT_FALSE(_rewards.Tap(chest, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(sounds.played.empty());
}

// A chest still falling takes the tap with its sound, but does not open
TEST_F(RewardTap, AFallingChestsTapSoundsButNothingOpens)
{
	const test::RestoreService<Locator::audio> keepAudio;
	auto& sounds = static_cast<ChestSounds&>(Locator::audio::emplace<ChestSounds>());
	const auto chest = Chest(true);
	ASSERT_TRUE(_rewards.Tap(chest, PlayerNames::PLAYER_ONE));
	ASSERT_EQ(sounds.played.size(), 1u);
	EXPECT_EQ(sounds.played[0].sample, 173);
}

// Another player's tap on its own chest plays no tap for this computer's player; the opening's two are heard alike
TEST_F(RewardTap, AnotherPlayersTapIsSilentButItsOpeningIsHeard)
{
	const test::RestoreService<Locator::audio> keepAudio;
	auto& sounds = static_cast<ChestSounds&>(Locator::audio::emplace<ChestSounds>());
	const auto chest =
	    _rewards.Create(glm::vec3(0.0f), RewardObjectInfo::SmallFood, PlayerNames::PLAYER_TWO, entt::null, false);
	ASSERT_TRUE(_rewards.Tap(chest, PlayerNames::PLAYER_TWO));
	ASSERT_EQ(sounds.played.size(), 2u);
	EXPECT_EQ(sounds.played[0].sample, 40);
	EXPECT_EQ(sounds.played[1].sample, 41);
}

namespace
{
/// The stores and piles about a point, as far as an opening chest pours into them: a record of each pour
class PouredResources final: public ecs::systems::ResourceStoreSystemInterface
{
public:
	struct Pour
	{
		ResourceType type;
		glm::vec3 point;
		uint32_t amount;
		bool speedUp;
		PlayerNames player;
		bool poisoned;
	};

	[[nodiscard]] ObjectResource ResourceOf(entt::entity /*object*/) const override { return {}; }
	[[nodiscard]] bool IsStore(entt::entity /*store*/, ResourceType /*type*/) const override { return false; }
	uint32_t AddToStore(entt::entity /*store*/, ResourceType /*type*/, uint32_t /*amount*/,
	                    std::optional<PlayerNames> /*giver*/, bool /*poisoned*/) override
	{
		return 0;
	}
	uint32_t AddToPile(entt::entity /*pile*/, ResourceType /*type*/, uint32_t /*amount*/, bool /*poisoned*/) override
	{
		return 0;
	}
	uint32_t TakeFromPile(entt::entity /*pile*/, ResourceType /*type*/, uint32_t /*amount*/,
	                      std::optional<PlayerNames> /*taker*/) override
	{
		return 0;
	}
	bool TakeObject(entt::entity /*store*/, entt::entity /*object*/, std::optional<PlayerNames> /*giver*/) override
	{
		return false;
	}
	bool PourAt(ResourceType type, glm::vec3 point, uint32_t amount, bool speedUp, PlayerNames player, bool poisoned) override
	{
		poured.push_back(
		    {.type = type, .point = point, .amount = amount, .speedUp = speedUp, .player = player, .poisoned = poisoned});
		return true;
	}
	[[nodiscard]] std::optional<entt::entity> StoreOf(entt::entity /*pile*/) const override { return std::nullopt; }

	std::vector<Pour> poured;
};
} // namespace

// An opened chest gives what its row of the reward table names: a food row its food and a wood row its wood, poured at
// the chest by the player at this computer, as the original
// (docs/wiki/scripts/map-script-objects.md#reward-chests-create_reward-239-create_reward_in_town-240)
TEST_F(RewardTap, AnOpenedChestPoursTheFoodOrWoodItsRowOfTheTableNames)
{
	const test::RestoreService<Locator::resourceStoreSystem> keepStores;
	auto& stores = static_cast<PouredResources&>(Locator::resourceStoreSystem::emplace<PouredResources>());
	// the game's own amounts: 100 food for the small food kind, 250 wood for the medium wood kind
	auto info = std::make_unique<InfoConstants>();
	info->reward.at(static_cast<size_t>(RewardObjectInfo::SmallFood)).foodValue = 100.0f;
	info->reward.at(static_cast<size_t>(RewardObjectInfo::MediumWood)).woodValue = 250;
	Locator::infoConstants::reset(info.release());
	const glm::vec3 foodAt {1200.0f, 0.0f, 1500.0f};
	const glm::vec3 woodAt {800.0f, 0.0f, 900.0f};
	const auto food = _rewards.Create(foodAt, RewardObjectInfo::SmallFood, PlayerNames::PLAYER_ONE, entt::null, false);
	const auto wood = _rewards.Create(woodAt, RewardObjectInfo::MediumWood, PlayerNames::PLAYER_ONE, entt::null, false);
	ASSERT_TRUE(_rewards.Tap(food, PlayerNames::PLAYER_ONE));
	ASSERT_TRUE(_rewards.Tap(wood, PlayerNames::PLAYER_ONE));
	ASSERT_EQ(stores.poured.size(), 2u);
	EXPECT_EQ(stores.poured[0].type, ResourceType::Food);
	EXPECT_EQ(stores.poured[0].point, foodAt);
	EXPECT_EQ(stores.poured[0].amount, 100u);
	EXPECT_FALSE(stores.poured[0].speedUp);
	EXPECT_EQ(stores.poured[0].player, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(stores.poured[0].poisoned);
	EXPECT_EQ(stores.poured[1].type, ResourceType::Wood);
	EXPECT_EQ(stores.poured[1].point, woodAt);
	EXPECT_EQ(stores.poured[1].amount, 250u);
}
