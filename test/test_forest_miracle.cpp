/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The forest miracle's rows that the spell's own tests (test_spell_forest.cpp) leave open: it is never cast at an
// object, every tree of its forest withers by the decay speed and goes at nothing while the spell wants no tree (its
// turn through the spell classes' table), a burning magic tree stops drawing looks and draws them again once put out,
// and a magic tree's wood is its own multiplier times an ordinary tree's. Then the forest's effect: what the game's
// SF_Forest gives it, and its seed, sparkles, vortices, flocks and light on the land stepped from a twin of that file.

// The fakes of the miracles own the real stores of the miracles' service
#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/AxisAlignedBoundingBox.h"
#include "3D/FrameAnim.h"
#include "Audio/AudioManagerNoOp.h"
#include "Audio/Engine/AnimEffects.h"
#include "Audio/Services/SpellSounds.h"
#include "Common/Zip.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "ECS/Systems/MeshBoxProviderInterface.h"
#include "ECS/Trees.h"
#include "Enums.h"
#include "FileSystem/DefaultFileSystem.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Magic/Core/Spell.h"
#include "Magic/Objects/MagicTree.h"
#include "Magic/Spells/SpellForest.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/SoundAction.h"
#include "Particles/SpellLink.h"
#include "Resources/Loaders.h"
#include "Resources/Resources.h"
#include "Resources/ResourcesInterface.h"
#include "support/LandFakes.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;

namespace
{
namespace reactions = openblack::ecs::effects::reactions;

/// The NATURE row of the game's info.dat (SpellForest.realInfoDat reads both): while the spell wants fewer trees than
/// its forest has, each tree shrinks this much a turn; a magic tree's wood multiplier is this quarter times the spell's
/// tribal power
constexpr float k_DecaySpeed = 0.05f;
constexpr float k_WoodValueMultiplier = 0.25f;
/// A wood value for the beech of the tests' table (any value: the tests compare a magic tree with an ordinary one)
constexpr uint32_t k_BeechWood = 100;

/// The cast rules over the miracles' fake world: a registry, the animals' shared state the spell classes register with
/// and the inert miracles' service, whose world is a FakeMagicWorld (flat land, all in the influence), each put back as
/// it was when the test ends
class ForestMiracleCastRules: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::animalSystem::emplace<ecs::systems::AnimalSystem>();
		Locator::magicSystem::emplace<test::InertMagicSystem>();
	}

	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }

private:
	// put back in reverse order: the miracles' service with its stores, the registry, then the animals' shared state
	test::RestoreService<Locator::animalSystem> _restoreAnimals;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::magicSystem> _restoreMagic;
};

/// No mesh is loaded: the trees have no footprint
class NoMeshBoxes final: public ecs::systems::MeshBoxProviderInterface
{
public:
	[[nodiscard]] std::optional<AxisAlignedBoundingBox> MeshBox(entt::id_type /*meshId*/) const override
	{
		return std::nullopt;
	}
};

/// The game's trees, forests, reactions and spell classes over a fresh registry and world lists, the inert miracles'
/// service (whose spell store the spell classes register in), a zeroed table but for the forest row and the beech's wood
/// value, and no meshes
class ForestMiracleTrees: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		auto info = std::make_unique<InfoConstants>();
		info->tree.at(static_cast<size_t>(TreeInfo::Beech)).woodValue = k_BeechWood;
		// the NATURE row of the game's info.dat
		auto& forestRow = info->magicForest[0];
		forestRow.finalNoTrees = 18;
		forestRow.growSpeed = 0.01f;
		forestRow.decaySpeed = k_DecaySpeed;
		forestRow.woodValueMultiplier = k_WoodValueMultiplier;
		Locator::infoConstants::reset(info.release());
		Locator::meshBoxProvider::emplace<NoMeshBoxes>();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		Locator::magicSystem::emplace<test::InertMagicSystem>();
	}

	void TearDown() override
	{
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
	}

	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }

	/// A beech of a forest (0: none) at a size, with a wood value multiplier (1 for an ordinary tree)
	static entt::entity MakeTree(uint32_t forestId, float scale, float woodValueMultiplier = 1.0f)
	{
		auto& registry = Registry();
		const auto tree = registry.Create();
		registry.Assign<ecs::components::Tree>(tree, ecs::components::Tree {.type = TreeInfo::Beech,
		                                                                    .maxSize = 1.0f,
		                                                                    .forestId = forestId,
		                                                                    .woodValueMultiplier = woodValueMultiplier});
		registry.Assign<ecs::components::Transform>(tree, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(scale));
		return tree;
	}

	/// A magic tree of a player's forest miracle at a size (half grown unless said), with its wood multiplier
	static entt::entity MakeMagicTree(uint32_t forestId, PlayerNames player, float woodValueMultiplier, float scale = 0.5f)
	{
		const auto tree = MakeTree(forestId, scale, woodValueMultiplier);
		Registry().Assign<ecs::components::MagicTree>(tree, player, woodValueMultiplier);
		return tree;
	}

private:
	// put back in reverse order: the miracles' service with its stores, the mesh boxes, then the tables
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::meshBoxProvider> _restoreBoxes;
	test::RestoreService<Locator::magicSystem> _restoreMagic;
};
} // namespace

