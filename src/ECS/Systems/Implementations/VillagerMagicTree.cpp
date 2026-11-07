/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerMagicTree.h"

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/MagicTreeWatch.h"
#include "ECS/Components/Tree.h"
#include "ECS/MagicTreeWatchRules.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerCore.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerAnimate.h"
#include "VillagerReactions.h"

namespace openblack::ecs::villager_magic_tree
{
using components::LivingAction;
using components::MagicTreeWatch;
namespace watch = magic_tree_watch;

namespace
{
/// The seconds of the look's draw, and the seconds taken off it (so two are added) after the draw
constexpr float k_LookRandomSeconds = 3.0f;
constexpr float k_LookTakenOffSeconds = -2.0f;
/// The turning step of the villager's look as it turns to the tree: the normal one
constexpr uint32_t k_TurnLookMode = 1;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The reaction's tree, after the test every state starts with: none when it is gone or not available, and then the
/// villager stops reacting
std::optional<entt::entity> TreeThere(entt::entity villager)
{
	const auto tree = villager_reactions::SlotObject(villager);
	if (tree == entt::null || !Entities().Valid(tree) || !IsAvailable(tree))
	{
		villager_reactions::StopReactingAndSetState(villager);
		return std::nullopt;
	}
	return tree;
}

/// The forest of a forest miracle's tree; none for any other thing, and then the villager stops reacting
std::optional<uint32_t> MagicForestOf(entt::entity villager, entt::entity tree)
{
	const auto& registry = Entities();
	const auto* component = registry.TryGet<const components::Tree>(tree);
	if (component == nullptr || !registry.AllOf<components::MagicTree>(tree))
	{
		villager_reactions::StopReactingAndSetState(villager);
		return std::nullopt;
	}
	return component->forestId;
}

MagicTreeWatch& WatchOf(entt::entity villager)
{
	auto& registry = Entities();
	if (auto* watching = registry.TryGet<MagicTreeWatch>(villager); watching != nullptr)
	{
		return *watching;
	}
	return registry.AssignOrReplaceState<MagicTreeWatch>(villager);
}
} // namespace

void SetupReactToMagicTree(entt::entity villager, entt::entity tree, uint32_t reaction)
{
	// it takes the reaction up, then keeps the tree as what it reacts to
	villager_reactions::AddReaction(villager, reaction, VillagerStates::InitialiseBewilderedByMagicTreeReaction);
	villager_reactions::SetSlotObject(villager, tree);
}

uint8_t ReactToMagicTreePriority([[maybe_unused]] entt::entity villager, [[maybe_unused]] uint32_t reaction,
                                 [[maybe_unused]] uint32_t other)
{
	const auto row = static_cast<size_t>(openblack::Reaction::ReactToMagicTree);
	return static_cast<uint8_t>(Locator::infoConstants::value().reaction.at(row).priority);
}

uint32_t InitialiseBewilderedByMagicTreeReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto tree = TreeThere(villager);
	if (!tree.has_value())
	{
		return 1;
	}
	const auto forest = MagicForestOf(villager, *tree);
	if (!forest.has_value())
	{
		return 1;
	}
	// it starts at a tree of the forest drawn at random, none looked at yet
	const auto trees = static_cast<uint32_t>(ForestTreesGrowingFirst(*forest).size());
	auto& watching = WatchOf(villager);
	watching.next = villager::GameRand(watch::FirstTreeRollRange(trees));
	watching.looked = 0;
	villager_reactions::SetTopState(villager, VillagerStates::PerformBewilderedByMagicTreeReaction);
	return 1;
}

uint32_t PerformBewilderedByMagicTreeReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto tree = TreeThere(villager);
	if (!tree.has_value())
	{
		return 1;
	}
	const auto forest = MagicForestOf(villager, *tree);
	if (!forest.has_value())
	{
		return 1;
	}
	const auto trees = ForestTreesGrowingFirst(*forest);
	const auto count = static_cast<uint32_t>(trees.size());
	auto& watching = WatchOf(villager);
	if (!watch::LooksAtAnother(watching.looked, count, villager::GameRand(watch::SkippedRollRange(count))))
	{
		villager_reactions::StopReactingAndSetState(villager);
		return 1;
	}
	// the next tree becomes what it reacts to (none past the forest's end); it walks to the edge, nearest it, of the
	// tree it looked at last
	const auto next = watching.next < count ? trees.at(watching.next) : entt::entity(entt::null);
	villager_reactions::SetSlotObject(villager, next);
	watching.looked += 1;
	watching.next = watch::NextTree(watching.next, count);
	const auto edge = object::GetNearestPosOfObject(*tree, villager);
	villager::SetupMoveToWithHug(villager, glm::vec2(map_coords::ToMetres(edge.x), map_coords::ToMetres(edge.z)),
	                             VillagerStates::TurnToFaceMagicTree);
	// its look: a draw below three seconds, and two seconds more, the turns left and all of them in the state counter
	const auto roll = villager::GameRand(static_cast<uint32_t>(game_clock::TicksForSeconds(k_LookRandomSeconds)));
	const auto turns = watch::LookTurns(roll, game_clock::TicksForSeconds(k_LookTakenOffSeconds));
	action.turnsUntilStateChange = watch::LookCounter(turns);
	return 1;
}

uint32_t TurnToFaceMagicTree(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto tree = TreeThere(villager);
	if (!tree.has_value())
	{
		return 1;
	}
	if (villager::LookAtPos(villager, town_queries::PosOf(*tree), k_TurnLookMode) == 1)
	{
		villager_reactions::SetTopState(villager, VillagerStates::LookAtMagicTree);
	}
	return 1;
}

uint32_t LookAtMagicTree(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	if (!TreeThere(villager).has_value())
	{
		return 1;
	}
	// half way through the look its clip is chosen again: it stops looking up at the tree and looks about
	if (watch::ChangesLookNow(action.turnsUntilStateChange))
	{
		villager_animate::PlayStateClip(villager, true);
	}
	action.turnsUntilStateChange = watch::CountDown(action.turnsUntilStateChange);
	if (watch::LookOver(action.turnsUntilStateChange))
	{
		villager_reactions::SetTopState(villager, VillagerStates::PerformBewilderedByMagicTreeReaction);
	}
	return 1;
}

} // namespace openblack::ecs::villager_magic_tree
