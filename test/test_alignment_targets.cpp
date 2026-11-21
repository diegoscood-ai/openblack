/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The kind an effect's target counts as for alignment (ecs::effects::alignment::TargetOf): a feature's and a patch of
// flowers' come from their own info rows, as every object's comes from its own info in the original; an animal's from
// its kind's row, which moves the player's alignment by that kind's column.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <gtest/gtest.h>

#include "ECS/Components/Alignment.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Life.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/DynamicsFakes.h"
#include "support/RestoreService.h"

using namespace openblack;

TEST(EffectAlignment, AFeatureAndAPatchOfFlowersTakeTheirOwnRowsKind)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	const test::RestoreService<Locator::dynamicsSystem> restoreDynamics;
	auto info = std::make_unique<InfoConstants>();
	// both kinds as info.dat gives them: a feature's and the flowers' rows are FEATURE (9); one made different here so
	// the row is seen to be read
	for (auto& feature : info->feature)
	{
		feature.alignmentType = AlignmentType::Feature;
	}
	info->flowers.at(static_cast<size_t>(FlowersInfo::WhiteOne)).alignmentType = AlignmentType::Feature;
	info->flowers.at(static_cast<size_t>(FlowersInfo::Pink)).alignmentType = AlignmentType::Plant;
	Locator::infoConstants::emplace(*info);
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>(
	    std::make_unique<test::FakeDynamicsWorld>(registry, info.get()));

	const auto feature = registry.Create();
	registry.Assign<ecs::components::Feature>(feature, ecs::components::Feature {.type = static_cast<FeatureInfo>(0)});
	EXPECT_EQ(ecs::effects::alignment::TargetOf(feature, ecs::effects::EffectValues {}).type, AlignmentType::Feature);
	const auto white = registry.Create();
	registry.Assign<ecs::components::Flowers>(white, ecs::components::Flowers {.type = FlowersInfo::WhiteOne});
	EXPECT_EQ(ecs::effects::alignment::TargetOf(white, ecs::effects::EffectValues {}).type, AlignmentType::Feature);
	const auto pink = registry.Create();
	registry.Assign<ecs::components::Flowers>(pink, ecs::components::Flowers {.type = FlowersInfo::Pink});
	EXPECT_EQ(ecs::effects::alignment::TargetOf(pink, ecs::effects::EffectValues {}).type, AlignmentType::Plant);
	// a thing with no info is unimportant
	EXPECT_EQ(ecs::effects::alignment::TargetOf(registry.Create(), ecs::effects::EffectValues {}).type,
	          AlignmentType::Unimportant);
	Locator::dynamicsSystem::reset();
}

TEST(EffectAlignment, HurtingOrHelpingAnAnimalMovesTheAlignmentByItsKindsColumn)
{
	// An animal's kind is its own info row's: info.dat has the sheep ANIMAL_NICE (0) and the lion ANIMAL_NASTY (1).
	// The change is K = |life before - life after| + applyEffectAlignmentChangeAddition, each effect's amount x its
	// alignment row's column x K, then scaled by the alignment A: of A's sign (0 counts as positive) x (1 - |A| / 2),
	// of the other sign x (1 + |A| / 2) (docs/wiki/magic/influence-and-alignment.md#spell-effects)
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	const test::RestoreService<Locator::dynamicsSystem> restoreDynamics;
	auto info = std::make_unique<InfoConstants>();
	auto& sheepRow = info->animal.at(static_cast<size_t>(AnimalInfo::Sheep));
	auto& lionRow = info->animal.at(static_cast<size_t>(AnimalInfo::Lion));
	sheepRow.alignmentType = AlignmentType::AnimalNice;
	lionRow.alignmentType = AlignmentType::AnimalNasty;
	// a burn below their combustion temperature does them no damage
	sheepRow.combustionTemperature = 100.0f;
	lionRow.combustionTemperature = 100.0f;
	// the hit row: hurting a nice animal is evil, a nasty one good; every other column 0
	constexpr auto k_Hit = static_cast<size_t>(ecs::effects::EffectValues::Number::Hit);
	info->alignment.at(k_Hit).animalNice = -0.5f;
	info->alignment.at(k_Hit).animalNasty = 0.25f;
	info->player.applyEffectAlignmentChangeAddition = 0.1f;
	Locator::infoConstants::emplace(*info);
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>(
	    std::make_unique<test::FakeDynamicsWorld>(registry, info.get()));
	const auto makeAnimal = [&registry](AnimalInfo type, float life) {
		const auto animal = registry.Create();
		registry.Assign<ecs::components::Animal>(animal, ecs::components::Animal {.type = type, .age = 13});
		registry.Assign<ecs::components::Life>(animal, ecs::components::Life {.value = life});
		return animal;
	};
	// a hit of 2 that took each from 1 to 0.6: K = 0.4 + 0.1 = 0.5
	ecs::effects::EffectValues hit {};
	hit.numbers.at(k_Hit) = 2.0f;
	const auto sheep = makeAnimal(AnimalInfo::Sheep, 0.6f);
	const auto lion = makeAnimal(AnimalInfo::Lion, 0.6f);
	EXPECT_EQ(ecs::effects::alignment::TargetOf(sheep, hit).type, AlignmentType::AnimalNice);
	EXPECT_EQ(ecs::effects::alignment::TargetOf(lion, hit).type, AlignmentType::AnimalNasty);

	// the sheep: 2 x -0.5 x 0.5 = -0.5, against a neutral alignment (0 counts as good) x 1
	ecs::components::Alignment neutral {};
	ecs::effects::alignment::Update(neutral, sheep, hit, 1.0f);
	EXPECT_FLOAT_EQ(neutral.pending, -0.5f);
	EXPECT_FLOAT_EQ(neutral.value, 0.0f);
	// against a good one of 0.5, x 1.25
	ecs::components::Alignment good {.value = 0.5f};
	ecs::effects::alignment::Update(good, sheep, hit, 1.0f);
	EXPECT_FLOAT_EQ(good.pending, -0.625f);
	// the lion: 2 x 0.25 x 0.5 = 0.25, of the good one's sign, x 0.75
	ecs::components::Alignment alsoGood {.value = 0.5f};
	ecs::effects::alignment::Update(alsoGood, lion, hit, 1.0f);
	EXPECT_FLOAT_EQ(alsoGood.pending, 0.1875f);
	// no change of life, no change
	ecs::components::Alignment unchanged {};
	ecs::effects::alignment::Update(unchanged, sheep, hit, 0.6f);
	EXPECT_FLOAT_EQ(unchanged.pending, 0.0f);
	Locator::dynamicsSystem::reset();
}
