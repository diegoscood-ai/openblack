/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TreeGrowth.h"

#include <glm/common.hpp>

#include "Archetypes/Utils.h"
#include "Components/Fixed.h"
#include "Components/MagicTree.h"
#include "Components/Transform.h"
#include "Components/Tree.h"
#include "Forests.h"
#include "InfoConstants.h"
#include "Common/RandomNumberManager.h"
#include "Locator.h"
#include "Magic/Objects/MagicTree.h"
#include "Physics/PhysicsObjects.h"
#include "Registry.h"
#include "Systems/HandSystemInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
const GTreeInfo& InfoOf(const Tree& tree)
{
	return Locator::infoConstants::value().tree.at(static_cast<size_t>(tree.type));
}
} // namespace

void trees::InitGrowth(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& tree = registry.Get<Tree>(entity);
	const float scale = registry.Get<Transform>(entity).scale.x;
	// fld maxScale; fcomp scale; test ah, 0x40: any difference
	if (tree.maxSize != scale)
	{
		tree.growing = true;
		const auto turns = InfoOf(tree).growsAfterNumGameTurns;
		// GameRand(n) = 0 .. n - 1, stored as a word
		tree.growCountdown =
		    static_cast<int16_t>(turns > 0 ? Locator::rng::value().NextValue<uint32_t>(0, turns - 1) : 0);
	}
}

float trees::GetScale(entt::entity tree)
{
	return Locator::entitiesRegistry::value().Get<Transform>(tree).scale.x;
}

void trees::SetScale(entt::entity entity, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& transform = registry.Get<Transform>(entity);
	transform.scale = glm::vec3(scale);
	if (auto* fixed = registry.TryGet<Fixed>(entity); fixed != nullptr)
	{
		const auto& info = InfoOf(registry.Get<Tree>(entity));
		const auto [point, radius] = archetypes::GetFixedObstacleBoundingCircle(info.normal, transform);
		fixed->boundingCenter = point;
		fixed->boundingRadius = radius;
	}
	registry.SetDirty();
}

float trees::Grow(entt::entity entity, float amount, bool setScale, bool raiseMax)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& tree = registry.Get<Tree>(entity);
	const float old = GetScale(entity);
	if (raiseMax)
	{
		tree.maxSize = glm::max(tree.maxSize, old + amount); // arg 3
	}
	if (old == tree.maxSize)
	{
		return 0.0f;
	}
	const float scale = old + amount < tree.maxSize ? old + amount : tree.maxSize;
	// both paths change the scale: SetScale (vt 0x124) when it did not change or with setScale, else SetJustScale
	// (vt 0x51C) and the LH3DObject's matrix rebuilt from the scale, the Y angle and the position (the Transform here)
	SetScale(entity, scale);
	return GetScale(entity) - old;
}

float trees::Shrink(entt::entity entity, float amount)
{
	const float scale = GetScale(entity) - amount;
	// fcomp 0; test ah, 0x41: <= 0
	if (scale <= 0.0f)
	{
		ToBeDeleted(entity); // vt 0xC
		return 0.0f;
	}
	SetScale(entity, scale);
	return amount;
}

bool trees::Process(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& tree = registry.Get<Tree>(entity);
	// dec word +0x60; not 0: 1
	--tree.growCountdown;
	if (tree.growCountdown != 0)
	{
		return true;
	}
	tree.growCountdown = static_cast<int16_t>(InfoOf(tree).growsAfterNumGameTurns);
	if (!tree.growing || !(GetScale(entity) < tree.maxSize))
	{
		return false;
	}
	// TODO(natural growth, postponed by the user): Grow(growthAmount x (1 + GetMaxRainingOrSnowing x 0.01 x
	// rainingAcceleratorMultiplier) x (1 + the land's alignment x 0.5), false, false)
	return GetScale(entity) < tree.maxSize;
}

void trees::ToBeDeleted(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return;
	}
	if (registry.AllOf<MagicTree>(entity))
	{
		magic::magic_tree::ToBeDeleted(entity); // MagicTree::ToBeDeleted 0x5FD070
		return;
	}
	BaseToBeDeleted(entity);
}

void trees::BaseToBeDeleted(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return;
	}
	// GetForest (vt 0x86C): out of its lists (fn_0053A220), +0x68 = 0. The global tree list g_game +0x205CDC is the
	// registry here.
	if (auto* tree = registry.TryGet<Tree>(entity); tree != nullptr && tree->forest != entt::null)
	{
		forests::RemoveTree(tree->forest, entity);
		tree->forest = entt::null;
	}
	// Object::ToBeDeleted 0x636670 (inf: it also leaves the hand and the physics)
	if (Locator::handSystem::has_value())
	{
		const auto held = Locator::handSystem::value().GetHeldObject();
		if (held.has_value() && *held == entity)
		{
			Locator::handSystem::value().ForceDropHeld();
		}
	}
	physics::PhysicsObjects::RemoveObject(entity);
	registry.Destroy(entity);
	registry.SetDirty();
}