/// The forest's check on an object always says no, while a miracle whose class checks the object's place (a resource:
/// land there) may be cast at the same object
TEST_F(ForestMiracleCastRules, ItIsNeverCastAtAnObject)
{
	auto& registry = Registry();
	const auto object = registry.Create();
	registry.Assign<ecs::components::Transform>(object, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_TRUE(magic::cast_rules::CanCastOn(MagicType::Wood, object));
	EXPECT_FALSE(magic::cast_rules::CanCastOn(MagicType::Forest, object));
	// nor at an object with no place
	EXPECT_FALSE(magic::cast_rules::CanCastOn(MagicType::Forest, registry.Create()));
}

/// While the spell wants no tree (no strength left), every tree of its forest shrinks by the decay speed each turn, a
/// tree that would come to nothing or less goes, and another forest's tree is left as it is
TEST_F(ForestMiracleTrees, AllTheTreesWitherByTheDecaySpeedEachTurnAndGoAtNothing)
{
	// no strength: no tree is wanted, fewer than the forest has
	ASSERT_EQ(magic::spell_forest::TreesWanted(0.0f, -1, 18), 0);
	auto& registry = Registry();
	const auto forest = ecs::CreateForest(0, glm::vec3(100.0f, 0.0f, 100.0f));
	const auto other = ecs::CreateForest(0, glm::vec3(300.0f, 0.0f, 300.0f));
	const auto half = MakeTree(forest, 0.5f);
	const auto sapling = MakeTree(forest, 0.04f);
	const auto exact = MakeTree(forest, k_DecaySpeed);
	const auto elsewhere = MakeTree(other, 0.5f);
	ASSERT_EQ(ecs::ForestTreeCount(forest), 3u);

	ecs::ShrinkAllTrees(forest, k_DecaySpeed);
	ASSERT_TRUE(registry.Valid(half));
	EXPECT_FLOAT_EQ(registry.Get<ecs::components::Transform>(half).scale.x, 0.5f - k_DecaySpeed);
	EXPECT_FALSE(registry.Valid(sapling)); // below nothing
	EXPECT_FALSE(registry.Valid(exact));   // exactly nothing
	EXPECT_EQ(ecs::ForestTreeCount(forest), 1u);
	ASSERT_TRUE(registry.Valid(elsewhere));
	EXPECT_FLOAT_EQ(registry.Get<ecs::components::Transform>(elsewhere).scale.x, 0.5f);

	// a half grown tree goes on the tenth turn of it: each step is rounded to a float, and ten of them take a little more
	// than the half
	for (int turn = 2; turn <= 9; ++turn)
	{
		ecs::ShrinkAllTrees(forest, k_DecaySpeed);
	}
	EXPECT_TRUE(registry.Valid(half));
	ecs::ShrinkAllTrees(forest, k_DecaySpeed);
	EXPECT_FALSE(registry.Valid(half));
	EXPECT_EQ(ecs::ForestTreeCount(forest), 0u);
	EXPECT_EQ(ecs::ForestTreeCount(other), 1u);
}

/// A forest miracle that could no longer be paid for is closed and has dropped its creator, so it has no strength and
/// wants no tree: each of its turns every tree withers by the decay speed and a tree that comes to nothing goes; with
/// the last tree the forest goes, and the spell's next turn, finding no forest and no effect left, deletes the spell
TEST_F(ForestMiracleTrees, UnpaidEveryTreeWithersEachTurnUntilTheForestAndThenTheSpellGo)
{
	auto& registry = Registry();
	const auto spell = registry.Create();
	{
		auto& miracle = registry.Assign<ecs::components::Spell>(spell);
		miracle.magicType = MagicType::Forest;
		miracle.spellClass = ecs::components::SpellClass::Forest;
		miracle.closedDown = true; // no creator, no effect
	}
	const auto forest = ecs::CreateForest(0, glm::vec3(100.0f, 0.0f, 100.0f));
	{
		auto& data = registry.Assign<magic::SpellForestData>(spell);
		data.forestId = forest;
		data.forestCreated = true;
		data.maxTrees = 18;
	}
	const auto half = MakeMagicTree(forest, PlayerNames::PLAYER_ONE, k_WoodValueMultiplier, 0.5f);
	const auto sapling = MakeMagicTree(forest, PlayerNames::PLAYER_ONE, k_WoodValueMultiplier, 0.1f);
	const auto& ops = magic::OpsOf(ecs::components::SpellClass::Forest);
	ASSERT_TRUE(ops.process != nullptr);

	EXPECT_EQ(ops.process(spell), 1);
	EXPECT_FLOAT_EQ(registry.Get<ecs::components::Transform>(half).scale.x, 0.5f - k_DecaySpeed);
	EXPECT_FLOAT_EQ(registry.Get<ecs::components::Transform>(sapling).scale.x, 0.1f - k_DecaySpeed);
	EXPECT_EQ(ops.process(spell), 1);
	EXPECT_FALSE(registry.Valid(sapling));
	for (int turn = 3; turn <= 9; ++turn)
	{
		EXPECT_EQ(ops.process(spell), 1);
	}
	ASSERT_TRUE(registry.Valid(half));
	EXPECT_TRUE(ecs::IsInForest(forest));
	// the tenth turn takes the last tree, and the forest with it
	EXPECT_EQ(ops.process(spell), 1);
	EXPECT_FALSE(registry.Valid(half));
	EXPECT_FALSE(ecs::IsInForest(forest));
	// the next turn finds no forest: with no effect left the spell is deleted
	EXPECT_EQ(ops.process(spell), 5);
	const auto* data = magic::spell_forest::DataOf(spell);
	ASSERT_NE(data, nullptr);
	EXPECT_EQ(data->forestId, 0u);
}

/// A magic tree that catches fire no longer draws looks: its magic tree reaction (type 8) goes. Put out, it makes the
/// reaction again for its spell's player, not stamped with the turn; a tree that is no magic tree makes none
TEST_F(ForestMiracleTrees, FireTakesAMagicTreesLookAwayAndPuttingItOutMakesItAgain)
{
	constexpr auto k_Type = openblack::Reaction::ReactToMagicTree;
	EXPECT_EQ(static_cast<int>(k_Type), 8);
	const auto tree = MakeMagicTree(0, PlayerNames::PLAYER_TWO, k_WoodValueMultiplier);
	// the reaction the tree was made with
	reactions::CreateReaction(tree, k_Type, PlayerNames::PLAYER_TWO, false);
	ASSERT_NE(reactions::GetReactionOfTypeInitiatedBy(tree, k_Type), 0u);

	magic::magic_tree::StartOnFire(tree);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(tree, k_Type), 0u);

	game_clock::SetTurn(50);
	magic::magic_tree::EndOnFire(tree);
	const auto id = reactions::GetReactionOfTypeInitiatedBy(tree, k_Type);
	ASSERT_NE(id, 0u);
	const auto* reaction = reactions::Find(id);
	ASSERT_NE(reaction, nullptr);
	EXPECT_EQ(reaction->type, k_Type);
	ASSERT_TRUE(reaction->player.has_value());
	EXPECT_EQ(*reaction->player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(reaction->turnCreated, 0u);

	// on fire again, the look goes again
	magic::magic_tree::StartOnFire(tree);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(tree, k_Type), 0u);

	const auto ordinary = MakeTree(0, 0.5f);
	magic::magic_tree::EndOnFire(ordinary);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(ordinary, k_Type), 0u);
}

/// A magic tree's wood is an ordinary tree's times its own multiplier (the NATURE row's quarter times the spell's tribal
/// power); an ordinary tree's multiplier is 1. A tree's wood value is its life (1) x the multiplier x its kind's wood
/// value x its scale x the land balance's wood value (1 on a new land)
TEST_F(ForestMiracleTrees, AMagicTreesWoodIsItsMultiplierTimesAnOrdinaryTreesWood)
{
	constexpr float k_TribalPower = 2.0f;
	const auto ordinary = MakeTree(0, 0.5f);
	const auto magicTree = MakeMagicTree(0, PlayerNames::PLAYER_ONE, k_WoodValueMultiplier * k_TribalPower);
	EXPECT_FLOAT_EQ(magic::magic_tree::WoodValueMultiplier(ordinary), 1.0f);
	EXPECT_FLOAT_EQ(magic::magic_tree::WoodValueMultiplier(magicTree), 0.5f);
	ASSERT_FLOAT_EQ(land_balance::Get(5), 1.0f);
	EXPECT_FLOAT_EQ(ecs::TreeWoodValue(ordinary), 50.0f);  // 1 x 1 x 100 x 0.5 x 1
	EXPECT_FLOAT_EQ(ecs::TreeWoodValue(magicTree), 25.0f); // 1 x 0.5 x 100 x 0.5 x 1
}

