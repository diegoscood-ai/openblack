/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "Locator.h"

// The hand's tap on an object, one handler per class: the virtuals Object::InterfaceValidToTap (vt 0x740, Object's
// 0x4196B0 = 0) and Object::InterfaceTap (vt 0x744, Object's 0x4196C0 = 1) that GInterface::SendTap 0x5D38A0 and the
// packet 0x20 handler 0x5DA650 call. Each owner registers the classes it ports; an object of no registered class cannot
// be tapped, as with Object's defaults. The hand checks the influence and IsCannotBePickedUp before (SendTap).
// Wiki: docs/bw1-notes/hand-and-interface.md, "Tapping objects".
namespace openblack::ecs::hand_tap
{

/// vt 0x740 InterfaceValidToTap(GInterfaceStatus*): `is` is the tapping interface (the hand's Dropper)
using ValidToTapFn = bool (*)(entt::entity object, const pot_resource::Dropper& is);
/// vt 0x744 InterfaceTap(GInterfaceStatus*): `handPos` is the hand's point (GInterfaceStatus +0xC8)
using TapFn = uint32_t (*)(entt::entity object, const pot_resource::Dropper& is, glm::vec3 handPos);

struct Handler
{
	bool (*isClass)(entt::entity object);
	ValidToTapFn validToTap;
	TapFn tap;
};

inline std::vector<Handler>& Handlers()
{
	static std::vector<Handler> handlers;
	return handlers;
}

/// A class told by a test of its own (a class with no component, such as Rock: Rocks::IsRock). The classes are tested in
/// registration order and the first one the object belongs to answers.
inline void Register(bool (*isClass)(entt::entity object), ValidToTapFn validToTap, TapFn tap)
{
	Handlers().push_back({isClass, validToTap, tap});
}

/// One class = one component
template <typename Component>
void Register(ValidToTapFn validToTap, TapFn tap)
{
	Register([](entt::entity object) { return Locator::entitiesRegistry::value().AllOf<Component>(object); }, validToTap,
	         tap);
}

/// The handler of the object's class, nullptr when none is registered (Object's defaults: not tappable)
inline const Handler* Find(entt::entity object)
{
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		return nullptr;
	}
	for (const auto& handler : Handlers())
	{
		if (handler.isClass(object))
		{
			return &handler;
		}
	}
	return nullptr;
}

/// vt 0x740: false for an object of no registered class (Object::InterfaceValidToTap 0x4196B0)
inline bool ValidToTap(entt::entity object, const pot_resource::Dropper& is)
{
	const auto* handler = Find(object);
	return handler != nullptr && handler->validToTap(object, is);
}

/// vt 0x744: 1 for an object of no registered class (Object::InterfaceTap 0x4196C0)
inline uint32_t Tap(entt::entity object, const pot_resource::Dropper& is, glm::vec3 handPos)
{
	const auto* handler = Find(object);
	return handler != nullptr ? handler->tap(object, is, handPos) : 1;
}

} // namespace openblack::ecs::hand_tap
