/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The worship economy (docs/wiki/magic/worship.md#how-worship-fits-together): the site's capacity and battery, its per-turn
// accounting, the icons' chant store with the original's excess quirk, how many villagers a town needs, and the fireflies'
// weighted draw; and the gesture selection's request for a seed is the miracles' summon of its plain miracle, for the human
// player. No world is needed beyond a registry.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdlib>

#include <array>
#include <bit>
#include <memory>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Common/GameRandomTesting.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Influence/InfluenceState.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/ShowNeeds.h"
#include "ECS/Systems/Implementations/VillagerWorship.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerFood.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/Script/CHLWorship.h"
#include "Worship/FireFlyReward.h"
#include "Worship/GestureIconProvider.h"
#include "Worship/TownMagic.h"
#include "Worship/Worship.h"
#include "Worship/WorshipPercentage.h"
#include "Worship/WorshipSite.h"
#include "Worship/WorshipSpellIcon.h"
#include "support/LivingActionFakes.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// GWorshipSiteInfo of a Norse site as info.dat ships it. chantsToReserveForMaintaining is the int 500 read as a
/// float, which is what the original does.
GWorshipSiteInfo ShippedNorseSiteInfo()
{
	GWorshipSiteInfo info {};
	info.radiusFromCitadel = 37.5f;
	info.chantsPerVillager = 3.0f;
	info.prayerSiteDistance = 44.0f;
	info.maxDancersVisible = 20;
	info.chantsToFillBattery = 9000.0f;
	info.eachVillagerAddToFillBattery = 300.0f;
	info.chantsToReserveForMaintaining = 7.0e-43f;
	info.artifactPowerupMultiplier = 1e-5f;
	return info;
}

class WorshipTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		info->worshipSite.at(static_cast<size_t>(Tribe::NORSE)) = ShippedNorseSiteInfo();
		info->villager.at(0).damageThresholdToGoHome = 0.3f;
		info->villager.at(0).chantLifeRate = 5e-6f;
		// the desire for food (the dancers' foodReqiredForDinner; the food pot's info holds FOOD)
		info->villager.at(0).foodReqiredForDinner = 4;
		info->pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile)).resourceType = ResourceType::Food;
		// the food pot takes all it is given (no cap: as the magic and loose piles)
		info->pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile)).nextPotForResource = static_cast<PotInfo>(19);
		// the FIRE seed and its rows: base magic FIREBALL, costToCreate 3500
		info->spellSeed.at(static_cast<size_t>(SpellSeedType::Fire)).magicTypes = {MagicType::Fireball, MagicType::None,
		                                                                           MagicType::None, MagicType::None};
		info->magicEffect.at(static_cast<size_t>(MagicType::Fireball)).costToCreate = 3500.0f;
		// the GMagicInfo rows live in per-class sections: types 0..9 are magicGeneral
		info->magicGeneral.at(static_cast<size_t>(MagicType::Fireball)).magicType = MagicType::Fireball;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
	}

	void TearDown() override
	{
		// the fireflies' reward probabilities as the program starts (all 0, and so their running sums): Reset clears
		// the probabilities only, a SetRewardProbability then sums them again
		worship::fire_fly::Reset();
		worship::fire_fly::SetRewardProbability(MagicType::Fireball, 0.0f);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	/// A bare site: only the fields the accounting reads (the real one comes from worship::site::Create)
	static WorshipSite NorseSite(int dancers)
	{
		WorshipSite site;
		site.infoIndex = static_cast<uint8_t>(Tribe::NORSE);
		site.tribe = Tribe::NORSE;
		site.player = PlayerNames::NEUTRAL;
		site.dancers.assign(static_cast<size_t>(dancers), entt::null);
		return site;
	}

	static entt::entity MakeTown(uint32_t id, float percentage, int villagers)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto town = registry.Create();
		registry.Assign<Town>(town, id).owner = PlayerNames::PLAYER_ONE;
		registry.Assign<Transform>(town, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<TownInfluence>(town);
		auto& magic = registry.Assign<TownMagic>(town);
		magic.worshipPercentage = percentage;
		const auto site = registry.Create();
		registry.Assign<Transform>(site, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<WorshipSite>(site, NorseSite(0));
		magic.worshipSite = site;
		for (int i = 0; i < villagers; ++i)
		{
			const auto villager = registry.Create();
			auto& component = registry.Assign<Villager>(villager);
			component.town = town;
			registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		}
		return town;
	}

	/// An icon with no site: its store is its own (worship::icon::AddToChantStore reads only the requirement)
	static entt::entity MakeIcon(SpellSeedType seed)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto icon = registry.Create();
		auto& spellIcon = registry.Assign<SpellIcon>(icon);
		spellIcon.seedType = seed;
		spellIcon.player = PlayerNames::PLAYER_ONE;
		registry.Assign<WorshipSpellIcon>(icon);
		return icon;
	}
};
} // namespace

TEST_F(WorshipTest, CapacityAndBattery)
{
	// Capacity: N x chantsPerVillager x the player's tribal power (1 by default). MaxBattery: 9000 + N x 300.
	const auto empty = NorseSite(0);
	EXPECT_FLOAT_EQ(worship::site::Capacity(empty), 0.0f);
	EXPECT_FLOAT_EQ(worship::site::MaxBattery(empty), 9000.0f);
	const auto full = NorseSite(11);
	EXPECT_FLOAT_EQ(worship::site::Capacity(full), 33.0f);
	EXPECT_FLOAT_EQ(worship::site::MaxBattery(full), 12300.0f);
	EXPECT_EQ(worship::site::DancerCount(full), 11);
}

TEST_F(WorshipTest, ReserveForMaintainingIsTheInfoDatBug)
{
	// the file holds the int 500 and the original reads it as a float: the reserve is ~0, so seeds out change nothing
	auto site = NorseSite(0);
	site.available = 1000.0f;
	site.used = 0.0f;
	EXPECT_FLOAT_EQ(worship::site::Available(site), 1000.0f);
	EXPECT_FLOAT_EQ(worship::site::AvailableForIcons(site, false), 1000.0f);
	EXPECT_NEAR(worship::site::AvailableForIcons(site, true), 1000.0f, 1e-3f);
}

TEST_F(WorshipTest, InfiniteChantsCheat)
{
	auto site = NorseSite(0);
	site.infiniteChants = true;
	EXPECT_FLOAT_EQ(worship::site::Available(site), 1e6f);
	EXPECT_FLOAT_EQ(worship::site::TotalChantsAvailable(site), 1e6f);
}

TEST_F(WorshipTest, UseChantsTakesAtMostWhatIsAvailable)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	auto site = NorseSite(0);
	site.available = 100.0f;
	registry.Assign<WorshipSite>(entity, site);
	EXPECT_FLOAT_EQ(worship::site::UseChants(entity, 40.0f), 40.0f);
	EXPECT_FLOAT_EQ(worship::site::UseChants(entity, 100.0f), 60.0f); // clamped
	EXPECT_FLOAT_EQ(registry.Get<const WorshipSite>(entity).used, 100.0f);
	EXPECT_FLOAT_EQ(registry.Get<const WorshipSite>(entity).requested, 140.0f);
	EXPECT_FLOAT_EQ(worship::site::UseChants(entity, -5.0f), 0.0f);
}

TEST_F(WorshipTest, EndOfTurnDanceIntensityAndBattery)
{
	// ProcessSpellIcons with 11 dancers, an empty battery and nothing used: the boost is 0.5, so k = 0.5 and the
	// battery gains capacity x k
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<WorshipSite>(entity, NorseSite(11));
	worship::site::ProcessSpellIcons(entity);
	const auto& site = registry.Get<const WorshipSite>(entity);
	EXPECT_FLOAT_EQ(site.danceSpeed, 0.5f);
	EXPECT_FLOAT_EQ(site.battery, 16.5f); // 33 x 0.5
	EXPECT_FLOAT_EQ(site.chantDamage, 16.5f / 11.0f);
	EXPECT_FLOAT_EQ(site.available, 16.5f + 33.0f);
	EXPECT_FLOAT_EQ(site.strain, -1.0f); // nothing asked and the dancers make 33: (0 - 33) / 33
	EXPECT_EQ(site.danceState, 1);       // k > 0 starts the dance
}

TEST_F(WorshipTest, StrainIsDemandOverCapacity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto site = NorseSite(10); // capacity 30
	site.requested = 90.0f;    // three times what the dancers make
	site.available = 90.0f;
	site.used = 90.0f;
	registry.Assign<WorshipSite>(entity, site);
	worship::site::ProcessSpellIcons(entity);
	// (90 - 30) / 30 = 2; with the strain positive the charging icons get nothing
	EXPECT_FLOAT_EQ(registry.Get<const WorshipSite>(entity).strain, 2.0f);
}

TEST_F(WorshipTest, ChantStoreKeepsTheExcessQuirk)
{
	// AddToChantStore with no site: below the requirement it returns what it took, above it
	// returns the excess and the store stops at the requirement
	const auto icon = MakeIcon(SpellSeedType::Fire);
	const float required = worship::icon::GetChantRequired(icon);
	ASSERT_FLOAT_EQ(required, 3500.0f);
	EXPECT_FLOAT_EQ(worship::icon::AddToChantStore(icon, required * 0.25f), required * 0.25f);
	EXPECT_FLOAT_EQ(worship::icon::GetChantNeeded(icon), required * 0.75f);
	const float excess = worship::icon::AddToChantStore(icon, required);
	EXPECT_FLOAT_EQ(excess, required - required * 0.75f);
	EXPECT_FLOAT_EQ(worship::icon::GetChantNeeded(icon), 0.0f);
	EXPECT_FLOAT_EQ(worship::icon::ChargeFraction(icon), 1.0f);
	// RemoveFromChantStore takes at most the store
	EXPECT_FLOAT_EQ(worship::icon::RemoveFromChantStore(icon, required * 2.0f), required);
	EXPECT_FLOAT_EQ(worship::icon::ChargeFraction(icon), 0.0f);
}