namespace
{
/// The sound actions the forest's effect plays, with the values the game's Data\SoundAction.h gives them
constexpr int32_t k_SeedSound = 34;    ///< SOUND_SPELL_FOREST_1
constexpr int32_t k_FlockSound = 36;   ///< SOUND_SPELL_FOREST_3
constexpr int32_t k_SparkleSound = 37; ///< SOUND_SPELL_FOREST_4
constexpr std::string_view k_ForestSoundActions = "enum LHSoundAction\n{\n\tNO_SOUND_ACTION = 0,\n"
                                                  "\tSOUND_SPELL_FOREST_1 = 34,\n\tSOUND_SPELL_FOREST_2 = 35,\n"
                                                  "\tSOUND_SPELL_FOREST_3 = 36,\n\tSOUND_SPELL_FOREST_4 = 37,\n};\n";

/// Where the tests cast the forest: on a flat land at height 0
constexpr glm::vec3 k_CastPoint {100.0f, 0.0f, 100.0f};

/// SF_Forest with the game's file's header and rules for the seed (group 0), the camera particle everything else hangs
/// from (2), the sparkle root and its sparkles (3, 4), the vortices (11), the first burst (12) and the flocks (5, 6, 7),
/// each rule's lines as the file has them. The creators are points in place of the file's meshes, sprites, camera
/// particle and good or evil creator, and what nothing here reads is left out: the seed's trail of blobs (group 1) and
/// spin, the flocks' path and flocking, the vortices' growth and the camera's, sparkles' and vortices' fades
constexpr std::string_view k_ForestTwin = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 1
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 1 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS ParticlePointCreator ParticlePointCreator_Seed
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 0.207124
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_Camera
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 0.140486
ENDPROPERTIES
ENDCLASS
BEGINCLASS LandscapeCollide LandscapeCollide_Seed
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SendEvent BOOL 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS UpdateRuleGravity UpdateRuleGravity_Seed
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Damping FLOAT 0
PROPERTY DisableDampingForNonHuman BOOL 0
PROPERTY DisableWindForNonHuman BOOL 0
PROPERTY Gravity FLOAT 1.6
PROPERTY Group INTEGER 0
PROPERTY MaxSpeed FLOAT 3.34513
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY UseDamping BOOL 0
PROPERTY UseWind BOOL 0
PROPERTY WindMagnification FLOAT 100
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_Seed
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY InitScaleFP PERSIS_PNTR NULL_STRING
PROPERTY NextGroups ARRAY SIZE 1 1
PROPERTY OffsetX FLOAT 0
PROPERTY OffsetY FLOAT 9.435
PROPERTY OffsetZ FLOAT 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_Seed
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION SOUND_SPELL_FOREST_1 LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS EmitterRuleConical EmitterRuleConical_Sparkle
BEGINPROPERTIES
PROPERTY AllowMultipleEmits BOOL 1
PROPERTY Condition PERSIS_PNTR EventConditionCollectionLimitedTime_Sparkles
PROPERTY EmissionFreq FLOAT 20
PROPERTY Group INTEGER 4
PROPERTY InitiallyVisible BOOL 1
PROPERTY MaxAtoms INTEGER 75
PROPERTY MaxTotalAtomsToEmit INTEGER -1
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_Sparkle
PROPERTY Radius FLOAT 15
PROPERTY Randomise BOOL 0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundEmission SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
PROPERTY Speed FLOAT 6.23451
PROPERTY Spread FLOAT 1.002
ENDPROPERTIES
ENDCLASS
BEGINCLASS RemoveRuleOldAgeOnly RemoveRuleOldAgeOnly_Sparkle
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY DieAge FLOAT 1.5
PROPERTY Group INTEGER 4
PROPERTY MinAtoms INTEGER 0
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_UpdatePosnFromVelocity UR_UpdatePosnFromVelocity_Sparkle
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 4
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_Sparkle
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS EmitterRuleConical EmitterRuleConical_InitSparkle
BEGINPROPERTIES
PROPERTY AllowMultipleEmits BOOL 1
PROPERTY EmissionFreq FLOAT 30
PROPERTY Group INTEGER 12
PROPERTY InitiallyVisible BOOL 1
PROPERTY MaxAtoms INTEGER 100
PROPERTY MaxTotalAtomsToEmit INTEGER 10
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_InitSparkle
PROPERTY Radius FLOAT 0
PROPERTY Randomise BOOL 0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundEmission SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
PROPERTY Speed FLOAT 6.23451
PROPERTY Spread FLOAT 1.002
ENDPROPERTIES
ENDCLASS
BEGINCLASS RemoveRuleOldAgeOnly RemoveRuleOldAgeOnly_InitSparkle
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY DieAge FLOAT 1.5
PROPERTY Group INTEGER 12
PROPERTY MinAtoms INTEGER 0
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_UpdatePosnFromVelocity UR_UpdatePosnFromVelocity_InitSparkle
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 12
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_InitSparkle
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_Camera
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 2
PROPERTY InitScaleFP PERSIS_PNTR NULL_STRING
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY OffsetX FLOAT 0
PROPERTY OffsetY FLOAT 0
PROPERTY OffsetZ FLOAT 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_Camera
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS RemoveRuleOldAgeOnly RemoveRuleOldAgeOnly_Camera
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY DieAge FLOAT 25
PROPERTY Group INTEGER 2
PROPERTY MinAtoms INTEGER 0
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS AddSubCollectionsToAtom AddSubCollectionsToAtom_Sparkles
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR EventConditionCollectionDelay_Sparkles
PROPERTY Group INTEGER 2
PROPERTY NextGroups ARRAY SIZE 3 3 11 12
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS AddSubCollectionsToAtom AddSubCollectionsToAtom_Butterflies
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR EventConditionCollectionDelay_Butterflies
PROPERTY Group INTEGER 2
PROPERTY NextGroups ARRAY SIZE 1 5
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_SparkleRoot
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 3
PROPERTY InitScaleFP PERSIS_PNTR NULL_STRING
PROPERTY NextGroups ARRAY SIZE 1 4
PROPERTY OffsetX FLOAT 0
PROPERTY OffsetY FLOAT 0
PROPERTY OffsetZ FLOAT 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_SparkleRoot
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION SOUND_SPELL_FOREST_4 LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_ChangeScale UR_ChangeScale_SparkleRoot
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 3
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY StartScale FLOAT 0
PROPERTY StartTime FLOAT 0
PROPERTY StopScale FLOAT 1
PROPERTY StopTime FLOAT 3
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_SparkleRoot
BEGINPROPERTIES
PROPERTY ColorA INTEGER 255
PROPERTY InitialScale FLOAT 1
PROPERTY LoopAnim BOOL 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS EventConditionCollectionDelay EventConditionCollectionDelay_Sparkles
BEGINPROPERTIES
PROPERTY DelayTime FLOAT 3.5
PROPERTY InvertResponse BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS EventConditionCollectionDelay EventConditionCollectionDelay_Butterflies
BEGINPROPERTIES
PROPERTY DelayTime FLOAT 5.5
PROPERTY InvertResponse BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_Butt
BEGINPROPERTIES
PROPERTY ColorA INTEGER 255
PROPERTY InitialScale FLOAT 1
PROPERTY LoopAnim BOOL 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleSphere CreateRuleSphere_Butterflies
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 7
PROPERTY InitScaleFP PERSIS_PNTR NULL_STRING
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY NumAtoms INTEGER 10
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_Butterfly
PROPERTY Radius FLOAT 2.66814
PROPERTY RadiusScaleFP PERSIS_PNTR NULL_STRING
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SetInitFrameFromIndex BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS RemoveRuleOldAgeOnly RemoveRuleOldAgeOnly_ButterflyGroups
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY DieAge FLOAT 16
PROPERTY Group INTEGER 6
PROPERTY MinAtoms INTEGER 0
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleSphere CreateRuleSphere_ButterflyGroups
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 6
PROPERTY InitScaleFP PERSIS_PNTR NULL_STRING
PROPERTY NextGroups ARRAY SIZE 1 7
PROPERTY NumAtoms INTEGER 5
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_Butt
PROPERTY Radius FLOAT 0
PROPERTY RadiusScaleFP PERSIS_PNTR NULL_STRING
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SetInitFrameFromIndex BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION SOUND_SPELL_FOREST_3 LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_ButterflyRoot
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 5
PROPERTY InitScaleFP PERSIS_PNTR NULL_STRING
PROPERTY NextGroups ARRAY SIZE 1 6
PROPERTY OffsetX FLOAT 0
PROPERTY OffsetY FLOAT 0
PROPERTY OffsetZ FLOAT 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_Butt
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_Butterfly
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 4
ENDPROPERTIES
ENDCLASS
BEGINCLASS AR_FadeAlpha AR_FadeAlpha_ButterflyIn
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 7
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY StartAlpha INTEGER 0
PROPERTY StartTime FLOAT 0
PROPERTY StopAlpha INTEGER 255
PROPERTY StopTime FLOAT 4
ENDPROPERTIES
ENDCLASS
BEGINCLASS AR_FadeAlpha AR_FadeAlpha_ButterflyOut
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 7
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY StartAlpha INTEGER 255
PROPERTY StartTime FLOAT 12
PROPERTY StopAlpha INTEGER 0
PROPERTY StopTime FLOAT 14
ENDPROPERTIES
ENDCLASS
BEGINCLASS EventConditionCollectionLimitedTime EventConditionCollectionLimitedTime_Sparkles
BEGINPROPERTIES
PROPERTY InvertResponse BOOL 0
PROPERTY StartTime FLOAT 0
PROPERTY StopTime FLOAT 10
ENDPROPERTIES
ENDCLASS
BEGINCLASS EmitterRuleSimple EmitterRuleSimple_MultiVortex
BEGINPROPERTIES
PROPERTY AllowMultipleEmits BOOL 0
PROPERTY Condition PERSIS_PNTR EventConditionCollectionLimitedTime_VoretxEmit
PROPERTY EmissionFreq FLOAT 1.99558
PROPERTY Group INTEGER 11
PROPERTY InitiallyVisible BOOL 1
PROPERTY MaxAtoms INTEGER 6
PROPERTY MaxTotalAtomsToEmit INTEGER -1
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY OrientWithParent BOOL 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_Vortex
PROPERTY Randomise BOOL 0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundEmission SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
PROPERTY Speed FLOAT 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UpdateRuleRotatePrincipalAxis UpdateRuleRotatePrincipalAxis_MultiVortex
BEGINPROPERTIES
PROPERTY AngularVel FLOAT 8.22566
PROPERTY AxisChosen FLOAT 1
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 11
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS RemoveRuleOldAgeOnly RemoveRuleOldAgeOnly_MultiVortex
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY DieAge FLOAT 1.5
PROPERTY Group INTEGER 11
PROPERTY MinAtoms INTEGER 0
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS EventConditionCollectionLimitedTime EventConditionCollectionLimitedTime_VoretxEmit
BEGINPROPERTIES
PROPERTY InvertResponse BOOL 0
PROPERTY StartTime FLOAT 0
PROPERTY StopTime FLOAT 4
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_Vortex
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 0.157079
ENDPROPERTIES
ENDCLASS
)";

