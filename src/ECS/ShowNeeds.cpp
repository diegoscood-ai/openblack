/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ShowNeeds.h"

#include <cmath>

#include <vector>

#include <glm/mat3x3.hpp>

#include "3D/AllMeshes.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/Workshops.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Worship/SpecialPoints.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// A workshop's needs sign stands at its special point 9 (any index)
constexpr int k_WorkshopShowNeedsPoint = 9;

Registry& Reg()
{
	return Locator::entitiesRegistry::value();
}

const GShowNeedsInfo* InfoRow(uint8_t index)
{
	const auto& rows = Locator::infoConstants::value().showNeeds;
	return index < rows.size() ? &rows.at(index) : nullptr;
}

/// min(desire, MaxNeedValue) / MaxNeedValue (the max when it is not above the desire)
float Target(const ShowNeedsVisuals& v, const GShowNeedsInfo& info)
{
	if (info.maxNeedValue > v.desire)
	{
		return v.desire / info.maxNeedValue;
	}
	return info.maxNeedValue / info.maxNeedValue;
}

/// The Zoomer advance as the sign does it inline: t = dt + time; t >= duration the destination; else speed =
/// ((t c2 + a c3) + b c4) + start speed, value = (((C c4 + start speed t) + b c3) + a c2) + start value (not the plain
/// Zoomer update's sum order)
void Advance(Zoomer& z, float dt)
{
	const float t = dt + z.time;
	z.time = t;
	if (!(t < z.duration))
	{
		z.value = z.destination;
		z.speed = z.destinationSpeed;
		z.time = z.duration; // the original also clears a second time, not kept by openblack's Zoomer
		return;
	}
	const float a = (t * t) * 0.5f;
	const float b = (t * a) * 0.33333334f;
	z.speed = ((t * z.c2 + a * z.c3) + b * z.c4) + z.startSpeed;
	const float c = (a * a) * 0.16666667f;
	z.value = (((c * z.c4 + z.startSpeed * t) + b * z.c3) + a * z.c2) + z.startValue;
}

/// The sign's frame, standing at `building`'s special point `pointIndex`. Returns whether it is drawn
bool DrawVisuals(entt::entity entity, ShowNeedsVisuals& v, float frameSeconds, const glm::vec3& eye, entt::entity building,
                 int pointIndex)
{
	auto& registry = Reg();
	const auto* info = InfoRow(v.infoIndex);
	if (info == nullptr)
	{
		return false;
	}
	// The original also sets two fields of the 3D object (pending: not identified).
	// Drawn only while the desire is over ShowNeedGreater (else the reset below)
	if (!(v.desire > info->showNeedGreater))
	{
		// The Zoomer jumps to the desire, nothing drawn
		v.height.SetPosition(v.desire);
		return false;
	}
	// Heads for the target with speed 0 in 1.0 s, every frame
	v.height.SetDestinationWithSpeedAndTime(Target(v, *info), 0.0f, 1.0f);
	// The advance by the frame's seconds
	Advance(v.height, frameSeconds);
	const float shown = v.height.value;
	// The height = value x MaxHieght
	const float height = shown * info->maxHieght;
	// The building's needs sign point; its altitude = the height (the ground altitude there is computed and dropped)
	const auto point = worship::GetSpecialPoint(building, pointIndex);
	if (!point.has_value())
	{
		return false;
	}
	auto coords = map_coords::FromWorld(point->position);
	coords.altitude = height;
	// x, z of the coords in metres, the yaw toward the camera: atan2(camera z - z, camera x - x) + pi/2; the point is the
	// land plus the altitude
	const auto metres = map_coords::ToMetres(coords);
	const float dx = eye.x - metres.x;
	const float dz = eye.z - metres.y;
	const float yaw = std::atan2(dz, dx) + 1.5707964f;
	const auto world = map_coords::ToWorld(coords);
	// The sign's matrix at the point, yaw and scale 1.0
	const glm::mat4 m = affine::PlacementMatrix(world, yaw, 1.0f);
	// L = (MaxNeedValue - ShowNeedGreater) x 0.2; value < L: the 3x3 scaled for the draw only by
	// (value - ShowNeedGreater) / (L - ShowNeedGreater)
	const float l = (info->maxNeedValue - info->showNeedGreater) * 0.2f;
	float scale = 1.0f;
	if (shown < l)
	{
		const float num = shown - info->showNeedGreater;
		const float den = l - info->showNeedGreater;
		scale = num / den;
	}
	auto& transform = registry.Get<Transform>(entity);
	transform.position = glm::vec3(m[3]);
	transform.rotation = glm::mat3(m);
	transform.scale = glm::vec3(scale);
	return true;
}
} // namespace

