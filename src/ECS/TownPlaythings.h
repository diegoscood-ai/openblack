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

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

/// The things a town's people play with, and the town a new one goes to
namespace openblack::ecs::town_playthings
{

/// A town takes a thing to play with unless one of the same kind it has is still about; the newest comes first. Whether
/// it took it.
bool Add(std::vector<entt::entity>& playthings, entt::entity thing,
         const std::function<bool(entt::entity)>& stillAboutOfSameKind);

/// A town a new plaything might go to
struct Candidate
{
	entt::entity town;
	PlayerNames owner;
	uint32_t id;
	glm::vec3 position;
};
/// The town nearest a point, in the game's whole metres, however far: the players' towns are looked at in turn and the
/// neutral ones last, each player's by id, and a later town is taken only when strictly nearer
[[nodiscard]] std::optional<entt::entity> Nearest(std::span<const Candidate> candidates, glm::vec3 point);

/// A football given to a town, as the script's CREATE gives one to the town nearest it: the town takes it unless a
/// football it has is still about (one that is not to be deleted). Whether it took it
bool GiveBall(Registry& registry, entt::entity town, entt::entity ball);

/// Whether a football is free to be picked up: no town has taken it from a villager's pick-up. Anything that is not a
/// football is not
[[nodiscard]] bool IsBallFree(const Registry& registry, entt::entity ball);
/// A villager's pick-up gives the ball to its town: a ball that already has a town stays with it (true); else the town
/// takes it unless a football it has is still about (GiveBall), and once taken the ball is that town's and no longer
/// free. Whether the ball has a town now
bool AddBallToTown(Registry& registry, entt::entity ball, entt::entity town);

} // namespace openblack::ecs::town_playthings
