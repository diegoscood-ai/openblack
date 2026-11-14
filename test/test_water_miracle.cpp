/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The water miracle, pinned to the original's rules (docs/wiki/magic/food-wood-water-heal.md#water): how the pour
// moves the caster's hand and when it stops, what a drop does to things that are not burning, that a burning field is
// neither sown nor grown, and the deed of a tree it plants (good, by the players' tree alignment change:
// docs/wiki/magic/influence-and-alignment.md). The numbers are the original's: SF_Water's raise (ClampHand 0, TotalTime
// 8 s, HeightToRaise 8, AngleToRaise 0, the key points (0, 0) (0.2, 1) (0.8, 1) (1, 0)), the drop's burn of -4000,
// which only ever cools a fire that is already there, and the tree alignment change of 0.005 in info.dat.

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <memory>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Alignment.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "ECS/Systems/Implementations/FireSystem.h"
#include "ECS/Systems/Implementations/HandGrain.h"
#include "ECS/Systems/Implementations/ReactionSystem.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Chants.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "Magic/HandMotion.h"
#include "Magic/Spells/SpellWater.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::magic;
using openblack::ecs::components::Spell;
using openblack::ecs::components::SpellCreator;

namespace
{
/// SF_Water's raise of the hand: 8 m over an 8 s loop, no tilt, and the hand is not held where the pour began
constexpr float k_WaterTotalTime = 8.0f;
constexpr float k_WaterHeightToRaise = 8.0f;
constexpr float k_WaterAngleToRaise = 0.0f;
constexpr bool k_WaterClampHand = false;
/// A game turn
constexpr float k_TurnSeconds = 0.1f;

/// The water miracle over the tests' fake world: a registry, tables zeroed, the animals' shared state the spell classes
/// register with, and the inert miracles' service, each put back as it was when the test ends
class WaterMiracleSpell: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		// the spell classes register on their first use, and the flock classes register the flock animals' dying
		Locator::animalSystem::emplace<ecs::systems::AnimalSystem>();
		Locator::magicSystem::emplace<test::InertMagicSystem>();
		Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
	}

	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }

	/// A water miracle the first player cast at (100, 0, 100), with the prayer power in its hand
	[[nodiscard]] static entt::entity MakeWater(float prayerPower)
	{
		const auto spell = Registry().Create();
		auto& component = Registry().Assign<Spell>(spell);
		component.magicType = MagicType::Water;
		component.spellClass = ClassOf(MagicType::Water);
		component.creator = {.kind = SpellCreator::Kind::Player, .player = PlayerNames::PLAYER_ONE, .entity = entt::null};
		component.player = PlayerNames::PLAYER_ONE;
		component.hasPlayer = true;
		chants::SetChants(component, prayerPower);
		component.position = glm::vec3(100.0f, 0.0f, 100.0f);
		component.castPos = component.position;
		return spell;
	}

private:
	// put back in reverse order: the tables, the miracles' service, the registry, then the animals' shared state
	test::RestoreService<Locator::animalSystem> _restoreAnimals;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::magicSystem> _restoreMagic;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
};

/// The game's fires, real, over a registry of their own, each put back as it was when the test ends
class WaterMiracleFire: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::fireSystem::emplace<ecs::systems::FireSystem>();
	}

private:
	// the registry goes first, as the game's shutdown does
	test::RestoreService<Locator::fireSystem> _restoreFires;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};

/// The forests as the water miracle asks them: the young tree planted next to a full grown one, as the test says
class WaterForests final: public ecs::systems::ForestSystemInterface
{
public:
	[[nodiscard]] uint32_t Create(uint32_t /*id*/, glm::vec3 /*centre*/) override { return 0; }
	[[nodiscard]] Forests& All() override { return _forests; }
	[[nodiscard]] TownForestLists& TownLists() override { return _lists; }
	[[nodiscard]] uint32_t LastTreeCreatedTurn() const override { return 0; }
	void SetLastTreeCreatedTurn(uint32_t /*turn*/) override {}
	[[nodiscard]] uint32_t NextId() const override { return 1; }
	void SetNextId(uint32_t /*id*/) override {}
	void NoteForestLostAMagicTree(uint32_t /*forestId*/) override {}
	[[nodiscard]] bool TakeForestLostAMagicTree(uint32_t /*forestId*/) override { return false; }
	void ClearForestsThatLostAMagicTree() override {}
	uint32_t Plant(entt::entity /*spell*/, Spell& /*miracle*/, float /*tribalPower*/) override { return 0; }
	[[nodiscard]] bool CanGrowAt(glm::vec3 /*point*/) const override { return true; }
	[[nodiscard]] bool HasTrees(entt::entity /*spell*/) const override { return false; }
	std::optional<entt::entity> AddTreeNear(entt::entity tree) override
	{
		asked.push_back(tree);
		return sapling;
	}
	void Reset() override {}