TEST_F(WorshipTest, WorshipersNeeded)
{
	// GetWorshipersNeeded: target = max(1, int(pop x pct + 0.5)), result = target - current + go-homes
	const auto town = MakeTown(1, 0.5f, 22);
	bool reachable = false;
	EXPECT_EQ(worship::percentage::GetWorshipersNeeded(town, true, true, &reachable), 11);
	EXPECT_FALSE(reachable); // result > 0 but nobody is there yet
	worship::percentage::AddWorshipper(town);
	EXPECT_EQ(worship::percentage::GetWorshipersNeeded(town, true, true, nullptr), 10);
	// a percentage that rounds to 0 still asks for one villager
	auto& magic = Locator::entitiesRegistry::value().Get<TownMagic>(town);
	magic.worshipPercentage = 0.01f;
	EXPECT_EQ(worship::percentage::GetWorshipersNeeded(town, true, true, nullptr), 0); // max(1, 0) - 1
	// no percentage, no target
	magic.worshipPercentage = 0.0f;
	EXPECT_EQ(worship::percentage::GetWorshipersNeeded(town, true, true, nullptr), -1);
}

TEST_F(WorshipTest, WorshipScoreFallsOffWithTheDistance)
{
	// WorshipScore uses GetDistanceModifier(d1, d2), which is SigmoidThreshold(0.5, 1 - min / d2) with the threshold
	// as its FIRST argument. openblack used to pass them the other way round, which mirrored the curve and sent the
	// farthest villagers first; it is the nearest who go. d1 is the villager's distance to the site's centre (the
	// site + (12.55, 0, -26.1) here, as the site is at the origin unrotated) and d2 the town's + 100, so about 128.9 m
	const auto town = MakeTown(1, 0.5f, 0);
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = [&](glm::vec3 position, float life) {
		const auto entity = registry.Create();
		auto& component = registry.Assign<Villager>(entity);
		component.town = town;
		component.life = life;
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	};
	const auto atCentre = villager(glm::vec3(12.55f, 0.0f, -26.1f), 0.5f);
	const auto farAway = villager(glm::vec3(1000.0f, 0.0f, 1000.0f), 1.0f);
	// at the centre the modifier is k_Sigmoid[30] = 0.99996 and the life goes in THREE times, not twice: 0.5^3, not
	// 0.5^2. The mirrored curve would give k_Sigmoid[10] = 3.6e-5 here
	EXPECT_FLOAT_EQ(worship::percentage::WorshipScore(atCentre), 0.5f * 0.5f * 0.5f * 0.999963939f);
	// beyond d2 the modifier is k_Sigmoid[10]: a full-life villager far away scores below a half-life one at the centre
	EXPECT_FLOAT_EQ(worship::percentage::WorshipScore(farAway), 3.60351005e-5f);
	EXPECT_GT(worship::percentage::WorshipScore(atCentre), worship::percentage::WorshipScore(farAway));
}

TEST_F(WorshipTest, FireFlyRewardProbabilities)
{
	// SetRewardProbability keeps running sums; Land1.txt gives HEAL 20 and six miracles 1 each
	worship::fire_fly::Reset();
	EXPECT_FLOAT_EQ(worship::fire_fly::Total(), 0.0f);
	worship::fire_fly::SetRewardProbability(MagicType::Fireball, 1.0f);
	worship::fire_fly::SetRewardProbability(MagicType::Heal, 20.0f);
	worship::fire_fly::SetRewardProbability(MagicType::Food, 1.0f);
	worship::fire_fly::SetRewardProbability(MagicType::Wood, 1.0f);
	worship::fire_fly::SetRewardProbability(MagicType::Water, 1.0f);
	worship::fire_fly::SetRewardProbability(MagicType::Forest, 1.0f);
	worship::fire_fly::SetRewardProbability(MagicType::LightningBolt, 1.0f);
	EXPECT_FLOAT_EQ(worship::fire_fly::Total(), 26.0f);
	// Reset (on clearing the map) zeroes the probabilities but not the running sums: the total of the land before
	// stays until the next FIRE_FLY_SPELL_REWARD_PROB (the original's quirk, kept)
	worship::fire_fly::Reset();
	EXPECT_FLOAT_EQ(worship::fire_fly::Total(), 26.0f);
	worship::fire_fly::SetRewardProbability(MagicType::Heal, 5.0f);
	EXPECT_FLOAT_EQ(worship::fire_fly::Total(), 5.0f);
}

