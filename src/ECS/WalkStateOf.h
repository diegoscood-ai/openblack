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

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/WallHugBookkeeping.h"

namespace openblack::ecs::wall_hug
{
/// The walker's move state as the game numbers it, from its move state tag; none without one
[[nodiscard]] std::optional<WalkState> WalkStateOf(const Registry& registry, entt::entity walker);

/// Keeps the walker's move state from its tags, while it has one, as the state the original keeps after the walk
/// (components::WalkStateAfterWalk)
void KeepWalkState(Registry& registry, entt::entity walker);

/// The walker lets go of the circle it holds and forgets the turns to it and its entry distance, as the circle hug's
/// reset does; it no longer waits for a trial of both ways round, and may have another
void ResetCircleHug(Registry& registry, entt::entity walker);
/// A walker that had its trial of both ways round may have another (a new walk set up, or it leaves its circle); one
/// still waiting for its trial keeps waiting
void ForgetLookaheadTrial(Registry& registry, entt::entity walker);
/// After its turn's walk: a walker handed over to water, the land's edge or a fence waits for its trial of both ways
/// round, unless it had it
void WaitForLookaheadTrial(Registry& registry, entt::entity walker, bool handedOver);

/// What a trial of both ways round copies of a walker (a villager or an animal) to its stand-in: its place, goal and
/// step in whole map units, the step's height part, its game angle and whole speed, the turns to its circle, its entry
/// distance, the circle it holds and its move state
struct TrialCopy
{
	WalkState state;
	glm::ivec2 position;
	glm::ivec2 goal;
	float goalAltitude;
	glm::ivec2 step;
	float stepAltitude;
	uint16_t gameAngle;
	uint16_t speed;
	uint8_t turnsToObj;
	uint32_t entryDistance;
	std::optional<components::WallHugObjectReference> circle;
};

/// Drops these move state tags of the walker, keeping the move state they gave first
template <typename... Tags>
void DropWalkTags(Registry& registry, entt::entity walker)
{
	KeepWalkState(registry, walker);
	registry.Remove<Tags...>(walker);
}
} // namespace openblack::ecs::wall_hug