	std::optional<entt::entity> sapling;
	std::vector<entt::entity> asked;

private:
	Forests _forests;
	TownForestLists _lists;
};

/// Rain on a full grown tree of a forest: a registry, the fires, the forests, tables zeroed but for the tree's row and
/// the alignment a planted tree is worth, and the first player's alignment, each put back as it was when the test ends
class WaterMiracleTree: public ::testing::Test
{
protected:
	/// The players' tree alignment change, as info.dat has it
	static constexpr float k_TreeAlignment = 0.005f;

	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::fireSystem::emplace<ecs::systems::FireSystem>();
		_forests = &static_cast<WaterForests&>(Locator::forestSystem::emplace<WaterForests>());
		auto info = std::make_unique<InfoConstants>();
		info->player.treePullPutAlignmentChange = k_TreeAlignment;
		auto& beech = info->tree.at(static_cast<size_t>(TreeInfo::Beech));
		beech.growthAmount = 0.1f;
		beech.waterSpellAcceleratorMultiplier = 1.0f;
		Locator::infoConstants::reset(info.release());
		auto& alignment = players::AlignmentOf(PlayerNames::PLAYER_ONE);
		_alignment = alignment;
		alignment = {};
	}

	void TearDown() override { players::AlignmentOf(PlayerNames::PLAYER_ONE) = _alignment; }

	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] WaterForests& Forests() const { return *_forests; }

	/// A beech of the first forest that has reached its full size
	[[nodiscard]] static entt::entity MakeFullGrownTree()
	{
		const auto tree = Registry().Create();
		Registry().Assign<ecs::components::Tree>(
		    tree, ecs::components::Tree {.type = TreeInfo::Beech, .maxSize = 1.0f, .forestId = 1});
		Registry().Assign<ecs::components::Transform>(tree, glm::vec3(50.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return tree;
	}

	/// A plain water miracle, of the first player or of nobody
	[[nodiscard]] static entt::entity MakeWater(bool hasPlayer)
	{
		const auto spell = Registry().Create();
		auto& component = Registry().Assign<Spell>(spell);
		component.magicType = MagicType::Water;
		component.spellClass = ClassOf(MagicType::Water);
		component.player = PlayerNames::PLAYER_ONE;
		component.hasPlayer = hasPlayer;
		return spell;
	}

	[[nodiscard]] static float Pending() { return players::AlignmentOf(PlayerNames::PLAYER_ONE).pending; }

private:
	// the registry goes first, while the tables, the forests and the fires it was made with are still there
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::forestSystem> _restoreForests;
	test::RestoreService<Locator::fireSystem> _restoreFires;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	WaterForests* _forests {nullptr};
	ecs::components::Alignment _alignment;
};

/// Rain on fields: a registry, the fires, the reactions a burning thing starts, tables zeroed but for what a drop
/// grows a field by, and no map for the reactions to spread over, each put back as it was when the test ends
class WaterMiracleField: public ::testing::Test
{
protected:
	/// What a drop grows a growing field by (the fields' water effect)
	static constexpr float k_WaterEffect = 2.0f;

	void SetUp() override
	{
		// the miracles' log lines go nowhere
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_fires = &Locator::fireSystem::emplace<ecs::systems::FireSystem>();
		Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
		Locator::entitiesMap::reset();
		auto info = std::make_unique<InfoConstants>();
		info->fieldType.at(0).effectOfWaterSpell = k_WaterEffect;
		Locator::infoConstants::reset(info.release());
	}

	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }

	/// A field with crops sown and grown so far
	[[nodiscard]] static entt::entity MakeField(uint8_t crops, float growth)
	{
		const auto field = Registry().Create();
		auto& component = Registry().Assign<ecs::components::Field>(field);
		component.crops = crops;
		component.growth = growth;
		return field;
	}

	/// The field burns, far above any temperature things catch fire at
	void SetAlight(entt::entity object)
	{
		auto fire = std::make_unique<ecs::fire::FireEffect>();
		fire->id = _fires->TakeId();
		fire->object = object;
		fire->temperature = 1.0e6f;
		fire->previous = 1.0e6f;
		_fires->Insert(std::move(fire));
	}

	/// A plain water miracle of nobody
	[[nodiscard]] static entt::entity MakeWater()
	{
		const auto spell = Registry().Create();
		auto& component = Registry().Assign<Spell>(spell);
		component.magicType = MagicType::Water;
		component.spellClass = ClassOf(MagicType::Water);
		component.hasPlayer = false;
		return spell;
	}

	[[nodiscard]] static const ecs::components::Field& FieldOf(entt::entity field)
	{
		return Registry().Get<const ecs::components::Field>(field);
	}

private:
	// the registry goes first, while the tables, the reactions and the fires it was made with are still there
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::entitiesMap> _restoreMap;
	test::RestoreService<Locator::reactionSystem> _restoreReactions;
	test::RestoreService<Locator::fireSystem> _restoreFires;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	ecs::systems::FireSystemInterface* _fires {nullptr};
};
} // namespace

