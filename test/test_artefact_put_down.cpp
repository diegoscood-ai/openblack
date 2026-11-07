/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What makes a thing at the end of its physics an artefact (docs/wiki/beings/towns.md#artefacts): a rock put down
// gently from a hand by a player, of a kind whose artefact multiplier is above 0 and that no script holds, becomes an
// artefact of the town of the nearest building within 50 m. A synthetic town and house in the map cells, no meshes; the
// artefacts' service is a fake that records what it is given.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AxisAlignedBoundingBox.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/MapFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace map_cells = openblack::ecs::map_cells;
namespace object_index = openblack::ecs::object_index;
namespace object_physics = openblack::ecs::object_physics;
using openblack::ecs::PhysicsEntry;

namespace
{
/// Every rock's artefact multiplier in the object tables
constexpr float k_RockArtefactMultiplier = 0.0002f;
/// The fences', the dead trees' and most toys' multiplier, the smallest above 0 in the tables
constexpr float k_SmallestArtefactMultiplier = 0.0001f;
constexpr uint32_t k_TownId = 1;
const glm::vec3 k_HouseAt {1000.0f, 0.0f, 1000.0f};

/// Any mesh is a box 4 across and 1 high
std::optional<AxisAlignedBoundingBox> FourAcross(entt::id_type /*meshId*/)
{
	return AxisAlignedBoundingBox {{-2.0f, 0.0f, -2.0f}, {2.0f, 1.0f, 2.0f}};
}

/// The artefacts' service, recording the things it is given to join a town or a worship site
class RecordedArtefacts final: public ecs::systems::ArtefactSystemInterface
{
public:
	struct Joined
	{
		entt::entity object;
		entt::entity place;
		PlayerNames player;
		bool site;
	};

	void AddToTown(entt::entity object, entt::entity town, PlayerNames player) override
	{
		joined.push_back({object, town, player, false});
	}
	void AddToSite(entt::entity object, entt::entity site, PlayerNames player) override
	{
		joined.push_back({object, site, player, true});
	}
	void Taken(entt::entity /*object*/, PlayerNames /*player*/) override {}
	void ProcessTown(entt::entity /*town*/) override {}
	bool OfferRelaxation(entt::entity /*town*/, entt::entity /*villager*/) override { return false; }
	void PowerUpSite(entt::entity /*site*/, uint32_t /*dancers*/) override {}
	void SetWorth(entt::entity /*object*/, float /*worth*/) override {}
	[[nodiscard]] uint32_t CountFor(PlayerNames /*player*/) const override { return 0; }
	[[nodiscard]] float ImpressiveModifier(entt::entity /*object*/) const override { return 1.0f; }
	void ClearMap() override {}
	[[nodiscard]] uint32_t CountGone() const override { return 0; }

	std::vector<Joined> joined;
};

class ArtefactPutDown: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		test::FakeMeshBoxProvider boxes;
		boxes.meshBox = &FourAcross;
		Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>(boxes);
		auto info = std::make_unique<InfoConstants>();
		info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Rock)).artifactMultiplier = k_RockArtefactMultiplier;
		_info = info.get();
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		map_cells::Clear();
		object_index::OnLoadMap();
		_artefacts = &static_cast<RecordedArtefacts&>(Locator::artefactSystem::emplace<RecordedArtefacts>());
		auto& registry = Reg();
		_town = registry.Create();
		auto& town = registry.Assign<Town>(_town);
		town.id = k_TownId;
		town.owner = PlayerNames::PLAYER_ONE;
		registry.Context().towns[k_TownId] = _town;
		_house = MakeHouse(k_HouseAt);
		_rock = MakeRock(k_HouseAt + glm::vec3(10.0f, 0.0f, 0.0f));
	}

	void TearDown() override
	{
		map_cells::Clear();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A house of the town at the point, in the map cells
	static entt::entity MakeHouse(const glm::vec3& at)
	{
		auto& registry = Reg();
		const auto house = registry.Create();
		registry.Assign<Abode>(house, AbodeNumber::A, k_TownId, 0u, 0u);
		object_index::Assign(house);
		registry.Assign<Transform>(house, at, glm::mat3(1.0f), glm::vec3(1.0f));
		map_cells::InsertMapObject(house);
		return house;
	}

	/// A rock at the point, coming to the end of its physics
	static entt::entity MakeRock(const glm::vec3& at)
	{
		auto& registry = Reg();
		const auto rock = registry.Create();
		registry.Assign<MobileStatic>(rock, MobileStaticInfo::Rock);
		object_index::Assign(rock);
		registry.Assign<Transform>(rock, at, glm::mat3(1.0f), glm::vec3(1.0f));
		return rock;
	}

	/// The rock's body as the physics lets it go: put down gently from the hand of the player (landed), unless the
	/// flags say otherwise
	[[nodiscard]] PhysicsEntry Body(std::optional<PlayerNames> player = PlayerNames::PLAYER_ONE,
	                                uint32_t flags = PhysicsEntry::k_FromHand | PhysicsEntry::k_Landed) const
	{
		PhysicsEntry entry;
		entry.entity = _rock;
		entry.flags = flags;
		entry.throwerPlayer = player;
		return entry;
	}

	void SetRockMultiplier(float multiplier)
	{
		_info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Rock)).artifactMultiplier = multiplier;
	}

	const test::RestoreService<Locator::artefactSystem> _keepArtefacts;
	RecordedArtefacts* _artefacts {nullptr};
	/// The object tables the locator holds, for a test to change a row
	InfoConstants* _info {nullptr};
	entt::entity _town {entt::null};
	entt::entity _house {entt::null};
	entt::entity _rock {entt::null};
};
} // namespace