/// SF_Forest's light on the land as the game's file has it: group 14, made with the effect, gets one light map atom
/// once it is 3.5 s old, and the light map creator's lines are the file's
constexpr std::string_view k_ForestLightMap = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 1
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 1 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS EventConditionCollectionDelay EventConditionCollectionDelay_PermSparkles
BEGINPROPERTIES
PROPERTY DelayTime FLOAT 3.5
PROPERTY InvertResponse BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_LightMap
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR EventConditionCollectionDelay_PermSparkles
PROPERTY Group INTEGER 14
PROPERTY NextGroups ARRAY SIZE 1 15
PROPERTY OffsetX FLOAT 0
PROPERTY OffsetY FLOAT 0
PROPERTY OffsetZ FLOAT 0
PROPERTY PCreator PERSIS_PNTR ParticleLightMapCreator0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticleLightMapCreator ParticleLightMapCreator0
BEGINPROPERTIES
PROPERTY FrameRate FLOAT 15
PROPERTY InitialScale FLOAT 1
PROPERTY LoopAnim BOOL 1
PROPERTY NumFramesInFile INTEGER 9
PROPERTY NumFramesInUse INTEGER 9
PROPERTY Pitch INTEGER 7
PROPERTY PlayAnim BOOL 1
PROPERTY RandJitter FLOAT 0
PROPERTY ShiftX FLOAT 10
PROPERTY ShiftZ FLOAT 10
PROPERTY TextureFileName STRING .\Data\SPELLS\LightMaps\Forest_LMap.raw
PROPERTY UseRandJitter BOOL 0
ENDPROPERTIES
ENDCLASS
)";

std::shared_ptr<const particles::File> ParseTwin(std::string_view text)
{
	auto file = particles::File::Parse(text, "SF_ForestMiracleTest");
	return file.has_value() ? std::make_shared<const particles::File>(*file) : nullptr;
}

/// Data\Spells\ZSpellFiles\SF_Forest_txt.zzz: the length then a deflate stream
std::optional<particles::File> LoadForestFile(const std::filesystem::path& root)
{
	std::ifstream stream(root / "Data" / "Spells" / "ZSpellFiles" / "SF_Forest_txt.zzz", std::ios::binary);
	if (!stream.is_open())
	{
		return std::nullopt;
	}
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	if (bytes.size() <= 4)
	{
		return std::nullopt;
	}
	uint32_t size = 0;
	std::memcpy(&size, bytes.data(), sizeof(size));
	const auto inflated = zip::Inflate(std::vector<uint8_t>(bytes.begin() + 4, bytes.end()), size);
	return particles::File::Parse(std::string(inflated.begin(), inflated.end()), "SF_Forest");
}

/// The object of that name, nullptr when the file has none or it is of another class
const particles::Object* Named(const particles::File& file, std::string_view name, std::string_view className)
{
	const auto* object = file.Find(name);
	return object != nullptr && object->className == className ? object : nullptr;
}

/// A rule of the camera particle (group 2) that hangs these groups from it once its collection is older than `delay`
void ExpectAddedAfter(const particles::File& file, std::string_view name, const std::vector<int>& groups, float delay)
{
	const auto* rule = Named(file, name, "AddSubCollectionsToAtom");
	ASSERT_NE(rule, nullptr) << name;
	EXPECT_EQ(rule->Int("Group", -1), 2) << name;
	EXPECT_EQ(rule->IntArray("NextGroups"), groups) << name;
	const auto* condition = file.Find(rule->String("Condition"));
	ASSERT_NE(condition, nullptr) << name;
	EXPECT_EQ(condition->className, "EventConditionCollectionDelay") << name;
	EXPECT_FLOAT_EQ(condition->Float("DelayTime", 0.0f), delay) << name;
	EXPECT_FALSE(condition->Bool("InvertResponse", true)) << name;
}

/// A create rule's sound: played once, neither looping nor released softly
void ExpectPlainSound(const particles::Object& rule, std::string_view sound)
{
	const auto action = rule.Sound("SoundOfCreate");
	EXPECT_EQ(action.sound, sound) << rule.name;
	EXPECT_FALSE(action.looping) << rule.name;
	EXPECT_FALSE(action.softRelease) << rule.name;
}