// ---- The hand while it rains ----

TEST(WaterMiracle, TheHandBobsUpEightMetresAndBackOverAnEightSecondLoopWithoutBeingHeld)
{
	using namespace ecs::systems;
	hand_grain::Reset();
	hand_grain::Start(k_WaterClampHand, k_WaterTotalTime, k_WaterHeightToRaise, k_WaterAngleToRaise, true);
	const CubicSpline spline(k_PourKeyPoints);
	// 1.6 s in, a fifth of the loop: the curve's first key point of 1, so 8 m up
	for (int turn = 1; turn <= 16; ++turn)
	{
		hand_grain::GameTurnUpdate(k_TurnSeconds);
	}
	auto state = hand_grain::Debug();
	EXPECT_NEAR(state.x, 0.2f, 1e-4f);
	EXPECT_NEAR(state.y, 8.0f, 1e-3f);
	EXPECT_EQ(state.z, 0.0f);
	// the hand follows the mouse: it is not held where the pour began
	EXPECT_FALSE(hand_grain::ClampedPosition().has_value());
	// 4 s in, half way: the top of the bob, 8 m times the curve's peak, and still no tilt
	for (int turn = 17; turn <= 40; ++turn)
	{
		hand_grain::GameTurnUpdate(k_TurnSeconds);
	}
	state = hand_grain::Debug();
	EXPECT_NEAR(state.x, 0.5f, 1e-4f);
	EXPECT_NEAR(state.y, 8.0f * spline(0.5f), 1e-3f);
	EXPECT_EQ(state.z, 0.0f);
	// 6.4 s in, four fifths of the loop: back at 8 m on the way down
	for (int turn = 41; turn <= 64; ++turn)
	{
		hand_grain::GameTurnUpdate(k_TurnSeconds);
	}
	state = hand_grain::Debug();
	EXPECT_NEAR(state.x, 0.8f, 1e-4f);
	EXPECT_NEAR(state.y, 8.0f, 1e-3f);
	// past 8 s it starts over from the bottom, while it rains
	for (int turn = 65; turn <= 85; ++turn)
	{
		hand_grain::GameTurnUpdate(k_TurnSeconds);
	}
	state = hand_grain::Debug();
	EXPECT_TRUE(hand_grain::Active());
	EXPECT_LT(state.x, 0.07f);
	EXPECT_LT(state.y, 8.0f * spline(0.07f) + 1e-3f);
	hand_grain::Reset();
}

TEST_F(WaterMiracleSpell, TheBobStopsWhenTheRainOfAMiracleThisComputerCastEnds)
{
	using namespace ecs::systems;
	hand_grain::Reset();
	const auto spell = MakeWater(7000.0f);
	Registry().Get<Spell>(spell).isMyInterfaceCasting = true;
	hand_grain::Start(k_WaterClampHand, k_WaterTotalTime, k_WaterHeightToRaise, k_WaterAngleToRaise, true);
	hand_grain::GameTurnUpdate(k_TurnSeconds);
	ASSERT_TRUE(hand_grain::Active());
	// a water miracle no interface of this computer cast leaves the hand alone when it ends
	const auto other = MakeWater(7000.0f);
	OpsOf(SpellClass::Water).closeDown(other);
	EXPECT_TRUE(Registry().Get<const Spell>(other).closedDown);
	EXPECT_TRUE(hand_grain::Active());
	// the caster's own: its close-down stops the bob
	OpsOf(SpellClass::Water).closeDown(spell);
	EXPECT_TRUE(Registry().Get<const Spell>(spell).closedDown);
	EXPECT_FALSE(hand_grain::Active());
	// from the next game turn the hand is at rest
	hand_grain::GameTurnUpdate(k_TurnSeconds);
	EXPECT_EQ(hand_grain::Debug().y, 0.0f);
	hand_grain::Reset();
}

