/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

// A tree's scale over its life (Tree.cpp of the original, 0x749E00..0x74A7C0): the growing flag and countdown its
// constructor sets, Tree::Grow, the forest miracle's decay, and its deletion (Tree::ToBeDeleted, with the MagicTree
// override). The scale is the Transform's; components::Tree::maxSize is the scale it grows to (+0x64). The natural
// growth over time (Tree::Process's Grow, Forest::Process's new trees) is postponed: only the forest miracle grows trees
// yet. Wiki: docs/bw1-notes/magic.md ("Bosque").

namespace openblack::ecs::trees
{
/// The Tree ctor 0x749E00's growth part: maxScale != scale sets the growing flag (+0x5E bit 0) and the countdown +0x60 =
/// GameRand(info.growsAfterNumGameTurns). TreeArchetype::Create calls it.
void InitGrowth(entt::entity tree);

/// Object::SetScale (vt 0x124) / SetJustScale (vt 0x51C): the Transform's scale and the Fixed bounding circle
void SetScale(entt::entity tree, float scale);
[[nodiscard]] float GetScale(entt::entity tree);

/// Tree::Grow 0x74A3F0 (amount, setScale, raiseMax): with raiseMax, maxScale = max(maxScale, scale + amount); nothing when
/// the scale is already maxScale; else the scale goes to min(scale + amount, maxScale) (SetScale with setScale, else
/// SetJustScale and the 3D matrix). Returns how much it grew.
float Grow(entt::entity tree, float amount, bool setScale, bool raiseMax);

/// fn_0074A3A0 (the forest miracle's decay): scale - amount; at 0 or below the tree is deleted (ToBeDeleted) and it
/// returns 0, else SetScale and it returns amount
float Shrink(entt::entity tree, float amount);

/// Tree::Process 0x74A290 (vt 0x5FC, Forest::Process for the growing list): the countdown; at 0 it restarts at
/// growsAfterNumGameTurns and, while growing below maxScale, the natural growth step (postponed: not ported). 1 = still
/// growing, 0 = done (the forest moves it to its grown list).
[[nodiscard]] bool Process(entt::entity tree);

/// Tree::ToBeDeleted (vt 0xC): MagicTree 0x5FD070 for a magic tree (Magic/Objects/MagicTree), else the base 0x74A210
void ToBeDeleted(entt::entity tree);
/// Tree::ToBeDeleted 0x74A210: out of its forest (fn_0053A220), then Object::ToBeDeleted 0x636670 (the entity goes)
void BaseToBeDeleted(entt::entity tree);
} // namespace openblack::ecs::trees
