/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerHome.h"

#include <cstdint>
#include <unordered_set>

#include <fmt/format.h>

#include "ECS/Components/Villager.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "Locator.h"

namespace openblack::ecs::villager
{
using namespace components;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}
} // namespace

uint32_t GoHome(entt::entity villager)
{
	// 0x760270: DoGoingHome(0x25, 0xEE)
	return DoGoingHome(villager, VillagerStates::ArrivesHome, VillagerStates::SleepInTent);
}

uint32_t GoHomeState(LivingAction& action)
{
	return GoHome(Entities().ToEntity(action));
}

uint32_t DoGoingHome(entt::entity villager, VillagerStates arrive, [[maybe_unused]] VillagerStates tent)
{
	auto* v = Entities().TryGet<Villager>(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// 0x760289..0x7602A5: IsDancing (vt +0x978) -> RemoveFromDance((flags >> 1) & 1) (vt +0xB08). TODO(V14): openblack's
	// villagers dance only at the worship site (Milagros), not in a DanceGroup that GO_HOME leaves: nothing here
	// 0x7602AD: GetAbode
	const auto abode = v->abode != entt::null && Entities().Valid(v->abode) ? v->abode : entt::null;
	if (abode == entt::null)
	{
		// TODO(V4): 0x760310.. without an abode: with a town (100 m from it, a tent 10..35 m away: 238 `tent`), else
		// 130 VAGRANT_START. Nothing yet: the villager stays in its state (a homeless 36, or 114 via CheckChild)
		static std::unordered_set<int64_t> traced;
		if (TraceOn(villager) && traced.insert(object_index::Of(villager)).second)
		{
			Trace(villager, "home: no abode -> DoGoingHome's homeless branch 0x760310 (TODO V4: nothing)");
		}
		return 1;
	}
	// 0x7602B8..0x7602C5: inside (flags & 4) -> SetTopState(38 AT_HOME). (V4: nobody sets the bit yet)
	if ((v->flags & Villager::k_FlagAtHome) != 0)
	{
		SetTopState(villager, VillagerStates::AtHome);
		return 1;
	}
	// 0x7602CA..0x7602E4: GetFinalState == arrive -> 1 (already going)
	if (GetFinalState(villager) == arrive)
	{
		return 1;
	}
	// 0x7602EA..0x7602FE: SetupMoveToOnFootpath(abode, abode.GetArrivePos(), arrive)
	const auto door = abode_queries::GetArrivePos(abode);
	if (TraceOn(villager))
	{
		const auto m = town_queries::ToMetres(door);
		Trace(villager, fmt::format("home 36: to the door ({:.1f}, {:.1f}) -> {}", m.x, m.y, static_cast<uint32_t>(arrive)));
	}
	SetupMoveToOnFootpath(villager, abode, door, arrive);
	return 1;
}

void SetupMoveToOnFootpath(entt::entity villager, entt::entity object, glm::ivec2 pos, VillagerStates final)
{
	// 0x5EDD39..0x5EDD63: me == object.GetArrivePos() && pos != me -> SetupMoveToWithHug(pos, final)
	const auto me = town_queries::PosOf(villager);
	if (me == abode_queries::GetArrivePos(object) && pos != me)
	{
		SetupMoveToWithHug(villager, town_queries::ToMetres(pos), final);
		return;
	}
	// 0x5EDD72..0x5EDD7D: object.UseFootpathIfNecessary(this, pos, final) (vt +0x80): MultiMapFixed 0x52EEC0 /
	// GameThingWithPos 0x570350 with a GetFootpathLink -> GFootpathLink::UseFootpathIfNecessary 0x5362E0; without one
	// SetupMoveToWithHug(pos, final) (0x57037E). (aproximado, P-9) the footpath walk is not ported: always the direct walk
	SetupMoveToWithHug(villager, town_queries::ToMetres(pos), final);
}
} // namespace openblack::ecs::villager
