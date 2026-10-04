/*******************************************************************************
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

// The town's graveyard (Graveyard.cpp of runblack.exe W120, 0x595CB0..0x595FB0) and the town's pointer to it (Town
// +0x748). Its dead count +0xC4 drives a 3-bit graves stage on the 3D object.

namespace openblack::ecs::components
{
/// Graveyard +0xC4: the dead buried here (fn_00595E50 counts up while below 50; saved by Graveyard::Save 0x595EE0)
struct Graveyard
{
	uint32_t dead {0};
	/// the graves stage put on the LH3DObject (+4 bits 21..23, vt +0x1D8 fn_007F9C40); (pending) what the draw shows
	/// for it
	uint8_t gravesStage {0};
};
} // namespace openblack::ecs::components

namespace openblack::ecs::graveyard
{
/// [0x8C6CA4] = 50.0: fn_00595E50 counts only while the dead are below it
constexpr float k_MaxDead = 50.0f;
/// [0x9003E0] = 0.18: graves stage = ftol(dead x this), at least 1 once someone is buried
constexpr float k_GravesPerDead = 0.18f;

/// Town::GetGraveyard: Town +0x748 (entt::null when none)
[[nodiscard]] entt::entity GetGraveyard(entt::entity town);
/// fn_0073D690 (Town::SetGraveyard): +0x748 = g when +0x748 is null or g is null (0x73D690..0x73D6A2): the first
/// functional graveyard stays, a null clears it
void SetGraveyard(entt::entity town, entt::entity graveyard);
/// fn_00595E50 (Graveyard::AddDead): with a town and IsFunctional (vt +0xD4), and (float)+0xC4 < 50 (fild, fcomp
/// [0x8C6CA4], 0x595E7F..0x595E8E): +0xC4++, stage = ftol(+0xC4 x 0.18) ([0x9003E0]), 0 with someone buried -> 1
/// (0x595EB5..0x595EBD), LH3DObject vt +0x1D8(stage) = +4 bits 21..23 (fn_007F9C40, & 7: stage 8 shows as 0). Callers:
/// Graveyard::MakeFunctional 0x595E3A and the town's death book-keeping fn_0073E440 0x73E48B (Personas, V12)
void AddDead(entt::entity graveyard);
/// Graveyard::MakeFunctional 0x595E00: Abode::MakeFunctional 0x4047E0 first (the caller's), then with a town whose
/// +0x748 is null SetGraveyard(this) (0x595E1A..0x595E33), then AddDead (0x595E3A, literal: one dead counted)
void MakeFunctional(entt::entity graveyard);
/// Graveyard::DeleteDependancys 0x595CE0 (from ToBeDeleted 0x595CB0, vt +0x910): with a town whose +0x748 is this one,
/// the town's abode list (+0x754, next +0x9C) is searched for the first other functional abode whose ABODE_TYPE (info
/// +0x120) has bit 2 or bit 9 (`test ..., 0x204`, 0x595D17: literal, so any civic building such as the storage pit
/// passes, not only a graveyard), IsFunctional (vt +0xD4), not this one (0x595D14..0x595D3D); SetGraveyard(null),
/// SetGraveyard(found or null) (0x595D3F..0x595D59); then Abode::DeleteDependancys 0x403F00
void DeleteDependancys(entt::entity graveyard);
} // namespace openblack::ecs::graveyard
