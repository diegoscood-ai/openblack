/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The debug state the disciples' marks switch and the help key's readout keep in the debug hooks' store: shown and
// empty in a new store, kept while it lives

#define LOCATOR_IMPLEMENTATIONS

#include <gtest/gtest.h>

#include "ECS/DiscipleMarks.h"
#include "ECS/Systems/Implementations/DebugHooks.h"
#include "Help/HelpKey.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(DebugSwitchState, TheDiscipleMarksShowInANewStoreAndKeepTheirSwitch)
{
	const test::RestoreService<Locator::debugHooks> restoreHooks;
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	Locator::entitiesRegistry::reset();
	Locator::debugHooks::emplace<ecs::systems::DebugHooks>();
	EXPECT_TRUE(ecs::disciple_marks::IsShown());
	ecs::disciple_marks::SetShown(false);
	EXPECT_FALSE(ecs::disciple_marks::IsShown());
	Locator::debugHooks::emplace<ecs::systems::DebugHooks>();
	EXPECT_TRUE(ecs::disciple_marks::IsShown());
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(DebugSwitchState, TheHelpKeyHasNoAnswerInANewStore)
{
	const test::RestoreService<Locator::debugHooks> restoreHooks;
	Locator::debugHooks::emplace<ecs::systems::DebugHooks>();
	EXPECT_TRUE(help::help_key::LastAnswer().empty());
}
