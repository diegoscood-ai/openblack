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
/// The game's milliseconds to seconds
constexpr float k_Milli = 0.001f;
/// The crop is drawn at most 80% sunk
constexpr float k_MostSunk = -0.8f;

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

bool FieldSystem::Draw(entt::entity entity, uint32_t frameGameMs)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* field = registry.TryGet<Field>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	// a field marked for deletion is not drawn (it is out of the map): nothing to sink or fade
	if (field == nullptr || transform == nullptr || mesh == nullptr || !ecs::IsAvailable(entity))
	{
		return false;
	}
	auto* fade = registry.TryGet<Alpha>(entity);
	// not drawn at all below a quarter of the growing age or 25 food, and its ease waits where it is
	if (!Shows(*field))
	{
		if (fade == nullptr)
		{
			registry.Assign<Alpha>(entity, 0.0f);
		}
		else
		{
			fade->value = 0.0f;
		}
		return false;
	}
	if (fade != nullptr)
	{
		registry.Remove<Alpha>(entity);
	}
	// v = food / 350 - 1, its one-second ease started again from where it is at every draw, then stepped by the frame's
	// whole game ms in seconds (the speed's terms summed in the game's order for this step); the drawn matrix's y +=
	// 2 v scale half height (the mesh's half height read inline, so the mesh level)
	const float v = field->food / Field::k_TotalFood - 1.0f;
	field->sink.SetDestinationWithSpeedAndTime(v, 0.0f, 1.0f);
	field->sink.UpdateInlineSpeed(static_cast<float>(frameGameMs) * k_Milli);
	// held at 80% sunk; the fade the game works out for it is written over by the land's colour before the crop is
	// drawn, so it stays fully opaque
	const float shown = std::max(field->sink.value, k_MostSunk);
	// scale x half height x v, doubled: only for the draw, the Transform stays where the field is
	const float sunk = transform->scale.y * ecs::object::MeshHalfHeight(mesh->id) * shown;
	field->drawnRise = sunk + sunk;
	return true;
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
