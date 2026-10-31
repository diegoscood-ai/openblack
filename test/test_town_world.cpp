/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A land script's town and its store, played turn by turn in a small world of the game's own systems and data: Land 1's
// first town and the Norse storage pit its land script gives it, with 20000 food and 30000 wood. The store is made with
// its food pile and its five wood piles at its model's points, turned with it; the script's food goes into the food pile
// and then its wood into the wood piles in turn, each up to its cap and the last with none; the store becomes its
// town's, and pulses it; and the town's desire flags go to the store's points after the first twelve as it is made, and
// stay there as the turns go by. Every expected value is the original's (docs/wiki/world/objects.md#store-storagepit,
// docs/wiki/world/objects.md#store-details, docs/wiki/beings/towns.md#the-desire-flags).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <L3DFile.h>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/MapCoords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectResources.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/HandToolTipRules.h"
#include "ECS/Systems/Implementations/TownDesireSystem.h"
#include "ECS/Town/TownQueries.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "support/RestoreService.h"
#include "support/WorldHarness.h"

using namespace openblack;
using ecs::components::Abode;
using ecs::components::Mesh;
using ecs::components::Pot;
using ecs::components::StoragePit;
using ecs::components::Town;
using ecs::components::TownDesireFlags;
using ecs::components::Transform;
using lhscriptx::FeatureScriptCommands;
using lhscriptx::Script;

namespace
{
/// Land 1's first town and its store, as the land script's lines make them: the town's number, place, player and tribe,
/// and the store's place, abode, angle and size in thousandths, food and wood
constexpr int32_t k_Town = 0;
const glm::vec2 k_TownAt {1865.61f, 2641.24f};
const glm::vec2 k_StoreAt {1826.79f, 2641.36f};
constexpr int32_t k_StoreAngle = 3730;
constexpr int32_t k_StoreSize = 1000;
constexpr int32_t k_StoreFood = 20000;
constexpr int32_t k_StoreWood = 30000;
/// The store's row and model
constexpr auto k_StoreRow = AbodeInfo::NorseStoragePit;
constexpr auto k_StoreMesh = MeshId::BuildingNorseStoragePit;
/// The store's special points: its wood piles' are the first five, its food pile's the sixth, and a desire flag flies at
/// the point twelve past its place
constexpr size_t k_FoodPilePoint = 5;
constexpr size_t k_FirstFlagPoint = 12;

/// The bytes of an L3D file with no sub-mesh and no skin, holding only the special points given (the extra metrics
/// block: its size, the count, where the metrics start, then each metric, an unturned matrix and the point), laid out
/// as test_land_script_mesh_commands.cpp's meshes
std::vector<uint8_t> MeshOfPoints(const std::vector<glm::vec3>& points)
{
	constexpr uint32_t k_None = std::numeric_limits<uint32_t>::max();
	using Metric = std::array<float, 12>;
	const auto count = static_cast<uint32_t>(points.size());
	const auto metricsAt = static_cast<uint32_t>(sizeof(l3d::L3DHeader) + (3 * sizeof(uint32_t)));
	const std::array<uint32_t, 3> block {static_cast<uint32_t>((3 * sizeof(uint32_t)) + (count * sizeof(Metric))), count,
	                                     metricsAt};
	std::vector<uint8_t> file(metricsAt + (count * sizeof(Metric)));
	l3d::L3DHeader header {};
	std::memcpy(header.magic.data(), "L3D0", header.magic.size());
	header.flags = l3d::L3DMeshFlags::ContainsExtraMetrics;
	header.size = static_cast<uint32_t>(file.size());
	header.submeshOffsetsOffset = k_None;
	header.skinOffsetsOffset = k_None;
	header.extraDataOffset = k_None;
	header.footprintDataOffset = static_cast<uint32_t>(sizeof(l3d::L3DHeader));
	std::memcpy(file.data(), &header, sizeof(header));
	std::memcpy(file.data() + sizeof(header), block.data(), sizeof(block));
	for (size_t i = 0; i < points.size(); ++i)
	{
		const auto& point = points[i];
		const Metric metric {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, point.x, point.y, point.z};
		std::memcpy(file.data() + metricsAt + (i * sizeof(Metric)), metric.data(), sizeof(Metric));
	}
	return file;
}

/// The town desires the game has, which also note, each time a store's flags go to it, what the store holds then and
/// which store its town works from
class NotingTownDesires final: public ecs::systems::TownDesireSystemInterface
{
public:
	struct Noted
	{
		uint32_t food;
		uint32_t wood;
		entt::entity townsStore;
	};
	std::vector<Noted> noted;

