/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Chants.h"
#include "ECS/Components/Spell.h"
#include "Enums.h"
#include "GameClock.h"
#include "Particles/SpellLink.h"
#include "SpellCastData.h"

namespace openblack
{
struct GMagicInfo;
struct GMagicEffectInfo;
} // namespace openblack

namespace openblack::particles
{
class Effect;
} // namespace openblack::particles

// The spell core: a spell is an entity with a components::Spell; the original's virtuals are a SpellOps table per
// SpellClass, each class in its own Spells/*.cpp. Wiki: docs/wiki/magic/spell-core.md#spell-core.

namespace openblack::magic
{
class MagicWorldInterface;

using ecs::components::SpellClass;

/// A spell class's virtuals (the ones the core calls)
struct SpellOps
{
	/// InitWithPos(creator, pos, castData, psInfo): 1 ok
	int (*initWithPos)(entt::entity spell, const glm::vec3& position, SpellCastData* castData,
	                   const particles::ProcessInfo& info);
	/// InitWithObject: InitWithPos(the object's position), then psys->AddTarget(object)
	int (*initWithObject)(entt::entity spell, entt::entity object, SpellCastData* castData, const particles::ProcessInfo& info);
	/// Process: 5 = delete the spell
	int (*process)(entt::entity spell);
	/// SpellEvent
	int (*spellEvent)(entt::entity spell, const particles::SpellEventInfo& event);
	/// CalculateCostToMaintain
	float (*costToMaintain)(entt::entity spell);
	/// CloseDown
	void (*closeDown)(entt::entity spell);
	/// the class part of ToBeDeleted (before the base's)
	void (*toBeDeleted)(entt::entity spell);
	/// HasEnoughChantsAndLifeForRecast
	bool (*hasEnoughChantsForRecast)(entt::entity spell);
	/// GetParticleType
	ParticleType (*particleType)(entt::entity spell);
	/// GetMaxObjectsToCreate (SpellSeed::StoreChantsAndAgeFromSpell); nullptr: the Spell's maxObjectsToCreate
	int (*maxObjectsToCreate)(entt::entity spell) = nullptr;
	/// UpdateStruckReaction / SetUpDestroyedReaction (on the spell that was hit by another); nullptr: the Spell's,
	/// which do nothing. SpellShield has them.
	void (*updateStruckReaction)(entt::entity spell) = nullptr;
	void (*setUpDestroyedReaction)(entt::entity spell) = nullptr;
	/// ProcessSpellSeed, each turn before Process; nullptr: the Spell's (base::ProcessSpellSeed)
	int (*processSpellSeed)(entt::entity spell) = nullptr;
	/// Whether the hand of the interface of `player` must send its place every turn for this spell; nullptr: the
	/// Spell's (base::NeedsContinualPackets). The flock miracles have their own.
	bool (*needsContinualPackets)(entt::entity spell, PlayerNames player) = nullptr;
};

/// The base Spell's virtuals, for the classes to call
namespace base
{
int InitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const particles::ProcessInfo& info);
int InitWithObject(entt::entity spell, entt::entity object, SpellCastData* castData, const particles::ProcessInfo& info);
int Process(entt::entity spell);
/// The seed follows the spell (its seed's ProcessFromSpell), or the spell lets go of a seed that is gone; returns 1
int ProcessSpellSeed(entt::entity spell);
int SpellEvent(entt::entity spell, const particles::SpellEventInfo& event);
float CalculateCostToMaintain(entt::entity spell);
void CloseDown(entt::entity spell);
void ToBeDeleted(entt::entity spell);
bool HasEnoughChantsAndLifeForRecast(entt::entity spell);
ParticleType GetParticleType(entt::entity spell);
/// A spell cast from the hand, not closed down, cast by a human player, whose player is the interface's own
bool NeedsContinualPackets(entt::entity spell, PlayerNames player);
/// Recharge, close at strength 0, one PSys step; the strength before (psInfo.power)
float CoreProcess(entt::entity spell);
} // namespace base

/// The ops of a class (registered by Spells/*.cpp through RegisterSpellClasses)
[[nodiscard]] const SpellOps& OpsOf(SpellClass spellClass);
void RegisterOps(SpellClass spellClass, const SpellOps& ops);
/// The classes' registration list (Spell.cpp calls it once)
void RegisterSpellClasses();

