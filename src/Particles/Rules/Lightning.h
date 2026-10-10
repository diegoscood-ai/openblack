/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec3.hpp>

#include "ECS/Effects/EffectValues.h"

// What a lightning fork's tip does when no spell is behind the bolt (a storm's fork strike, a script's lightning strike):
// it applies the weather lightning effect around the tip, instead of sending the spell its "landed" event. Wiki:
// docs/bw1-notes/miracles.md, "Lightning".

namespace openblack
{
class LandIslandInterface;
namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;
} // namespace openblack

namespace openblack::psys
{
class SpellSink;
struct SpellEventInfo;
} // namespace openblack::psys

namespace openblack::psys::lightning
{

/// The effect a tip applies without a spell: the weather lightning row of the effect info (in the game's data burn
/// 10000, alignment 1, radius 1), applied by nobody, for no player and not scaled
[[nodiscard]] ecs::effects::EffectValues NoSpellTipValues(const InfoConstants& info);

/// Where that effect is applied: the tip's x and z in metres (the effect turns them into map coordinates itself) and y
/// the altitude above the island's ground there, as a map position holds it (without an island the ground is 0)
[[nodiscard]] glm::vec3 TipMapPosition(const LandIslandInterface* island, const glm::vec3& tip);

/// A struck tip: with a spell behind the bolt (a sink) the event goes to it and nothing is applied to the map; with
/// none, the values to apply around the tip (NoSpellTipValues), or nothing without the game's info
[[nodiscard]] std::optional<ecs::effects::EffectValues> StrikeTip(SpellSink* sink, const SpellEventInfo& event,
                                                                  const InfoConstants* info);

} // namespace openblack::psys::lightning