	uint32_t OfferVillager(entt::entity town, bool child, float trigger,
	                       const std::function<uint32_t(TownDesireInfo)>& satisfy) override
	{
		return _desires.OfferVillager(town, child, trigger, satisfy);
	}
	[[nodiscard]] float GetDesire(entt::entity town, TownDesireInfo desire) const override
	{
		return _desires.GetDesire(town, desire);
	}
	[[nodiscard]] float GetRawDesire(entt::entity town, TownDesireInfo desire) const override
	{
		return _desires.GetRawDesire(town, desire);
	}
	[[nodiscard]] TownDesireInfo GetMostWanted(entt::entity town) const override { return _desires.GetMostWanted(town); }
	void SetBoost(entt::entity town, TownDesireInfo desire, float boost, bool resort) override
	{
		_desires.SetBoost(town, desire, boost, resort);
	}
	void UpdateFlags() override { _desires.UpdateFlags(); }
	void OnStoragePitMade(entt::entity town, entt::entity pit) override
	{
		noted.push_back({.food = ecs::StoragePitStore::GetResource(pit, ResourceType::Food),
		                 .wood = ecs::StoragePitStore::GetResource(pit, ResourceType::Wood),
		                 .townsStore = Locator::entitiesRegistry::value().Get<const Town>(town).storagePit});
		_desires.OnStoragePitMade(town, pit);
	}
	[[nodiscard]] bool DrawFlag(entt::entity flag, float seconds, const glm::vec3& eye) override
	{
		return _desires.DrawFlag(flag, seconds, eye);
	}
	void ProcessFlags(entt::entity town) override { _desires.ProcessFlags(town); }
	[[nodiscard]] std::optional<FlagToolTip> GetFlagToolTip(entt::entity flag) const override
	{
		return _desires.GetFlagToolTip(flag);
	}

private:
	ecs::systems::TownDesireSystem _desires;
};

class TownWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		// the town desires, which take a town's flags to its store as the store is made
		Locator::townDesireSystem::emplace<ecs::systems::TownDesireSystem>();
		if (GameData())
		{
			LoadMesh(Info().abode.at(static_cast<size_t>(k_StoreRow)).meshId, glm::vec3(0.0f), glm::vec3(0.0f));
			return;
		}
		FillSyntheticRows();
		// on synthetic data the store's model is only its points: the wood piles' five in a row along one side, the food
		// pile's on the other, and the six flags' in a row behind it
		std::vector<glm::vec3> points(k_FirstFlagPoint + 6, glm::vec3(0.0f));
		for (size_t i = 0; i < 5; ++i)
		{
			points.at(i) = {6.0f, 0.0f, (static_cast<float>(i) * 3.0f) - 6.0f};
		}
		points.at(k_FoodPilePoint) = {-6.0f, 0.0f, 1.5f};
		for (size_t i = 0; i < 6; ++i)
		{
			points.at(k_FirstFlagPoint + i) = {(static_cast<float>(i) * 2.0f) - 5.0f, 4.0f, 10.0f};
		}
		Locator::resources::value().GetMeshes().Load(k_StoreMesh, resources::L3DLoader::FromBufferTag {},
		                                             std::string("test store"), MeshOfPoints(points));
	}

	void TearDown() override
	{
		Script::SetLastCreated(entt::null);
		WorldHarness::TearDown();
	}

	/// The rows the scene reads, as info.dat has them: the Norse storage pit, its piles and the desires that fly a flag
	void FillSyntheticRows() const
	{
		auto& info = Info();
		auto& store = info.abode.at(static_cast<size_t>(k_StoreRow));
		store.abodeType = AbodeType::StoragePit;
		store.abodeNumber = AbodeNumber::StoragePit;
		store.tribeType = Tribe::NORSE;
		store.debugString.fill('\0');
		std::ranges::copy(std::string_view("ABODE_STORAGE_PIT"), store.debugString.begin());
		store.meshId = k_StoreMesh;
		store.potForResourceFood = PotInfo::StoragePitFoodPile;
		store.potForResourceWood = PotInfo::WoodPile_1;
		// the food pile and the last wood pile lead to no other pile, so they have no cap; the other wood piles hold
		// 5000 each and lead to the next
		auto& food = info.pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile));
		food.potType = PotType::PileFood;
		food.resourceType = ResourceType::Food;
		food.maxAmountInPot = 15000;
		food.nextPotForResource = PotInfo::_COUNT;
		for (int32_t i = 0; i < 5; ++i)
		{
			const auto type = static_cast<PotInfo>(static_cast<int32_t>(PotInfo::WoodPile_1) + i);
			auto& wood = info.pot.at(static_cast<size_t>(type));
			wood.potType = PotType::PileWood;
			wood.resourceType = ResourceType::Wood;
			wood.maxAmountInPot = 5000;
			wood.nextPotForResource = i < 4 ? static_cast<PotInfo>(static_cast<int32_t>(type) + 1) : PotInfo::_COUNT;
		}
		// seven desires fly a flag: food, wood, protection, mercy, abodes, civic building and children, at the places
		// 0, 1, 4, 5, 2, 3 and 5
		for (auto& desire : info.townDesire)
		{
			desire.worshipSiteSlot = -1;
		}
		constexpr std::array<std::pair<TownDesireInfo, int>, 7> k_Flags {{
		    {TownDesireInfo::ForFood, 0},
		    {TownDesireInfo::ForWood, 1},
		    {TownDesireInfo::ForProtection, 4},
		    {TownDesireInfo::ForMercy, 5},
		    {TownDesireInfo::ForAbodes, 2},
		    {TownDesireInfo::ForCivicBuilding, 3},
		    {TownDesireInfo::ForChildren, 5},
		}};
		for (const auto& [desire, place] : k_Flags)
		{
			info.townDesire.at(static_cast<size_t>(desire)).worshipSiteSlot = place;
		}
	}

	/// Land 1's first town and its store, as its land script's lines make them (or at another angle and size, in
	/// thousandths); the store, the last created object
	[[nodiscard]] static entt::entity MakeTownAndStore(int32_t angle = k_StoreAngle, int32_t size = k_StoreSize)
	{
		FeatureScriptCommands::CreateTown(k_Town, glm::vec3(k_TownAt.x, 0.0f, k_TownAt.y), "PLAYER_ONE", 7, "NORSE");
		FeatureScriptCommands::CreateAbode(k_Town, glm::vec3(k_StoreAt.x, 0.0f, k_StoreAt.y), "NORSE_ABODE_STORAGE_PIT", angle,
		                                   size, k_StoreFood, k_StoreWood);
		return Script::LastCreated();
	}

	[[nodiscard]] static entt::entity ScriptTown() { return ecs::town_queries::FindTownWithID(static_cast<uint32_t>(k_Town)); }

	[[nodiscard]] static uint32_t AmountOf(entt::entity pile) { return Registry().Get<const Pot>(pile).amount; }

	[[nodiscard]] static glm::vec3 PlaceOf(entt::entity thing) { return Registry().Get<const Transform>(thing).position; }

	/// Where the original puts a thing at the store's special point: the point of the store's model through the store's
	/// matrix, or the store's own place when its model has no such point
	[[nodiscard]] static glm::vec3 PointOnStore(entt::entity store, size_t point)
	{
		const auto& transform = Registry().Get<const Transform>(store);
		const auto mesh = Locator::resources::value().GetMeshes().Handle(Registry().Get<const Mesh>(store).id);
		const auto& metrics = mesh->GetExtraMetrics();
		if (point >= metrics.size())
		{
			return transform.position;
		}
		return transform.position + (transform.rotation * (glm::vec3(metrics.at(point)[3]) * transform.scale));
	}

	/// The original keeps a place in its map coordinates, so two places are the same to it within one of their units
	/// in x and in z
	static void ExpectSamePlace(const glm::vec3& at, const glm::vec3& expected, const std::string& what)
	{
		EXPECT_NEAR(at.x, expected.x, map_coords::k_MetresPerFixed) << what;
		EXPECT_NEAR(at.z, expected.z, map_coords::k_MetresPerFixed) << what;
	}

