/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The wood miracle, pinned to the original's rules (docs/wiki/magic/food-wood-water-heal.md#food-and-wood): where a log
// becomes wood and how much, how the pour lets go of the hand, and where a landing log goes. The numbers are the
// original's: the WOOD row of info.dat (500 wood for the first log, then 20, 3 prayer a unit), SF_Wood's raise
// (ClampHand 1, TotalTime 4 s, HeightToRaise 10, AngleToRaise 1.07257) and the put-down's rules (nine cells, the first
// taker that qualifies, a pile capped at its most only when it has a next pot, a pot reaching twice its radius).

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <memory>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AxisAlignedBoundingBox.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/MeshBoxProviderInterface.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Chants.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "Magic/HandMotion.h"
#include "Particles/SpellLink.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::magic;
using openblack::ecs::components::Spell;
using openblack::ecs::components::SpellCreator;
using EventType = openblack::particles::SpellEventInfo::Type;

namespace
{
/// SF_Wood's raise of the hand: the hand is held where the pour began, rises and tips over 4 s, and starts over
constexpr PourSettings k_WoodPour {
    .totalTime = 4.0f, .heightToRaise = 10.0f, .angleToRaise = 1.07257f, .loops = true, .clampHand = true};
/// A game turn
constexpr float k_TurnSeconds = 0.1f;

/// The wood miracle over the tests' fake world: a registry, tables zeroed but for the WOOD row, the animals' shared
/// state the spell classes register with, and the inert miracles' service, whose world is a FakeMagicWorld, each put
/// back as it was when the test ends
class WoodMiracleSpell: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		// the spell classes register on their first use, and the flock classes register the flock animals' dying
		Locator::animalSystem::emplace<ecs::systems::AnimalSystem>();
		_magic = &static_cast<test::InertMagicSystem&>(Locator::magicSystem::emplace<test::InertMagicSystem>());
		auto info = std::make_unique<InfoConstants>();
		// the WOOD row: 500 wood for the first log, 20 for each later one, 3 prayer a unit
		auto& wood = info->magicWood.at(0);
		wood.resourceType = ResourceType::Wood;
		wood.resourceAmountFirstEvent = 500;
		wood.resourceAmountPerEvent = 20;
		wood.costPerUnit = 3;
		_info = info.get();
		Locator::infoConstants::reset(info.release());
	}

	[[nodiscard]] test::FakeMagicWorld& World() const { return _magic->FakeWorld(); }
	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] GMagicEffectInfo& Effect(MagicType type) const { return _info->magicEffect.at(static_cast<size_t>(type)); }

	/// A wood miracle the first player cast at (100, 0, 100), with the prayer power in its horn
	[[nodiscard]] static entt::entity MakeWood(float prayerPower)
	{
		const auto spell = Registry().Create();
		auto& component = Registry().Assign<Spell>(spell);
		component.magicType = MagicType::Wood;
		component.spellClass = ClassOf(MagicType::Wood);
		component.creator = {.kind = SpellCreator::Kind::Player, .player = PlayerNames::PLAYER_ONE, .entity = entt::null};
		component.player = PlayerNames::PLAYER_ONE;
		component.hasPlayer = true;
		chants::SetChants(component, prayerPower);
		component.position = glm::vec3(100.0f, 0.0f, 100.0f);
		component.castPos = component.position;
		return spell;
	}

	/// A log of the miracle lands at a point
	static void Land(entt::entity spell, glm::vec3 point)
	{
		const particles::SpellEventInfo event {.type = EventType::Landed,
		                                       .position = point,
		                                       .velocity = glm::vec3(0.0f, -1.0f, 0.0f),
		                                       .strength = 1.0f,
		                                       .checkShields = false,
		                                       .target = entt::null};
		OpsOf(SpellClass::Resource).spellEvent(spell, event);
	}

	[[nodiscard]] static float Chants(entt::entity spell) { return Registry().Get<const Spell>(spell).chants; }

