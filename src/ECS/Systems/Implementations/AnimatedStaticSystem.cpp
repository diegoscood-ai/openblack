/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AnimatedStaticSystem.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>

#include <glm/geometric.hpp>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/SkeletalPose.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

bool AnimatedStaticSystem::SetOpenState(entt::entity object, int32_t openState)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* still = registry.Valid(object) ? registry.TryGet<AnimatedStatic>(object) : nullptr;
	if (still == nullptr)
	{
		return false;
	}
	still->openState = openState;
	return true;
}

std::optional<uint32_t> AnimatedStaticSystem::GateStoneValue(entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* still = registry.Valid(object) ? registry.TryGet<const AnimatedStatic>(object) : nullptr;
	if (still == nullptr)
	{
		return std::nullopt;
	}
	return animated_static::GateStoneValue(still->gateStones);
}

bool AnimatedStaticSystem::LayGateStone(entt::entity plinth, entt::entity stone)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(plinth) || !registry.Valid(stone) || !Locator::infoConstants::has_value())
	{
		return false;
	}
	auto* still = registry.TryGet<AnimatedStatic>(plinth);
	const auto* mobile = registry.TryGet<const MobileStatic>(stone);
	if (still == nullptr || mobile == nullptr || still->type != AnimatedStaticInfo::GateStonePlinth)
	{
		return false;
	}
	const auto& infos = Locator::infoConstants::value().mobileStatic;
	const auto index = static_cast<size_t>(mobile->type);
	if (index >= infos.size() || !animated_static::IsGateStoneKind(infos[index].mobileType))
	{
		return false;
	}
	// A full plinth still takes the stone, which is then lost
	[[maybe_unused]] const bool laid = animated_static::AddGateStone(still->gateStones, infos[index].meshId);
	return true;
}

std::optional<std::vector<animated_static::RouteCircle>> AnimatedStaticSystem::RouteCircles(entt::entity gate) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(gate) || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto* still = registry.TryGet<const AnimatedStatic>(gate);
	const auto* pose = registry.TryGet<const AnimatedStaticPose>(gate);
	const auto* transform = registry.TryGet<const Transform>(gate);
	const auto* mesh = registry.TryGet<const Mesh>(gate);
	if (still == nullptr || transform == nullptr || mesh == nullptr || still->type != AnimatedStaticInfo::NorseGate)
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const float halfWidth = meshes.Contains(mesh->id) ? meshes.Handle(mesh->id)->GetBoundingBox().Size().x * 0.5f : 0.0f;
	// Its own across direction on the land, sized as it is
	const auto across = transform->rotation * glm::vec3(transform->scale.x, 0.0f, 0.0f);
	// A gate whose clip is not posed stands at its clip's start, where an open gate is still opening
	const uint32_t place = pose != nullptr ? pose->place : 0;
	const uint32_t restingPlace = pose != nullptr ? pose->restingPlace : 0;
	const bool openAndStill =
	    still->openState == animated_static::k_Open && !animated_static::IsMoving(still->openState, place, restingPlace);
	return animated_static::GateRouteCircles(animated_static::GateRouteMiddle(transform->position),
	                                         glm::vec2(across.x, across.z), halfWidth, transform->scale.x, openAndStill);
}

void AnimatedStaticSystem::Draw(entt::entity object, uint32_t frameMs)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* still = registry.TryGet<const AnimatedStatic>(object);
	auto* pose = registry.TryGet<AnimatedStaticPose>(object);
	if (still == nullptr || pose == nullptr)
	{
		return;
	}
	pose->place = animated_static::StepClip(still->openState, pose->place, frameMs, pose->restingPlace);
	PoseClip(object, pose->place);
	if (still->type == AnimatedStaticInfo::GateStonePlinth)
	{
		PlaceStones(object);
	}
}

void AnimatedStaticSystem::PoseClip(entt::entity object, uint32_t place)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* animation = registry.TryGet<SkeletalAnimation>(object);
	const auto time = static_cast<float>(place);
	if (animation == nullptr || animation->time == time)
	{
		return;
	}
	// The clip's own time follows the place, which the frame's animations have already posed at the last one: posed
	// again here, as they pose it, so this frame draws the new place. Without its clip or model they left it unposed.
	animation->time = time;
	if (!animation->hasClip || !Locator::resources::has_value())
	{
		return;
	}
	const auto* mesh = registry.TryGet<const Mesh>(object);
	auto& resources = Locator::resources::value();
	const auto& animations = resources.GetAnimations();
	const auto& meshes = resources.GetMeshes();
	const auto clip = animations.Handle(animation->clip);
	const auto model = clip && mesh != nullptr ? meshes.Handle(mesh->id) : decltype(meshes.Handle(0)) {};
	if (!clip || !model)
	{
		return;
	}
	graphics::ComputePoseInto(*model, *clip, animation->time, animation->pose, animation->locals);
}