private:
	test::RestoreService<Locator::townDesireSystem> _restoreDesires;
};
} // namespace

// The script's food goes into the store's food pile, which has no cap, and then its wood fills the wood piles in turn,
// the first four up to their 5000 and the last, which has no cap, taking the rest; the store's totals are its piles', as
// the original (docs/wiki/world/objects.md#store-storagepit, docs/wiki/world/objects.md#store-details)
TEST_P(TownWorld, ALandScriptsStoreTakesItsFoodIntoItsFoodPileAndItsWoodIntoItsWoodPilesInTurn)
{
	const auto store = MakeTownAndStore();
	ASSERT_TRUE(store != entt::null);
	ASSERT_TRUE(Registry().AllOf<StoragePit>(store));
	const auto& pit = Registry().Get<const StoragePit>(store);
	EXPECT_EQ(AmountOf(pit.foodPile), 20000u);
	constexpr std::array<uint32_t, 5> k_Wood {5000, 5000, 5000, 5000, 10000};
	for (size_t i = 0; i < k_Wood.size(); ++i)
	{
		EXPECT_EQ(AmountOf(pit.woodPiles.at(i)), k_Wood.at(i)) << "wood pile " << i + 1;
	}
	const auto& abode = Registry().Get<const Abode>(store);
	EXPECT_EQ(abode.foodAmount, 20000u);
	EXPECT_EQ(abode.woodAmount, 30000u);
}

