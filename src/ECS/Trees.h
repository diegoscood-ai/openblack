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

#include <entt/fwd.hpp>

namespace openblack::ecs
{

/// Tree::Process 0x74A290, once per game turn: a tree that belongs to a forest and has not reached its maximum size
/// grows every growTurns turns. Only forests process their trees in the original (Forest::Process 0x539DA0), so the
/// script's forest-less trees (every tree of Land 1 and Land 2) never grow.
void ProcessTreesTurn(uint32_t turn);

/// Per frame (Tree::PreDraw 0x74A7C0 and the tail of Tree::Draw 0x74B111): the brightness of every tree this frame and
/// the leaf rustle of the tall ones next to the camera.
void UpdateTrees(float seconds);

/// Tree::PreDraw 0x74A883, the global 0xC22FA0 / 256: every RGB channel of a tree's colour is multiplied by this,
/// 200/256 (looking against the light) to 255/256. Only trees use it.
[[nodiscard]] uint8_t TreeBrightness();

/// Tree::Grow 0x74A3F0: grows by `amount` up to maxSize (raising maxSize first when `raiseMax`), and re-registers the
/// obstacle circle like the original's SetScale. Returns how much it actually grew.
float GrowTree(entt::entity tree, float amount, bool raiseMax);

/// The forest a town's replanted trees join (the original keeps a list of forests per town, Town +0x608, and
/// Tree::EndPhysics joins the last of them). Creates one the first time the town needs it.
uint32_t TownForestId(uint32_t townId);

/// The id for a brand new forest (`new Forest(pos, 0)` in Tree::EndPhysics): one past the highest in use.
uint32_t NewForestId();

/// Whether that forest id means "in a forest" (the scripts use 0 and -1 for scenic trees).
[[nodiscard]] bool IsInForest(uint32_t forestId);

/// On map load: the towns' forests go with the map.
void ClearForests();

} // namespace openblack::ecs
