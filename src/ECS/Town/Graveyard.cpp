/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Graveyard.h"

#include "ECS/Abodes.h"
#include "ECS/Components/Town.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownStats.h"
#include "Locator.h"

namespace openblack::ecs::graveyard
{
namespace
{
components::Town* TownComponent(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	return town != entt::null && registry.Valid(town) ? registry.TryGet<components::Town>(town) : nullptr;
}
} // namespace

entt::entity GetGraveyard(entt::entity town)
{
	const auto* t = TownComponent(town);
	return t != nullptr ? t->graveyard : entt::null;
}

void SetGraveyard(entt::entity town, entt::entity graveyard)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	// fn_0073D690: +0x748 == 0 -> set; else only a null argument is written (0x73D69C..0x73D6A2)
	if (t->graveyard == entt::null || graveyard == entt::null)
	{
		t->graveyard = graveyard;
	}
}

void AddDead(entt::entity graveyard)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* g = graveyard != entt::null && registry.Valid(graveyard) ? registry.TryGet<components::Graveyard>(graveyard)
	                                                               : nullptr;
	// 0x595E58..0x595E6B: a town and IsFunctional (vt +0xD4)
	if (g == nullptr || abode_villagers::TownOf(graveyard) == entt::null || !abode_queries::IsFunctional(graveyard))
	{
		return;
	}
	// 0x595E6D..0x595E8E: fild qword +0xC4 (unsigned), fcomp 50: only below
	if (!(static_cast<float>(g->dead) < k_MaxDead))
	{
		return;
	}
	// 0x595E91..0x595EBD: +0xC4 = n + 1; stage = ftol((n + 1) x 0.18); stage 0 with someone buried -> 1
	++g->dead;
	auto stage = static_cast<int32_t>(static_cast<float>(g->dead) * k_GravesPerDead);
	if (stage == 0 && g->dead != 0)
	{
		stage = 1;
	}
	// 0x595EBE..0x595EC5: Game3DObject vt +0x1D8(edx = stage) = fn_007F9C40: +4 bits 21..23 = stage & 7
	g->gravesStage = static_cast<uint8_t>(stage & 7);
}

void MakeFunctional(entt::entity graveyard)
{
	// 0x595E03 Abode::MakeFunctional is the caller's (abodes::MakeFunctional runs the class part after it)
	const auto town = abode_villagers::TownOf(graveyard);
	// 0x595E0C..0x595E33: with a town whose +0x748 is null -> SetGraveyard(this)
	if (town != entt::null && GetGraveyard(town) == entt::null)
	{
		SetGraveyard(town, graveyard);
	}
	// 0x595E3A: fn_00595E50
	AddDead(graveyard);
}

void DeleteDependancys(entt::entity graveyard)
{
	const auto town = abode_villagers::TownOf(graveyard);
	// 0x595CE5..0x595CFC: a town whose +0x748 is this graveyard
	if (town == entt::null)
	{
		return;
	}
	entt::entity found = entt::null;
	if (GetGraveyard(town) == graveyard)
	{
		// 0x595D05..0x595D3D: the first other functional abode whose ABODE_TYPE has bit 2 or bit 9 (test 0x204)
		for (const auto abode : town_stats::AbodesOf(town))
		{
			const auto type = abodes::TypeOf(abode);
			const auto bits = type.has_value() ? static_cast<uint32_t>(*type) : 0u;
			if ((bits & 0x204u) != 0 && abode_queries::IsFunctional(abode) && abode != graveyard)
			{
				found = abode;
				break;
			}
		}
	}
	// 0x595D3F..0x595D59: SetGraveyard(0) then SetGraveyard(esi): esi is the one found, 0 when the town's graveyard was
	// another one (esi = 0 from 0x595CF1) or nothing was found
	SetGraveyard(town, entt::null);
	SetGraveyard(town, found);
	// 0x595D61: Abode::DeleteDependancys 0x403F00 is the caller's
}
} // namespace openblack::ecs::graveyard
