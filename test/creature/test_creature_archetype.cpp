/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature archetype: what a new creature is made of, its start from its species' row, and its turn-start
// positions when it is put somewhere outside its own turn, and its place in the map cells from the moment it is made

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/CreatureBody.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Common/EventManager.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureBodyMetrics.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/Events/TeleportEvents.h"
#include "ECS/MapCells.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/Implementations/ObjectCreationIndexSystem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/Resources.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
class CreatureArchetypeTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		Locator::objectCreationIndexSystem::emplace<systems::ObjectCreationIndexSystem>();
		Locator::mapCellsSystem::emplace<systems::MapCellsSystem>();
		Locator::resources::emplace<resources::Resources>();
		auto info = std::make_unique<InfoConstants>();
		// as in info.dat: a creature's row is of type CREATURE, which goes in the mobile list
		for (auto& row : info->creature)
		{
			row.type = ObjectType::Creature;
		}
		// the Giant Ape's row is the first; the Tiger's is its own number
		auto& ape = info->creature.at(creature::InfoRow(CreatureType::GiantApe));
		ape.startScale = 0.4f;
		ape.startFatness = 0.3f;
		ape.strength = 0.8f;
		auto& tiger = info->creature.at(creature::InfoRow(CreatureType::Tiger));
		tiger.startScale = 9.0f;
		tiger.startFatness = 0.7f;
		tiger.strength = 0.2f;
		Locator::infoConstants::reset(info.release());
		Locator::events::emplace<EventManager>();
		Locator::events::value().AddHandler<events::Teleported>(
		    [this](const events::Teleported& event) { _teleported.push_back(event.thing); });
	}

	static Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity Make(float scale = 1.0f, const CreatureArchetype::Body& body = {})
	{
		return CreatureArchetype::Create(glm::vec3(100.0f, 5.0f, 200.0f), PlayerNames::PLAYER_TWO, CreatureType::GiantApe,
		                                 entt::hashed_string("mind").value(), 0.5f, scale, body);
	}

	std::vector<entt::entity> _teleported;

private:
	// put back in reverse order: the events, the tables, the caches, the counter, the registry, then the map cells
	const test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const test::RestoreService<Locator::objectCreationIndexSystem> _restoreIndex;
	const test::RestoreService<Locator::resources> _restoreResources;
	const test::RestoreService<Locator::infoConstants> _restoreInfo;
	const test::RestoreService<Locator::events> _restoreEvents;
};
} // namespace

TEST_F(CreatureArchetypeTest, CreatePutsOnEveryCreatureComponent)
{
	const auto entity = Make();
	const auto& registry = std::as_const(Reg());
	EXPECT_TRUE(
	    (registry.AllOf<Creature, CreatureMorph, CreatureAnimation, CreatureEyes, CreatureMindState, CreatureNeeds>(entity)));
	EXPECT_TRUE(
	    (registry.AllOf<CreatureHair, CreatureTattoos, CreatureMarks, CreatureSkin, CreatureLocomotion, CreatureDrawPose>(
	        entity)));
	EXPECT_TRUE((registry.AllOf<Mesh, Transform, ObjectCreationIndex>(entity)));
	EXPECT_EQ(registry.Get<Mesh>(entity).id,
	          creature::GetIdFromType(CreatureType::GiantApe, creature::CreatureBody::Appearance::Base));
	EXPECT_GE(object_index::Of(entity), 0);
	EXPECT_FALSE(registry.Get<CreatureLocomotion>(entity).follower);
	// its body's measures, nothing without its base mesh
	ASSERT_TRUE(registry.AllOf<CreatureBodyMetrics>(entity));
	EXPECT_EQ(registry.Get<CreatureBodyMetrics>(entity).restHeight, 0.0f);
	EXPECT_EQ(registry.Get<CreatureBodyMetrics>(entity).reach, 0.0f);
}