/// What SF_Forest gives the seed, the sparkles, the vortices and the flocks
void ExpectForestRules(const particles::File& file)
{
	// made with the effect: the seed (0), the camera particle (2) and the light on the land (14); the sparkle root's
	// group (3) is the frame of its sparkles
	const auto initially = file.header.IntArray("InitiallyCreated");
	ASSERT_EQ(initially.size(), 25u);
	for (size_t group = 0; group < initially.size(); ++group)
	{
		EXPECT_EQ(initially[group], (group == 0 || group == 2 || group == 14) ? 1 : 0) << "group " << group;
	}
	const auto hierarchies = file.header.IntArray("Hierarchies");
	ASSERT_EQ(hierarchies.size(), 25u);
	EXPECT_EQ(hierarchies[3], 1);

	// the seed: made 9.435 m up with its sound, falling by 1.6 m/s a second up to 3.34513 m/s, taken away (with an
	// event to the spell) where it meets the land
	const auto* seed = Named(file, "CreateRuleAnAtom_Seed", "CreateRuleAnAtom");
	ASSERT_NE(seed, nullptr);
	EXPECT_EQ(seed->Int("Group", -1), 0);
	EXPECT_FLOAT_EQ(seed->Float("OffsetY", 0.0f), 9.435f);
	ExpectPlainSound(*seed, "SOUND_SPELL_FOREST_1");
	const auto* fall = Named(file, "UpdateRuleGravity_Seed", "UpdateRuleGravity");
	ASSERT_NE(fall, nullptr);
	EXPECT_EQ(fall->Int("Group", -1), 0);
	EXPECT_FLOAT_EQ(fall->Float("Gravity", 0.0f), 1.6f);
	EXPECT_FLOAT_EQ(fall->Float("MaxSpeed", 0.0f), 3.34513f);
	const auto* collide = Named(file, "LandscapeCollide_Seed", "LandscapeCollide");
	ASSERT_NE(collide, nullptr);
	EXPECT_EQ(collide->Int("Group", -1), 0);
	EXPECT_TRUE(collide->Bool("SendEvent", false));

	// at 3.5 s the sparkle root, the vortices and the first burst; at 5.5 s the flocks' root
	ExpectAddedAfter(file, "AddSubCollectionsToAtom_Sparkles", {3, 11, 12}, 3.5f);
	ExpectAddedAfter(file, "AddSubCollectionsToAtom_Butterflies", {5}, 5.5f);

	// the sparkle root plays its sound and emits the sparkles
	const auto* sparkleRoot = Named(file, "CreateRuleAnAtom_SparkleRoot", "CreateRuleAnAtom");
	ASSERT_NE(sparkleRoot, nullptr);
	EXPECT_EQ(sparkleRoot->Int("Group", -1), 3);
	EXPECT_EQ(sparkleRoot->IntArray("NextGroups"), std::vector<int> {4});
	ExpectPlainSound(*sparkleRoot, "SOUND_SPELL_FOREST_4");
	// both kinds of sparkle leave within 1.002 rad of straight up, short of level, so they rise; the burst is ten
	const auto* sparkles = Named(file, "EmitterRuleConical_Sparkle", "EmitterRuleConical");
	const auto* burst = Named(file, "EmitterRuleConical_InitSparkle", "EmitterRuleConical");
	ASSERT_NE(sparkles, nullptr);
	ASSERT_NE(burst, nullptr);
	EXPECT_EQ(sparkles->Int("Group", -1), 4);
	EXPECT_EQ(burst->Int("Group", -1), 12);
	EXPECT_EQ(burst->Int("MaxTotalAtomsToEmit", -1), 10);
	for (const auto* emitter : {sparkles, burst})
	{
		EXPECT_FLOAT_EQ(emitter->Float("Spread", 0.0f), 1.002f) << emitter->name;
		EXPECT_LT(emitter->Float("Spread", 0.0f), std::numbers::pi_v<float> / 2.0f) << emitter->name;
		EXPECT_FLOAT_EQ(emitter->Float("Speed", 0.0f), 6.23451f) << emitter->name;
	}
	// at most six vortices at once, each turning 8.22566 rad a second about its upright axis
	const auto* vortices = Named(file, "EmitterRuleSimple_MultiVortex", "EmitterRuleSimple");
	ASSERT_NE(vortices, nullptr);
	EXPECT_EQ(vortices->Int("Group", -1), 11);
	EXPECT_EQ(vortices->Int("MaxAtoms", -1), 6);
	const auto* spin = Named(file, "UpdateRuleRotatePrincipalAxis_MultiVortex", "UpdateRuleRotatePrincipalAxis");
	ASSERT_NE(spin, nullptr);
	EXPECT_EQ(spin->Int("Group", -1), 11);
	EXPECT_FLOAT_EQ(spin->Float("AngularVel", 0.0f), 8.22566f);
	EXPECT_EQ(spin->Int("AxisChosen", -1), 1);

	// the flocks' root makes five flocks with their sound, each of ten, which fade in over 4 s and out from 12 to 14 s;
	// the flocks go at 16 s
	const auto* flockRoot = Named(file, "CreateRuleAnAtom_ButterflyRoot", "CreateRuleAnAtom");
	ASSERT_NE(flockRoot, nullptr);
	EXPECT_EQ(flockRoot->Int("Group", -1), 5);
	EXPECT_EQ(flockRoot->IntArray("NextGroups"), std::vector<int> {6});
	const auto* flocks = Named(file, "CreateRuleSphere_ButterflyGroups", "CreateRuleSphere");
	ASSERT_NE(flocks, nullptr);
	EXPECT_EQ(flocks->Int("Group", -1), 6);
	EXPECT_EQ(flocks->Int("NumAtoms", 0), 5);
	EXPECT_EQ(flocks->IntArray("NextGroups"), std::vector<int> {7});
	ExpectPlainSound(*flocks, "SOUND_SPELL_FOREST_3");
	const auto* flock = Named(file, "CreateRuleSphere_Butterflies", "CreateRuleSphere");
	ASSERT_NE(flock, nullptr);
	EXPECT_EQ(flock->Int("Group", -1), 7);
	EXPECT_EQ(flock->Int("NumAtoms", 0), 10);
	const auto* fadeIn = Named(file, "AR_FadeAlpha_ButterflyIn", "AR_FadeAlpha");
	const auto* fadeOut = Named(file, "AR_FadeAlpha_ButterflyOut", "AR_FadeAlpha");
	ASSERT_NE(fadeIn, nullptr);
	ASSERT_NE(fadeOut, nullptr);
	EXPECT_EQ(fadeIn->Int("Group", -1), 7);
	EXPECT_EQ(fadeIn->Int("StartAlpha", -1), 0);
	EXPECT_EQ(fadeIn->Int("StopAlpha", -1), 255);
	EXPECT_FLOAT_EQ(fadeIn->Float("StartTime", -1.0f), 0.0f);
	EXPECT_FLOAT_EQ(fadeIn->Float("StopTime", -1.0f), 4.0f);
	EXPECT_EQ(fadeOut->Int("Group", -1), 7);
	EXPECT_EQ(fadeOut->Int("StartAlpha", -1), 255);
	EXPECT_EQ(fadeOut->Int("StopAlpha", -1), 0);
	EXPECT_FLOAT_EQ(fadeOut->Float("StartTime", -1.0f), 12.0f);
	EXPECT_FLOAT_EQ(fadeOut->Float("StopTime", -1.0f), 14.0f);
	const auto* flocksGo = Named(file, "RemoveRuleOldAgeOnly_ButterflyGroups", "RemoveRuleOldAgeOnly");
	ASSERT_NE(flocksGo, nullptr);
	EXPECT_EQ(flocksGo->Int("Group", -1), 6);
	EXPECT_FLOAT_EQ(flocksGo->Float("DieAge", 0.0f), 16.0f);
}