// ---- The drops ----

TEST_F(WaterMiracleFire, ADropNeverSetsAnythingAlight)
{
	// a drop's effect: a burn of -4000 (times strength and tribal power, both 1 here)
	ecs::effects::EffectValues drop;
	drop.numbers.at(static_cast<size_t>(ecs::effects::EffectValues::Number::Burn)) = -4000.0f;
	const auto thing = Locator::entitiesRegistry::value().Create();
	ecs::fire::ApplyEffectToFireEffectIfNecessary(thing, drop);
	// with no fire on it, a cooling burn makes none
	EXPECT_EQ(ecs::fire::Find(thing), nullptr);
	EXPECT_FALSE(ecs::fire::IsOnFire(thing));
	// nor does a drop that has lost all its strength
	drop.numbers.at(static_cast<size_t>(ecs::effects::EffectValues::Number::Burn)) = 0.0f;
	ecs::fire::ApplyEffectToFireEffectIfNecessary(thing, drop);
	EXPECT_EQ(ecs::fire::Find(thing), nullptr);
}

// ---- The trees ----

TEST_F(WaterMiracleTree, ATreeTheRainPlantsIsAGoodDeedForTheCaster)
{
	const auto tree = MakeFullGrownTree();
	const auto spell = MakeWater(true);
	// no spot found (or too soon after the last young tree): nothing planted and no deed
	EXPECT_EQ(magic::water::ApplyWaterSpell(tree, spell), 1.0f);
	ASSERT_EQ(Forests().asked.size(), 1u);
	EXPECT_EQ(Forests().asked.front(), tree);
	EXPECT_EQ(Pending(), 0.0f);
	// a young tree planted next to it: the caster's alignment moves towards good by the planted tree's change, whole
	// while the alignment is neutral
	Forests().sapling = Registry().Create();
	EXPECT_EQ(magic::water::ApplyWaterSpell(tree, spell), 1.0f);
	EXPECT_EQ(Forests().asked.size(), 2u);
	EXPECT_FLOAT_EQ(Pending(), k_TreeAlignment);
	// a caster leaning evil: the change against the way it leans counts (1 + |a| / 2) times
	players::AlignmentOf(PlayerNames::PLAYER_ONE) = {.value = -0.5f, .pending = 0.0f};
	EXPECT_EQ(magic::water::ApplyWaterSpell(tree, spell), 1.0f);
	EXPECT_FLOAT_EQ(Pending(), 1.25f * k_TreeAlignment);
	// a water miracle of nobody plants it for nobody
	const auto nobodys = MakeWater(false);
	EXPECT_EQ(magic::water::ApplyWaterSpell(tree, nobodys), 1.0f);
	EXPECT_EQ(Forests().asked.size(), 4u);
	EXPECT_FLOAT_EQ(Pending(), 1.25f * k_TreeAlignment);
}

// ---- The fields ----

TEST_F(WaterMiracleField, ABurningFieldsCropIsLeftAsItIs)
{
	const auto spell = MakeWater();
	// an unsown field not on fire is sown at once, a growing one grows by the fields' water effect
	const auto unsown = MakeField(0, 0.0f);
	const auto growing = MakeField(31, 100.0f);
	magic::water::ApplyWaterSpell(unsown, spell);
	magic::water::ApplyWaterSpell(growing, spell);
	EXPECT_EQ(FieldOf(unsown).crops, 31);
	EXPECT_EQ(FieldOf(growing).growth, 100.0f + k_WaterEffect);
	// the same fields burning are neither sown nor grown
	const auto burningUnsown = MakeField(0, 0.0f);
	const auto burningGrowing = MakeField(31, 100.0f);
	SetAlight(burningUnsown);
	SetAlight(burningGrowing);
	ASSERT_TRUE(ecs::fire::IsOnFire(burningUnsown));
	ASSERT_TRUE(ecs::fire::IsOnFire(burningGrowing));
	magic::water::ApplyWaterSpell(burningUnsown, spell);
	magic::water::ApplyWaterSpell(burningGrowing, spell);
	EXPECT_EQ(FieldOf(burningUnsown).crops, 0);
	EXPECT_EQ(FieldOf(burningUnsown).growth, 0.0f);
	EXPECT_EQ(FieldOf(burningGrowing).crops, 31);
	EXPECT_EQ(FieldOf(burningGrowing).growth, 100.0f);
	EXPECT_EQ(FieldOf(burningGrowing).food, 0.0f);
}