int show_needs::NeedsPoint(bool altar, uint8_t infoIndex)
{
	return altar ? static_cast<int>(infoIndex) : k_WorkshopShowNeedsPoint;
}

entt::entity show_needs::Create(const glm::vec3& at, entt::entity owner, uint8_t infoIndex)
{
	const auto* info = InfoRow(infoIndex);
	if (info == nullptr)
	{
		return entt::null;
	}
	auto& registry = Reg();
	const auto entity = registry.Create();
	// The next creation index
	object_index::Assign(entity);
	auto& v = registry.Assign<ShowNeedsVisuals>(entity);
	v.owner = owner;
	v.infoIndex = infoIndex;
	registry.Assign<Transform>(entity, at, glm::mat3(1.0f), glm::vec3(1.0f));
	// The info's mesh, scale 1.0
	// (openblack) not drawn = no Mesh: the sign's Mesh is assigned while its frame draws it (NotDrawn keeps a
	// building's footprint pass; the sign must be out of every pass)
	registry.SetDirty();
	return entity;
}

void show_needs::SetDesire(entt::entity visuals, float desire)
{
	if (auto* v = visuals != entt::null && Reg().Valid(visuals) ? Reg().TryGet<ShowNeedsVisuals>(visuals) : nullptr)
	{
		v->desire = desire;
	}
}

void show_needs::Delete(entt::entity visuals, bool now)
{
	if (visuals != entt::null && Reg().Valid(visuals))
	{
		ecs::ToBeDeleted(visuals, now);
	}
}

void show_needs::DrawFor(entt::entity building, float frameSeconds, const glm::vec3& eye)
{
	auto& registry = Reg();
	if (building == entt::null || !registry.Valid(building))
	{
		return;
	}
	// one sign's frame, standing at the building's point, marked drawn when it is
	const auto drawSign = [&](entt::entity sign, int pointIndex) {
		auto* v = sign != entt::null && registry.Valid(sign) && ecs::IsAvailable(sign) ? registry.TryGet<ShowNeedsVisuals>(sign)
		                                                                               : nullptr;
		if (v != nullptr)
		{
			v->drawnThisFrame = DrawVisuals(sign, *v, frameSeconds, eye, building, pointIndex);
		}
	};
	// A functional workshop not on fire draws its sign, before its own draw
	if (workshops::IsWorkshop(building))
	{
		const auto* workshop = registry.TryGet<const Workshop>(building);
		if (workshop != nullptr && abode_queries::IsFunctional(building) && !fire::IsOnFire(building))
		{
			drawSign(workshop->showNeeds, NeedsPoint(false, k_WorkshopInfo));
		}
		return;
	}
	// A built worship site draws its altar's three, in their order, before its own fire is looked at, so on fire too;
	// one still being built draws none
	if (const auto* site = registry.TryGet<const WorshipSite>(building); site != nullptr && abode_queries::IsBuilt(building))
	{
		const auto signs = site->needsSigns;
		for (uint8_t row = 0; row < k_AltarSigns; ++row)
		{
			drawSign(signs.at(row), NeedsPoint(true, row));
		}
	}
}

void show_needs::EndDrawnFrame()
{
	auto& registry = Reg();
	bool changed = false;
	std::vector<entt::entity> entities;
	registry.Each<ShowNeedsVisuals>([&](entt::entity e, ShowNeedsVisuals&) { entities.push_back(e); });
	for (const auto entity : entities)
	{
		auto& v = registry.Get<ShowNeedsVisuals>(entity);
		// shown only when its building's draw drew it this frame; a sign no draw reached keeps its Zoomer as it was
		const bool drawn = v.drawnThisFrame && ecs::IsAvailable(entity);
		v.drawnThisFrame = false;
		const bool wasDrawn = registry.AllOf<Mesh>(entity);
		if (drawn != wasDrawn)
		{
			if (drawn)
			{
				const auto* info = InfoRow(v.infoIndex);
				registry.Assign<Mesh>(entity, resources::HashIdentifier(info->mesh), static_cast<int8_t>(0),
				                      static_cast<int8_t>(0));
			}
			else
			{
				registry.Remove<Mesh>(entity);
			}
			changed = true;
		}
		changed = changed || drawn;
	}
	if (changed)
	{
		registry.SetDirty();
	}
}