/// What SF_Forest gives the light on the land
void ExpectForestLightMap(const particles::File& file)
{
	const auto initially = file.header.IntArray("InitiallyCreated");
	ASSERT_EQ(initially.size(), 25u);
	EXPECT_EQ(initially[14], 1);
	const auto* rule = Named(file, "CreateRuleAnAtom_LightMap", "CreateRuleAnAtom");
	ASSERT_NE(rule, nullptr);
	EXPECT_EQ(rule->Int("Group", -1), 14);
	EXPECT_EQ(rule->String("PCreator"), "ParticleLightMapCreator0");
	const auto* condition = file.Find(rule->String("Condition"));
	ASSERT_NE(condition, nullptr);
	EXPECT_EQ(condition->className, "EventConditionCollectionDelay");
	EXPECT_FLOAT_EQ(condition->Float("DelayTime", 0.0f), 3.5f);
	EXPECT_FALSE(condition->Bool("InvertResponse", true));
	// nine frames of Forest_LMap.raw at 15 a second, looped
	const auto* creator = Named(file, "ParticleLightMapCreator0", "ParticleLightMapCreator");
	ASSERT_NE(creator, nullptr);
	EXPECT_EQ(creator->String("TextureFileName"), R"(.\Data\SPELLS\LightMaps\Forest_LMap.raw)");
	EXPECT_FLOAT_EQ(creator->Float("FrameRate", 0.0f), 15.0f);
	EXPECT_EQ(creator->Int("NumFramesInFile", 0), 9);
	EXPECT_EQ(creator->Int("NumFramesInUse", 0), 9);
	EXPECT_TRUE(creator->Bool("PlayAnim", false));
	EXPECT_TRUE(creator->Bool("LoopAnim", false));
	// its create rule is the only rule of its group: nothing takes the light away while the effect lasts
	EXPECT_EQ(
	    std::ranges::count_if(file.objects, [](const particles::Object& object) { return object.Int("Group", -1) == 14; }), 1);
}

/// Puts the synthetic SoundAction.h with the forest's sounds into a fresh resource cache, under the path the sound
/// actions are read from, so no file is opened. The previous file system and resources come back when it goes
class ForestSoundActions
{
public:
	ForestSoundActions()
	{
		Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
		Locator::resources::emplace<resources::Resources>();
		const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Data>() / "SoundAction.h";
		Locator::resources::value().GetBlobs().Load(
		    resources::BlobId(path), resources::BlobLoader::FromBufferTag {},
		    std::vector<uint8_t>(k_ForestSoundActions.begin(), k_ForestSoundActions.end()));
	}

private:
	// built before the constructor's body, so they keep the services that were there before
	const test::RestoreService<Locator::filesystem> _restoreFilesystem;
	const test::RestoreService<Locator::resources> _restoreResources;
};

/// An audio service that hears every animation effect asked of it, and has a channel still playing for every owner,
/// so a particle sound whose atom has gone is let go rather than forgotten
class HeardAnimEffects final: public audio::AudioManagerNoOp
{
public:
	struct Request
	{
		audio::AnimKey key;
		audio::AnimAction action;
	};
	[[nodiscard]] audio::Channel PlayingChannel(audio::Owner /*owner*/, audio::BankId /*bank*/) override { return 7; }
	audio::Channel PlayAnimationEffect(audio::Owner /*owner*/, float /*distance*/, const audio::AnimKey& key,
	                                   audio::AnimAction action, audio::BankId /*bank*/, bool /*track*/, float /*minDistance*/,
	                                   float /*maxDistance*/) override
	{
		requests.push_back({key, action});
		return audio::k_NoChannel;
	}
	/// The actions asked for the sounds of that action (the key's last attribute), in order
	[[nodiscard]] std::vector<audio::AnimAction> ActionsOf(int32_t sound) const
	{
		std::vector<audio::AnimAction> actions;
		for (const auto& request : requests)
		{
			if (request.key[4] == sound)
			{
				actions.push_back(request.action);
			}
		}
		return actions;
	}
	std::vector<Request> requests;
};

/// The spell behind the effect, as its rules see it: it counts the landings it is told of and answers every event with 1
class LandingSink final: public particles::SpellSink
{
public:
	int SpellEvent(const particles::SpellEventInfo& event) override
	{
		if (event.type == particles::SpellEventInfo::Type::Landed)
		{
			++landings;
			shieldChecks += event.checkShields ? 1 : 0;
		}
		return 1;
	}
	[[nodiscard]] int PowerUpLevel() const override { return -1; }
	int landings {0};
	/// Landings that asked for a shield test (none: a shield does not stop the planting)
	int shieldChecks {0};
};

/// The drawn atoms of an effect made by its creator of that name, of that kind
std::vector<const particles::Atom*> AtomsOf(const particles::Effect& effect, std::string_view creator,
                                            particles::Creator::Kind kind = particles::Creator::Kind::Point)
{
	std::vector<particles::Effect::DrawAtom> drawn;
	effect.Collect(1.0f, drawn, kind);
	const auto* wanted = effect.FindCreator(std::string(creator));
	std::vector<const particles::Atom*> atoms;
	for (const auto& atom : drawn)
	{
		if (atom.creator == wanted && atom.atom != nullptr)
		{
			atoms.push_back(atom.atom);
		}
	}
	return atoms;
}

/// Those of the atoms whose collection is of that group
std::vector<const particles::Atom*> InGroup(const std::vector<const particles::Atom*>& atoms, int group)
{
	std::vector<const particles::Atom*> found;
	std::ranges::copy_if(atoms, std::back_inserter(found), [group](const particles::Atom* atom) {
		return atom->collection != nullptr && atom->collection->group == group;
	});
	return found;
}

/// The atoms of the atom's sub-collection of that group, none when it has none
std::vector<const particles::Atom*> ChildrenIn(const particles::Atom& atom, int group)
{
	std::vector<const particles::Atom*> children;
	for (const auto& sub : atom.subCollections)
	{
		if (sub->group == group)
		{
			for (const auto& child : sub->atoms)
			{
				children.push_back(child.get());
			}
		}
	}
	return children;
}

/// `count` turns of the effect, each followed by the particle sounds' turn, as the game runs them
void StepTurns(particles::Effect& effect, int count)
{
	for (int i = 0; i < count; ++i)
	{
		effect.Step(0.1f);
		audio::spell_sounds::ProcessTurn(0.1f);
	}
}

/// The forest's effect over a flat land at height 0 (the seed's collision with the land reads it) and the null "game"
/// logger; the land is put back when the test ends, and no particle sound is left over, also when an ASSERT stops a
/// test. A test that listens to the sounds sets them up itself
class ForestMiracleEffects: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2 /*xz*/) { return 0.0f; });
	}
	void TearDown() override { audio::spell_sounds::Clear(); }

private:
	test::RestoreService<Locator::terrainSystem> _restoreLand;
};
} // namespace

/// With OPENBLACK_GAME_PATH set to the install: what the game's SF_Forest gives the seed, the sparkles, the vortices,
/// the flocks and the light on the land
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST_F(ForestMiracleEffects, realDataSFForest)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto file = LoadForestFile(std::filesystem::path(game));
	ASSERT_TRUE(file.has_value());
	ExpectForestRules(*file);
	ExpectForestLightMap(*file);
}

