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

#include "Hand/HandPickUpRules.h"

namespace openblack::ecs
{
class Registry;
}

/// How far below a hand a thing hangs, by its class: the classes of the hold table (hand_pick_up::HoldParametersOf).
/// docs/wiki/hand/grab-hold-and-throw.md#holding-spring-and-throwing
namespace openblack::ecs::hold_lowering
{

/// The hold class of a thing that is not a miracle's seed: a tree or a dead tree, a lump of poo, a mobile object, pot or
/// one-shot orb, a villager or an animal, a static that can be moved, or anything else, the first that fits
[[nodiscard]] hand_pick_up::HoldClass ClassOf(const Registry& registry, entt::entity object);

/// The thing's hold lowering multiplier, which times its height is how far below the hand it hangs
[[nodiscard]] float MultiplierOf(const Registry& registry, entt::entity object);

/// How far below a hand the thing hangs: its height times its hold lowering multiplier
[[nodiscard]] float LoweringOf(const Registry& registry, entt::entity object);

} // namespace openblack::ecs::hold_lowering
