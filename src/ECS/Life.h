/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

// Object life in 0..1 (Object +0x48): villagers keep it in Villager::life, rocks and animals in components::Life, the
// rest have none yet (1). Used by the physics impacts, and by the miracles' effects (heal, fire, lightning, worship).

namespace openblack::ecs::life
{
/// Object::GetLife (vt +0x11C, 0x402600)
[[nodiscard]] float LifeOf(entt::entity entity);

/// Villager::SetLife 0x756B40 -> Object::SetLife 0x63A140
void SetLife(entt::entity entity, float life);

/// Object::ReduceLife 0x637810 (villagers and animals don't override it): life - amount, or 0 when the amount is more
/// than the life. Returns the new life. It doesn't kill: that is the caller's (the dying states are not ported).
float ReduceLife(entt::entity entity, float amount);

/// Object::IncreaseLife 0x637870 (Villager::IncreaseLife 0x753460 calls it): up to 1. Returns the new life.
float IncreaseLife(entt::entity entity, float amount);

/// Villager::VillagerDead / Animal::SetDying. TODO(physics): the corpse and the death states; the object goes.
void Kill(entt::entity entity, const char* reason);
} // namespace openblack::ecs::life