/// The same reads on the synthetic twins of the game's file
TEST_F(ForestMiracleEffects, realDataSFForestSynthetic)
{
	const auto rules = particles::File::Parse(k_ForestTwin, "SF_Forest");
	ASSERT_TRUE(rules.has_value());
	ExpectForestRules(*rules);
	const auto light = particles::File::Parse(k_ForestLightMap, "SF_Forest");
	ASSERT_TRUE(light.has_value());
	ExpectForestLightMap(*light);
}

// The seed is made 9.435 m over the cast point with SOUND_SPELL_FOREST_1, played at once, neither looping nor released
// softly. It falls by 1.6 m/s a second, never faster than 3.34513 m/s, and meets the land on the effect's 41st turn
// (4.1 s), where it goes and tells the spell it landed. The next sound turn cuts its sound: a stop, sent once
// (docs/wiki/magic/forest.md#what-is-seen-the-sf_forest-effect; docs/wiki/audio/magic-and-weather-sounds.md, the
// lifetime of the particle sound; SF_Forest's CreateRuleAnAtom_Seed, UpdateRuleGravity_Seed and LandscapeCollide_Seed)
TEST_F(ForestMiracleEffects, TheSeedsSoundStartsWithItAndIsCutWhenItLandsOnTheFortyFirstTurn)
{
	const ForestSoundActions soundActions;
	ASSERT_EQ(particles::SoundActionByName("SOUND_SPELL_FOREST_1"), k_SeedSound);
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& heard = static_cast<HeardAnimEffects&>(Locator::audio::emplace<HeardAnimEffects>());
	// no particle sound left from before
	audio::spell_sounds::Clear();
	const auto file = ParseTwin(k_ForestTwin);
	ASSERT_NE(file, nullptr);
	{
		// the sink outlives the effect that points to it
		LandingSink sink;
		particles::Effect effect(file, k_CastPoint, 1.0f);
		effect.SetSink(&sink);
		StepTurns(effect, 1);
		const auto seeds = AtomsOf(effect, "ParticlePointCreator_Seed");
		ASSERT_EQ(seeds.size(), 1u);
		EXPECT_FLOAT_EQ(seeds[0]->position.y, 9.435f);
		ASSERT_EQ(seeds[0]->sounds.size(), 1u);
		const auto& action = seeds[0]->sounds.front()->action;
		EXPECT_EQ(action.action, k_SeedSound);
		EXPECT_EQ(action.flags, uint8_t {0});
		EXPECT_EQ(heard.ActionsOf(k_SeedSound), std::vector<audio::AnimAction> {audio::AnimAction::Play});
		// forty turns: still falling, its sound still with it
		StepTurns(effect, 39);
		ASSERT_EQ(AtomsOf(effect, "ParticlePointCreator_Seed").size(), 1u);
		EXPECT_EQ(sink.landings, 0);
		EXPECT_EQ(heard.ActionsOf(k_SeedSound).size(), 1u);
		// the 41st: it meets the land
		effect.Step(0.1f);
		EXPECT_TRUE(AtomsOf(effect, "ParticlePointCreator_Seed").empty());
		EXPECT_EQ(sink.landings, 1);
		EXPECT_EQ(sink.shieldChecks, 0);
		audio::spell_sounds::ProcessTurn(0.1f);
		const auto actions = heard.ActionsOf(k_SeedSound);
		ASSERT_EQ(actions.size(), 2u);
		EXPECT_EQ(actions.back(), audio::AnimAction::Stop);
		// cut once only
		audio::spell_sounds::ProcessTurn(0.1f);
		EXPECT_EQ(heard.ActionsOf(k_SeedSound).size(), 2u);
	}
	audio::spell_sounds::Clear();
}

// At 3.5 s the camera particle gains the sparkle root, which plays SOUND_SPELL_FOREST_4 once, the vortices and a burst
// of ten sparkles, and the root then emits sparkles of its own. Every sparkle leaves within 1.002 rad of straight up and
// rises, and each vortex turns about its upright axis (docs/wiki/magic/forest.md#what-is-seen-the-sf_forest-effect;
// SF_Forest's AddSubCollectionsToAtom_Sparkles, CreateRuleAnAtom_SparkleRoot, EmitterRuleConical_Sparkle,
// EmitterRuleConical_InitSparkle and the vortices' rules)
TEST_F(ForestMiracleEffects, AtThreeAndAHalfSecondsTheSparklesBurstWithTheirSoundAndRiseBesideSpinningVortices)
{
	const ForestSoundActions soundActions;
	ASSERT_EQ(particles::SoundActionByName("SOUND_SPELL_FOREST_4"), k_SparkleSound);
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& heard = static_cast<HeardAnimEffects&>(Locator::audio::emplace<HeardAnimEffects>());
	audio::spell_sounds::Clear();
	const auto file = ParseTwin(k_ForestTwin);
	ASSERT_NE(file, nullptr);
	{
		particles::Effect effect(file, k_CastPoint, 1.0f);
		StepTurns(effect, 35);
		EXPECT_TRUE(AtomsOf(effect, "ParticlePointCreator_SparkleRoot").empty());
		EXPECT_TRUE(AtomsOf(effect, "ParticlePointCreator_InitSparkle").empty());
		EXPECT_TRUE(AtomsOf(effect, "ParticlePointCreator_Vortex").empty());
		EXPECT_TRUE(heard.ActionsOf(k_SparkleSound).empty());
		// by 3.7 s
		StepTurns(effect, 2);
		const auto roots = AtomsOf(effect, "ParticlePointCreator_SparkleRoot");
		ASSERT_EQ(roots.size(), 1u);
		ASSERT_EQ(roots[0]->sounds.size(), 1u);
		const auto& action = roots[0]->sounds.front()->action;
		EXPECT_EQ(action.action, k_SparkleSound);
		EXPECT_EQ(action.flags, uint8_t {0});
		EXPECT_EQ(heard.ActionsOf(k_SparkleSound), std::vector<audio::AnimAction> {audio::AnimAction::Play});
		// 4.8 s: the burst is out, ten in all and none gone yet, and the root's sparkles too; all of them rise
		StepTurns(effect, 11);
		const auto burst = AtomsOf(effect, "ParticlePointCreator_InitSparkle");
		EXPECT_EQ(burst.size(), 10u);
		for (const auto* spark : burst)
		{
			EXPECT_GT(spark->velocity.y, 0.0f);
			EXPECT_GT(spark->position.y, k_CastPoint.y);
		}
		const auto sparkles = AtomsOf(effect, "ParticlePointCreator_Sparkle");
		EXPECT_FALSE(sparkles.empty());
		for (const auto* spark : sparkles)
		{
			EXPECT_GT(spark->velocity.y, 0.0f);
		}
		// the vortices, at most six, each turning about its upright axis from one turn to the next
		const auto vortices = AtomsOf(effect, "ParticlePointCreator_Vortex");
		ASSERT_FALSE(vortices.empty());
		EXPECT_LE(vortices.size(), 6u);
		const auto* vortex = vortices.front();
		const glm::mat3 before = vortex->rotation;
		StepTurns(effect, 1);
		const auto after = AtomsOf(effect, "ParticlePointCreator_Vortex");
		ASSERT_NE(std::ranges::find(after, vortex), after.end());
		EXPECT_NE(vortex->rotation, before);
		EXPECT_FLOAT_EQ(vortex->rotation[1][1], 1.0f);
		// the sparkles' sound was played once
		EXPECT_EQ(heard.ActionsOf(k_SparkleSound).size(), 1u);
	}
	audio::spell_sounds::Clear();
}

