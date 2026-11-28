/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SnowLevels.h"

#include <glm/vec3.hpp>

#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/DrawMesh.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/SnowLevel.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "Locator.h"

namespace openblack::ecs::snow_levels
{

using components::BrokenModelSnow;
using components::BuildingDamage;
using components::CarriedByTornado;
using components::DrawMesh;
using components::Field;
using components::Fragment;
using components::HandDrawPose;
using components::HeldByCreature;
using components::PhysicsDrawPose;
using components::ShowNeedsVisuals;
using components::SnowLevel;
using components::TownDesireFlag;
using components::Transform;
using components::Tree;

snow_cover::SnowHold HoldOf(const Registry& registry, entt::entity object)
{
	// each of these draws sets its object's rate and cap before the object is drawn
	if (registry.AllOf<Field>(object))
	{
		return snow_cover::k_FieldCropHold;
	}
	if (registry.AnyOf<TownDesireFlag, ShowNeedsVisuals>(object))
	{
		return snow_cover::k_NeedsFlagHold;
	}
	// the tree's out of the map draw sets these on it and nothing puts them back
	if (const auto* tree = registry.TryGet<const Tree>(object); tree != nullptr && tree->drawnOutOfMap)
	{
		return snow_cover::k_TreeOutOfMapHold;
	}
	return {};
}

void NoteTreesDrawnOutOfMap(Registry& registry, std::span<const entt::entity> physicsDrawn)
{
	const auto note = [&registry](entt::entity object) {
		if (auto* tree = registry.TryGet<Tree>(object); tree != nullptr)
		{
			tree->drawnOutOfMap = true;
		}
	};
	registry.Each<const HandDrawPose>([&note](entt::entity object, const HandDrawPose&) { note(object); });
	registry.Each<const HeldByCreature>([&note](entt::entity object, const HeldByCreature&) { note(object); });
	registry.Each<const CarriedByTornado>([&note](entt::entity object) { note(object); });
	for (const auto object : physicsDrawn)
	{
		if (registry.Valid(object))
		{
			note(object);
		}
	}
}

bool Step(Registry& registry, entt::entity object, float depth, int32_t step)
{
	const auto hold = HoldOf(registry, object);
	const int32_t target = snow_cover::ObjectLevel(depth, hold.rate, hold.cap);
	if (auto* snow = registry.TryGet<SnowLevel>(object); snow != nullptr)
	{
		const int32_t level = snow_cover::EaseLevel(snow->level, target, step);
		if (level == snow->level)
		{
			return false;
		}
		snow->level = level;
		return true;
	}
	const int32_t level = snow_cover::EaseLevel(0, target, step);
	if (level == 0)
	{
		return false;
	}
	registry.Assign<SnowLevel>(object, SnowLevel {.level = level});
	return true;
}

std::optional<int32_t> LevelOf(const Registry& registry, entt::entity object)
{
	const auto* snow = registry.TryGet<const SnowLevel>(object);
	return snow != nullptr ? std::optional(snow->level) : std::nullopt;
}

void Drawn(entt::entity object, uint32_t frameGameMs)
{
	if (!Locator::snowSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& snow = Locator::snowSystem::value();
	if (!registry.AllOf<Transform>(object) || (!snow.HasSnow() && !registry.AllOf<SnowLevel>(object)))
	{
		return;
	}
	const glm::vec3 place = DrawnPosition(registry, object, PoseOf::Drawing);
	if (Step(registry, object, snow.GetDepth({place.x, place.z}), snow_cover::LevelStep(frameGameMs)))
	{
		registry.SetDirty();
	}
}

void FollowBreak(Registry& registry, entt::entity building)
{
	// the snow the building's own model shows as the broken one is made from it
	registry.AssignOrReplaceState<BrokenModelSnow>(building,
	                                               BrokenModelSnow {.level = LevelOf(registry, building).value_or(0)});
}

void ForgetBreak(Registry& registry, entt::entity building)
{
	registry.RemoveState<BrokenModelSnow>(building);
}

void TakePieceSnow(Registry& registry, entt::entity piece, entt::entity building)
{
	int32_t level = 0;
	if (building != entt::null && registry.Valid(building))
	{
		if (const auto* snow = registry.TryGet<const BrokenModelSnow>(building); snow != nullptr)
		{
			level = snow->level;
		}
	}
	registry.AssignOrReplaceState<BrokenModelSnow>(piece, BrokenModelSnow {.level = level, .held = true});
}

void PieceLanded(Registry& registry, entt::entity piece)
{
	if (auto* snow = registry.TryGet<BrokenModelSnow>(piece); snow != nullptr)
	{
		snow->held = false;
	}
}

bool StepBrokenModel(Registry& registry, entt::entity object, float depth, int32_t step)
{
	auto* snow = registry.TryGet<BrokenModelSnow>(object);
	if (snow == nullptr || snow->held)
	{
		return false;
	}
	const int32_t level = snow_cover::EaseLevel(snow->level, snow_cover::BrokenModelLevel(depth), step);
	if (level == snow->level)
	{
		return false;
	}
	snow->level = level;
	return true;
}

int32_t BrokenModelShownLevelOf(const Registry& registry, entt::entity object)
{
	const auto* snow = registry.TryGet<const BrokenModelSnow>(object);
	return snow != nullptr ? snow_cover::BrokenModelShownLevel(snow->level) : 0;
}

void BrokenModelDrawn(entt::entity object, uint32_t frameGameMs)
{
	if (!Locator::snowSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<BrokenModelSnow>(object))
	{
		return;
	}
	// a broken building's only while its broken model is what it draws, a piece's while it has one
	if (const auto* damage = registry.TryGet<const BuildingDamage>(object); damage != nullptr)
	{
		const auto* draw = registry.TryGet<const DrawMesh>(object);
		if (!damage->mesh || damage->generatedMesh == 0 || draw == nullptr || draw->id != damage->generatedMesh)
		{
			return;
		}
	}
	else if (const auto* fragment = registry.TryGet<const Fragment>(object); fragment == nullptr || !fragment->mesh)
	{
		return;
	}
	// where the draw puts it: a flying piece's pose between its last two turns, else its place
	glm::vec3 place {0.0f};
	if (const auto* flying = registry.TryGet<const PhysicsDrawPose>(object); flying != nullptr)
	{
		place = flying->position;
	}
	else if (const auto* transform = registry.TryGet<const Transform>(object); transform != nullptr)
	{
		place = transform->position;
	}
	else
	{
		return;
	}
	// read by the damage meshes' draw, which takes the level each frame: no drawn instance changes
	(void)StepBrokenModel(registry, object, Locator::snowSystem::value().GetDepth({place.x, place.z}),
	                      snow_cover::LevelStep(frameGameMs));
}

} // namespace openblack::ecs::snow_levels