void AnimatedStaticSystem::PlaceStones(entt::entity plinth)
{
	auto& registry = Locator::entitiesRegistry::value();
	_drawnPlinths.push_back(plinth);
	const auto& still = registry.Get<const AnimatedStatic>(plinth);
	auto& pose = registry.Get<AnimatedStaticPose>(plinth);
	// By value: making and unmaking the stones can move the Transform storage
	const auto* transformOf = registry.TryGet<const Transform>(plinth);
	const auto transform = transformOf != nullptr ? std::optional<Transform>(*transformOf) : std::nullopt;
	std::vector<animated_static::StoneDraw> draws;
	if (transform.has_value() && std::ranges::any_of(still.gateStones, [](const auto& slot) { return slot.has_value(); }))
	{
		if (!Locator::meshBoxProvider::has_value())
		{
			std::fputs("animated statics: no mesh boxes in the locator (Locator::meshBoxProvider)\n", stderr);
			std::abort();
		}
		// The heights are the models' own, half their boxes' height, unscaled
		const auto* mesh = registry.TryGet<const Mesh>(plinth);
		animated_static::PlinthLook look {
		    .openState = still.openState,
		    .moving = animated_static::IsMoving(still.openState, pose.place, pose.restingPlace),
		    .place = pose.place,
		    .playTime = pose.playTime,
		    .plinthHalfHeight = mesh != nullptr ? ecs::object::MeshHalfHeight(mesh->id) : 0.0f,
		};
		for (size_t slot = 0; slot < animated_static::k_GateStoneSlots; ++slot)
		{
			if (const auto& stone = still.gateStones.at(slot); stone.has_value())
			{
				look.stoneHalfHeights.at(slot) = ecs::object::MeshHalfHeight(resources::HashIdentifier(*stone));
			}
		}
		draws = animated_static::PlinthStoneDraws(look);
	}

	// One model for each stone drawn, made and unmade as their number changes
	auto& stones = pose.stones;
	while (stones.size() > draws.size())
	{
		if (registry.Valid(stones.back()) && registry.AllOf<PlinthStone>(stones.back()))
		{
			std::erase(_stones, stones.back());
			registry.Destroy(stones.back());
		}
		stones.pop_back();
	}
	if (draws.empty())
	{
		return;
	}
	while (stones.size() < draws.size())
	{
		const auto stone = registry.Create();
		registry.Assign<PlinthStone>(stone, PlinthStone {.plinth = plinth, .pickable = false});
		stones.push_back(stone);
		_stones.push_back(stone);
	}
	// Each placed on the plinth as it stands, raised by its lift, and turned and sized as the plinth is
	bool moved = false;
	for (size_t i = 0; i < draws.size(); ++i)
	{
		const auto& draw = draws[i];
		const auto meshId = resources::HashIdentifier(*still.gateStones.at(draw.slot));
		if (const auto* mesh = registry.TryGet<const Mesh>(stones[i]); mesh == nullptr || mesh->id != meshId)
		{
			registry.AssignOrReplace<Mesh>(stones[i], meshId, static_cast<int8_t>(0), static_cast<int8_t>(1));
		}
		const Transform placed {.position = transform->position + glm::vec3(0.0f, draw.lift, 0.0f),
		                        .rotation = transform->rotation,
		                        .scale = transform->scale};
		if (auto* current = registry.TryGet<Transform>(stones[i]); current == nullptr)
		{
			registry.Assign<Transform>(stones[i], placed);
		}
		else if (current->position != placed.position || current->rotation != placed.rotation || current->scale != placed.scale)
		{
			*current = placed;
			moved = true;
		}
	}
	if (moved)
	{
		registry.SetDirty();
	}
}

void AnimatedStaticSystem::EndDrawnFrame()
{
	if (_stones.empty())
	{
		_drawnPlinths.clear();
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::ranges::sort(_drawnPlinths);
	// A stone stays only while its plinth was drawn this frame; one whose entity is no longer a stone (the land was
	// cleared) is only forgotten
	std::erase_if(_stones, [this, &registry](entt::entity stone) {
		if (!registry.Valid(stone) || !registry.AllOf<PlinthStone>(stone))
		{
			return true;
		}
		if (std::ranges::binary_search(_drawnPlinths, registry.Get<const PlinthStone>(stone).plinth))
		{
			return false;
		}
		DestroyStone(stone);
		return true;
	});
	_drawnPlinths.clear();
}

void AnimatedStaticSystem::DestroyStone(entt::entity stone)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto plinth = registry.Get<const PlinthStone>(stone).plinth;
	if (auto* pose = registry.Valid(plinth) ? registry.TryGet<AnimatedStaticPose>(plinth) : nullptr; pose != nullptr)
	{
		pose->stones.clear();
	}
	registry.Destroy(stone);
}

void AnimatedStaticSystem::Update(uint32_t /*turn*/, float /*turnFraction*/) {}

void AnimatedStaticSystem::Reset()
{
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		for (const auto stone : _stones)
		{
			if (registry.Valid(stone) && registry.AllOf<PlinthStone>(stone))
			{
				DestroyStone(stone);
			}
		}
	}
	_stones.clear();
	_drawnPlinths.clear();
}
