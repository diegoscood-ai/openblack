/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/CreatureBody.h"

namespace openblack::ecs::systems
{

/// Brings the creatures' bodies to life: their shape follows what each creature has become, they are posed by their
/// animations, and their eyes look about and blink (see components::CreatureMorph, CreatureAnimation, CreatureEyes)
class CreatureAnimationSystemInterface
{
public:
	virtual ~CreatureAnimationSystemInterface() = default;

	/// Once a game turn: the fatness each body shows follows its creature's a step
	virtual void ProcessTurn() = 0;
	/// Once a frame, by the game time, which stops while the game is paused: the bodies are reshaped where they have
	/// changed enough, posed, and their eyes placed
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;
	/// What each creature's body is set to play at the end of the turn, by creature
	using TurnInputs = std::vector<std::pair<entt::entity, components::CreatureAnimationInputs>>;
	/// At the end of a game turn: each body's pose becomes its pose for the turn (components::CreatureTurnPose), in the
	/// world. The body is posed as it plays `inputs` at the turn's end (a creature without any plays what it is set to),
	/// with its face, gestures and head as the frames left them, and nothing is moved on. A body that has not been posed
	/// yet gets none. The original can pose a body more than once in a turn, and each pose moves the current one to the
	/// previous; only this end-of-turn pose is made so far
	virtual void PoseTurn(const TurnInputs& inputs) = 0;

	/// Where a bone of a creature is in one of its animations at a time, played left to right or not, in its mesh's space,
	/// the animation blended as the body is drawn; nothing when the creature has no such animation or bone, or hasn't been
	/// posed yet
	[[nodiscard]] virtual std::optional<glm::vec3> BoneInAnimation(entt::entity creature, size_t animation, float timeMs,
	                                                               uint32_t bone, bool mirrored) = 0;
	/// How long one of a creature's animations lasts, in milliseconds, if it has it
	[[nodiscard]] virtual std::optional<float> AnimationDuration(entt::entity creature, size_t animation) = 0;
};

} // namespace openblack::ecs::systems
