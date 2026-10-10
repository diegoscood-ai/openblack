/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>

#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs
{
class Registry;
}
namespace openblack::ecs::components
{
struct CreatureEyes;
struct Transform;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

class CreatureAnimationSystem final: public CreatureAnimationSystemInterface
{
public:
	void ProcessTurn() override;
	void Update(std::chrono::duration<float, std::milli> gameTime) override;
	void PoseTurn(const TurnInputs& inputs) override;
	[[nodiscard]] std::optional<glm::vec3> BoneInAnimation(entt::entity creature, size_t animation, float timeMs, uint32_t bone,
	                                                       bool mirrored) override;
	[[nodiscard]] std::optional<float> AnimationDuration(entt::entity creature, size_t animation) override;

	/// The eyes placed for the frame on the body as it is drawn (ecs::DrawnBodyModel, turned as ecs::creature_pose::
	/// DrawnPlacementOf), posed by `bones`. Only reads the registry
	static void PlaceEyes(const ecs::Registry& registry, entt::entity entity, components::CreatureEyes& eyes,
	                      const creature::CreatureRig::Eyes& rig, const creature_morph::Morph& morph,
	                      const std::vector<glm::mat4>& bones, float size, float seconds);

	/// The body posed for a frame some milliseconds on: its layers moved on (AdvanceLayers), then sampled as it plays
	/// (SampleBody) into its bone matrices. Nothing is posed without the species' stand animation. Its animations play
	/// at the rate of the size it is drawn at (`drawnSize`: the pen's in its pen); the head looks from its own size's
	/// height
	static void PoseBody(components::CreatureAnimation& animation, const creature::CreatureRig& rig,
	                     const creature_morph::Morph& morph, const components::Transform& transform, float size,
	                     float drawnSize, float milliseconds, float seconds);
	/// A frame's step of what the body plays: its action, face, gesture and wobble move on by the milliseconds, and the
	/// head turns towards where it looks
	static void AdvanceLayers(components::CreatureAnimation& animation, const creature::CreatureRig& rig,
	                          const creature_morph::Morph& morph, const components::Transform& transform, float size,
	                          float drawnSize, float milliseconds, float seconds);
	/// The bone matrices of the body playing `body`, or `slots` blended when there are any, with the head turned, the
	/// face, gesture and wobble on top as they stand: nothing is moved on, and only the cache of blended animations may
	/// grow. Empty without the species' stand animation
	[[nodiscard]] static std::vector<glm::mat4> SampleBody(components::CreatureAnimation& animation,
	                                                       const creature::CreatureRig& rig, const creature_morph::Morph& morph,
	                                                       const creature_layers::BodyAction& body,
	                                                       std::span<const components::CreatureAnimation::Slot> slots);
};

} // namespace openblack::ecs::systems
