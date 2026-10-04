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
#include "3D/ObjectMatrix.h"
#include "ECS/Components/Animal.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/PhysicsDrawPose.h"
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

float StepYawFollow(float& followYaw, float target, float milliseconds, float radiansPerSecond, bool snap)
{
	const float aim = lh_matrix::WrapAngle(target);      // fn_007FAAF0 0x82556F
	const float current = lh_matrix::WrapAngle(followYaw); // 0x82557C
	followYaw = current;
	if (current == aim || snap)
	{
		followYaw = aim; // 0x8256C1
		return 0.0f;
	}
	const float difference = lh_matrix::WrapAngle(aim - current);
	const float step = (milliseconds * 0.001f) * radiansPerSecond; // [0x8AC418], [0x9A392C]
	if (std::abs(difference) <= step)
	{
		followYaw = aim; // 0x8255F8
		return 0.0f;
	}
	followYaw = difference > 0.0f ? step + current : current - step;
	return followYaw - aim; // 0x825626
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
			// fn_007FAAF0 0x7FAAF0 (lh_matrix::WrapAngle): once into [-pi, pi]
			const float d = lh_matrix::WrapAngle(lh_matrix::WrapAngle(yaw) - draw.yaw);
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
			draw.yaw = lh_matrix::WrapAngle(draw.yaw);
			// the same rotation the pathfinding gives the transform (InitializeStep: AngleY(angle + 90 degrees))
			draw.rotation = lh_matrix::AngleY(draw.yaw + glm::half_pi<float>());
			// a SuperVillager (ECS/SuperVillager.h): fn_00825530's own turn over this one, in the object's yaw
			// (obj+0x48 = the angle of AngleY above, this yaw + 90 degrees), so that Wrap folds the same values. Only
			// followDrawnTurn takes it: fn_00825530 turns a local copy of the sheared matrix (0x8255AB rep movsd), so
			// draw.rotation stays the object's for the shear below and every other reader (ecs::DrawnBodyModel)
			if (draw.followRate > 0.0f)
			{
				const float objectYaw = draw.yaw + glm::half_pi<float>();
				if (!draw.hasFollowYaw)
				{
					// (openblack) made before its first draw (no drawn yaw yet): ECS/SuperVillager's Create sets it
					// otherwise (fn_00825F20 0x825FBC: +0x14 = obj+0x48)
					draw.followYaw = objectYaw;
					draw.hasFollowYaw = true;
				}
				draw.followDrawnTurn = 0.0f;
				// fn_00825400: only when CheckRegionOnScreen 0x82541D passes (0x825422 je 0x82543D)
				if (!draw.followFrozen)
				{
					const float turn = StepYawFollow(draw.followYaw, objectYaw, milliseconds, draw.followRate, draw.followSnap);
					if (draw.followTurn)
					{
						draw.followDrawnTurn = turn;
					}
				}
			}
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
		const auto* animal = registry.TryGet<const Animal>(entity);
		const bool bird = animal != nullptr && ecs::animal_ai::IsFlyingSpecies(animal->type);
		if (draw.position.y - ground <= 0.2f && !bird)
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

glm::mat4 DrawnModel(const Registry& registry, entt::entity entity, bool slopeShear)
{
	const auto& transform = registry.Get<const Transform>(entity);
	const auto* flying = registry.TryGet<const PhysicsDrawPose>(entity);
	const auto* draw = flying == nullptr ? registry.TryGet<const DrawPosition>(entity) : nullptr;
	const auto& rotation = flying != nullptr ? flying->rotation : draw != nullptr ? draw->rotation : transform.rotation;
	const auto& position = flying != nullptr ? flying->position : draw != nullptr ? draw->position : transform.position;
	auto model = lh_matrix::Model(position, rotation, transform.scale);
	if (draw != nullptr && slopeShear)
	{
		model[0] += draw->shearX * model[1];
		model[2] += draw->shearZ * model[1];
	}
	return model;
}

glm::mat4 DrawnBodyModel(const Registry& registry, entt::entity entity)
{
	auto model = DrawnModel(registry, entity);
	// (inferred) not over a physics pose: the yaw stage follows the villager's drawn yaw, which that pose does not use
	const auto* draw = registry.AllOf<PhysicsDrawPose>(entity) ? nullptr : registry.TryGet<const DrawPosition>(entity);
	if (draw == nullptr || draw->followDrawnTurn == 0.0f)
	{
		return model;
	}
	// fn_00825530 0x8255AB: the sheared matrix copied; 0x825626..0x8256BB turn only the copy (rows 0 and 2): c stored as
	// a float (fstp [esp+0x10] 0x825631), s on the FPU stack; the translation is not touched
	const double turn = static_cast<double>(draw->followDrawnTurn);
	glm::mat3 axes(model);
	lh_matrix::RotateY(axes, static_cast<float>(std::cos(turn)), std::sin(turn));
	for (int i = 0; i < 3; ++i)
	{
		model[i] = glm::vec4(axes[i], model[i][3]);
	}
	return model;
}

} // namespace openblack::ecs
