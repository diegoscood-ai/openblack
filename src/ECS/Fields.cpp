/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Fields.h"

#include <algorithm>

#include <glm/geometric.hpp>

#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "EngineConfig.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs::components;

void openblack::ecs::ProcessFieldsTurn(uint32_t turn)
{
	Locator::entitiesRegistry::value().Each<Field>([turn](Field& field) {
		if ((turn + field.turnOffset) % 10 != 0 || field.crops < Field::k_TimesToSow || field.growth > Field::k_AgeRecolt)
		{
			return;
		}
		// d = 2 (0.5 alignment + 1) x (0.5 growing, 1.5 ripening; 1.5 in the rain). No land alignment or weather
		// yet: alignment 0, dry. Mod world.crops: d times its speed.
		const float multiplier = field.growth < Field::k_AgeGrowth ? 0.5f : 1.5f;
		const float d = 2.0f * (0.5f * 0.0f + 1.0f) * multiplier * Locator::config::value().fieldGrowthMultiplier;
		field.growth += d;
		field.food += d * Field::k_TotalFood / Field::k_AgeRecolt;
	});
}

bool openblack::ecs::IsFieldRipe(entt::entity entity)
{
	const auto* field = Locator::entitiesRegistry::value().TryGet<const Field>(entity);
	return field != nullptr && field->growth >= Field::k_AgeRecolt;
}

uint32_t openblack::ecs::RemoveFieldFood(entt::entity entity, float amount)
{
	auto* field = Locator::entitiesRegistry::value().TryGet<Field>(entity);
	if (field == nullptr || field->food == 0.0f || field->crops < Field::k_TimesToSow)
	{
		return 0;
	}
	const bool ripe = field->growth >= Field::k_AgeRecolt;
	const auto k = static_cast<int>(amount);
	const int cost = ripe ? k : static_cast<int>(1.2f * amount);
	if (static_cast<float>(cost) < field->food)
	{
		field->food -= static_cast<float>(cost);
		return static_cast<uint32_t>(std::max(k, 0));
	}
	// SetTemperature(0) and town+0x5E8 = 1 in the original (no town food system yet)
	if (!ripe)
	{
		field->food = 0.0f;
		return static_cast<uint32_t>(std::max(static_cast<int>(0.2f * amount), 0));
	}
	const auto left = static_cast<uint32_t>(field->food);
	field->food = 0.0f;
	field->crops = 0;
	field->growth = 0.0f;
	return left;
}

void openblack::ecs::UpdateFields(float seconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	registry.Each<Field, Transform, const Mesh>([&](entt::entity entity, Field& field, Transform& transform, const Mesh& mesh) {
		// Mod world.crops, standing in for the farmers (Villager::FarmerPlantsCrop sows a crop at a time): they would
		// take what the hand leaves (a ripe field only clears when asked for more than it has, and the hand's halved,
		// truncated amounts leave the last food unit there for good) and sow the field again. So a ripe field with less
		// than it takes to be drawn (25) is cleared, and an empty field is sown again at once.
		if (Locator::config::value().fieldsWithoutFarmers)
		{
			if (field.growth >= Field::k_AgeRecolt && field.food < 25.0f)
			{
				field.food = 0.0f;
				field.crops = 0;
				field.growth = 0.0f;
			}
			if (field.crops < Field::k_TimesToSow)
			{
				field.crops = Field::k_TimesToSow;
			}
		}
		// v = food / 350 - 1, eased over 1 s; y += 2 v scale height (mesh +0x28, taken as the box height)
		auto* sink = registry.TryGet<PileSink>(entity);
		if (sink == nullptr)
		{
			const float height = meshes.Contains(mesh.id) ? meshes.Handle(mesh.id)->GetBoundingBox().Size().y : 1.0f;
			sink = &registry.Assign<PileSink>(entity, transform.position.y, height);
		}
		// mod world.foliage, fields = wheat: the mesh is only the far view of the plants, whole and tinted by the
		// growth (Foliage::UpdateFields), so it doesn't sink with the food
		const auto& config = Locator::config::value();
		const bool plants = config.foliageFields && config.foliageDensity > 0.0f;
		const float v = plants ? 0.0f : field.food / Field::k_TotalFood - 1.0f;
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
			// alpha byte (+0x4F) = ((v + 0.8) 2 + 1) 255, v held at -0.8
			alpha = std::clamp((shown + 0.8f) * 2.0f + 1.0f, 0.0f, 1.0f);
			shown = -0.8f;
		}
		// not drawn at all below a quarter of the growing age or 25 food
		if (!plants && (field.growth < 0.25f * Field::k_AgeGrowth || field.food < 25.0f))
		{
			alpha = 0.0f;
		}
		// mod world.foliage, fields = wheat: the plants of 3D/Foliage stand in for the mesh (still there for the hand);
		// far away, where the plants shrink into the ground (the last fifth of the draw distance), the mesh fades in
		// (nothing while unsown or emptied)
		if (plants)
		{
			alpha = field.crops >= Field::k_TimesToSow && field.food >= 1.0f ? 1.0f : 0.0f;
			float far = 0.0f;
			if (Locator::camera::has_value())
			{
				const float away = glm::distance(Locator::camera::value().GetOrigin(), transform.position);
				far = std::clamp((away - 0.8f * config.foliageDistance) / (0.2f * config.foliageDistance), 0.0f, 1.0f);
			}
			alpha = std::min(alpha, far);
		}
		sink->offset.SetPosition(2.0f * shown * transform.scale.y * sink->height);
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
