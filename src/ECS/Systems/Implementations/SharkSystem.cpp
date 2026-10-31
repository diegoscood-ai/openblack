/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SharkSystem.h"

#include <cstdint>

#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SharkRules.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::systems;
using components::DrawPosition;
using components::Shark;
using components::Transform;

void SharkSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<Shark, const Transform>([](Shark& shark, const Transform& transform) {
		// the turn's start = Pos
		shark.turnStart = transform.position;
	});
	// (the walk path system's turn is a later step of the turn: Game::GameLogicLoop)
}

void SharkSystem::Update(std::chrono::duration<float, std::milli> gameTime, float turnFraction)
{
	if (!Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& island = Locator::terrainSystem::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const float f = turnFraction;
	// the frame's whole game ms, game_clock::FrameGameMs from Game
	const auto frameMilliseconds = static_cast<int32_t>(gameTime.count());
	std::vector<entt::entity> undrawn;
	registry.Each<Shark, Transform>([&](entt::entity entity, Shark& shark, Transform& transform) {
		const auto pos = transform.position;
		const auto prev = shark.turnStart;
		// the heading of the move (pos - prevPos) * 10 / 65536 with y = 0; the last one while it stands still
		shark.heading = shark_rules::Heading(prev, pos, shark.heading);
		// between the start and the end of the turn, each end at GetAltitude + relY
		const float y0 = island.GetHeightAt(glm::vec2(prev.x, prev.z));
		const float y1 = island.GetHeightAt(glm::vec2(pos.x, pos.z));
		const auto drawn = shark_rules::Drawn(prev, y0, pos, y1, f);
		// the object's position (P, heading, scale): RotateY(heading), the same turn as at the creation
		transform.rotation = affine::AngleY(shark.heading);
		if (auto* draw = registry.TryGet<DrawPosition>(entity); draw != nullptr)
		{
			draw->position = drawn;
			draw->rotation = transform.rotation;
			draw->shearX = 0.0f;
			draw->shearZ = 0.0f;
		}
		else
		{
			undrawn.push_back(entity);
		}

		// the wake: the translation of the bone matrix of EBone[0]'s bone x the EBone point. The matrices are the ones the
		// shark's own cut draw just filled: the clip's frames blended and each bone multiplied by its parent, the root by
		// the object's world matrix (position, heading and scale), so it is world space; no camera matrix is involved.
		// Here: model x this frame's pose (model space bones) x the point, the same product.
		const auto* mesh = registry.TryGet<const components::Mesh>(entity);
		if (mesh == nullptr || !meshes.Contains(mesh->id))
		{
			return;
		}
		const auto l3d = meshes.Handle(mesh->id);
		const auto& point = l3d->GetEBonePoint0();
		if (!point.has_value())
		{
			return;
		}
		const auto* animation = registry.TryGet<const components::SkeletalAnimation>(entity);
		const auto& bones = animation != nullptr && animation->pose.size() == l3d->GetBoneMatrices().size()
		                        ? animation->pose
		                        : l3d->GetBoneMatrices();
		const auto bone = point->first < bones.size() ? bones[point->first] : glm::mat4(1.0f);
		const auto model = affine::Model(drawn, transform.rotation, transform.scale);
		// A full pool (1024) makes no ring; the timer goes on all the same
		const auto ring = shark_rules::WakeRing(_wakeTimer, glm::vec3(model * bone * glm::vec4(point->second, 1.0f)),
		                                        shark.heading, frameMilliseconds);
		if (ring.has_value())
		{
			Locator::waterRingSystem::value().Add(*ring);
		}
	});
	for (const auto entity : undrawn)
	{
		const auto& transform = registry.Get<const Transform>(entity);
		const auto position = transform.position;
		const auto rotation = transform.rotation;
		auto& draw = registry.Assign<DrawPosition>(entity);
		draw.turnStart = position;
		draw.started = true;
		draw.position = position;
		draw.rotation = rotation;
	}
}