private:
	// put back in reverse order: the tables, the miracles' service with its stores, the registry, then the animals'
	// shared state
	test::RestoreService<Locator::animalSystem> _restoreAnimals;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::magicSystem> _restoreMagic;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	InfoConstants* _info {nullptr};
	test::InertMagicSystem* _magic {nullptr};
};

/// Every mesh is a box 2 m tall standing on its origin, as wide as asked (2 m across unless told)
class WoodMeshBoxes final: public ecs::systems::MeshBoxProviderInterface
{
public:
	explicit WoodMeshBoxes(float halfWidth = 1.0f)
	    : _halfWidth(halfWidth)
	{
	}
	[[nodiscard]] std::optional<AxisAlignedBoundingBox> MeshBox(entt::id_type /*meshId*/) const override
	{
		return AxisAlignedBoundingBox {.minima = glm::vec3(-_halfWidth, 0.0f, -_halfWidth),
		                               .maxima = glm::vec3(_halfWidth, 2.0f, _halfWidth)};
	}

private:
	float _halfWidth;
};

/// Wood put down on the land among wood piles of the map cells: a registry, the wood piles' rows of the pots' table,
/// the meshes' boxes and the map cells, each put back as it was when the test ends. There is no land, so what no pile
/// takes makes no new pile (the sea)
class WoodMiracleDrop: public ::testing::Test
{
protected:
	static constexpr uint16_t k_Most = 1000;
	/// No next pot: what such a pile takes is not capped at its most
	static constexpr auto k_NoNextPot = static_cast<PotInfo>(19);

	void SetUp() override
	{
		Locator::meshBoxProvider::emplace<WoodMeshBoxes>();
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		auto info = std::make_unique<InfoConstants>();
		// pots and piles are things of type 21, filed at the tail of their cell's fixed list
		const auto woodPile = [&info](PotInfo type, PotInfo next) {
			auto& row = info->pot.at(static_cast<size_t>(type));
			row.type = ObjectType::Pot;
			row.potType = PotType::PileWood;
			row.resourceType = ResourceType::Wood;
			row.maxAmountInPot = k_Most;
			row.nextPotForResource = next;
		};
		// the miracle's own pile: no next pot (19, as info.dat has it), not capped
		woodPile(PotInfo::MagicWood, k_NoNextPot);
		// a pile with a next pot: capped at its most
		woodPile(PotInfo::WoodPile_1, PotInfo::WoodPile_2);
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
	}

	/// A wood pile of a type and amount at a point, at scale 1, sized and filed in its map cell as a new pile is
	static entt::entity MakePile(PotInfo type, uint16_t amount, glm::vec3 at)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto pile = registry.Create();
		registry.Assign<ecs::components::Transform>(pile, at, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<ecs::components::Pot>(pile, amount, k_Most, type);
		registry.Assign<ecs::components::Mesh>(pile, entt::id_type {1}, int8_t {0}, int8_t {1});
		ecs::archetypes::PotArchetype::SetSize(pile, false);
		ecs::map_cells::InsertMapObject(pile);
		return pile;
	}

	/// Wood put down at a point by no interface; what the piles there took
	static uint32_t Drop(glm::vec3 at, uint32_t amount)
	{
		return ecs::pot_resource::AddResourceToPos(at, {}, ResourceType::Wood, amount, false, false);
	}

	[[nodiscard]] static uint16_t AmountOf(entt::entity pile)
	{
		return Locator::entitiesRegistry::value().Get<const ecs::components::Pot>(pile).amount;
	}

private:
	// the registry goes first, then the map cells, while the tables and the boxes are still there
	test::RestoreService<Locator::meshBoxProvider> _restoreBoxes;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};
} // namespace

// ---- Casting and paying ----