// Over any of the land script's store's piles the hand shows the store's whole total, not what that pile holds: its
// food over the food pile and its wood over each wood pile, as the original (docs/wiki/interface/tooltips.md)
TEST_P(TownWorld, OverEachOfALandScriptsStoresPilesTheHandShowsTheStoresWholeTotal)
{
	const auto store = MakeTownAndStore();
	ASSERT_TRUE(store != entt::null);
	const auto& pit = Registry().Get<const StoragePit>(store);
	namespace tips = ecs::systems::hand_tooltips;
	const auto food = tips::PileToolTip(true, ecs::object_resources::GetResource(pit.foodPile, ResourceType::Food));
	ASSERT_EQ(food.numbers.count, 1u);
	EXPECT_EQ(food.numbers.values[0], 20000.0);
	for (size_t i = 0; i < pit.woodPiles.size(); ++i)
	{
		const auto wood = tips::PileToolTip(false, ecs::object_resources::GetResource(pit.woodPiles.at(i), ResourceType::Wood));
		ASSERT_EQ(wood.numbers.count, 1u);
		EXPECT_EQ(wood.numbers.values[0], 30000.0) << "wood pile " << i + 1;
	}
}

// The land script's store is its town's working store at once, and the food going into it, where there was none,
// pulses the town, as the original (docs/wiki/world/objects.md#store-storagepit,
// docs/wiki/world/objects.md#store-details)
TEST_P(TownWorld, ALandScriptsStoreIsItsTownsAtOnceAndItsFirstFoodPulsesTheTown)
{
	const auto store = MakeTownAndStore();
	ASSERT_TRUE(store != entt::null);
	const auto town = ScriptTown();
	ASSERT_TRUE(town != entt::null);
	const auto& data = Registry().Get<const Town>(town);
	EXPECT_EQ(data.storagePit, store);
	EXPECT_EQ(data.buildPulse, 1u);
	EXPECT_EQ(data.buildPulsePrevious, 0u);
}

