/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PotArchetype.h"

#include <glm/gtx/euler_angles.hpp>

#include <algorithm>

#include "3D/L3DMesh.h"

#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "3D/LandIslandInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity PotArchetype::Create(const glm::vec3& position, float yAngleRadians, PotInfo type, int32_t amount, bool allowEmpty)
{
	if (static_cast<int32_t>(type) < 0 || static_cast<int32_t>(type) >= static_cast<int32_t>(PotInfo::_COUNT))
	{
		return entt::null;
	}
	if (amount < 0 || (amount == 0 && !allowEmpty))
	{
		return entt::null;
	}

	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();

	const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(type));

	// MagicFood / MagicWood constructors (0x5FA9F0 / 0x600E20) SetScale 0.3 / 0.7; every other pot is created at 1.
	const float scale = type == PotInfo::MagicFood ? 0.3f : (type == PotInfo::MagicWood ? 0.7f : 1.0f);
	registry.Assign<Transform>(entity, position, glm::mat3(glm::eulerAngleY(-yAngleRadians)), glm::vec3(scale));
	registry.Assign<Pot>(entity, static_cast<uint16_t>(amount), static_cast<uint16_t>(info.maxAmountInPot), type);
	const auto resourceId = resources::HashIdentifier(info.meshId);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(1));
	if (info.potType == PotType::PileFood)
	{
		registry.Assign<MorphWithTerrain>(entity);
	}
	// PileResource::CallVirtualFunctionsForCreation 0x66E300: a new pile starts fully buried (offset = -GetHeight)
	// and SetSize raises it out of the ground over 1 s.
	SetSize(entity, info.potType != PotType::Pot);

	return entity;
}

void PotArchetype::SetSize(entt::entity entity, bool animate)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* pot = registry.TryGet<const Pot>(entity);
	if (pot == nullptr || pot->type == PotInfo::_COUNT || pot->type == PotInfo::HandWood || pot->type == PotInfo::HandFood)
	{
		return; // hand pots keep scale 1 and follow the hand
	}
	const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type));
	auto& transform = registry.Get<Transform>(entity);
	if (info.potType == PotType::Pot)
	{
		// PotStructure::SetSize 0x66D480 -> Pot::GetScaleFromAmount 0x66D4A0 (the only reader of scaleEvery).
		const float every = static_cast<float>(std::max(1u, info.scaleEvery));
		transform.scale = glm::vec3(std::min(5.0f, static_cast<float>(pot->amount) / every + 0.25f));
		registry.SetDirty();
		return;
	}

	// PileResource::SetSize 0x66E900: target offset = (GetProportionRaised - 1) * GetHeight.
	// GetProportionRaised (PileWood 0x66F1B0 / PileFood 0x66EB60): x = amount / maxInPot clamped to [0, 1],
	// p = x > 0 ? 0.05 + 0.95 * x : 0 (an empty pile is fully buried and not drawn); food: 1 - (1 - p)^2.
	const float x = std::clamp(static_cast<float>(pot->amount) / std::max(1.0f, static_cast<float>(info.maxAmountInPot)), 0.0f, 1.0f);
	const float p = x > 0.0f ? 0.05f + 0.95f * x : 0.0f;
	const float proportion = info.resourceType == ResourceType::Food ? 1.0f - (1.0f - p) * (1.0f - p) : p;
	float height = 1.0f;
	auto& meshes = Locator::resources::value().GetMeshes();
	if (const auto* mesh = registry.TryGet<const Mesh>(entity); mesh != nullptr && meshes.Contains(mesh->id))
	{
		height = meshes.Handle(mesh->id)->GetBoundingBox().Size().y * transform.scale.y;
	}
	auto* sink = registry.TryGet<PileSink>(entity);
	if (sink == nullptr)
	{
		// PileResource::SetSize: the pile stands at LH3DIsland::GetAltitude(pos) + offset.
		const float ground = Locator::terrainSystem::has_value()
		                         ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z))
		                         : transform.position.y;
		sink = &registry.Assign<PileSink>(entity, ground);
		sink->offset = -height;
		// PileFood::Draw: only the storage pit food pile (info 2) and magic food (info 10) scroll their grain.
		if (pot->type == PotInfo::StoragePitFoodPile || pot->type == PotInfo::MagicFood)
		{
			registry.Assign<UvScroll>(entity);
		}
	}
	sink->height = height;
	sink->target = (proportion - 1.0f) * height;
	if (!animate)
	{
		sink->offset = sink->target;
		sink->velocity = 0.0f;
		sink->time = 1.0f;
	}
	else
	{
		// Solve [[1/24,1/6,1/2],[1/6,1/2,1],[1/2,1,1]] (c4,c3,c2) = (target - a0 - v0, 0 - v0, 0) for T = 1 s:
		// end position = target, end velocity = 0, end acceleration = 0.
		sink->startOffset = sink->offset;
		sink->startVelocity = sink->velocity;
		sink->time = 0.0f;
		const float r1 = sink->target - sink->startOffset - sink->startVelocity;
		const float r2 = -sink->startVelocity;
		sink->c4 = 72.0f * r1 - 48.0f * r2;
		sink->c3 = -2.0f * r2 - 2.0f * sink->c4 / 3.0f;
		sink->c2 = 2.0f * r2 + sink->c4 / 6.0f;
	}
	// Food piles keep MorphWithTerrain: the renderer passes the sink offset on to the height-map shader.
	transform.position.y = sink->baseY + sink->offset;
	if (auto* scroll = registry.TryGet<UvScroll>(entity); scroll != nullptr)
	{
		scroll->v = 0.25f * (1.0f - std::clamp(sink->offset / std::max(sink->height, 1e-3f) + 1.0f, 0.0f, 1.0f));
	}
	registry.SetDirty();
}

void PotArchetype::UpdateSizes(float seconds)
{
	// PileWood::Draw 0x51BC40 / PileFood::Draw 0x51BF80: t += frame time; past the 1 s duration the offset is the target.
	auto& registry = Locator::entitiesRegistry::value();
	bool dirty = false;
	registry.Each<PileSink, Transform>([&](entt::entity entity, PileSink& sink, Transform& transform) {
		if (sink.time >= 1.0f)
		{
			return;
		}
		sink.time += seconds;
		if (sink.time >= 1.0f)
		{
			sink.time = 1.0f;
			sink.offset = sink.target;
			sink.velocity = 0.0f;
		}
		else
		{
			const float t = sink.time;
			sink.velocity = sink.startVelocity + sink.c2 * t + sink.c3 * t * t / 2.0f + sink.c4 * t * t * t / 6.0f;
			sink.offset = sink.startOffset + sink.startVelocity * t + sink.c2 * t * t / 2.0f + sink.c3 * t * t * t / 6.0f +
			              sink.c4 * t * t * t * t / 24.0f;
		}
		transform.position.y = sink.baseY + sink.offset;
		// fn 0x51C0A3: 0.25 * (1 - clamp(offset / H + 1, 0, 1)) passed to the Game3DObject as its texture V offset.
		if (auto* scroll = registry.TryGet<UvScroll>(entity); scroll != nullptr)
		{
			scroll->v = 0.25f * (1.0f - std::clamp(sink.offset / std::max(sink.height, 1e-3f) + 1.0f, 0.0f, 1.0f));
		}
		dirty = true;
	});
	if (dirty)
	{
		registry.SetDirty();
	}
}