TEST_F(WoodMiracleSpell, ALogIsPaidForWhereverItLandsButIsWoodOnlyOnDryLand)
{
	World().wetBelowX = 50.0f;
	const auto spell = MakeWood(9000.0f);
	// over water: the first log's 500 wood are paid for (1500) and lost
	Land(spell, {10.0f, 0.0f, 100.0f});
	EXPECT_TRUE(World().resources.empty());
	EXPECT_FLOAT_EQ(Chants(spell), 9000.0f - 1500.0f);
	// on dry land it is wood: the next log's 20 (60), where it landed, with no sparkles
	Land(spell, {100.0f, 0.0f, 100.0f});
	ASSERT_EQ(World().resources.size(), 1u);
	EXPECT_EQ(World().resources[0].type, ResourceType::Wood);
	EXPECT_EQ(World().resources[0].amount, 20u);
	EXPECT_EQ(World().resources[0].point, glm::vec3(100.0f, 0.0f, 100.0f));
	EXPECT_FALSE(World().resources[0].sparkles);
	EXPECT_FLOAT_EQ(Chants(spell), 9000.0f - 1500.0f - 60.0f);
	// back over the water: paid for and lost again
	Land(spell, {49.0f, 0.0f, 100.0f});
	EXPECT_EQ(World().resources.size(), 1u);
	EXPECT_FLOAT_EQ(Chants(spell), 9000.0f - 1500.0f - 2.0f * 60.0f);
}

TEST_F(WoodMiracleSpell, TheWoodALogBringsIsItsAmountTimesTheTribalPower)
{
	// the caster's first tribe has a tribal power of 1.25, and the WOOD miracle uses that tribe's
	Effect(MagicType::Wood).useTribalPowerMultiplier.at(0) = 1;
	auto& power = players::MagicOf(PlayerNames::PLAYER_ONE).tribalPower.at(0);
	const float before = power;
	power = 1.25f;
	const auto spell = MakeWood(9000.0f);
	Land(spell, {100.0f, 0.0f, 100.0f});
	Land(spell, {101.0f, 0.0f, 100.0f});
	power = before;
	ASSERT_EQ(World().resources.size(), 2u);
	// the first log's 500 x 1.25
	EXPECT_EQ(World().resources[0].amount, 625u);
	// a later log's 20 x 1.25
	EXPECT_EQ(World().resources[1].amount, 25u);
}

// ---- The hand while pouring ----

TEST(WoodMiracle, StoppingEasesTheHandBackToRestOverOneGameTurn)
{
	PourState pour;
	StartPour(pour, k_WoodPour, glm::vec3(0.0f));
	for (int turn = 0; turn < 15; ++turn)
	{
		StepPour(pour, k_TurnSeconds);
	}
	// what the last turn drew, the start of the pose between this turn and the next
	const auto last = PourPoseAt(pour, 0.0f);
	ASSERT_GT(last.raise, 10.0f);
	ASSERT_GT(last.tilt, 1.0f);
	// the miracle ends: the hand is let go, and the pose drawn goes from the last turn's to rest by the fraction of the
	// turn drawn
	StopPour(pour);
	EXPECT_FALSE(PinnedHand(pour).has_value());
	for (const float fraction : {0.0f, 0.3f, 0.75f, 1.0f})
	{
		const auto pose = PourPoseAt(pour, fraction);
		EXPECT_FLOAT_EQ(pose.raise, last.raise + (0.0f - last.raise) * fraction);
		EXPECT_FLOAT_EQ(pose.tilt, last.tilt + (0.0f - last.tilt) * fraction);
	}
	// from the next game turn the hand is at rest, whatever the fraction
	StepPour(pour, k_TurnSeconds);
	for (const float fraction : {0.0f, 0.5f, 1.0f})
	{
		EXPECT_EQ(PourPoseAt(pour, fraction).raise, 0.0f);
		EXPECT_EQ(PourPoseAt(pour, fraction).tilt, 0.0f);
	}
	EXPECT_FALSE(pour.active);
}

// ---- Landing and piles ----

