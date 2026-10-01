/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Chants.h"
#include "ECS/Components/Spell.h"
#include "Enums.h"
#include "GameClock.h"
#include "PSys/SpellLink.h"
#include "SpellCastData.h"

namespace openblack
{
struct GMagicInfo;
struct GMagicEffectInfo;
} // namespace openblack

// The spell core (Spell.cpp 0x71FB40..0x722070): a spell is an entity with a components::Spell; the original's
// virtuals are a SpellOps table per SpellClass, each class in its own Spells/*.cpp. Wiki: docs/bw1-notes/magic.md.

namespace openblack::magic
{
using ecs::components::SpellClass;

/// Spell vt 0x504..0x550 (the ones the core calls)
struct SpellOps
{
	/// vt 0x534 InitWithPos(creator, pos, castData, psInfo): 1 ok
	int (*initWithPos)(entt::entity spell, const glm::vec3& position, SpellCastData* castData,
	                   const psys::ProcessInfo& info);
	/// vt 0x538 InitWithObject: InitWithPos(the object's position), then psys->AddTarget(object)
	int (*initWithObject)(entt::entity spell, entt::entity object, SpellCastData* castData, const psys::ProcessInfo& info);
	/// vt 0x528 Process: 5 = delete the spell
	int (*process)(entt::entity spell);
	/// vt 0x52C SpellEvent
	int (*spellEvent)(entt::entity spell, const psys::SpellEventInfo& event);
	/// vt 0x53C CalculateCostToMaintain
	float (*costToMaintain)(entt::entity spell);
	/// vt 0x530 CloseDown
	void (*closeDown)(entt::entity spell);
	/// the class part of vt 0xC ToBeDeleted (before the base's)
	void (*toBeDeleted)(entt::entity spell);
	/// vt 0x548 HasEnoughChantsAndLifeForRecast
	bool (*hasEnoughChantsForRecast)(entt::entity spell);
	/// vt 0x504 GetParticleType
	ParticleType (*particleType)(entt::entity spell);
	/// vt 0x550 GetMaxObjectsToCreate (SpellSeed::StoreChantsAndAgeFromSpell); nullptr: the Spell's maxObjectsToCreate
	int (*maxObjectsToCreate)(entt::entity spell) = nullptr;
	/// vt 0x51C UpdateStruckReaction / vt 0x520 SetUpDestroyedReaction (SpellHitSpell fn_00720B70 on the spell that was
	/// hit); nullptr: the Spell's, which do nothing (0x55CE10 / 0x55CE20). SpellShield has them.
	void (*updateStruckReaction)(entt::entity spell) = nullptr;
	void (*setUpDestroyedReaction)(entt::entity spell) = nullptr;
};

/// The base Spell's virtuals, for the classes to call
namespace base
{
int InitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info);
int InitWithObject(entt::entity spell, entt::entity object, SpellCastData* castData, const psys::ProcessInfo& info);
int Process(entt::entity spell);
int SpellEvent(entt::entity spell, const psys::SpellEventInfo& event);
float CalculateCostToMaintain(entt::entity spell);
void CloseDown(entt::entity spell);
void ToBeDeleted(entt::entity spell);
bool HasEnoughChantsAndLifeForRecast(entt::entity spell);
ParticleType GetParticleType(entt::entity spell);
/// Spell::CoreProcess 0x720660: recharge, close at strength 0, one PSys step; the strength before (psInfo.power)
float CoreProcess(entt::entity spell);
} // namespace base

/// The ops of a class (registered by Spells/*.cpp through RegisterSpellClasses)
[[nodiscard]] const SpellOps& OpsOf(SpellClass spellClass);
void RegisterOps(SpellClass spellClass, const SpellOps& ops);
/// The classes' registration list (Spell.cpp calls it once)
void RegisterSpellClasses();

/// The GMagicInfo class of a magic type (core.md §1): which Spell class its AllocSpell (vt 0x34) makes
[[nodiscard]] SpellClass ClassOf(MagicType type);

/// GMagicInfo::AllocSpell (vt 0x34) -> the Spell ctor 0x71FB40: a new spell entity, pushed at the front of the list
[[nodiscard]] entt::entity AllocSpell(MagicType type, const ecs::components::SpellCreator& creator);

/// GMagicInfo::CastAtPos fn_005FB490: AllocSpell, InitWithPos; a failed init deletes the spell. 1 ok (out = the spell)
int CastAtPos(MagicType type, ecs::components::SpellCreator creator, const glm::vec3& position, entt::entity* out,
              SpellCastData* castData, const psys::ProcessInfo& info);
/// fn_005FB520: InitWithObject when castOnObject, else CastAtPos(the object's position). The original reads the flag at
/// GetSpellSeedInfo(spellSeedType = -1) + 0x118 = 0xD9D600, inside GSpellIconInfo[1] (research R2); here the magic
/// type's first seed's castOnObject is used (PLAN §4.2 R2).
int CastAtObject(MagicType type, ecs::components::SpellCreator creator, entt::entity object, entt::entity* out,
                 SpellCastData* castData, const psys::ProcessInfo& info);

/// Spell::ProcessSpells 0x720300: once per game turn (`turn` is game_clock::Turn, which CurrentTurn reads)
void ProcessSpells(unsigned int turn);

/// ToBeDeleted (vt 0xC) of a spell: the class part, then the base Spell::ToBeDeleted 0x71FD90, and the entity goes
void DeleteSpell(entt::entity spell);
/// CloseDown (vt 0x530) through the ops
void CloseDown(entt::entity spell);

/// The game turn (g_game +0x205A40, game_clock::Turn)
[[nodiscard]] unsigned int CurrentTurn();

/// The spells, in processing order (g_game +0x205BC4)
[[nodiscard]] const std::vector<entt::entity>& Spells();

/// The chant context of a spell now (the creator, its tribal power, the seed's power, the upkeep)
[[nodiscard]] chants::Context ChantContextOf(entt::entity spell);
/// Spell::GetSpellStrength 0x720750
[[nodiscard]] float GetSpellStrength(entt::entity spell);
/// Spell::GetTribalPower 0x7216F0
[[nodiscard]] float GetTribalPower(entt::entity spell);

[[nodiscard]] const GMagicInfo& MagicInfoOf(entt::entity spell);        ///< Spell::GetMagicInfo 0x7201D0
[[nodiscard]] const GMagicEffectInfo& EffectInfoOf(entt::entity spell); ///< GMagicEffectInfo::GetInfo 0x720730

/// Spell::IsCastFromHand 0x721510: the seed's castType is IN_HAND
[[nodiscard]] bool IsCastFromHand(entt::entity spell);

/// The turn length (*(u32*)0xD01A38, game_clock::k_MsPerTurn)
constexpr unsigned int k_TurnMs = game_clock::k_MsPerTurn;

/// MapCoords (x, z metres, y above the land) <-> world points (LHPoint, y absolute): MapCoords::GetLHPoint 0x605C40,
/// MapCoords(LHPoint) 0x603160
[[nodiscard]] glm::vec3 ToWorld(const glm::vec3& mapPosition);
[[nodiscard]] glm::vec3 ToMap(const glm::vec3& worldPoint);

/// OPENBLACK_SPELL_TRACE
[[nodiscard]] bool TraceEnabled();

/// A land is loaded: every spell goes
void ClearSpells();
} // namespace openblack::magic