// The land script's food and wood go into the store only once it is made: its town's flags go to it while it is still
// empty and before it is its town's store, and it takes them and becomes its town's store afterwards, as the original
// (docs/wiki/world/objects.md#store-details)
TEST_P(TownWorld, ALandScriptsStoreTakesItsFoodAndWoodOnlyOnceItIsMade)
{
	auto& desires = static_cast<NotingTownDesires&>(Locator::townDesireSystem::emplace<NotingTownDesires>());
	const auto store = MakeTownAndStore();
	ASSERT_TRUE(store != entt::null);
	ASSERT_EQ(desires.noted.size(), 1u);
	EXPECT_EQ(desires.noted.front().food, 0u);
	EXPECT_EQ(desires.noted.front().wood, 0u);
	EXPECT_TRUE(desires.noted.front().townsStore == entt::null);
	EXPECT_EQ(ecs::StoragePitStore::GetResource(store, ResourceType::Food), static_cast<uint32_t>(k_StoreFood));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(store, ResourceType::Wood), static_cast<uint32_t>(k_StoreWood));
	const auto town = ScriptTown();
	ASSERT_TRUE(town != entt::null);
	EXPECT_EQ(Registry().Get<const Town>(town).storagePit, store);
}

// The store makes its food pile first and then its five wood piles in turn, so the food pile takes the creation index
// right after the store's and wood pile k the k-th after that, as the original (docs/wiki/world/objects.md#store-details)
TEST_P(TownWorld, AStoreMakesItsFoodPileFirstThenItsWoodPilesInTurn)
{
	const auto store = MakeTownAndStore();
	ASSERT_TRUE(store != entt::null);
	const auto& pit = Registry().Get<const StoragePit>(store);
	const auto first = ecs::object_index::Of(store);
	ASSERT_GE(first, 0);
	EXPECT_EQ(ecs::object_index::Of(pit.foodPile), first + 1) << "food pile";
	for (size_t i = 0; i < pit.woodPiles.size(); ++i)
	{
		EXPECT_EQ(ecs::object_index::Of(pit.woodPiles.at(i)), first + 2 + static_cast<int64_t>(i)) << "wood pile " << i + 1;
	}
}

// The store's food pile stands at its model's sixth point and its wood piles at the first five, in order, each turned
// and moved with the store, as the original (docs/wiki/world/objects.md#store-details)
TEST_P(TownWorld, TheStoresPilesStandAtItsModelsPointsTurnedWithIt)
{
	const auto store = MakeTownAndStore();
	ASSERT_TRUE(store != entt::null);
	const auto& pit = Registry().Get<const StoragePit>(store);
	ExpectSamePlace(PlaceOf(pit.foodPile), PointOnStore(store, k_FoodPilePoint), "food pile");
	for (size_t i = 0; i < pit.woodPiles.size(); ++i)
	{
		ExpectSamePlace(PlaceOf(pit.woodPiles.at(i)), PointOnStore(store, i), "wood pile " + std::to_string(i + 1));
	}
}

