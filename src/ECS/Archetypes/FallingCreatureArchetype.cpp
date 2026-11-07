/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FallingCreatureArchetype.h"

#include <algorithm>
#include <utility>

#include <glm/vec3.hpp>

#include "3D/ObjectMatrix.h"
#include "Creature/CreatureMorph.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/FallingCreature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
/// The copy takes the creature's own component, when it has one
template <typename Component>
void CopyFrom(ecs::Registry& registry, entt::entity source, entt::entity copy)
{
	if (const auto* component = std::as_const(registry).TryGet<const Component>(source); component != nullptr)
	{
		registry.AssignState<Component>(copy, *component);
	}
}
} // namespace

std::optional<entt::entity> FallingCreatureArchetype::Create(entt::entity source)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(source) || !std::as_const(registry).AllOf<Creature, Mesh>(source))
	{
		return std::nullopt;
	}
	const auto& creature = std::as_const(registry).Get<const Creature>(source);
	// a fresh body of the creature's species, given its size held within the limits a body is drawn at, and the
	// alignment its body is to show, held to -1 .. 1
	const auto shown = ecs::creature_pose::TargetMorph(std::as_const(registry), source).evilGood;
	const auto alignment = std::clamp(shown, -1.0f, 1.0f);
	const auto copy = registry.Create();
	registry.AssignState<FallingCreature>(copy, FallingCreature {
	                                                .source = source,
	                                                .species = creature.species,
	                                                .size = creature_morph::ClampScale(creature.size),
	                                                .alignment = alignment,
	                                                .filmTimed = false,
	                                                .sparkling = true,
	                                            });
	// its body as the creature's: shape, pose and eyes. (It has no mesh of its own: it is drawn in its creature's, so
	// that nothing that draws or picks meshes finds it)
	CopyFrom<CreatureMorph>(registry, source, copy);
	CopyFrom<CreatureAnimation>(registry, source, copy);
	CopyFrom<CreatureEyes>(registry, source, copy);
	// a body of its own, made afresh: it stands, plays nothing and looks nowhere, its head straight, and its eyes look
	// nowhere either and blink from the start
	if (auto* pose = registry.TryGet<CreatureAnimation>(copy); pose != nullptr)
	{
		pose->body = {};
		pose->slots.clear();
		pose->face = {};
		pose->gesture = {};
		pose->wobble = {};
		pose->lookAt.reset();
		pose->yaw = {};
		pose->pitch = {};
		pose->playbackScale = 1.0f;
		pose->drawnBones.clear();
	}
	if (auto* eyes = registry.TryGet<CreatureEyes>(copy); eyes != nullptr)
	{
		eyes->lookAt.reset();
		eyes->blink = {};
	}
	// and hair of its own, loaded with the body, its strands not started yet
	registry.AssignState<CreatureHair>(copy);
	// at the world's origin, facing heading 0, at the creature's scale
	if (const auto* transform = std::as_const(registry).TryGet<const Transform>(source); transform != nullptr)
	{
		registry.AssignState<Transform>(copy, Transform {
		                                          .position = glm::vec3(0.0f),
		                                          .rotation = affine::AngleY(0.0f),
		                                          .scale = transform->scale,
		                                      });
	}
	return copy;
}
