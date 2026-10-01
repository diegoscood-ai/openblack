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

#include <entt/entity/entity.hpp>

// Object life in 0..1 (Object +0x48): villagers keep it in Villager::life, rocks and animals in components::Life, the
// rest have none yet (1). Used by the physics impacts, and by the miracles' effects (heal, fire, lightning, worship).
// Poison (Living +0xB4 bit 1, components::Poisoned) is a life effect too, so it lives here: only the heal miracle
// cures it (Spell::ApplyDefaultSpellEffect 0x720E34).

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

// ---- poison (Living +0xB4 bit 1) ---------------------------------------------------------------------------------

/// Living::IsPoisoned 0x416F90 (vt 0x4A4): bit 1 of Living +0xB4, here components::Poisoned
[[nodiscard]] bool IsPoisoned(entt::entity entity);

/// Living::SetPoisoned 0x416FA0 (vt 0x69C; Object::SetPoisoned 0x402780 is the pots' own flag, components::Pot).
/// Only a Living (villager or animal) can be poisoned.
void SetPoisoned(entt::entity entity, bool poisoned);

/// The two ways a villager gets poisoned in the original, both when the food *reaches it*, not when it is eaten:
/// Villager::AddResource 0x7564D0 (0x7564E5..0x7564F3: FOOD given with the poisoned argument, e.g. a poisoned pile put
/// in its hands) and Villager::GetResourceFrom 0x753390 (0x7533E8..0x7533FC: taking a resource from an object whose
/// IsPoisoned is 1, Pot::IsPoisoned 0x55D4E0 / StoragePit::IsPoisonedResource 0x733550). The eating side (the states,
/// the eat animation 0xD4 of Villager::EatFood 0x75C00E / EatFoodAtHome 0x75C0BE) belongs to the villagers' session:
/// call this from the place where the poisoned food reaches the villager.
void TakePoisonedResource(entt::entity living);

/// Villager::CheckHungry 0x75BCC0: the life a villager loses on every periodic check when it is hungry *or* poisoned
/// (0x75BD83..0x75BD9E). The amount (0x75BDA6..0x75BDE0) is max(1 - food / hungryForFood, 1) x
/// hungerToLifeMultiplier: 0x75BDBB..0x75BDCA keeps the *bigger* of the two (fcom + test ah,0x41 + je), so with food
/// clamped to >= 0 (0x75BD59..0x75BD6E) the food term is dead code and the loss is just hungerToLifeMultiplier.
[[nodiscard]] float HungerLifeLoss(float food, float hungryForFood, float hungerToLifeMultiplier);

/// The poison's harm over time, the half of Villager::CheckHungry that does not need the hunger states: a poisoned
/// villager takes HungerLifeLoss with its own GVillagerInfo. Returns the life lost (0 when it is not poisoned).
/// Villager::DoSleeping 0x760DB6 also skips the sleep's life recovery while it is poisoned (that state is not ported).
float ProcessPoison(entt::entity villager);

/// The poisoned tint, fn_0051B3D0 0x51B43D..0x51B45D (the Draw helper of Living 0x51AEEC and Villager 0x51BA74):
/// a poisoned Living with no specular colour of its own (Living +0xD0, components::SpecularColour, which the heal
/// chakra uses and which wins over the tint) is drawn with this diffuse, ARGB, from 0x51BB50 (the symbol file calls it
/// Pot::GetPoisonSpecular, but it goes in the diffuse slot of fn_0080BEC0, the same one the white 0xFFFFFFFF uses).
constexpr uint32_t k_PoisonDiffuse = 0xFFE8FFDDU;
/// ... and this specular, from 0x51BB60 (the symbol file calls it Object::GetFireEffect): fn_0080BF10 adds it to the
/// object's colour channel by channel with saturation (0x80BF2D..0x80BF68). Data only: drawing the tint is the
/// shaders' session (LH3DColor), nothing reads these two yet.
constexpr uint32_t k_PoisonSpecular = 0xFF001000U;
} // namespace openblack::ecs::life