TEST_F(WorshipTest, AFireflyRewardIsAOneShotAtFullStrength)
{
	// the one-shot logs its making
	if (spdlog::get("game") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
	// the FIRE seed in use, with the fireball's two power-ups in its second and third places
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	auto& fire = info->spellSeed.at(static_cast<size_t>(SpellSeedType::Fire));
	fire.exists = 1;
	fire.magicTypes = {MagicType::Fireball, MagicType::FireballPowerUpOne, MagicType::FireballPowerUpTwo, MagicType::None};
	Locator::infoConstants::reset(info.release());
	float drawn = 0.5f;
	game_random::testing::SetGameRand(nullptr, [&drawn](float /*range*/) { return drawn; });
	const glm::vec3 at {1200.0f, 30.0f, 900.0f};
	const auto& registry = Locator::entitiesRegistry::value();

	// the draw falls on the fireball: an orb of the FIRE seed where the tree or rock was, at strength 1, with no
	// power-up (the seed's own magic)
	worship::fire_fly::SetRewardProbability(MagicType::Fireball, 1.0f);
	const auto orb = worship::fire_fly::Reward(at);
	ASSERT_NE(orb, entt::entity {entt::null});
	const auto& seed = registry.Get<const OneOffSpellSeed>(orb);
	EXPECT_EQ(seed.seedType, SpellSeedType::Fire);
	EXPECT_EQ(seed.scale, 1.0f);
	EXPECT_EQ(seed.powerUp, -1);
	EXPECT_EQ(registry.Get<const Transform>(orb).position, at);

	// the second power-up is the seed's third magic: level 1
	worship::fire_fly::SetRewardProbability(MagicType::Fireball, 0.0f);
	worship::fire_fly::SetRewardProbability(MagicType::FireballPowerUpTwo, 1.0f);
	const auto poweredUp = worship::fire_fly::Reward(at);
	ASSERT_NE(poweredUp, entt::entity {entt::null});
	EXPECT_EQ(registry.Get<const OneOffSpellSeed>(poweredUp).seedType, SpellSeedType::Fire);
	EXPECT_EQ(registry.Get<const OneOffSpellSeed>(poweredUp).scale, 1.0f);
	EXPECT_EQ(registry.Get<const OneOffSpellSeed>(poweredUp).powerUp, 1);

	// a draw of 0 gives nothing, nor does one that falls on the first magic, NONE
	drawn = 0.0f;
	EXPECT_EQ(worship::fire_fly::Reward(at), entt::entity {entt::null});
	drawn = 0.5f;
	worship::fire_fly::SetRewardProbability(MagicType::FireballPowerUpTwo, 0.0f);
	worship::fire_fly::SetRewardProbability(MagicType::None, 1.0f);
	EXPECT_EQ(worship::fire_fly::Reward(at), entt::entity {entt::null});
	worship::fire_fly::SetRewardProbability(MagicType::None, 0.0f);
	game_random::testing::SetGameRand({}, {});
}

TEST_F(WorshipTest, DesireForFoodOfASite)
{
	// CalculateDesireForFood: 1 - min((food + 1e-4) / (needed + 1e-4), 1); needed is CalculateFoodNeededByDancers,
	// the sum of (1 - food) x foodReqiredForDinner
	auto& registry = Locator::entitiesRegistry::value();
	const auto siteEntity = registry.Create();
	registry.Assign<WorshipSite>(siteEntity, NorseSite(0));
	EXPECT_EQ(worship::site::DancerCount(siteEntity), 0);
	// no dancers and no pot: 1 - min(1e-4 / 1e-4, 1) = 0
	EXPECT_FLOAT_EQ(worship::site::CalculateDesireForFood(siteEntity), 0.0f);
	for (const float food : {0.5f, 0.75f})
	{
		const auto villager = registry.Create();
		registry.Assign<Villager>(villager).food = food;
		registry.Get<WorshipSite>(siteEntity).dancers.push_back(villager);
	}
	EXPECT_EQ(worship::site::DancerCount(siteEntity), 2);
	// (1 - 0.5) x 4 + (1 - 0.75) x 4 = 3
	EXPECT_FLOAT_EQ(worship::site::CalculateFoodNeededByDancers(siteEntity), 3.0f);
	// still no pot: 1 - 1e-4 / 3.0001
	EXPECT_FLOAT_EQ(worship::site::CalculateDesireForFood(siteEntity), 1.0f - 0.0001f / 3.0001f);
	const auto pot = registry.Create();
	registry.Assign<Pot>(pot, static_cast<uint16_t>(1), static_cast<uint16_t>(100), PotInfo::StoragePitFoodPile);
	registry.Get<WorshipSite>(siteEntity).foodPot = pot;
	EXPECT_EQ(worship::site::GetFoodResource(siteEntity), 1u);
	EXPECT_FLOAT_EQ(worship::site::CalculateDesireForFood(siteEntity), 1.0f - 1.0001f / 3.0001f);
	// enough food: the ratio is held at 1, no desire
	registry.Get<Pot>(pot).amount = 50;
	EXPECT_FLOAT_EQ(worship::site::CalculateDesireForFood(siteEntity), 0.0f);
	// not a site: nothing
	EXPECT_FLOAT_EQ(worship::site::CalculateDesireForFood(pot), 0.0f);
}

TEST(WorshipSiteAddResource, FoodGoesToThePotWoodOnlyToABuildingSite)
{
	using worship::site::AddResourceRoute;
	using worship::site::AddResourceRouteOf;
	EXPECT_EQ(AddResourceRouteOf(ResourceType::Food, false), AddResourceRoute::FoodPot);
	EXPECT_EQ(AddResourceRouteOf(ResourceType::Food, true), AddResourceRoute::FoodPot);
	EXPECT_EQ(AddResourceRouteOf(ResourceType::Wood, true), AddResourceRoute::BuildingSite);
	EXPECT_EQ(AddResourceRouteOf(ResourceType::Any, true), AddResourceRoute::BuildingSite);
	EXPECT_EQ(AddResourceRouteOf(ResourceType::Wood, false), AddResourceRoute::Nothing);
	EXPECT_EQ(AddResourceRouteOf(ResourceType::Any, false), AddResourceRoute::Nothing); // ANY is not FOOD
	EXPECT_EQ(AddResourceRouteOf(ResourceType::None, false), AddResourceRoute::Nothing);
}

TEST_F(WorshipTest, FoodGivenToASiteFillsItsFoodPot)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto siteEntity = registry.Create();
	registry.Assign<WorshipSite>(siteEntity, NorseSite(0));
	const auto pot = registry.Create();
	registry.Assign<Pot>(pot, static_cast<uint16_t>(10), static_cast<uint16_t>(100), PotInfo::StoragePitFoodPile);
	registry.Assign<Transform>(pot, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Get<WorshipSite>(siteEntity).foodPot = pot;
	EXPECT_EQ(worship::site::AddResource(siteEntity, ResourceType::Food, 300, true), 300u);
	EXPECT_EQ(worship::site::GetFoodResource(siteEntity), 310u);
	EXPECT_TRUE(registry.Get<Pot>(pot).poisoned);
	// through the objects' AddResource too, as a store's delivery asks it
	EXPECT_EQ(ecs::object_resources::AddResource(siteEntity, ResourceType::Food, 40), 40u);
	EXPECT_EQ(worship::site::GetFoodResource(siteEntity), 350u);
	// wood with no building site, or not a site: nothing
	EXPECT_EQ(worship::site::AddResource(siteEntity, ResourceType::Wood, 50), 0u);
	EXPECT_EQ(worship::site::AddResource(pot, ResourceType::Food, 50), 0u);
	EXPECT_EQ(worship::site::GetFoodResource(siteEntity), 350u);
}

TEST_F(WorshipTest, TheAltarsNeedSignsTakeTheSitesDesiresEachTurn)
{
	// A site with its altar and the altar's three signs, made in their order after the altar
	auto& registry = Locator::entitiesRegistry::value();
	const auto siteEntity = registry.Create();
	registry.Assign<WorshipSite>(siteEntity, NorseSite(0));
	const auto totem = registry.Create();
	ecs::object_index::Assign(totem);
	registry.Assign<Transform>(totem, glm::vec3(10.0f, 0.0f, 20.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<WorshipTotem>(totem, siteEntity);
	registry.Get<WorshipSite>(siteEntity).totem = totem;
	for (uint8_t row = 0; row < ecs::show_needs::k_AltarSigns; ++row)
	{
		registry.Get<WorshipSite>(siteEntity).needsSigns.at(row) =
		    ecs::show_needs::Create(glm::vec3(10.0f, 0.0f, 20.0f), totem, row);
	}
	const auto signs = registry.Get<WorshipSite>(siteEntity).needsSigns;
	for (uint8_t row = 0; row < ecs::show_needs::k_AltarSigns; ++row)
	{
		// each an object of its own, the next creation index after the altar's, standing where the altar does
		EXPECT_EQ(ecs::object_index::Of(signs.at(row)), ecs::object_index::Of(totem) + 1 + row);
		EXPECT_EQ(registry.Get<ShowNeedsVisuals>(signs.at(row)).infoIndex, row);
		EXPECT_EQ(registry.Get<ShowNeedsVisuals>(signs.at(row)).owner, totem);
		EXPECT_TRUE(registry.Get<Transform>(signs.at(row)).position == glm::vec3(10.0f, 0.0f, 20.0f));
	}
	// two dancers, half and three quarters fed, lives 0.4 and 0.9; five people at the site
	for (const auto& [food, life] : {std::pair {0.5f, 0.4f}, std::pair {0.75f, 0.9f}})
	{
		const auto villager = registry.Create();
		auto& v = registry.Assign<Villager>(villager);
		v.food = food;
		v.life = life;
		registry.Get<WorshipSite>(siteEntity).dancers.push_back(villager);
	}
	registry.Get<WorshipSite>(siteEntity).villagersAtSite = 5;
	worship::site::ProcessNeedsSigns(siteEntity);
	// hunger: no pot, 1 - 1e-4 / 3.0001; rest: ((1 - 0.4) + (1 - 0.9)) / 2; hiding: 3 people not dancing, 0.03
	EXPECT_FLOAT_EQ(registry.Get<ShowNeedsVisuals>(signs.at(0)).desire, 1.0f - 0.0001f / 3.0001f);
	EXPECT_FLOAT_EQ(registry.Get<ShowNeedsVisuals>(signs.at(1)).desire, 0.35f);
	EXPECT_FLOAT_EQ(registry.Get<ShowNeedsVisuals>(signs.at(2)).desire, 0.03f);
	EXPECT_FLOAT_EQ(worship::site::CalculateDesireForRest(siteEntity), 0.35f);
	EXPECT_FLOAT_EQ(worship::site::CalculatePeopleHidingIndicator(siteEntity), 0.03f);
	// not a site: no desire
	EXPECT_FLOAT_EQ(worship::site::CalculateDesireForRest(totem), 0.0f);
	EXPECT_FLOAT_EQ(worship::site::CalculatePeopleHidingIndicator(totem), 0.0f);
	// the altar gone: its signs go at the site's next turn, and the site forgets them
	ecs::ToBeDeleted(totem, true);
	worship::site::ProcessNeedsSigns(siteEntity);
	for (const auto sign : signs)
	{
		EXPECT_FALSE(ecs::IsAvailable(sign));
	}
	for (const auto sign : registry.Get<WorshipSite>(siteEntity).needsSigns)
	{
		EXPECT_TRUE(sign == entt::null);
	}
}

TEST_F(WorshipTest, AnAltarsNeedSignOnlyMovesOnTheFramesItsSiteIsDrawn)
{
	// A sign wanting to show (its desire over the row's least), on a site the object draw list does not draw: the end
	// of the frame finds it not drawn, so it has no Mesh and its Zoomer stays where it was
	auto& registry = Locator::entitiesRegistry::value();
	const auto siteEntity = registry.Create();
	registry.Assign<WorshipSite>(siteEntity, NorseSite(0));
	const auto totem = registry.Create();
	registry.Assign<WorshipTotem>(totem, siteEntity);
	const auto sign = ecs::show_needs::Create(glm::vec3(0.0f), totem, ecs::show_needs::k_HungerInfo);
	registry.Get<WorshipSite>(siteEntity).needsSigns.at(0) = sign;
	ecs::show_needs::SetDesire(sign, 0.8f);
	registry.Get<ShowNeedsVisuals>(sign).height.SetPosition(0.1f);
	// a draw of something that is not a workshop or a site draws no sign
	ecs::show_needs::DrawFor(totem, 0.5f, glm::vec3(0.0f, 10.0f, 0.0f));
	ecs::show_needs::EndDrawnFrame();
	EXPECT_FALSE(registry.AllOf<Mesh>(sign));
	EXPECT_FALSE(registry.Get<ShowNeedsVisuals>(sign).drawnThisFrame);
	EXPECT_FLOAT_EQ(registry.Get<ShowNeedsVisuals>(sign).height.value, 0.1f);
}

TEST_F(WorshipTest, ASitesDeletionTakesItsAltarsNeedSigns)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto siteEntity = registry.Create();
	registry.Assign<WorshipSite>(siteEntity, NorseSite(0));
	const auto totem = registry.Create();
	registry.Assign<Transform>(totem, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<WorshipTotem>(totem, siteEntity);
	registry.Get<WorshipSite>(siteEntity).totem = totem;
	for (uint8_t row = 0; row < ecs::show_needs::k_AltarSigns; ++row)
	{
		registry.Get<WorshipSite>(siteEntity).needsSigns.at(row) = ecs::show_needs::Create(glm::vec3(0.0f), totem, row);
	}
	const auto signs = registry.Get<WorshipSite>(siteEntity).needsSigns;
	worship::site::ToBeDeleted(siteEntity, true);
	for (const auto sign : signs)
	{
		EXPECT_FALSE(ecs::IsAvailable(sign));
	}
	EXPECT_FALSE(ecs::IsAvailable(totem));
}

namespace
{
/// The miracles as the gesture selection's request reaches them: each summon asked for, kept
class SummonRecorder final: public test::InertMagicSystem
{
public:
	struct Summon
	{
		PlayerNames player;
		SpellSeedType seed;
		int powerUp;
	};
	entt::entity SummonSeed(PlayerNames player, SpellSeedType seed, int powerUp) override
	{
		summons.push_back({player, seed, powerUp});
		return entt::null;
	}
	std::vector<Summon> summons;
};
} // namespace

TEST_F(WorshipTest, TheSelectionsRequestIsTheMiraclesSummonOfThePlainMiracleForTheHumanPlayer)
{
	test::RestoreService<Locator::magicSystem> keepMagic;
	auto& miracles = static_cast<SummonRecorder&>(Locator::magicSystem::emplace<SummonRecorder>());
	auto& registry = Locator::entitiesRegistry::value();
	// the first player is not human, the second is: the interface's player is the second
	const auto first = registry.Create();
	registry.Assign<Player>(first, PlayerNames::PLAYER_ONE);
	registry.Assign<PlayerMagic>(first);
	const auto human = registry.Create();
	registry.Assign<Player>(human, PlayerNames::PLAYER_TWO);
	registry.Assign<PlayerMagic>(human).playerType = 1;
	worship::gesture_icons::Register();

	magic::gestures::GetIconProvider()->RequestSpell(static_cast<int>(SpellSeedType::Fire));
	ASSERT_EQ(miracles.summons.size(), 1u);
	EXPECT_EQ(miracles.summons.at(0).player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(miracles.summons.at(0).seed, SpellSeedType::Fire);
	EXPECT_EQ(miracles.summons.at(0).powerUp, -1);
}

namespace
{
/// A state table for the worshippers' states: the state asked for is the state kept, every entry and exit accepts (no
/// function = 1)
class WorshipperStateTable final: public test::LivingActionFakeBase
{
public:
	void Update() override {}
	[[nodiscard]] VillagerStates VillagerGetState(const LivingAction& action, LivingAction::Index index) const override
	{
		return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	}
	[[nodiscard]] VillagerStates VillagerGetFinalState(const LivingAction& action) const override
	{
		return VillagerGetState(action, LivingAction::Index::Final);
	}
	void VillagerSetState(LivingAction& action, LivingAction::Index index, VillagerStates state,
	                      bool /*skipTransition*/) const override
	{
		action.states.at(static_cast<size_t>(index)) = static_cast<uint8_t>(state);
	}
	uint32_t VillagerCallState(LivingAction& /*action*/, LivingAction::Index /*index*/) const override { return 0; }
	uint32_t VillagerCallEntryState(LivingAction& /*action*/, VillagerStates /*row*/, VillagerStates /*final*/,
	                                VillagerStates /*next*/) const override
	{
		return 1;
	}
	uint32_t VillagerCallExitState(LivingAction& /*action*/, VillagerStates /*row*/, VillagerStates /*next*/) const override
	{
		return 1;
	}
	int VillagerCallOutOfAnimation(LivingAction& /*action*/, LivingAction::Index /*index*/) const override { return -1; }
	bool VillagerCallValidate(LivingAction& /*action*/, LivingAction::Index /*index*/) const override { return false; }
};

/// The worship tests with villagers in states: the state table as info.dat has it for the rows read here
class WorshipperTest: public WorshipTest
{
protected:
	void SetUp() override
	{
		WorshipTest::SetUp();
		// the base fixture's info, with the rows read here
		auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
		info->villager.at(0).hungryForFood = 0.5f;
		// the walks' move state (MOVE_TO_POS in every villager row of info.dat)
		info->villager.at(0).moveState = LivingStates::LivingMoveToPos;
		auto& table = info->villagerStateTable;
		// MOVE_ON_PATH: not a final state, and available, as info.dat has it
		table.at(static_cast<size_t>(VillagerStates::MoveOnPath)).field0xa8 = 1;
		// DECIDE_WHAT_TO_DO: a final state a villager may be sent to worship from
		table.at(static_cast<size_t>(VillagerStates::DecideWhatToDo)).isFinalState = 1;
		table.at(static_cast<size_t>(VillagerStates::DecideWhatToDo)).field0xa8 = 1;
		// the worship states are final, and those of 58-60, 213 and 241 keep a villager counted at worship
		for (const auto state : {VillagerStates::GotoWorshipSiteForWorship, VillagerStates::ArrivesAtWorshipSiteForWorship,
		                         VillagerStates::WorshippingAtWorshipSite, VillagerStates::HidingAtWorshipSite,
		                         VillagerStates::GetFoodAtWorshipSite})
		{
			table.at(static_cast<size_t>(state)).isFinalState = 1;
			table.at(static_cast<size_t>(state)).field0xa8 = 4;
			table.at(static_cast<size_t>(state)).field0xd8 = 1;
		}
		table.at(static_cast<size_t>(VillagerStates::RestartWorshippingAtWorshipSite)).isFinalState = 1;
		Locator::infoConstants::reset(info.release());
		Locator::livingActionSystem::emplace<WorshipperStateTable>();
	}

	void TearDown() override
	{
		Locator::livingActionSystem::reset();
		WorshipTest::TearDown();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A villager of the town in a state
	static entt::entity MakeWorshipper(entt::entity town, VillagerStates state)
	{
		auto& registry = Reg();
		const auto villager = registry.Create();
		auto& component = registry.Assign<Villager>(villager);
		component.town = town;
		component.life = 1.0f;
		component.food = 1.0f;
		registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<LivingAction>(villager, state, static_cast<uint16_t>(0));
		return villager;
	}

	static VillagerStates TopOf(entt::entity villager)
	{
		return static_cast<VillagerStates>(
		    Reg().Get<const LivingAction>(villager).states.at(static_cast<size_t>(LivingAction::Index::Top)));
	}

	static VillagerStates FinalOf(entt::entity villager)
	{
		return static_cast<VillagerStates>(
		    Reg().Get<const LivingAction>(villager).states.at(static_cast<size_t>(LivingAction::Index::Final)));
	}
};
} // namespace

TEST_F(WorshipperTest, ADiscipleIsSentOnlyOnTheSecondPass)
{
	using ecs::villager_worship::IsAvailableForWorshipSite;
	const auto town = MakeTown(1, 0.5f, 0);
	const auto plain = MakeWorshipper(town, VillagerStates::DecideWhatToDo);
	const auto disciple = MakeWorshipper(town, VillagerStates::DecideWhatToDo);
	Reg().Get<Villager>(disciple).flags |= Villager::k_FlagDisciple;
	EXPECT_TRUE(IsAvailableForWorshipSite(plain, false));
	EXPECT_TRUE(IsAvailableForWorshipSite(plain, true));
	// the first pass leaves the disciples for last
	EXPECT_FALSE(IsAvailableForWorshipSite(disciple, false));
	EXPECT_TRUE(IsAvailableForWorshipSite(disciple, true));
	// already on its way: never
	const auto walking = MakeWorshipper(town, VillagerStates::ArrivesAtWorshipSiteForWorship);
	EXPECT_FALSE(IsAvailableForWorshipSite(walking, true));
}

TEST_F(WorshipperTest, AWalkingVillagerIsJudgedByWhereItsWalkTakesIt)
{
	// MOVE_TO_POS is available, the altar's rest (62) is not, as info.dat has them
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->villagerStateTable.at(static_cast<size_t>(VillagerStates::MoveToPos)).field0xa8 = 1;
	auto& rest = info->villagerStateTable.at(static_cast<size_t>(VillagerStates::ArrivesAtAltarForRest));
	rest.isFinalState = 1;
	rest.field0xa8 = 4;
	Locator::infoConstants::reset(info.release());
	using ecs::villager_worship::IsAvailableForWorshipSite;
	const auto town = MakeTown(1, 0.5f, 0);
	const auto walkTo = [&](VillagerStates final) {
		const auto villager = MakeWorshipper(town, VillagerStates::MoveToPos);
		Reg().Get<LivingAction>(villager).states.at(static_cast<size_t>(LivingAction::Index::Final)) =
		    static_cast<uint8_t>(final);
		return villager;
	};
	// walking to the altar's rest: not available, although its walk is
	EXPECT_FALSE(IsAvailableForWorshipSite(walkTo(VillagerStates::ArrivesAtAltarForRest), true));
	// walking to decide afresh: available
	EXPECT_TRUE(IsAvailableForWorshipSite(walkTo(VillagerStates::DecideWhatToDo), true));
}

TEST_F(WorshipperTest, AWorshipDiscipleSettingOffStopsBeingOneAndRaisesTheShare)
{
	// a town of ten whose share wants one: the disciple sets off, and then the town wants none more
	const auto town = MakeTown(1, 0.1f, 9);
	const auto disciple = MakeWorshipper(town, VillagerStates::DecideWhatToDo);
	ecs::villager::SetVillagerDisciple(disciple, entt::null, VillagerDisciple::Worship, 0);
	ASSERT_NE(Reg().Get<const Villager>(disciple).flags & Villager::k_FlagDisciple, 0);
	EXPECT_EQ(ecs::villager_worship::GotoWorshipSiteForWorshipState(Reg().Get<LivingAction>(disciple)), 1u);
	EXPECT_EQ(TopOf(disciple), VillagerStates::ArrivesAtWorshipSiteForWorship);
	// no longer a disciple
	EXPECT_EQ(Reg().Get<const Villager>(disciple).discipleType, 0);
	EXPECT_EQ(Reg().Get<const Villager>(disciple).flags & Villager::k_FlagDisciple, 0);
	// the share is (0 wanted + 1.4 + 1 on the way + 0 worshipping) / 10 people, rounded as the original rounds it:
	// one bit above 0.24f
	EXPECT_EQ(worship::percentage::GetWorshipPercentage(town), std::bit_cast<float>(0x3E75C290u));
}

TEST_F(WorshipperTest, NobodyIsSentToASiteStillBeingBuilt)
{
	const auto town = MakeTown(1, 0.1f, 9);
	const auto siteEntity = Reg().Get<const TownMagic>(town).worshipSite;
	auto& build = Reg().Assign<CitadelPartBuild>(siteEntity);
	build.buildFlags = CitadelPartBuild::k_UnderConstruction;
	build.percentBuilt = 0.5f;
	const auto disciple = MakeWorshipper(town, VillagerStates::DecideWhatToDo);
	ecs::villager::SetVillagerDisciple(disciple, entt::null, VillagerDisciple::Worship, 0);
	// not sent: nobody on the way, the share left as it is, but the disciple is handed over all the same
	EXPECT_EQ(ecs::villager_worship::GotoWorshipSiteForWorshipState(Reg().Get<LivingAction>(disciple)), 0u);
	EXPECT_EQ(TopOf(disciple), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(Reg().Get<const WorshipSite>(siteEntity).dancersOnWay, 0);
	EXPECT_EQ(Reg().Get<const Villager>(disciple).discipleType, 0);
	EXPECT_FLOAT_EQ(worship::percentage::GetWorshipPercentage(town), 0.1f);
	// once built (the flag cleared and all of it up), it is sent
	build.buildFlags = CitadelPartBuild::k_Built;
	build.percentBuilt = 1.0f;
	const auto next = MakeWorshipper(town, VillagerStates::DecideWhatToDo);
	EXPECT_EQ(ecs::villager_worship::GotoWorshipSiteForWorshipState(Reg().Get<LivingAction>(next)), 1u);
	EXPECT_EQ(TopOf(next), VillagerStates::ArrivesAtWorshipSiteForWorship);
	EXPECT_EQ(Reg().Get<const WorshipSite>(siteEntity).dancersOnWay, 1);
}

TEST_F(WorshipperTest, AWorshipDiscipleWithNoSiteStopsBeingOneAndLeavesTheShare)
{
	const auto town = MakeTown(1, 0.1f, 9);
	Reg().Get<TownMagic>(town).worshipSite = entt::null;
	const auto disciple = MakeWorshipper(town, VillagerStates::DecideWhatToDo);
	ecs::villager::SetVillagerDisciple(disciple, entt::null, VillagerDisciple::Worship, 0);
	EXPECT_EQ(ecs::villager_worship::GotoWorshipSiteForWorshipState(Reg().Get<LivingAction>(disciple)), 0u);
	EXPECT_EQ(TopOf(disciple), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(Reg().Get<const Villager>(disciple).discipleType, 0);
	EXPECT_FLOAT_EQ(worship::percentage::GetWorshipPercentage(town), 0.1f);
}

TEST_F(WorshipTest, ASiteKeepsItsFoodAtItsLocalPoint)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto siteEntity = registry.Create();
	registry.Assign<WorshipSite>(siteEntity, NorseSite(0));
	// no placement yet: none
	EXPECT_FALSE(worship::site::GetFoodPos(siteEntity).has_value());
	registry.Assign<Transform>(siteEntity, glm::vec3(100.0f, 5.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	// its local point (9, 0, -38) through its placement, at the height of its origin
	const auto food = worship::site::GetFoodPos(siteEntity);
	ASSERT_TRUE(food.has_value());
	EXPECT_FLOAT_EQ(food->x, 109.0f);
	EXPECT_FLOAT_EQ(food->y, 5.0f);
	EXPECT_FLOAT_EQ(food->z, 162.0f);
	// not a site: none
	const auto other = registry.Create();
	registry.Assign<Transform>(other, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_FALSE(worship::site::GetFoodPos(other).has_value());
}

TEST_F(WorshipperTest, AHungryWorshipperEatsOnlyWithAMealInThePot)
{
	using ecs::villager_worship::CheckAllowedToRestAtWorshipSite;
	const auto town = MakeTown(1, 0.5f, 0);
	const auto siteEntity = Reg().Get<const TownMagic>(town).worshipSite;
	const auto worshipper = MakeWorshipper(town, VillagerStates::WorshippingAtWorshipSite);
	Reg().Get<Villager>(worshipper).food = 0.0f;
	const auto meal = ecs::villager::GetAmountOfFoodRequiredForMeal(worshipper);
	ASSERT_GT(meal, 0u);
	// no pot: nothing to eat
	EXPECT_FALSE(CheckAllowedToRestAtWorshipSite(worshipper, false));
	const auto pot = Reg().Create();
	Reg().Assign<Pot>(pot, static_cast<uint16_t>(meal - 1), static_cast<uint16_t>(100), PotInfo::StoragePitFoodPile);
	Reg().Assign<Transform>(pot, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Get<WorshipSite>(siteEntity).foodPot = pot;
	// one short of a meal: it stays
	EXPECT_FALSE(CheckAllowedToRestAtWorshipSite(worshipper, false));
	Reg().Get<Pot>(pot).amount = static_cast<uint16_t>(meal);
	EXPECT_TRUE(CheckAllowedToRestAtWorshipSite(worshipper, false));
	// at the hunger level exactly it is not hungry: strictly below (not the needs' "food <= hunger")
	Reg().Get<Villager>(worshipper).food = 0.5f;
	EXPECT_FALSE(CheckAllowedToRestAtWorshipSite(worshipper, false));
	// nothing was set off by the test
	EXPECT_EQ(TopOf(worshipper), VillagerStates::WorshippingAtWorshipSite);
}

TEST_F(WorshipperTest, LeavingTheMealKeepsAWorshipperCountedOnlyForAWorshipState)
{
	using ecs::villager_worship::ExitGetFoodAtWorship;
	const auto town = MakeTown(1, 0.5f, 0);
	const auto worshipper = MakeWorshipper(town, VillagerStates::GetFoodAtWorshipSite);
	Reg().Assign<WorshipVillager>(worshipper).atSite = true;
	worship::percentage::AddWorshipper(town);
	auto& action = Reg().Get<LivingAction>(worshipper);
	// back to the dance (a state the table marks as worship): still counted
	EXPECT_FALSE(ExitGetFoodAtWorship(action, VillagerStates::WorshippingAtWorshipSite));
	EXPECT_TRUE(ecs::villager_worship::IsAtWorshipSite(worshipper));
	// to 65, which leaves by the same exit: still counted
	EXPECT_FALSE(ExitGetFoodAtWorship(action, VillagerStates::RestartWorshippingAtWorshipSite));
	EXPECT_TRUE(ecs::villager_worship::IsAtWorshipSite(worshipper));
	EXPECT_EQ(Reg().Get<const TownMagic>(town).worshipping, 1);
	// off to something else: no longer at worship
	EXPECT_FALSE(ExitGetFoodAtWorship(action, VillagerStates::DecideWhatToDo));
	EXPECT_FALSE(ecs::villager_worship::IsAtWorshipSite(worshipper));
	EXPECT_EQ(Reg().Get<const TownMagic>(town).worshipping, 0);
}

TEST_F(WorshipperTest, TheWayToTheSiteIsLeftOnlyForAFinalStateWithAnotherExitThatIsNoTeleport)
{
	// the teleport reactions are final states, as info.dat has them
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	for (const auto state : {VillagerStates::GoTowardsTeleportReaction, VillagerStates::TeleportReaction,
	                         VillagerStates::GoTowardsTeleportReactionQuickly})
	{
		info->villagerStateTable.at(static_cast<size_t>(state)).isFinalState = 1;
	}
	Locator::infoConstants::reset(info.release());
	using ecs::villager_worship::ExitMoveToWorshipSite;
	const auto town = MakeTown(1, 0.5f, 0);
	const auto walker = MakeWorshipper(town, VillagerStates::ArrivesAtWorshipSiteForWorship);
	auto& way = Reg().Assign<WorshipVillager>(walker);
	way.onWay = true;
	way.onWayInTown = true;
	worship::percentage::AddVillagerOnWay(town, walker);
	auto& action = Reg().Get<LivingAction>(walker);
	// 58 leaves by the same exit, a move is not a final state, and a teleport reaction keeps the way: all kept
	for (const auto next : {VillagerStates::GotoWorshipSiteForWorship, VillagerStates::MoveToPos, VillagerStates::MoveOnPath,
	                        VillagerStates::GoTowardsTeleportReaction, VillagerStates::TeleportReaction,
	                        VillagerStates::GoTowardsTeleportReactionQuickly})
	{
		EXPECT_FALSE(ExitMoveToWorshipSite(action, next));
		EXPECT_TRUE(Reg().Get<const WorshipVillager>(walker).onWay) << static_cast<int>(next);
		EXPECT_EQ(Reg().Get<const TownMagic>(town).onWayToWorship, 1) << static_cast<int>(next);
	}
	// a final state with another exit: off the town's list, the flag cleared
	EXPECT_FALSE(ExitMoveToWorshipSite(action, VillagerStates::DecideWhatToDo));
	EXPECT_FALSE(Reg().Get<const WorshipVillager>(walker).onWay);
	EXPECT_FALSE(Reg().Get<const WorshipVillager>(walker).onWayInTown);
	EXPECT_EQ(Reg().Get<const TownMagic>(town).onWayToWorship, 0);
	EXPECT_TRUE(Reg().Get<const TownMagic>(town).onWayVillagers.empty());
	// the walks to the dance place and to the door are told 60 and 213, final states with another exit: the same
	for (const auto next : {VillagerStates::WorshippingAtWorshipSite, VillagerStates::HidingAtWorshipSite})
	{
		auto& again = Reg().Get<WorshipVillager>(walker);
		again.onWay = true;
		again.onWayInTown = true;
		worship::percentage::AddVillagerOnWay(town, walker);
		EXPECT_FALSE(ExitMoveToWorshipSite(action, next));
		EXPECT_FALSE(Reg().Get<const WorshipVillager>(walker).onWay) << static_cast<int>(next);
		EXPECT_EQ(Reg().Get<const TownMagic>(town).onWayToWorship, 0) << static_cast<int>(next);
	}
}

TEST_F(WorshipTest, ASiteWantsWhatItsDancersNeedLessWhatItHasAndWhatIsOnItsWay)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto siteEntity = registry.Create();
	registry.Assign<WorshipSite>(siteEntity, NorseSite(0));
	for (const float food : {0.5f, 0.75f})
	{
		const auto villager = registry.Create();
		registry.Assign<Villager>(villager).food = food;
		registry.Get<WorshipSite>(siteEntity).dancers.push_back(villager);
	}
	// (1 - 0.5) x 4 + (1 - 0.75) x 4 = 3
	EXPECT_EQ(worship::site::CalculateFoodNeededFromTown(siteEntity), 3);
	const auto pot = registry.Create();
	registry.Assign<Pot>(pot, static_cast<uint16_t>(1), static_cast<uint16_t>(100), PotInfo::StoragePitFoodPile);
	registry.Get<WorshipSite>(siteEntity).foodPot = pot;
	EXPECT_EQ(worship::site::CalculateFoodNeededFromTown(siteEntity), 2);
	// food taken off its way below none raises the want
	worship::site::RemoveFromFoodOnTheWay(siteEntity, 2);
	EXPECT_EQ(registry.Get<const WorshipSite>(siteEntity).foodOnTheWay, -2);
	EXPECT_EQ(worship::site::CalculateFoodNeededFromTown(siteEntity), 4);
	// more in the pot than wanted: none
	registry.Get<Pot>(pot).amount = 50;
	EXPECT_EQ(worship::site::CalculateFoodNeededFromTown(siteEntity), 0);
	// not a site: none
	EXPECT_EQ(worship::site::CalculateFoodNeededFromTown(pot), 0);
}

TEST_F(WorshipperTest, TheHiddenWorshipAtTheSitesDoor)
{
	const auto town = MakeTown(1, 0.5f, 0);
	const auto siteEntity = Reg().Get<const TownMagic>(town).worshipSite;
	Reg().Get<Transform>(siteEntity).position = glm::vec3(100.0f, 0.0f, 50.0f);
	// the site's door (here, with no mesh door point, its own position), not one of its special points
	const auto place = ecs::villager_worship::HidingPlace(siteEntity);
	const auto door = ecs::town_queries::ToMetres(ecs::abode_queries::GetDoorPos(siteEntity));
	EXPECT_FLOAT_EQ(place.x, door.x);
	EXPECT_FLOAT_EQ(place.z, door.y);
	EXPECT_NEAR(place.x, 100.0f, 0.001f);
	EXPECT_NEAR(place.z, 50.0f, 0.001f);
	// a full dance sends an arriving worshipper there: the walk to the door (the site has no footpaths here, so the
	// plain walk) with 213 to follow, counted at the site
	Reg().Get<WorshipSite>(siteEntity).dancers.assign(20, entt::null);
	const auto worshipper = MakeWorshipper(town, VillagerStates::ArrivesAtWorshipSiteForWorship);
	Reg().Assign<WallHug>(worshipper, glm::vec2(0.0f), glm::vec2(0.0f), 0.0f, 0.1f);
	EXPECT_EQ(ecs::villager_worship::ArrivesAtWorshipSiteForWorship(Reg().Get<LivingAction>(worshipper)), 1u);
	EXPECT_EQ(TopOf(worshipper), VillagerStates::MoveToPos);
	EXPECT_EQ(FinalOf(worshipper), VillagerStates::HidingAtWorshipSite);
	EXPECT_NEAR(Reg().Get<const WallHug>(worshipper).goal.x, 100.0f, 0.001f);
	EXPECT_NEAR(Reg().Get<const WallHug>(worshipper).goal.y, 50.0f, 0.001f);
	EXPECT_TRUE(ecs::villager_worship::IsAtWorshipSite(worshipper));
	// one that cannot walk is not sent, and so not counted at the site
	const auto still = MakeWorshipper(town, VillagerStates::ArrivesAtWorshipSiteForWorship);
	static_cast<void>(ecs::villager_worship::ArrivesAtWorshipSiteForWorship(Reg().Get<LivingAction>(still)));
	EXPECT_EQ(TopOf(still), VillagerStates::ArrivesAtWorshipSiteForWorship);
	EXPECT_FALSE(ecs::villager_worship::IsAtWorshipSite(still));
}

TEST_F(WorshipperTest, AnArrivingWorshipperWalksToItsDancePlaceAndThenDances)
{
	const auto town = MakeTown(1, 0.5f, 0);
	const auto siteEntity = Reg().Get<const TownMagic>(town).worshipSite;
	const auto worshipper = MakeWorshipper(town, VillagerStates::ArrivesAtWorshipSiteForWorship);
	Reg().Assign<WallHug>(worshipper, glm::vec2(0.0f), glm::vec2(0.0f), 0.0f, 0.1f);
	EXPECT_EQ(ecs::villager_worship::ArrivesAtWorshipSiteForWorship(Reg().Get<LivingAction>(worshipper)), 1u);
	// in the dance group, walking to its place with 60 to follow, counted at the site
	EXPECT_EQ(TopOf(worshipper), VillagerStates::MoveToPos);
	EXPECT_EQ(FinalOf(worshipper), VillagerStates::WorshippingAtWorshipSite);
	EXPECT_EQ(Reg().Get<const WorshipSite>(siteEntity).dancers, std::vector<entt::entity> {worshipper});
	EXPECT_TRUE(Reg().Get<const WorshipVillager>(worshipper).dancing);
	EXPECT_TRUE(ecs::villager_worship::IsAtWorshipSite(worshipper));
	const auto place = worship::site::DancePosition(siteEntity, worshipper);
	EXPECT_FLOAT_EQ(Reg().Get<const WallHug>(worshipper).goal.x, place.x);
	EXPECT_FLOAT_EQ(Reg().Get<const WallHug>(worshipper).goal.y, place.z);
	// one that cannot walk joins the dance group but is not counted at the site
	const auto still = MakeWorshipper(town, VillagerStates::ArrivesAtWorshipSiteForWorship);
	static_cast<void>(ecs::villager_worship::ArrivesAtWorshipSiteForWorship(Reg().Get<LivingAction>(still)));
	EXPECT_EQ(TopOf(still), VillagerStates::ArrivesAtWorshipSiteForWorship);
	EXPECT_FALSE(ecs::villager_worship::IsAtWorshipSite(still));
}

TEST_F(WorshipperTest, AHidingWorshipperAwayFromTheDoorWalksBackToItWithAHug)
{
	const auto town = MakeTown(1, 0.5f, 0);
	const auto siteEntity = Reg().Get<const TownMagic>(town).worshipSite;
	Reg().Get<Transform>(siteEntity).position = glm::vec3(100.0f, 0.0f, 50.0f);
	const auto worshipper = MakeWorshipper(town, VillagerStates::HidingAtWorshipSite);
	Reg().Assign<WallHug>(worshipper, glm::vec2(0.0f), glm::vec2(0.0f), 0.0f, 0.1f);
	ASSERT_FALSE(ecs::villager_worship::IsAtHidingPlace(worshipper, siteEntity));
	EXPECT_EQ(ecs::villager_worship::HidingAtWorshipSite(Reg().Get<LivingAction>(worshipper)), 1u);
	EXPECT_EQ(TopOf(worshipper), VillagerStates::MoveToPos);
	EXPECT_EQ(FinalOf(worshipper), VillagerStates::HidingAtWorshipSite);
	EXPECT_NEAR(Reg().Get<const WallHug>(worshipper).goal.x, 100.0f, 0.001f);
	EXPECT_NEAR(Reg().Get<const WallHug>(worshipper).goal.y, 50.0f, 0.001f);
}

TEST_F(WorshipperTest, AVillagerIsOnItsWayByItsFinalStateOrTheStateATeleportGoesBackTo)
{
	using ecs::villager_worship::IsAtOrOnTheWayToWorshipSite;
	using ecs::villager_worship::IsAvailableForWorshipSite;
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->villagerStateTable.at(static_cast<size_t>(VillagerStates::TeleportReaction)).isFinalState = 1;
	Locator::infoConstants::reset(info.release());
	const auto town = MakeTown(1, 0.5f, 0);
	const auto setState = [](entt::entity villager, LivingAction::Index index, VillagerStates state) {
		Reg().Get<LivingAction>(villager).states.at(static_cast<size_t>(index)) = static_cast<uint8_t>(state);
	};
	// walking the footpath to the gate, 59 to follow: on its way, so not sent again although MOVE_ON_PATH is available
	const auto walking = MakeWorshipper(town, VillagerStates::MoveOnPath);
	setState(walking, LivingAction::Index::Final, VillagerStates::ArrivesAtWorshipSiteForWorship);
	EXPECT_TRUE(IsAtOrOnTheWayToWorshipSite(walking));
	EXPECT_FALSE(IsAvailableForWorshipSite(walking, true));
	// walking somewhere else: not
	const auto elsewhere = MakeWorshipper(town, VillagerStates::MoveOnPath);
	setState(elsewhere, LivingAction::Index::Final, VillagerStates::DecideWhatToDo);
	EXPECT_FALSE(IsAtOrOnTheWayToWorshipSite(elsewhere));
	// in a teleport reaction, by the state stored to go back to
	const auto jumping = MakeWorshipper(town, VillagerStates::TeleportReaction);
	setState(jumping, LivingAction::Index::Previous, VillagerStates::ArrivesAtWorshipSiteForWorship);
	EXPECT_TRUE(IsAtOrOnTheWayToWorshipSite(jumping));
	setState(jumping, LivingAction::Index::Previous, VillagerStates::DecideWhatToDo);
	EXPECT_FALSE(IsAtOrOnTheWayToWorshipSite(jumping));
	// setting off (58) is not yet on the way
	EXPECT_FALSE(IsAtOrOnTheWayToWorshipSite(MakeWorshipper(town, VillagerStates::GotoWorshipSiteForWorship)));
}

TEST_F(WorshipperTest, AHidingWorshipperIsAtTheDoorWithinItsSpeedAndOneUnit)
{
	const auto town = MakeTown(1, 0.5f, 0);
	const auto siteEntity = Reg().Get<const TownMagic>(town).worshipSite;
	Reg().Get<Transform>(siteEntity).position = glm::vec3(100.0f, 0.0f, 50.0f);
	const auto door = ecs::abode_queries::GetDoorPos(siteEntity);
	ASSERT_EQ(door, glm::ivec2(655360, 327680));
	const auto worshipper = MakeWorshipper(town, VillagerStates::HidingAtWorshipSite);
	// no walk: never there
	EXPECT_FALSE(ecs::villager_worship::IsAtHidingPlace(worshipper, siteEntity));
	// 0.15625 m a turn is 1024 whole units
	auto& wallHug = Reg().Assign<WallHug>(worshipper, glm::vec2(0.0f), glm::vec2(0.0f), 0.0f, 0.15625f);
	const auto placeAt = [&](int32_t unitsPastTheDoor) {
		const int32_t x = door.x + unitsPastTheDoor;
		Reg().Get<Transform>(worshipper).position = glm::vec3(map_coords::ToMetres(x), 0.0f, map_coords::ToMetres(door.y));
		ASSERT_EQ(map_coords::FromMetres(glm::vec2(map_coords::ToMetres(x), map_coords::ToMetres(door.y))).x, x);
	};
	// within the speed and one more unit, strictly: 1024 units away is there, 1025 is not
	const std::array<std::pair<int32_t, bool>, 5> cases {
	    {{0, true}, {1024, true}, {1025, false}, {-1025, false}, {13107, false}}};
	for (const auto& [units, there] : cases)
	{
		placeAt(units);
		EXPECT_EQ(ecs::villager_worship::IsAtHidingPlace(worshipper, siteEntity), there) << units;
	}
	// standing still, only within the one unit
	wallHug.speed = 0.0f;
	placeAt(0);
	EXPECT_TRUE(ecs::villager_worship::IsAtHidingPlace(worshipper, siteEntity));
	placeAt(1);
	EXPECT_FALSE(ecs::villager_worship::IsAtHidingPlace(worshipper, siteEntity));
}

TEST_F(WorshipperTest, TheChantCostsAWorshipperByItsOwnInfo)
{
	// a town that can worship and still needs worshippers, so its dancer stays
	const auto town = MakeTown(1, 0.5f, 3);
	const auto siteEntity = Reg().Get<const TownMagic>(town).worshipSite;
	Reg().Get<WorshipSite>(siteEntity).player = PlayerNames::PLAYER_ONE;
	Reg().Get<WorshipSite>(siteEntity).chantDamage = 100.0f;
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	auto& centreInfo = info->abode.at(static_cast<size_t>(AbodeInfo::NorseTownCentre));
	centreInfo.abodeType = AbodeType::TownCentre;
	centreInfo.abodeNumber = AbodeNumber::TownCentre;
	// a villager info row of its own, with a rate unlike the first row's 5e-6
	auto& own = info->villager.at(7);
	own.tribeType = Tribe::AZTEC;
	own.villagerNumber = VillagerNumber::Farmer;
	own.chantLifeRate = 1e-3f;
	Locator::infoConstants::reset(info.release());
	const auto centre = Reg().Create();
	auto& abode = Reg().Assign<Abode>(centre);
	abode.type = AbodeNumber::TownCentre;
	abode.townId = 1;
	const auto worshipper = MakeWorshipper(town, VillagerStates::WorshippingAtWorshipSite);
	Reg().Get<Villager>(worshipper).tribe = Tribe::AZTEC;
	Reg().Get<Villager>(worshipper).number = VillagerNumber::Farmer;
	static_cast<void>(ecs::villager_worship::WorshippingAtWorshipSite(Reg().Get<LivingAction>(worshipper)));
	// 1 - 100 x its own 1e-3, not the first row's
	EXPECT_EQ(TopOf(worshipper), VillagerStates::WorshippingAtWorshipSite);
	EXPECT_FLOAT_EQ(Reg().Get<const Villager>(worshipper).life, 1.0f - (100.0f * 1e-3f));
}

TEST_F(WorshipperTest, ADancerLeavesTheDanceEvenWhenItsTownsSiteIsGone)
{
	const auto town = MakeTown(1, 0.5f, 0);
	const auto siteEntity = Reg().Get<const TownMagic>(town).worshipSite;
	// going to eat at the site with its town's site link gone: out of the dance all the same
	const auto eating = MakeWorshipper(town, VillagerStates::WorshippingAtWorshipSite);
	const auto leaving = MakeWorshipper(town, VillagerStates::WorshippingAtWorshipSite);
	const auto staying = MakeWorshipper(town, VillagerStates::WorshippingAtWorshipSite);
	for (const auto dancer : {eating, leaving, staying})
	{
		worship::site::AddDancer(siteEntity, dancer);
	}
	Reg().Get<TownMagic>(town).worshipSite = entt::null;
	static_cast<void>(
	    ecs::villager_worship::ExitAtWorshipSite(Reg().Get<LivingAction>(eating), VillagerStates::GetFoodAtWorshipSite));
	// leaving the site altogether, the same
	ecs::villager_worship::RemoveVillagerFromWorshipSite(leaving);
	EXPECT_EQ(Reg().Get<const WorshipSite>(siteEntity).dancers, std::vector<entt::entity> {staying});
	// a villager with no town keeps its place in the dance, as the game leaves it
	Reg().Get<Villager>(staying).town = entt::null;
	ecs::villager_worship::RemoveVillagerFromWorshipSite(staying);
	EXPECT_EQ(Reg().Get<const WorshipSite>(siteEntity).dancers, std::vector<entt::entity> {staying});
}

TEST_F(WorshipperTest, ATiredWorshipperDoesNotAskToGoHome)
{
	// a town that can worship (its own site and a town centre) and still needs worshippers: 2 of 4
	const auto town = MakeTown(1, 0.5f, 3);
	const auto siteEntity = Reg().Get<const TownMagic>(town).worshipSite;
	Reg().Get<WorshipSite>(siteEntity).player = PlayerNames::PLAYER_ONE;
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	auto& centreInfo = info->abode.at(static_cast<size_t>(AbodeInfo::NorseTownCentre));
	centreInfo.abodeType = AbodeType::TownCentre;
	centreInfo.abodeNumber = AbodeNumber::TownCentre;
	Locator::infoConstants::reset(info.release());
	const auto centre = Reg().Create();
	auto& abode = Reg().Assign<Abode>(centre);
	abode.type = AbodeNumber::TownCentre;
	abode.townId = 1;
	// hiding at the site's door with its life below the threshold to go home (0.3)
	const auto worshipper = MakeWorshipper(town, VillagerStates::HidingAtWorshipSite);
	Reg().Assign<WallHug>(worshipper, glm::vec2(0.0f), glm::vec2(0.0f), 0.0f, 0.0f);
	Reg().Get<Transform>(worshipper).position = Reg().Get<const Transform>(siteEntity).position;
	ASSERT_TRUE(ecs::villager_worship::IsAtHidingPlace(worshipper, siteEntity));
	Reg().Get<Villager>(worshipper).life = 0.1f;
	static_cast<void>(ecs::villager_worship::HidingAtWorshipSite(Reg().Get<LivingAction>(worshipper)));
	// it worships on, and nobody is in the site's go-home queue
	EXPECT_EQ(TopOf(worshipper), VillagerStates::HidingAtWorshipSite);
	EXPECT_TRUE(Reg().Get<const WorshipSite>(siteEntity).goHomeRequests.empty());
	EXPECT_EQ(worship::site::VillagersRequestingToGoHome(Reg().Get<const WorshipSite>(siteEntity)), 0);
	// the worship link is made only when something of the villager's worship is kept (a missing one asks for nothing)
	const auto* link = Reg().TryGet<const WorshipVillager>(worshipper);
	EXPECT_TRUE(link == nullptr || !link->requestedGoHome);
	EXPECT_EQ(worship::percentage::GetWorshipersNeeded(town, true, true, nullptr), 2);
}

namespace
{
/// The script natives write their errors to the "scripting" log
void EnsureScriptingLog()
{
	if (!spdlog::get("scripting"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
	}
}
} // namespace

// A seed shaken or dropped from the hand, or thrown from it by force, gives its whole charge back to the battery of its
// icon's site and goes; a seed of no icon goes all the same (docs/wiki/magic/hand-casting.md, a seed leaving the hand)
TEST_F(WorshipTest, ASeedLeftOutOfTheHandGivesItsChargeBackToItsIconsSite)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto siteEntity = registry.Create();
	registry.Assign<WorshipSite>(siteEntity, NorseSite(0)).battery = 100.0f;
	const auto icon = MakeIcon(SpellSeedType::Fire);
	registry.Get<WorshipSpellIcon>(icon).site = siteEntity;
	const auto seed = registry.Create();
	auto& charged = registry.Assign<SpellSeed>(seed);
	charged.seedType = SpellSeedType::Fire;
	charged.icon = icon;
	charged.chantStore = 750.0f;
	registry.Get<WorshipSpellIcon>(icon).seeds.push_back(seed);

	EXPECT_EQ(worship::ReturnSeedToItsSite(seed), 3);
	EXPECT_FLOAT_EQ(registry.Get<const WorshipSite>(siteEntity).battery, 850.0f);
	EXPECT_FALSE(registry.Valid(seed));
	EXPECT_TRUE(registry.Get<const WorshipSpellIcon>(icon).seeds.empty());

	const auto loose = registry.Create();
	registry.Assign<SpellSeed>(loose).chantStore = 500.0f;
	EXPECT_EQ(worship::ReturnSeedToItsSite(loose), 3);
	EXPECT_FALSE(registry.Valid(loose));
	EXPECT_FLOAT_EQ(registry.Get<const WorshipSite>(siteEntity).battery, 850.0f);
}

// A seed given to its site's altar puts its whole charge into the site's battery and goes
// (docs/wiki/magic/hand-casting.md, the seed given back to a return point)
TEST_F(WorshipTest, ASeedGivenToTheAltarPutsItsChargeIntoTheSitesBattery)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto siteEntity = registry.Create();
	registry.Assign<WorshipSite>(siteEntity, NorseSite(0)).battery = 100.0f;
	const auto altar = registry.Create();
	registry.Assign<WorshipTotem>(altar).site = siteEntity;
	const auto seed = registry.Create();
	auto& charged = registry.Assign<SpellSeed>(seed);
	charged.seedType = SpellSeedType::Fire;
	charged.chantStore = 2000.0f;

	EXPECT_EQ(worship::ApplySeedToObject(seed, altar), 3);
	EXPECT_FLOAT_EQ(registry.Get<const WorshipSite>(siteEntity).battery, 2100.0f);
	EXPECT_FALSE(registry.Valid(seed));
}

// Powering a seed to a level that costs less than its charge sends the surplus back to its icon's site, and the seed
// keeps the cost; a charge at or under the cost stays whole (docs/wiki/magic/spell-core.md, the seed's power-up). The
// FIRE seed's base miracle costs 3500 chants (info.dat)
TEST_F(WorshipTest, PoweringASeedSendsTheChargeAboveTheCostBackToItsSite)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto siteEntity = registry.Create();
	registry.Assign<WorshipSite>(siteEntity, NorseSite(0)).battery = 100.0f;
	const auto icon = MakeIcon(SpellSeedType::Fire);
	registry.Get<WorshipSpellIcon>(icon).site = siteEntity;
	const auto seed = registry.Create();
	auto& charged = registry.Assign<SpellSeed>(seed);
	charged.seedType = SpellSeedType::Fire;
	charged.icon = icon;
	charged.chantStore = 5000.0f;

	magic::seed::SetPowerUp(seed, -1);
	EXPECT_FLOAT_EQ(registry.Get<const WorshipSite>(siteEntity).battery, 1600.0f);
	EXPECT_FLOAT_EQ(registry.Get<const SpellSeed>(seed).chantStore, 3500.0f);
	EXPECT_EQ(registry.Get<const SpellSeed>(seed).powerUp, -1);

	// at the cost nothing moves
	magic::seed::SetPowerUp(seed, -1);
	EXPECT_FLOAT_EQ(registry.Get<const WorshipSite>(siteEntity).battery, 1600.0f);
	EXPECT_FLOAT_EQ(registry.Get<const SpellSeed>(seed).chantStore, 3500.0f);

	// under it the charge stays
	registry.Get<SpellSeed>(seed).chantStore = 2000.0f;
	magic::seed::SetPowerUp(seed, -1);
	EXPECT_FLOAT_EQ(registry.Get<const WorshipSite>(siteEntity).battery, 1600.0f);
	EXPECT_FLOAT_EQ(registry.Get<const SpellSeed>(seed).chantStore, 2000.0f);
}

// A town leaving its site comes off the site's towns, and so do the site's icons of the miracles no town still on it
// holds; the site stays, even with no town left (docs/wiki/project/progress/worship/worship_sites.md, a town lost)
TEST_F(WorshipTest, ATownLeavingItsSiteTakesTheIconsNoOtherTownOfTheSiteHolds)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto siteEntity = registry.Create();
	registry.Assign<Transform>(siteEntity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<WorshipSite>(siteEntity, NorseSite(0));
	// the towns' own icons of their miracles
	const auto townIcon = [&registry](SpellSeedType seedType) {
		const auto entity = registry.Create();
		registry.Assign<SpellIcon>(entity).seedType = seedType;
		return entity;
	};
	const auto keeping = registry.Create();
	registry.Assign<TownMagic>(keeping);
	const auto keptFire = townIcon(SpellSeedType::Fire);
	registry.Get<TownMagic>(keeping).spellIcons = {keptFire};
	registry.Get<TownMagic>(keeping).worshipSite = siteEntity;
	const auto leaving = registry.Create();
	registry.Assign<TownMagic>(leaving);
	const auto leftFire = townIcon(SpellSeedType::Fire);
	const auto leftWater = townIcon(SpellSeedType::Water);
	registry.Get<TownMagic>(leaving).spellIcons = {leftFire, leftWater};
	registry.Get<TownMagic>(leaving).worshipSite = siteEntity;
	// the site's icons of those miracles
	const auto fire = MakeIcon(SpellSeedType::Fire);
	registry.Get<WorshipSpellIcon>(fire).site = siteEntity;
	const auto water = MakeIcon(SpellSeedType::Water);
	registry.Get<WorshipSpellIcon>(water).site = siteEntity;
	registry.Get<WorshipSite>(siteEntity).icons = {water, fire};
	registry.Get<WorshipSite>(siteEntity).towns = {leaving, keeping};

	worship::site::RemoveTown(siteEntity, leaving);
	EXPECT_EQ(registry.Get<const WorshipSite>(siteEntity).towns, std::vector<entt::entity> {keeping});
	EXPECT_EQ(registry.Get<const TownMagic>(leaving).worshipSite, entt::entity {entt::null});
	EXPECT_EQ(registry.Get<const TownMagic>(keeping).worshipSite, siteEntity);
	EXPECT_EQ(registry.Get<const WorshipSite>(siteEntity).icons, std::vector<entt::entity> {fire});
	EXPECT_FALSE(registry.Valid(water));
	EXPECT_TRUE(registry.Valid(fire));

	worship::site::RemoveTown(siteEntity, keeping);
	EXPECT_TRUE(registry.Get<const WorshipSite>(siteEntity).towns.empty());
	EXPECT_TRUE(registry.Get<const WorshipSite>(siteEntity).icons.empty());
	EXPECT_FALSE(registry.Valid(fire));
	EXPECT_TRUE(registry.Valid(siteEntity));
	EXPECT_TRUE(registry.AllOf<WorshipSite>(siteEntity));
}

// GET_SACRIFICE_TOTAL answers a worship site's sacrificed prayer power as a float, and anything else with an error and
// 0 (docs/wiki/project/progress/worship/prayer_power.md, the sacrifice total)
TEST_F(WorshipTest, AScriptReadsTheSacrificeTotalOfASiteAndNothingOfAnythingElse)
{
	const test::RestoreService<Locator::vm> restoreVm;
	EnsureScriptingLog();
	auto& vm = Locator::vm::emplace<lhvm::LHVM>();
	auto& registry = Locator::entitiesRegistry::value();
	// a script object of 0 is none: the site is not the first entity
	static_cast<void>(registry.Create());
	const auto siteEntity = registry.Create();
	registry.Assign<WorshipSite>(siteEntity, NorseSite(0)).sacrificed = 120.0f;

	vm.Pusho(static_cast<uint32_t>(siteEntity));
	magic::script::GetSacrificeTotal();
	lhvm::DataType type {};
	const auto total = vm.Pop(type);
	EXPECT_EQ(type, lhvm::DataType::Float);
	EXPECT_FLOAT_EQ(total.floatVal, 120.0f);

	// a town is not a site, though it has one
	const auto town = MakeTown(1, 0.0f, 0);
	registry.Get<WorshipSite>(registry.Get<const TownMagic>(town).worshipSite).sacrificed = 50.0f;
	vm.Pusho(static_cast<uint32_t>(town));
	magic::script::GetSacrificeTotal();
	EXPECT_FLOAT_EQ(vm.Pop(type).floatVal, 0.0f);
	EXPECT_EQ(type, lhvm::DataType::Float);
}

// SET_CAN_BUILD_WORSHIPSITE on a town: off forbids it a site, on lets it have one again (it then looks for its site:
// none here, as its player has no citadel). A town of Land 1, or one without people, may have none either way
// (docs/wiki/magic/temple-and-citadel.md, the towns' sites)
TEST_F(WorshipTest, AScriptForbidsATownItsWorshipSiteAndAllowsItAgain)
{
	const test::RestoreService<Locator::vm> restoreVm;
	EnsureScriptingLog();
	auto& vm = Locator::vm::emplace<lhvm::LHVM>();
	auto& registry = Locator::entitiesRegistry::value();
	// a script object of 0 is none: the town is not the first entity
	static_cast<void>(registry.Create());
	const auto town = MakeTown(1, 0.0f, 2);
	ASSERT_TRUE(worship::town::IsAllowedToCreateWorshipSite(town));

	// the switch first, the town on top
	vm.Pushi(0);
	vm.Pusho(static_cast<uint32_t>(town));
	magic::script::SetCanBuildWorshipsite();
	EXPECT_TRUE(registry.Get<const TownMagic>(town).forbidWorshipSite);
	EXPECT_FALSE(worship::town::IsAllowedToCreateWorshipSite(town));

	vm.Pushi(1);
	vm.Pusho(static_cast<uint32_t>(town));
	magic::script::SetCanBuildWorshipsite();
	EXPECT_FALSE(registry.Get<const TownMagic>(town).forbidWorshipSite);
	EXPECT_TRUE(worship::town::IsAllowedToCreateWorshipSite(town));

	influence::detail::MapGlobals().landNumber = 1;
	EXPECT_FALSE(worship::town::IsAllowedToCreateWorshipSite(town));
	influence::detail::MapGlobals().landNumber = 0;
	const auto empty = MakeTown(2, 0.0f, 0);
	EXPECT_FALSE(worship::town::IsAllowedToCreateWorshipSite(empty));
}