/// The GMagicInfo class of a magic type: which Spell class AllocSpell makes for it
[[nodiscard]] SpellClass ClassOf(MagicType type);

/// A new spell entity of the magic type's class, pushed at the front of the list
[[nodiscard]] entt::entity AllocSpell(MagicType type, const ecs::components::SpellCreator& creator);
/// A spell a saved game's load made again, as the game's load keeps it: its values as saved (its class taken from its
/// magic type), pushed at the front of the list (made from the list's last to its first, so the list keeps its order),
/// and its effect, made again from its records, taken in as its own with the spell as the owner its rules ask. Nothing
/// is started or counted again
[[nodiscard]] entt::entity RestoreSpell(const ecs::components::Spell& values, std::unique_ptr<particles::Effect> effect);

/// AllocSpell, InitWithPos; a failed init deletes the spell. 1 ok (out = the spell)
int CastAtPos(MagicType type, ecs::components::SpellCreator creator, const glm::vec3& position, entt::entity* out,
              SpellCastData* castData, const particles::ProcessInfo& info);
/// InitWithObject when castOnObject, else CastAtPos(the object's position). The original reads the flag from a seed
/// info entry for no seed type, which lands inside another table; here (openblack) the magic type's first seed's
/// castOnObject is used.
int CastAtObject(MagicType type, ecs::components::SpellCreator creator, entt::entity object, entt::entity* out,
                 SpellCastData* castData, const particles::ProcessInfo& info);

/// Once per game turn (`turn` is game_clock::Turn, which CurrentTurn reads)
void ProcessSpells(unsigned int turn);
/// A spell's upkeep turn: its age and duration; with its caster gone it closes at once and pays nothing; else its
/// caster's update and the turn's chants paid
void ProcessMaintainRequest(entt::entity entity);
/// Whether a miracle held in a hand may still be cast where the hand is: always for one a human player is not casting;
/// else its cast rule at the spell's cast place, for its caster's player
[[nodiscard]] bool CastRuleHoldsWhereHeld(entt::entity spell);

/// ToBeDeleted of a spell: the class part, then the base Spell's, and the entity goes
void DeleteSpell(entt::entity spell);
/// CloseDown through the ops
void CloseDown(entt::entity spell);

/// The game turn (game_clock::Turn)
[[nodiscard]] unsigned int CurrentTurn();

/// The spells, in processing order
[[nodiscard]] const std::vector<entt::entity>& Spells();
/// The first spell of a magic type, in processing order, whose position is strictly within a radius of a map position,
/// measured in x and z only; none for the shields
[[nodiscard]] entt::entity FindSpellAt(MagicType type, const glm::vec3& mapPosition, float radius);

/// The chant context of a spell now (the creator, its tribal power, the seed's power, the upkeep)
[[nodiscard]] chants::Context ChantContextOf(entt::entity spell);
/// The spell's strength from its chants
[[nodiscard]] float GetSpellStrength(entt::entity spell);
/// The caster's tribal power for this spell
[[nodiscard]] float GetTribalPower(entt::entity spell);

[[nodiscard]] const GMagicInfo& MagicInfoOf(entt::entity spell);        ///< The spell's magic info
[[nodiscard]] const GMagicEffectInfo& EffectInfoOf(entt::entity spell); ///< The spell's magic effect info

/// The seed's castType is IN_HAND
[[nodiscard]] bool IsCastFromHand(entt::entity spell);
/// Whether the hand of the interface of `player` must send its place every turn for this spell, as its class asks
[[nodiscard]] bool NeedsContinualPackets(entt::entity spell, PlayerNames player);

/// MapCoords (x, z metres, y above the land) <-> world points (y absolute)
[[nodiscard]] glm::vec3 ToWorld(const glm::vec3& mapPosition);
[[nodiscard]] glm::vec3 ToMap(const glm::vec3& worldPoint);

/// OPENBLACK_SPELL_TRACE
[[nodiscard]] bool TraceEnabled();

/// What the miracles do to the world and ask of it: the miracles' service's world (Locator::magicSystem). A missing
/// service aborts with a message
[[nodiscard]] MagicWorldInterface& TheWorld();

/// A land is loaded: every spell goes
void ClearSpells();
} // namespace openblack::magic
