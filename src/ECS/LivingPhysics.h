/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

/// The physics and hand virtuals of the Living classes (Villager and Animal): what a villager or an animal does when it
/// starts flying, is hit, lands or is picked up. The physics (ECS/Physics/PhysicsObjects) and the hand call them
/// through their per-class hooks; the body, its G and its sinking stay with the physics.
namespace openblack::ecs::living
{

/// PhysicsObjects::SetClassHandlers for PhysicsClass::Villager and PhysicsClass::Animal (once, at start-up).
void RegisterPhysicsHandlers();

/// Living::InterfaceSetInMagicHand 0x5ECCB0 (the villager: IN_HAND, its clip SCARED_STIFF) and
/// Animal::InterfaceSetInMagicHand (off its flock, IN_HAND), from GInterface::PlaceObjectInMagicHand. Nothing for an
/// object that is not a Living.
void InterfaceSetInMagicHand(entt::entity entity);

} // namespace openblack::ecs::living
