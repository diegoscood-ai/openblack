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

#include <optional>
#include <span>

#include <entt/entity/fwd.hpp>

#include "3D/SnowCover.h"

namespace openblack::ecs
{
class Registry;
}

/// The snow each object of the map shows (components::SnowLevel). Each time the land's draw draws an object in the map,
/// its snow moves a step towards the snow lying under it, at most the frame's game time in ms divided by 16
/// (snow_cover::LevelStep): so the snow on a thing comes and goes over a few seconds, and stays as it was on a thing
/// nothing draws there. A new thing has none. Refitted from raffclar's snow levels (his SnowLevels.cpp)
namespace openblack::ecs::snow_levels
{

/// How much of the snow under it the object takes: a field's crop a quarter, a town's desire flag and a building's
/// needs sign less still, a tree drawn out of the map once half of it and a quarter at most, anything else all of it
[[nodiscard]] snow_cover::SnowHold HoldOf(const Registry& registry, entt::entity object);

/// Notes each tree drawn out of the map this frame: the hand's held tree, a tree a creature holds or a particle
/// carries, and the trees among `physicsDrawn` (the physics objects the frame draws). Each takes less of the snow from
/// then on (HoldOf), also once it stands in the map again
void NoteTreesDrawnOutOfMap(Registry& registry, std::span<const entt::entity> physicsDrawn);

/// One draw of the object where it stands in the map, the snow lying `depth` deep under it: its snow moves towards
/// what that depth gives it by at most `step`. An object with no snow yet is given its level once the step gives it
/// some. Whether its snow changed
bool Step(Registry& registry, entt::entity object, float depth, int32_t step);

/// The snow the object shows, 0 to 255; none when no draw has given it any yet
[[nodiscard]] std::optional<int32_t> LevelOf(const Registry& registry, entt::entity object);

/// The land's draw has drawn the object in the map this frame, which took `frameGameMs` of game time: its snow takes a
/// step towards the snow under the place it is drawn at (Step, with the snow lying on the island now). The drawn
/// instances are written again when it changed
void Drawn(entt::entity object, uint32_t frameGameMs);

/// The snow of broken models (components::BrokenModelSnow), refitted from raffclar's FollowBreak, TakePieceSnow and
/// StepBroken (his SnowLevels.cpp):
/// - A building whose broken model is made from its whole one gives the new model the snow the building shows (none
///   without a level). A blow on a model already broken keeps its snow
void FollowBreak(Registry& registry, entt::entity building);
/// - A building whole again loses its broken model's snow
void ForgetBreak(Registry& registry, entt::entity building);
/// - A piece that broke off `building` starts with its broken model's snow (none without one) and holds it
void TakePieceSnow(Registry& registry, entt::entity piece, entt::entity building);
/// - A piece that lands and does not stick to its building holds its snow no more
void PieceLanded(Registry& registry, entt::entity piece);
/// One draw of a broken model, the snow lying `depth` deep under the place it is drawn at: unless it holds its snow, it
/// moves towards snow_cover::BrokenModelLevel of that depth by at most `step`. Whether it changed
bool StepBrokenModel(Registry& registry, entt::entity object, float depth, int32_t step);
/// The snow a broken model shows, 0 below snow_cover::k_BrokenModelLevelShown; 0 for anything without one
[[nodiscard]] int32_t BrokenModelShownLevelOf(const Registry& registry, entt::entity object);
/// The game has drawn the broken model of the object (a broken building on the screen, or a piece flying or lying in
/// the map) this frame, which took `frameGameMs` of game time: StepBrokenModel with the snow lying on the island now
/// where it is drawn
void BrokenModelDrawn(entt::entity object, uint32_t frameGameMs);

} // namespace openblack::ecs::snow_levels