// At 5.5 s the camera particle gains the flocks' root and with it five flocks of ten. SOUND_SPELL_FOREST_3 plays once,
// with the first flock only, as the original's sphere rule plays its sound with its first atom
// (docs/wiki/audio/magic-and-weather-sounds.md, who plays on creation; SF_Forest's AddSubCollectionsToAtom_Butterflies
// and CreateRuleSphere_ButterflyGroups)
TEST_F(ForestMiracleEffects, AtFiveAndAHalfSecondsTheFiveFlocksComeWithOneSound)
{
	const ForestSoundActions soundActions;
	ASSERT_EQ(particles::SoundActionByName("SOUND_SPELL_FOREST_3"), k_FlockSound);
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& heard = static_cast<HeardAnimEffects&>(Locator::audio::emplace<HeardAnimEffects>());
	audio::spell_sounds::Clear();
	const auto file = ParseTwin(k_ForestTwin);
	ASSERT_NE(file, nullptr);
	{
		particles::Effect effect(file, k_CastPoint, 1.0f);
		StepTurns(effect, 55);
		EXPECT_TRUE(InGroup(AtomsOf(effect, "ParticlePointCreator_Butt"), 6).empty());
		EXPECT_TRUE(heard.ActionsOf(k_FlockSound).empty());
		// by 5.7 s
		StepTurns(effect, 2);
		const auto flocks = InGroup(AtomsOf(effect, "ParticlePointCreator_Butt"), 6);
		ASSERT_EQ(flocks.size(), 5u);
		EXPECT_EQ(std::ranges::count_if(flocks, [](const particles::Atom* flock) { return !flock->sounds.empty(); }), 1);
		const auto sounding = std::ranges::find_if(flocks, [](const particles::Atom* flock) { return !flock->sounds.empty(); });
		ASSERT_NE(sounding, flocks.end());
		ASSERT_EQ((*sounding)->sounds.size(), 1u);
		const auto& action = (*sounding)->sounds.front()->action;
		EXPECT_EQ(action.action, k_FlockSound);
		EXPECT_EQ(action.flags, uint8_t {0});
		EXPECT_EQ(heard.ActionsOf(k_FlockSound), std::vector<audio::AnimAction> {audio::AnimAction::Play});
		for (const auto* flock : flocks)
		{
			EXPECT_EQ(ChildrenIn(*flock, 7).size(), 10u);
		}
	}
	audio::spell_sounds::Clear();
}

// A flock's butterflies fade in over their first 4 s, from nothing to full, stay full, and fade out from 12 to 14 s of
// their age; the flocks go at 16 s, their butterflies with them (docs/wiki/magic/forest.md
// #what-is-seen-the-sf_forest-effect; SF_Forest's AR_FadeAlpha_ButterflyIn, AR_FadeAlpha_ButterflyOut and
// RemoveRuleOldAgeOnly_ButterflyGroups)
TEST_F(ForestMiracleEffects, TheFlocksFadeInOverFourSecondsAndOutFromTwelveToFourteen)
{
	const auto file = ParseTwin(k_ForestTwin);
	ASSERT_NE(file, nullptr);
	particles::Effect effect(file, k_CastPoint, 1.0f);
	StepTurns(effect, 57);
	const auto flocks = InGroup(AtomsOf(effect, "ParticlePointCreator_Butt"), 6);
	ASSERT_FALSE(flocks.empty());
	const auto butterflies = ChildrenIn(*flocks.front(), 7);
	ASSERT_FALSE(butterflies.empty());
	const auto* butterfly = butterflies.front();
	const auto alpha = [butterfly]() { return static_cast<int>(butterfly->colour[3]); };
	// steps on until the butterfly is that old; each step's fade reads its age a turn before
	const auto ageTo = [&effect, butterfly](float age) {
		while (effect.AtomAge(*butterfly) < age)
		{
			StepTurns(effect, 1);
		}
	};
	// made this turn, faded in from nothing
	EXPECT_EQ(alpha(), 0);
	// half way in, give or take a turn (6.4 a turn)
	ageTo(2.05f);
	EXPECT_NEAR(alpha(), 127, 7);
	ageTo(4.25f);
	EXPECT_EQ(alpha(), 255);
	ageTo(11.95f);
	EXPECT_EQ(alpha(), 255);
	// half way out, give or take a turn
	ageTo(13.05f);
	EXPECT_NEAR(alpha(), 127, 7);
	ageTo(14.25f);
	EXPECT_EQ(alpha(), 0);
	// 16.75 s: the flocks have gone
	StepTurns(effect, 25);
	EXPECT_TRUE(InGroup(AtomsOf(effect, "ParticlePointCreator_Butt"), 6).empty());
}

// The light on the land: one light map atom at the cast point once the effect is 3.5 s old, played through the nine
// frames of Forest_LMap.raw at 15 a second, looped, and kept as long as the effect lasts, as no rule takes it away
// (docs/wiki/magic/forest.md#what-is-seen-the-sf_forest-effect; SF_Forest's CreateRuleAnAtom_LightMap and
// ParticleLightMapCreator0)
TEST_F(ForestMiracleEffects, AnAnimatedLightLiesOnTheLandFromThreeAndAHalfSecondsForTheEffectsLife)
{
	// no file system, whatever an earlier test left: the light's bitmap is not read and the atom is followed alone
	const test::RestoreService<Locator::filesystem> restoreFileSystem;
	Locator::filesystem::reset();
	const auto file = ParseTwin(k_ForestLightMap);
	ASSERT_NE(file, nullptr);
	particles::Effect effect(file, k_CastPoint, 1.0f);
	const auto lights = [&effect]() { return AtomsOf(effect, "ParticleLightMapCreator0", particles::Creator::Kind::Other); };
	for (int turn = 0; turn < 35; ++turn)
	{
		effect.Step(0.1f);
	}
	EXPECT_TRUE(lights().empty());
	// by 3.7 s
	for (int turn = 35; turn < 37; ++turn)
	{
		effect.Step(0.1f);
	}
	const auto made = lights();
	ASSERT_EQ(made.size(), 1u);
	const auto* light = made.front();
	EXPECT_EQ(light->position, k_CastPoint);
	ASSERT_NE(light->creator, nullptr);
	EXPECT_EQ(light->creator->numFrames, 9);
	EXPECT_TRUE(light->creator->loopAnim);
	EXPECT_TRUE(light->playAnim);
	EXPECT_FLOAT_EQ(light->frameRate, 15.0f);
	// a frame and a half a turn: the frame drawn moves every turn and comes back every sixth
	std::vector<int> frames;
	for (int turn = 37; turn < 49; ++turn)
	{
		effect.Step(0.1f);
		frames.push_back(graphics::frame_anim::ParticleFrameIndex(light->frame, 9, true));
	}
	for (size_t i = 0; i + 1 < frames.size(); ++i)
	{
		EXPECT_NE(frames[i], frames[i + 1]) << "turn " << i;
	}
	for (size_t i = 0; i + 6 < frames.size(); ++i)
	{
		EXPECT_EQ(frames[i], frames[i + 6]) << "turn " << i;
	}
	// a minute on, still the one light
	for (int turn = 49; turn < 600; ++turn)
	{
		effect.Step(0.1f);
	}
	const auto later = lights();
	ASSERT_EQ(later.size(), 1u);
	EXPECT_EQ(later.front(), light);
}
