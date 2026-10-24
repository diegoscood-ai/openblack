/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The kind an effect's target counts as for alignment (ecs::effects::alignment::TargetOf): a feature's and a patch of
// flowers' come from their own info rows, as every object's comes from its own info in the original.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <gtest/gtest.h>

#include "ECS/Components/Feature.h"
#include "ECS/Components/Flowers.h"
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
