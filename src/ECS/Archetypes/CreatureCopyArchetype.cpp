/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCopyArchetype.h"

#include <utility>

#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureMorph.h"
#include "Creature/TattooEditor.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
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

std::optional<entt::entity> CreatureCopyArchetype::Create(entt::entity source, const CavePlaces& places)
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
	// the copy is given the creature's size, held within the limits a body is drawn at
	const auto size = creature_morph::ClampScale(creature.size);
	// the coin, the first moment's sparkles, then the rest's flag, on the local stream; the body's bones are its
	// skeleton's
	const auto* animation = std::as_const(registry).TryGet<const CreatureAnimation>(source);
	const auto bones = animation != nullptr ? static_cast<int32_t>(animation->skeleton.parents.size()) : 0;
	const auto rest = creature_tattoo_editor::DrawCopyRest(size, creature.alignment, bones,
	                                                       [](int32_t n) { return game_random::LocalRand(n); });
	const auto place = creature_tattoo_editor::CavePlace(places.first, places.third, size, rest.sleeping);
	const auto heading = creature_tattoo_editor::CopyHeading(place, places.fire);

	const auto copy = registry.Create();
	registry.AssignState<CreatureCopy>(copy, CreatureCopy {
	                                             .source = source,
	                                             .species = creature.species,
	                                             .size = size,
	                                             .heading = heading,
	                                             .sleeping = rest.sleeping,
	                                             .restFlag = rest.restFlag,
	                                             .fire = places.fire,
	                                             .alignment = creature.alignment,
	                                         });
	// its body as the creature's: shape, pose and eyes. (It has no mesh of its own: it is drawn in its creature's, so
	// that nothing that draws or picks meshes finds it)
	CopyFrom<CreatureMorph>(registry, source, copy);
	CopyFrom<CreatureAnimation>(registry, source, copy);
	CopyFrom<CreatureEyes>(registry, source, copy);
	// a body of its own, made afresh: it stands, plays nothing and looks nowhere, its head straight, and its eyes look
	// nowhere either and blink from the start (its rest is started as the room is first drawn)
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
		pose->drawnInWorld.clear();
	}
	if (auto* eyes = registry.TryGet<CreatureEyes>(copy); eyes != nullptr)
	{
		eyes->lookAt.reset();
		eyes->blink = {};
	}
	// it stands at its place facing the fire, at the creature's scale
	if (const auto* transform = std::as_const(registry).TryGet<const Transform>(source); transform != nullptr)
	{
		registry.AssignState<Transform>(copy, Transform {
		                                          .position = place,
		                                          .rotation = affine::AngleY(heading),
		                                          .scale = transform->scale,
		                                      });
	}
	return copy;
}
