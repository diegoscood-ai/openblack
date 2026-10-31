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

// Object life in 0..1: villagers keep it in Villager::life, creatures in their body (CreatureNeeds), rocks and animals
// in components::Life, the rest have none yet (1). Used by the physics impacts, and by the miracles' effects (heal,
// fire, lightning, worship). Poison (components::Poisoned) is a life effect too, so it lives here: only the heal
// miracle cures it (the spell's default effect).

namespace openblack::ecs::life
{
/// The object's life (1 without a Life component)
[[nodiscard]] float LifeOf(entt::entity entity);

/// The villager's, the creature's or the object's life set; a thing marked indestructible is protected
/// (SetLifeUnlessProtected)
void SetLife(entt::entity entity, float life);

/// A protected thing keeps its life against one of this or less
constexpr float k_LowestLifeOfAProtectedThing = 0.01f;

/// How a class takes a new life: most through the object's own setter, which keeps a protected thing's life; a tree
/// is felled by a life of none or less before that setter is reached; a pile turns the life into its amount and never
/// reaches it
enum class LifeSetter : uint8_t
{
	Object,
	Tree,
	Pile,
};

/// Whether a thing keeps the life it has against `life`: only a protected one, through the object's setter, against a
/// life of k_LowestLifeOfAProtectedThing or less (or not a number)
[[nodiscard]] constexpr bool KeepsLife(bool protectedFromDeath, LifeSetter setter, float life)
{
	if (!protectedFromDeath || setter == LifeSetter::Pile || (setter == LifeSetter::Tree && life <= 0.0f))
	{
		return false;
	}
	return !(life > k_LowestLifeOfAProtectedThing);
}

/// SetLife, but a thing `protectedFromDeath` (an indestructible thing, or one a script holds while a script owns the
/// widescreen bars) keeps its life as KeepsLife says for its class. A villager's town still counts it among its injured
/// as the life it was given would cross the mark
void SetLifeUnlessProtected(entt::entity entity, float life, bool protectedFromDeath);

/// Life - amount (the same for villagers and animals), or 0 when the amount is more
/// than the life. Returns the new life. It doesn't kill: that is the caller's (the dying states are not ported).
float ReduceLife(entt::entity entity, float amount);
/// Whose life a change of `entity`'s goes to: a village totem's goes to its town centre while it has one (the totem
/// keeps its own), anything else's to itself
[[nodiscard]] constexpr entt::entity LifeTarget(entt::entity entity, bool isTotem, entt::entity townCentre)
{
	return isTotem && townCentre != entt::null ? townCentre : entity;
}

/// Life + amount, up to 1 (the same for villagers). Returns the new life.
float IncreaseLife(entt::entity entity, float amount);

/// A villager's or an animal's death. TODO(physics): the corpse and the death states; the object goes.
void Kill(entt::entity entity, const char* reason);

// ---- poison --------------------------------------------------------------------------------------------------------

/// The living's poisoned bit, here components::Poisoned
[[nodiscard]] bool IsPoisoned(entt::entity entity);

/// The living's poisoned bit (a pot's own flag is components::Pot's).
/// Only a Living (a villager, an animal or a creature) can be poisoned.
void SetPoisoned(entt::entity entity, bool poisoned);

/// The two ways a villager gets poisoned in the original, both when the food *reaches it*, not when it is eaten:
/// a resource added to the villager (FOOD given with the poisoned argument, e.g. a poisoned pile put in its hands) and
/// a resource it takes from an object that is poisoned (a poisoned pot or storage pit). The eating side (the states,
/// the eat animation 0xD4 of eating food, at home or not) belongs to the villagers' code: call this from the place
/// where the poisoned food reaches the villager.
void TakePoisonedResource(entt::entity living);

/// The villager's hunger check: the life a villager loses on every periodic check when it is hungry *or* poisoned.
/// The amount is max(1 - food / hungryForFood, 1) x hungerToLifeMultiplier: it keeps the *bigger* of the two, so with
/// food clamped to >= 0 the food term is dead code and the loss is just hungerToLifeMultiplier.
[[nodiscard]] float HungerLifeLoss(float food, float hungryForFood, float hungerToLifeMultiplier);

/// The poison's harm over time, the half of the hunger check that does not need the hunger states: a poisoned
/// villager takes HungerLifeLoss with its own GVillagerInfo. Returns the life lost (0 when it is not poisoned).
/// Sleeping also skips the sleep's life recovery while it is poisoned (that state is not ported).
float ProcessPoison(entt::entity villager);

/// The poisoned tint (the draw of a Living and a villager): a poisoned Living with no specular colour of its own
/// (components::SpecularColour, which the heal chakra uses and which wins over the tint) is drawn with this diffuse,
/// ARGB (despite the original's name for it, it goes in the diffuse slot, the same one the white 0xFFFFFFFF uses).
constexpr uint32_t k_PoisonDiffuse = 0xFFE8FFDDU;
/// ... and this specular, added to the object's specular channel by channel with saturation. RenderingSystem
/// (DrawColoursOf) packs the pair for vs_object, the villagers' and the poisoned pots'.
constexpr uint32_t k_PoisonSpecular = 0xFF001000U;
} // namespace openblack::ecs::life
