/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectDelivery.h"

#include "Audio/Audio.h"
#include "Audio/Services/Guidance.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Trees.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// GetResourceType (vt +0x690) and GetResource (vt +0x98) of the object given: a tree's wood (Tree 0x74B7A0, DeadTree
/// 0x511330: ecs::TreeWood), a pot's or a pile's own type and amount
std::pair<ResourceType, uint32_t> ResourceOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AnyOf<Tree, DeadTree>(object))
	{
		return {ResourceType::Wood, ecs::TreeWood(object)};
	}
	if (const auto* pot = registry.TryGet<const Pot>(object); pot != nullptr && Locator::infoConstants::has_value())
	{
		const auto type = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type)).resourceType;
		return {type, ecs::object_resources::GetResource(object, type)};
	}
	return {ResourceType::None, 0};
}
} // namespace

uint32_t ecs::object_delivery::DoDeleteObjectAndTakeResource(entt::entity structure, entt::entity object,
                                                             const pot_resource::Dropper& is)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(structure) || !registry.Valid(object))
	{
		return 0;
	}
	// 0x63A965..0x63A99C: this->AddResource(t = object->GetResourceType(), object->GetResource(t), is,
	// object->IsPoisoned(), &object->pos, 0) (vt +0x9C)
	const auto [type, amount] = ResourceOf(object);
	const uint32_t taken =
	    object_resources::AddResource(structure, type, amount, is, object_resources::IsPoisoned(object));
	// 0x63A9A2..0x63A9E6: with something taken and an interface, (not ported) DoCreatureMimicAfterAddingResource (vt
	// +0x68C: the creature); with the local interface ResourceDropSFX(is, this->pos, this->GetGuidanceResourceType()
	// (vt +0xE0: a StoragePit's is GameThing's 0, silent; (inferred) a worship site's the same))
	if (taken != 0 && is.hasInterface && is.isMyInterface)
	{
		audio::guidance::ResourceDropSFX(registry.Get<const Transform>(structure).position, audio::guidance::RainType::None);
	}
	// 0x63A9EE..0x63AA93: wood that is not a pot (IsPot vt +0x4B4): GAudio::PlaySoundEffect 0x429E30 bank InGame,
	// sample 155 G_TreeMulch_01 + the counter [0xD4437C] = ([0xD4437C] + 1) & 3, owner the object, 3D at its point, not
	// gated by what was taken
	if (type == ResourceType::Wood && !registry.AllOf<Pot>(object))
	{
		audio::PlayOptions options;
		options.sample = {audio::Bank(audio::SfxBank::InGame), 155 + audio::NextCounter(audio::Counter::TreeMulch)};
		options.owner = audio::Owner::Thing(object);
		options.is3D = true;
		options.track = false;
		options.position = registry.Get<const Transform>(object).position;
		audio::PlaySoundEffect(options);
	}
	// 0x63AAA3: (pending) GoolooGooloo 0x5E6540, the 500 ms ghost of the object's 3D object (graphics::frame_anim::GoolooFrame
	// has its maths, nothing draws it yet); 0x63AAB1: object->ToBeDeleted(0)
	ToBeDeleted(object);
	return taken;
}