TEST_F(CreatureArchetypeTest, TheCreatureTakesItsBodyAndSize)
{
	const auto entity = Make(1.5f, {.alignment = -0.5f, .fatness = 0.9f, .strength = 0.1f});
	const auto& creature = Reg().Get<Creature>(entity);
	EXPECT_EQ(creature.owner, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(creature.species, CreatureType::GiantApe);
	EXPECT_EQ(creature.mind, entt::hashed_string("mind").value());
	EXPECT_FALSE(creature.leashable);
	EXPECT_FLOAT_EQ(creature.alignment, -0.5f);
	EXPECT_FLOAT_EQ(creature.fatness, 0.9f);
	EXPECT_FLOAT_EQ(creature.strength, 0.1f);
	EXPECT_FLOAT_EQ(creature.size, 1.5f);
	const auto& morph = Reg().Get<CreatureMorph>(entity);
	EXPECT_FLOAT_EQ(morph.shownFatness, 0.9f);
	EXPECT_EQ(morph.revision, 0u);
	const auto expected = creature_morph::FromAttributes(-0.5f, 0.9f, 0.1f, 0.8f);
	EXPECT_FLOAT_EQ(morph.drawn.evilGood, expected.evilGood);
	EXPECT_FLOAT_EQ(morph.drawn.thinFat, expected.thinFat);
	EXPECT_FLOAT_EQ(morph.drawn.weakStrong, expected.weakStrong);
	// the size is kept within what a creature can be
	EXPECT_FLOAT_EQ(Reg().Get<Creature>(Make(0.0f)).size, creature_morph::k_MinScale);
	EXPECT_FLOAT_EQ(Reg().Get<Creature>(Make(10.0f)).size, creature_morph::k_MaxScale);
}

TEST_F(CreatureArchetypeTest, TheTransformIsTheGenericObjectOne)
{
	const auto entity = Make(2.0f);
	const auto& transform = Reg().Get<Transform>(entity);
	EXPECT_EQ(transform.position, glm::vec3(100.0f, 5.0f, 200.0f));
	EXPECT_EQ(transform.rotation, affine::AngleY(0.5f));
	// no mesh in the cache: the rest height is unknown and the body is drawn at its size
	EXPECT_EQ(transform.scale, glm::vec3(2.0f));
	EXPECT_FLOAT_EQ(CreatureArchetype::DrawnScale(CreatureType::GiantApe, 1.25f), 1.25f);
}

TEST_F(CreatureArchetypeTest, StartValuesComeFromTheSpeciesRow)
{
	EXPECT_FLOAT_EQ(CreatureArchetype::StartScale(CreatureType::GiantApe), 0.4f);
	EXPECT_FLOAT_EQ(CreatureArchetype::SpeciesStrength(CreatureType::GiantApe), 0.8f);
	const auto ape = CreatureArchetype::StartBody(CreatureType::GiantApe);
	EXPECT_FLOAT_EQ(ape.alignment, 0.0f);
	EXPECT_FLOAT_EQ(ape.fatness, 0.3f);
	EXPECT_FLOAT_EQ(ape.strength, 0.8f);
	// clamped like any size
	EXPECT_FLOAT_EQ(CreatureArchetype::StartScale(CreatureType::Tiger), creature_morph::k_MaxScale);
	EXPECT_FLOAT_EQ(CreatureArchetype::StartBody(CreatureType::Tiger).fatness, 0.7f);
}

TEST_F(CreatureArchetypeTest, StartValuesWithoutTheTables)
{
	Locator::infoConstants::reset();
	EXPECT_FLOAT_EQ(CreatureArchetype::StartScale(CreatureType::GiantApe), 0.22f);
	EXPECT_FLOAT_EQ(CreatureArchetype::SpeciesStrength(CreatureType::GiantApe), creature_morph::k_UnknownSpeciesStrength);
	const auto body = CreatureArchetype::StartBody(CreatureType::GiantApe);
	EXPECT_FLOAT_EQ(body.alignment, 0.0f);
	EXPECT_FLOAT_EQ(body.fatness, 0.5f);
	EXPECT_FLOAT_EQ(body.strength, 0.5f);
}

TEST_F(CreatureArchetypeTest, CreatePublishesTeleported)
{
	const auto entity = Make();
	ASSERT_EQ(_teleported.size(), 1u);
	EXPECT_EQ(_teleported.front(), entity);
	// and starts the turn where it stands
	const auto& locomotion = Reg().Get<CreatureLocomotion>(entity);
	EXPECT_EQ(locomotion.fromPosition, glm::vec3(100.0f, 5.0f, 200.0f));
	EXPECT_EQ(locomotion.toPosition, glm::vec3(100.0f, 5.0f, 200.0f));
}

TEST_F(CreatureArchetypeTest, SnapTurnStartPlacesACreature)
{
	const auto entity = Make();
	auto& transform = Reg().Get<Transform>(entity);
	transform.position = glm::vec3(7.0f, 8.0f, 9.0f);
	transform.rotation = affine::AngleY(1.0f);
	SnapTurnStart(Reg(), entity);
	const auto& locomotion = Reg().Get<CreatureLocomotion>(entity);
	EXPECT_EQ(locomotion.fromPosition, glm::vec3(7.0f, 8.0f, 9.0f));
	EXPECT_EQ(locomotion.toPosition, glm::vec3(7.0f, 8.0f, 9.0f));
	const auto& pose = Reg().Get<CreatureDrawPose>(entity);
	EXPECT_EQ(pose.position, glm::vec3(7.0f, 8.0f, 9.0f));
	EXPECT_EQ(pose.rotation, affine::AngleY(1.0f));
}

TEST_F(CreatureArchetypeTest, ANewCreatureIsInItsCellBeforeAnySync)
{
	// no map_cells::Sync runs in this test: the creation itself puts it at the head of its cell's mobile list
	const auto first = Make();
	EXPECT_TRUE(map_cells::IsObjectInMap(first));
	const auto cell = map_coords::CellOf(glm::vec3(100.0f, 5.0f, 200.0f));
	EXPECT_EQ(map_cells::MobileInCell(cell), (std::vector<entt::entity> {first}));
	EXPECT_EQ(map_cells::FindType(cell, ObjectType::Creature), first);
	// the next one made in the same cell goes in front of it
	const auto second = Make();
	EXPECT_EQ(map_cells::MobileInCell(cell), (std::vector<entt::entity> {second, first}));
}

TEST_F(CreatureArchetypeTest, TheTurnsMoveRefilesTheCreature)
{
	const auto creature = Make();
	const auto from = map_coords::CellOf(glm::vec3(100.0f, 5.0f, 200.0f));
	// within its cell it stays where it is in the list
	creature_pose::CommitTurnPose(creature, glm::vec3(104.0f, 5.0f, 203.0f), 0.0f);
	EXPECT_EQ(map_cells::MobileInCell(from), (std::vector<entt::entity> {creature}));
	// across a cell edge it leaves the old cell for the head of the new one, with no Sync
	const glm::vec3 to(112.0f, 5.0f, 203.0f);
	ASSERT_NE(map_coords::CellOf(to), from);
	creature_pose::CommitTurnPose(creature, to, 0.0f);
	EXPECT_TRUE(map_cells::MobileInCell(from).empty());
	EXPECT_EQ(map_cells::MobileInCell(map_coords::CellOf(to)), (std::vector<entt::entity> {creature}));
	EXPECT_EQ(Reg().Get<Transform>(creature).position, to);
}

TEST(CreatureSnapTurnStart, AVillagersSnapMakesNoCreatureStorage)
{
	// today's teleports (the animals', the villagers') must not add empty storages, which the state hash counts
	Registry registry;
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(1.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<DrawPosition>(entity);
	const auto count = [&registry]() {
		size_t storages = 0;
		registry.EachStorage([&storages](entt::id_type, const auto&) { ++storages; });
		return storages;
	};
	const auto before = count();
	SnapTurnStart(registry, entity);
	EXPECT_EQ(count(), before);
	const auto& lookup = std::as_const(registry);
	EXPECT_FALSE(lookup.AllOf<CreatureLocomotion>(entity));
	EXPECT_FALSE(lookup.AllOf<CreatureDrawPose>(entity));
	EXPECT_EQ(count(), before);
}
