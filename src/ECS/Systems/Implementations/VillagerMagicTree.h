/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager bewildered by the trees of a forest miracle (a magic tree's reaction): it goes from tree to tree of the
/// forest in the forest's order, from one picked at random, walking to the tree it looked at last, turning to the next
/// and looking at it, until it has seen about half to all of them (ecs::magic_tree_watch, ECS/MagicTreeWatchRules.h).
/// The state functions are the living action system's rows 149 to 152. Wiki:
/// docs/wiki/beings/villager-reactions.md#a-magic-tree-reaction-8.
namespace openblack::ecs::villager_magic_tree
{

/// The take-up (the slot type's setup): the reaction and its state, then the tree as its object
void SetupReactToMagicTree(entt::entity villager, entt::entity tree, uint32_t reaction);
/// The table's own priority for a magic tree, whatever the villager and the tree
[[nodiscard]] uint8_t ReactToMagicTreePriority(entt::entity villager, uint32_t reaction, uint32_t other);

/// Every state below starts the same way: the tree gone or not available, it stops reacting and goes back to what it
/// was doing. Each returns 1.
/// 149 INITIALISE_BEWILDERED_BY_MAGIC_TREE_REACTION: a tree that is not a forest miracle's stops it; else the first
/// tree is drawn from all but the last of the forest's, none looked at yet
uint32_t InitialiseBewilderedByMagicTreeReaction(components::LivingAction& action);
/// 150 PERFORM_BEWILDERED_BY_MAGIC_TREE_REACTION: a tree that is not a forest miracle's stops it; while it has looked
/// at fewer trees than all of them less a fresh draw below half of them, the next tree becomes its object, it walks to
/// the edge nearest it of the tree it looked at last, and its look of two seconds plus a draw below three is set; else
/// it stops reacting
uint32_t PerformBewilderedByMagicTreeReaction(components::LivingAction& action);
/// 151 TURN_TO_FACE_MAGIC_TREE: it turns to the tree and, once facing it, looks at it (152)
uint32_t TurnToFaceMagicTree(components::LivingAction& action);
/// 152 LOOK_AT_MAGIC_TREE: half way through the look its clip is chosen again (it looks about); at the look's end it
/// goes on to the next tree (150)
uint32_t LookAtMagicTree(components::LivingAction& action);

} // namespace openblack::ecs::villager_magic_tree
