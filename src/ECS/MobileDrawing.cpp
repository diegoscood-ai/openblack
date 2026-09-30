/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MobileDrawing.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/L3DAnim.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs
{
using namespace components;

namespace
{
float Wrap(float angle)
{
	// fn_007FAAF0: once into [-pi, pi]
	if (angle > glm::pi<float>())
	{
		angle -= glm::two_pi<float>();
	}
	else if (angle < -glm::pi<float>())
	{
		angle += glm::two_pi<float>();
	}
	return angle;
}

float Ground(const LandIslandInterface& island, float x, float z)
{
	return island.GetHeightAt(glm::vec2(x, z));
}

/// the villager moves for its animation (its state's info flag 0x14) and plays a distance-synced clip, or the animal moved
bool Interpolates(entt::entity entity, const Transform& transform, const DrawPosition& draw)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* animation = registry.TryGet<const SkeletalAnimation>(entity);
	if (animation == nullptr || !animation->hasClip)
	{
		return false;
	}
	auto& animations = Locator::resources::value().GetAnimations();
	if (!animations.Contains(animation->clip) || animations.Handle(animation->clip)->GetCycleDistance() < 0.05f)
	{
		return false; // flag 0x200
	}
	if (registry.AllOf<Villager>(entity))
	{
		const auto* action = registry.TryGet<const LivingAction>(entity);
		const auto state = action != nullptr ? action->states[static_cast<size_t>(LivingAction::Index::Top)] : 0;
		const auto& table = Locator::infoConstants::value().villagerStateTable;
		return state < table.size() && table[state].field0x14 != 0;
	}
	// Object::IsMoving: Pos differs from the turn's start in x or z
	return draw.turnStart.x != transform.position.x || draw.turnStart.z != transform.position.z;
}
} // namespace

void BeginMobileTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> fresh;
	registry.Each<const Transform>([&](entt::entity entity, const Transform& transform) {
		if (!registry.AnyOf<Villager, Animal>(entity))
		{
			return;
		}
		if (auto* draw = registry.TryGet<DrawPosition>(entity); draw != nullptr)
		{
			draw->turnStart = transform.position;
			draw->started = true;
		}
		else
		{
			fresh.push_back(entity);
		}
	});
	for (const auto entity : fresh)
	{
		auto& draw = registry.Assign<DrawPosition>(entity);
		draw.turnStart = registry.Get<const Transform>(entity).position;
		draw.started = true;
	}
}

void SnapDrawPosition(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* draw = registry.TryGet<DrawPosition>(entity); draw != nullptr)
	{
		draw->turnStart = registry.Get<const Transform>(entity).position;
	}
}

void UpdateMobileDrawing(float turnFraction, float milliseconds)
{
	if (!Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& island = Locator::terrainSystem::value();
	const float t = std::clamp(turnFraction, 0.0f, 0.99f);
	bool any = false;
	registry.Each<DrawPosition, const Transform>([&](entt::entity entity, DrawPosition& draw, const Transform& transform) {
		any = true;
		// position: fn_0051AF00 lerps the start and end of the last turn, each at its land height (+ its altitude)
		if (draw.started && Interpolates(entity, transform, draw))
		{
			const float y0 = draw.turnStart.y;
			const float y1 = transform.position.y;
			draw.position = glm::vec3(glm::mix(draw.turnStart.x, transform.position.x, t), glm::mix(y0, y1, t),
			                          glm::mix(draw.turnStart.z, transform.position.z, t));
		}
		else
		{
			draw.position = transform.position;
		}
		// yaw: Villager::Draw turns +0x108 towards the real yaw, 0.003 rad/ms, past 90 degrees 0.012 * |d| / pi rad/ms;
		// animals turn once per turn (their SetTowardsAngle limits it instead)
		draw.rotation = transform.rotation;
		if (const auto* wallHug = registry.TryGet<const WallHug>(entity);
		    wallHug != nullptr && registry.AllOf<Villager>(entity))
		{
			const float yaw = wallHug->yAngle;
			if (!draw.hasYaw)
			{
				draw.yaw = yaw;
				draw.hasYaw = true;
			}
			const float d = Wrap(Wrap(yaw) - draw.yaw);
			float rate = 0.003f;
			if (std::abs(d) > glm::half_pi<float>())
			{
				rate = std::abs(d) * (2.0f / glm::pi<float>()) * 2.0f * 0.003f;
			}
			const float step = milliseconds * rate;
			if (std::abs(d) < step)
			{
				draw.yaw = yaw;
			}
			else
			{
				draw.yaw += d > 0.0f ? step : -step;
			}
			draw.yaw = Wrap(draw.yaw);
			// the same rotation the pathfinding gives the transform (InitializeStep: eulerAngleY(-angle - 90 degrees))
			draw.rotation = glm::mat3(glm::eulerAngleY(-draw.yaw - glm::half_pi<float>()));
		}
		// Dove::Draw (0x41F680): the bank zoomer advances by the frame's game time and rolls the drawn matrix about its
		// forward axis (rows 0 and 1 rotated by the bank)
		if (auto* brain = registry.TryGet<AnimalBrain>(entity); brain != nullptr && (brain->bank.value != 0.0f || brain->bank.IsMoving()))
		{
			brain->bank.Update(milliseconds * 0.001f);
			const float c = std::cos(brain->bank.value);
			const float s = std::sin(brain->bank.value);
			const glm::vec3 x = draw.rotation[0];
			const glm::vec3 y = draw.rotation[1];
			draw.rotation[0] = c * x - s * y;
			draw.rotation[1] = s * x + c * y;
		}
		// slope: fn_0051B220 shears the object on the land (altitude <= 0.2): the rise one unit along its x and z axes,
		// each clamped to +-0.3
		draw.shearX = 0.0f;
		draw.shearZ = 0.0f;
		const float ground = Ground(island, draw.position.x, draw.position.z);
		if (draw.position.y - ground <= 0.2f)
		{
			// the object matrix's rows, with its scale
			const glm::vec3 x = draw.rotation[0] * transform.scale.x;
			const glm::vec3 z = draw.rotation[2] * transform.scale.z;
			draw.shearX = std::clamp(Ground(island, draw.position.x + x.x, draw.position.z + x.z) - draw.position.y, -0.3f, 0.3f);
			draw.shearZ = std::clamp(Ground(island, draw.position.x + z.x, draw.position.z + z.z) - draw.position.y, -0.3f, 0.3f);
		}
	});
	if (any)
	{
		registry.SetDirty(); // the drawn matrices change every frame
	}
	// OPENBLACK_DRAW_TRACE=1: the drawn and real position of the first walking villager, every frame for a while
	static int traced = 0;
	if (traced < 400 && milliseconds > 0.0f && std::getenv("OPENBLACK_DRAW_TRACE") != nullptr)
	{
		registry.Each<const DrawPosition, const Transform, const Villager>(
		    [&](entt::entity entity, const DrawPosition& draw, const Transform& transform, const Villager&) {
			    if (traced >= 400 || draw.position == transform.position || !Interpolates(entity, transform, draw))
			    {
				    return;
			    }
			    ++traced;
			    SPDLOG_LOGGER_INFO(spdlog::get("game"), "Draw trace: {} t {:.2f} drawn ({:.3f}, {:.3f}) real ({:.3f}, {:.3f}) yaw {:.3f}",
			                       static_cast<uint32_t>(entity), t, draw.position.x, draw.position.z, transform.position.x,
			                       transform.position.z, draw.yaw);
			    traced += 1000; // one villager per frame
		    });
		if (traced >= 1000)
		{
			traced -= 1000;
		}
	}
}

} // namespace openblack::ecs