// A store twice the usual size stands its piles twice as far out: each pile's point goes through the store's whole
// matrix, its size too, as the original (docs/wiki/world/objects.md#store-details). Unturned, the place is exact in x and
// z: the point times the size, then the store's place
TEST_P(TownWorld, AStoreTwiceTheUsualSizeStandsItsPilesTwiceAsFarOut)
{
	const auto store = MakeTownAndStore(0, 2000);
	ASSERT_TRUE(store != entt::null);
	const auto& transform = Registry().Get<const Transform>(store);
	const auto mesh = Locator::resources::value().GetMeshes().Handle(Registry().Get<const Mesh>(store).id);
	const auto& metrics = mesh->GetExtraMetrics();
	ASSERT_GT(metrics.size(), k_FoodPilePoint);
	const auto& pit = Registry().Get<const StoragePit>(store);
	const auto expectAt = [&](entt::entity pile, size_t point, const std::string& what) {
		const glm::vec3 metric(metrics.at(point)[3]);
		const auto at = PlaceOf(pile);
		EXPECT_EQ(at.x, (metric.x * 2.0f) + transform.position.x) << what;
		EXPECT_EQ(at.z, (metric.z * 2.0f) + transform.position.z) << what;
	};
	expectAt(pit.foodPile, k_FoodPilePoint, "food pile");
	for (size_t i = 0; i < pit.woodPiles.size(); ++i)
	{
		expectAt(pit.woodPiles.at(i), i, "wood pile " + std::to_string(i + 1));
	}
}

// Turned, a store twice the usual size stands each pile at its point through the store's turn and its size, as the
// original (docs/wiki/world/objects.md#store-details)
TEST_P(TownWorld, ATurnedStoreTwiceTheUsualSizeStandsItsPilesAtItsScaledPoints)
{
	const auto store = MakeTownAndStore(k_StoreAngle, 2000);
	ASSERT_TRUE(store != entt::null);
	ASSERT_EQ(Registry().Get<const Transform>(store).scale.x, 2.0f);
	const auto& pit = Registry().Get<const StoragePit>(store);
	ExpectSamePlace(PlaceOf(pit.foodPile), PointOnStore(store, k_FoodPilePoint), "food pile");
	for (size_t i = 0; i < pit.woodPiles.size(); ++i)
	{
		ExpectSamePlace(PlaceOf(pit.woodPiles.at(i)), PointOnStore(store, i), "wood pile " + std::to_string(i + 1));
	}
}

// As the store is made, each of its town's desire flags goes to the store's point twelve past the flag's place (the
// store's own place where its model has no such point), and nothing moves it from there as the turns go by; on the
// game's data the town has seven flags, as the original (docs/wiki/beings/towns.md#the-desire-flags)
TEST_P(TownWorld, TheTownsFlagsGoToTheStoresPointsPastTheFirstTwelveAsItIsMadeAndStayThere)
{
	const auto store = MakeTownAndStore();
	ASSERT_TRUE(store != entt::null);
	const auto town = ScriptTown();
	ASSERT_TRUE(town != entt::null);
	const auto flags = Registry().Get<const TownDesireFlags>(town).flags;
	std::vector<std::pair<entt::entity, glm::vec3>> placed;
	for (size_t i = 0; i < flags.size(); ++i)
	{
		const int place = Info().townDesire.at(i).worshipSiteSlot;
		if (place < 0)
		{
			EXPECT_TRUE(flags.at(i) == entt::null) << "desire " << i;
			continue;
		}
		ASSERT_TRUE(flags.at(i) != entt::null) << "desire " << i;
		const auto at = PlaceOf(flags.at(i));
		ExpectSamePlace(at, PointOnStore(store, k_FirstFlagPoint + static_cast<size_t>(place)), "desire " + std::to_string(i));
		placed.emplace_back(flags.at(i), at);
	}
	if (GameData())
	{
		EXPECT_EQ(placed.size(), 7u);
	}
	for (int turn = 0; turn < 100; ++turn)
	{
		Turn();
		for (const auto& [flag, at] : placed)
		{
			ASSERT_TRUE(Registry().Valid(flag));
			EXPECT_EQ(PlaceOf(flag).x, at.x) << "turn " << turn;
			EXPECT_EQ(PlaceOf(flag).z, at.z) << "turn " << turn;
		}
	}
}

INSTANTIATE_TEST_SUITE_P(Data, TownWorld, ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
