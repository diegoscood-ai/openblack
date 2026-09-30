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
#include <glm/vec3.hpp>

#include "ECS/Components/Spell.h"
#include "Enums.h"

// CHL natives of the spells, called from CHLApi.cpp. They pop and push the VM stack themselves.

namespace openblack::magic::script
{
/// GScript::CastSpellAtPos 0x70BD60 (pos, magic, from, creator, check, radius, time, curl, dir): creator none = the
/// neutral player; with `check` the class's CanCast at pos (vt 0x30) first; castData {radius, initialChants, time, -1};
/// PSysProcessInfo {+0x0C from, +0x18 pos - from, +0x24 dir, power 1, +0x34 curl, enabled}. Positions are world points
/// (the script's vectors; MapCoords(LHPoint) keeps x, z). The spell, or entt::null.
entt::entity CastSpellAtPos(const glm::vec3& position, MagicType magic, const glm::vec3& from,
                            ecs::components::SpellCreator creator, bool check, float radius, float time, float curl,
                            const glm::vec3& direction);

/// 195 SPELL_AT_THING, GScript::SpellAtThing 0x70BFA0
void SpellAtThing();
/// 196 SPELL_AT_POS, GScript::SpellAtPos 0x70C190 -> GScript::CastSpellAtPos 0x70BD60
void SpellAtPos();
/// 227 SPELL_AT_POINT, GScript::SpellAtPoint 0x70C560
void SpellAtPoint();
/// 244 SET_PLAYER_MAGIC, GScript::SetPlayerMagic 0x70C6C0
void SetPlayerMagic();
/// 245 HAS_PLAYER_MAGIC, GScript::HasPlayerMagic 0x70C750
void HasPlayerMagic();
/// 293 PLAYER_SPELL_CAST_TIME, GScript::PlayerSpellCastTime 0x70C9A0
void PlayerSpellCastTime();
/// 294 PLAYER_SPELL_LAST_CAST, GScript::PlayerSpellLastCast 0x70CA50
void PlayerSpellLastCast();
/// 295 GET_LAST_SPELL_CAST_POS, 0x70CAB0
void GetLastSpellCastPos();
/// 403 GET_MANA_FOR_SPELL, GScript::GetManaForSpell 0x70CD40
void GetManaForSpell();
} // namespace openblack::magic::script
