/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HoldLowering.h"

#include "ECS/Components/Animal.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs::components;

hand_pick_up::HoldClass ecs::hold_lowering::ClassOf(const Registry& registry, entt::entity object)
{
	using hand_pick_up::HoldClass;
	if (registry.AnyOf<Tree, DeadTree>(object))
	{
		return HoldClass::Tree;
	}
	if (const auto* mobile = registry.TryGet<const MobileObject>(object);
	    mobile != nullptr && mobile->type == MobileObjectInfo::LumpOfPoo)
	{
		return HoldClass::Poo;
	}
	if (registry.AnyOf<MobileObject, Pot, OneOffSpellSeed>(object))
	{
		return HoldClass::SideObject;
	}
	if (registry.AnyOf<Villager, Animal>(object))
	{
		return HoldClass::Living;
	}
	if (registry.AllOf<MobileStatic>(object))
	{
		return HoldClass::MobileStatic;
	}
	return HoldClass::Object;
}

float ecs::hold_lowering::MultiplierOf(const Registry& registry, entt::entity object)
{
	const auto* statics = registry.TryGet<const MobileStatic>(object);
	const auto staticType = statics != nullptr ? statics->type : MobileStaticInfo::None;
	// the lowering of the class's row: neither the height nor the radius changes it
	return hand_pick_up::HoldParametersOf(ClassOf(registry, object), staticType, 0.0f, 0.0f).lowering;
}

float ecs::hold_lowering::LoweringOf(const Registry& registry, entt::entity object)
{
	return object::GetHeight(object) * MultiplierOf(registry, object);
}