TEST_F(WoodMiracleDrop, ALogLooksForATakerInItsOwnCellThenTheEightAroundItAndNoFurther)
{
	// piles that reach 60 m: 30 m half across at scale 1 (a wood pile's radius is its mesh's, a pot reaching twice it)
	Locator::meshBoxProvider::emplace<WoodMeshBoxes>(30.0f);
	const glm::vec3 log(15.0f, 0.0f, 15.0f); // in the 10 m cell (1, 1)
	// two cells to the east and two to the north: within reach, but not among the nine cells looked in
	const auto twoCellsEast = MakePile(PotInfo::MagicWood, 500, {35.0f, 0.0f, 15.0f});
	const auto twoCellsNorth = MakePile(PotInfo::MagicWood, 500, {15.0f, 0.0f, 35.0f});
	EXPECT_EQ(Drop(log, 20), 0u);
	EXPECT_EQ(AmountOf(twoCellsEast), 500u);
	EXPECT_EQ(AmountOf(twoCellsNorth), 500u);
	// a corner cell is one of them
	const auto corner = MakePile(PotInfo::MagicWood, 500, {5.0f, 0.0f, 25.0f});
	EXPECT_EQ(Drop(log, 20), 20u);
	EXPECT_EQ(AmountOf(corner), 520u);
	// the log's own cell is looked in first, whichever pile was made first
	const auto ownCell = MakePile(PotInfo::MagicWood, 500, {18.0f, 0.0f, 12.0f});
	EXPECT_EQ(Drop(log, 20), 20u);
	EXPECT_EQ(AmountOf(ownCell), 520u);
	EXPECT_EQ(AmountOf(corner), 520u);
	EXPECT_EQ(AmountOf(twoCellsEast), 500u);
	EXPECT_EQ(AmountOf(twoCellsNorth), 500u);
}

TEST_F(WoodMiracleDrop, AWoodPileTakesWoodWithinTwiceItsRadius)
{
	// 1 m half across at scale 1: a radius of 1 m, whatever the pile holds
	const glm::vec3 at(55.0f, 0.0f, 55.0f);
	const auto pile = MakePile(PotInfo::MagicWood, 500, at);
	EXPECT_EQ(Drop(at + glm::vec3(1.9f, 0.0f, 0.0f), 20), 20u);
	EXPECT_EQ(AmountOf(pile), 520u);
	// beyond twice its radius it does not
	EXPECT_EQ(Drop(at + glm::vec3(0.0f, 0.0f, 2.1f), 20), 0u);
	EXPECT_EQ(AmountOf(pile), 520u);
}

TEST_F(WoodMiracleDrop, WoodPilesInReachTakeItInTurnAndOnlyAMagicPileTakesMoreThanItsMost)
{
	// two piles where the log lands, the one with a next pot made first: it is offered the wood first and takes what it
	// can, up to its most; the rest goes on to the magic pile
	const glm::vec3 at(55.0f, 0.0f, 55.0f);
	const auto capped = MakePile(PotInfo::WoodPile_1, 990, at);
	const auto magicPile = MakePile(PotInfo::MagicWood, 500, at);
	EXPECT_EQ(Drop(at, 500), 500u);
	EXPECT_EQ(AmountOf(capped), 1000u);
	EXPECT_EQ(AmountOf(magicPile), 990u);
	// full, it takes nothing more; the magic pile takes it all, past its most
	EXPECT_EQ(Drop(at, 500), 500u);
	EXPECT_EQ(AmountOf(capped), 1000u);
	EXPECT_EQ(AmountOf(magicPile), 1490u);
	// a full capped pile alone takes nothing, and with no land under it nothing more is made
	const glm::vec3 there(155.0f, 0.0f, 155.0f);
	const auto full = MakePile(PotInfo::WoodPile_1, 1000, there);
	EXPECT_EQ(Drop(there, 20), 0u);
	EXPECT_EQ(AmountOf(full), 1000u);
	// the first that qualifies, not the nearest: a pile made first a metre away takes a log before one under it
	const glm::vec3 third(255.0f, 0.0f, 255.0f);
	const auto first = MakePile(PotInfo::MagicWood, 500, third + glm::vec3(1.0f, 0.0f, 0.0f));
	const auto nearest = MakePile(PotInfo::MagicWood, 500, third);
	EXPECT_EQ(Drop(third, 20), 20u);
	EXPECT_EQ(AmountOf(first), 520u);
	EXPECT_EQ(AmountOf(nearest), 500u);
}