TEST_F(ArtefactPutDown, ARockPutDownGentlyByAPlayerBecomesAnArtefactOfTheNearestBuildingsTown)
{
	object_physics::ConsiderArtefact(Body(), _rock);
	ASSERT_EQ(_artefacts->joined.size(), 1u);
	const auto& joined = _artefacts->joined.front();
	EXPECT_EQ(joined.object, _rock);
	EXPECT_EQ(joined.place, _town);
	EXPECT_EQ(joined.player, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(joined.site);
}

TEST_F(ArtefactPutDown, OnlyAKindWhoseArtefactMultiplierIsAboveZeroAndThatNoScriptHoldsQualifies)
{
	// a kind whose table gives 0 never becomes one
	SetRockMultiplier(0.0f);
	object_physics::ConsiderArtefact(Body(), _rock);
	EXPECT_TRUE(_artefacts->joined.empty());
	// the smallest multiplier of the tables is enough
	SetRockMultiplier(k_SmallestArtefactMultiplier);
	object_physics::ConsiderArtefact(Body(), _rock);
	ASSERT_EQ(_artefacts->joined.size(), 1u);
	// nor does a rock that a script holds
	SetRockMultiplier(k_RockArtefactMultiplier);
	_artefacts->joined.clear();
	Reg().AssignState<InScript>(_rock);
	object_physics::ConsiderArtefact(Body(), _rock);
	EXPECT_TRUE(_artefacts->joined.empty());
}

TEST_F(ArtefactPutDown, OnlyALandedPutDownFromAHandByAPlayerMakesOne)
{
	// thrown: it left the hand without landing
	object_physics::ConsiderArtefact(Body(PlayerNames::PLAYER_ONE, PhysicsEntry::k_FromHand), _rock);
	// landed, but not from a hand
	object_physics::ConsiderArtefact(Body(PlayerNames::PLAYER_ONE, PhysicsEntry::k_Landed), _rock);
	// put down by no player
	object_physics::ConsiderArtefact(Body(std::nullopt), _rock);
	EXPECT_TRUE(_artefacts->joined.empty());
}

TEST_F(ArtefactPutDown, ABuildingFiftyMetresAwayOrMoreIsOutOfReach)
{
	auto& transform = Reg().Get<Transform>(_rock);
	transform.position = k_HouseAt + glm::vec3(50.0f, 0.0f, 0.0f);
	object_physics::ConsiderArtefact(Body(), _rock);
	EXPECT_TRUE(_artefacts->joined.empty());
	transform.position = k_HouseAt + glm::vec3(49.0f, 0.0f, 0.0f);
	object_physics::ConsiderArtefact(Body(), _rock);
	ASSERT_EQ(_artefacts->joined.size(), 1u);
	EXPECT_EQ(_artefacts->joined.front().place, _town);
}
