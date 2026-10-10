/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "FieldSystem.h"

#include <cmath>

#include <algorithm>
#include <vector>

#include "ECS/Components/Alpha.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Fields.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// A field shows from a quarter of its growing age, with at least a handful of food
constexpr float k_ShowsFrom = 0.25f;
constexpr float k_LeastFood = 25.0f;

[[nodiscard]] bool Shows(const Field& field)
{
	return !(field.growth < k_ShowsFrom * Field::k_AgeGrowth || field.food < k_LeastFood);
}
} // namespace

void FieldSystem::ProcessTurn(uint32_t turn)
{
	// The global lists' first loop: every field's turn, every turn. A snapshot of the fields first, in the registry's
	// order: a field's turn may change the registry. The original walks its list from the head, the newest field first
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> fields;
	registry.Each<const Field>([&fields](entt::entity entity, const Field&) { fields.push_back(entity); });
	for (const auto entity : fields)
	{
		ecs::ProcessField(entity, turn);
	}
}

void FieldSystem::Update(std::chrono::duration<float> frameTime)
{
	const float seconds = frameTime.count();
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<Field, Transform, const Mesh>([&](entt::entity entity, Field& field, Transform& transform, const Mesh& mesh) {
		// a field marked for deletion is not drawn (it is out of the map): nothing to sink or fade
		if (!ecs::IsAvailable(entity))
		{
			return;
		}
		// v = food / 350 - 1, eased over 1 s; y += 2 v scale half height (the mesh's half height read inline, so the mesh
		// level)
		auto* sink = registry.TryGet<PileSink>(entity);
		if (sink == nullptr)
		{
			const float height = ecs::object::MeshHalfHeight(mesh.id);
			sink = &registry.Assign<PileSink>(entity, transform.position.y, height);
		}
		const float v = field.food / Field::k_TotalFood - 1.0f;
		if (std::abs(v - field.sinkTarget) > 1e-4f || !field.sinkStarted)
		{
			field.sinkTarget = v;
			field.sinkStarted = true;
			field.sink.SetDestinationWithSpeedAndTime(v, 0.0f, 1.0f);
		}
		field.sink.Update(seconds);
		float shown = field.sink.value;
		float alpha = 1.0f;
		if (shown < -0.8f)
		{
			// it fades out as it sinks below 80%, held there
			alpha = std::clamp((shown + 0.8f) * 2.0f + 1.0f, 0.0f, 1.0f);
			shown = -0.8f;
		}
		// not drawn at all below a quarter of the growing age or 25 food
		if (!Shows(field))
		{
			alpha = 0.0f;
		}
		// scale x half height x v, doubled
		const float sunk = transform.scale.y * sink->height * shown;
		sink->offset.SetPosition(sunk + sunk);
		transform.position.y = sink->baseY + sink->offset.value;
		auto* fade = registry.TryGet<Alpha>(entity);
		if (alpha < 1.0f && fade == nullptr)
		{
			registry.Assign<Alpha>(entity, alpha);
		}
		else if (alpha < 1.0f)
		{
			fade->value = alpha;
		}
		else if (fade != nullptr)
		{
			registry.Remove<Alpha>(entity);
		}
	});
}

std::optional<field_crop::Look> FieldSystem::GetLook(const Field& field) const
{
	if (!Shows(field))
	{
		return std::nullopt;
	}
	const auto colour = ecs::FieldDrawColour(field);
	return field_crop::Look {
	    .tint = (static_cast<uint32_t>(colour.r) << 16u) | (static_cast<uint32_t>(colour.g) << 8u) | colour.b,
	    .height = field.food / Field::k_TotalFood - 1.0f,
	    .sways = field.growth >= Field::k_AgeRecolt,
	};
}
