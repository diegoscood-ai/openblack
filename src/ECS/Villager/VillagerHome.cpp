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

#include <optional>
#include <unordered_set>

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>

#include "ECS/Components/Abode.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/DetailMeshes.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/SmokyStuff.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerFood.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

// VillagerHome.cpp / Villager.cpp / VillagerStates.cpp of runblack.exe W120 (VillagerHome.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;
using state_info::StateInfo;

namespace
{
std::function<entt::entity(glm::ivec2, float)> g_NearestTreeForTests;
std::function<uint32_t(glm::ivec2)> g_CollideForTests;

/// DoGoingHome 0x76034D: more than 100 m (0x8AB41C) from the town -> towards it
constexpr float k_FarFromTown = 100.0f;
/// GetTentPos 0x760505: fn_00604AF0's radius 50 (0x42480000); 0x7605DC the 238 villagers within 5 (0x8AB6E4)
constexpr float k_TreeRadius = 50.0f;
constexpr float k_TentNeighbour = 5.0f;
/// GetTentPos 0x760584: MapCoords::Collide(0x19) (0x10 off the game map, 8 a fixed, 1 water)
constexpr uint32_t k_TentCollide = 0x19;
/// GetTentPos: 3 tries (byte [esp + 0x13], 0x7606AB `cmp al, 3`), 9 spiral cells each (0x7605A1)
constexpr int k_TentTries = 3;
constexpr int k_TentCells = 9;
/// fn_0074C650: 9 cells (0x74C685), occupants within 4 (0x8AB418), the tent 2 m (0x40000000) from the tree
constexpr int k_TreeCells = 9;
constexpr float k_TreeOccupant = 4.0f;
constexpr float k_TentFromTree = 2.0f;
/// pi / 8 (0x8C6CA0 = 0.392699)
constexpr float k_EighthPi = 0.39269909f;
/// VagrantStart 0x76A8DB: towns within 200 (0x43480000)
constexpr float k_VagrantTownRadius = 200.0f;
/// GoHomeAndChange 0x761887: a scale below 0.95 (0x8CF000) is reset for the age
constexpr float k_GrownUpScale = 0.95f;
/// CheckNeedsAtHome 0x760204: x 0.9 (0x8C5844)
constexpr float k_NeedsAtHome = 0.9f;
/// VillagerDisciple 10 CHANGE_HOUSE (ExitGoHomeAndChange 0x7619D9 `cmp byte +0xF2, 0xA`)
constexpr uint8_t k_DiscipleChangeHouse = 10;
/// VillagerDisciple 5 BREEDER (HomeDecideWhatToDo 0x75FF1D `cmp cl, 5`)
constexpr uint8_t k_DiscipleBreeder = 5;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

/// Villager::GetAbode 0x752160: +0x128 (entt::null without one, or when it is gone)
entt::entity AbodeOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr && v->abode != entt::null && Entities().Valid(v->abode) ? v->abode : entt::null;
}

/// Villager::GetTown 0x751F00 (vt +0x48): +0x12C
entt::entity TownOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr && v->town != entt::null && Entities().Valid(v->town) && Entities().AllOf<Town>(v->town)
	           ? v->town
	           : entt::null;
}

bool Inside(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr && (v->flags & Villager::k_FlagAtHome) != 0;
}

std::string Xz(glm::ivec2 pos)
{
	const auto m = tq::ToMetres(pos);
	return fmt::format("({:.1f}, {:.1f})", m.x, m.y);
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (TraceOn(villager))
	{
		Trace(villager, line);
	}
}

/// Living::SetupMoveToWithHug on a MapCoords goal
uint32_t MoveTo(entt::entity villager, glm::ivec2 goal, VillagerStates final)
{
	return SetupMoveToWithHug(villager, tq::ToMetres(goal), final);
}

/// The state counter (Object +0x58, LivingAction::turnsUntilStateChange)
uint16_t& Counter(entt::entity villager)
{
	return Entities().Get<LivingAction>(villager).turnsUntilStateChange;
}

/// GameThingWithPos::IsVillager (vt +0x2C8: Villager 0x55CAB0 1, the rest 0)
bool IsVillagerObject(entt::entity object)
{
	return Entities().AllOf<Villager>(object);
}

/// The TOP state (+0x8C) of a villager
VillagerStates TopOf(entt::entity object)
{
	const auto* action = Entities().TryGet<const LivingAction>(object);
	return action != nullptr ? static_cast<VillagerStates>(action->states.at(0)) : VillagerStates::InvalidState;
}

/// GameThingWithPos::IsTree (vt +0x338): Tree::IsTree 0x55D9D0 (1), GameThingWithPos 0x402320 (0). (inferido) the
/// subclasses of Tree are openblack's Tree component; DeadTree is another class
bool IsTree(entt::entity object)
{
	return Entities().AllOf<Tree>(object);
}

uint32_t CollideAt(glm::ivec2 pos)
{
	if (g_CollideForTests)
	{
		return g_CollideForTests(pos);
	}
	// MapCoords::Collide(COLLIDE_TYPE) 0x6033B0 = Collide 0x6033C0 & the type
	return map_cells::Collide(map_coords::MapCoords {pos.x, pos.y, 0.0f});
}

/// MapCoords += JustMapXZ 0x605470 on the x / z MapCoords pair
void AddCell(glm::ivec2& pos, const map_coords::JustMapXZ& step)
{
	map_coords::MapCoords c {pos.x, pos.y, 0.0f};
	map_coords::AddCells(c, step);
	pos = {c.x, c.z};
}

/// The villager's Object +0x14 as MapCoords x / z
glm::ivec2 Me(entt::entity villager)
{
	return tq::PosOf(villager);
}

/// Object::GetYAngle 0x402500 (vt +0x508): +0x4C. (aproximado) openblack's WallHug::yAngle, in radians
float YAngleOf(entt::entity villager)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(villager);
	return wallHug != nullptr ? wallHug->yAngle : 0.0f;
}
} // namespace

// ---- the villager's links ----------------------------------------------------------------------------------------

void SetAbode(entt::entity villager, entt::entity abode)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return;
	}
	// 0x750DEA: +0x128 = abode; 0x750DF0 SetTown(0); 0x750DF5..0x750E03: with an abode SetTown(abode.GetTown())
	v->abode = abode;
	v->town = entt::null;
	if (abode != entt::null)
	{
		v->town = abode_villagers::TownOf(abode);
	}
}

void SetTown(entt::entity villager, entt::entity town)
{
	if (auto* v = VillagerOf(villager))
	{
		v->town = town; // 0x756534
	}
}

bool IsAtHome(entt::entity villager)
{
	return Inside(villager);
}

bool IsReachable(entt::entity villager)
{
	// 0x756465 IsAvailable (Villager 0x751D50: not being deleted, the final state not 14 DYING): a corpse playing its
	// dying clip (TOP 23, FINAL 15) or lying in 15 is reachable (literal, V12 spec §5.6)
	if (!IsAvailable(villager))
	{
		return false;
	}
	// 0x75646E..0x75647A: flags & 4 -> 0
	if (Inside(villager))
	{
		return false;
	}
	// 0x756480..0x756489: +0x24 & 4 -> 0: in the hand (PlaceObjectInMagicHand 0x5FB014 `or byte [esi+0x24], 4`;
	// fire::traits::InHand, as VillagerReactions.cpp and Influence.cpp read it)
	if (fire::traits::InHand(villager))
	{
		return false;
	}
	// 0x75648F..0x75649A: TOP != 236 (0xEC)
	return TopOf(villager) != VillagerStates::GoAndHideInNearbyBuilding;
}

bool IsVillagerAvailable(entt::entity villager)
{
	// 0x752293: +0x25 & 4 (controlled by a script) -> 0
	if (script_held::IsControlledByScript(villager))
	{
		return false;
	}
	// 0x752299 IsAvailableForStateChange (GameThingWithPos 0x401A30: !(+0x24 & 4), in the hand: 0x5FB014)
	if (fire::traits::InHand(villager))
	{
		return false;
	}
	// 0x7522A5 GetVillagerAvailableState 0x751F40 (the file 0xA8 of GetFinalState's row) & 1
	return (state_info::AvailableState(StateInfo(GetFinalState(villager))) & 1) != 0;
}

void ArriveHome(entt::entity villager)
{
	// 0x751FA3..0x751FBC: with an abode, flags |= 4, Abode::ArriveHome 0x405FA0
	const auto abode = AbodeOf(villager);
	if (abode == entt::null)
	{
		return;
	}
	auto* v = VillagerOf(villager);
	v->flags = static_cast<uint16_t>(v->flags | Villager::k_FlagAtHome);
	abode_villagers::ArriveHome(abode);
}

void LeaveHome(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	// 0x751FD3..0x751FDE: only when inside
	if (v == nullptr || (v->flags & Villager::k_FlagAtHome) == 0)
	{
		return;
	}
	// 0x751FE0: flags &= 0xDFFB (4 and 0x2000); 0x751FEC..0x751FFE: with an abode, Abode::LeaveHome 0x405FB0
	v->flags = static_cast<uint16_t>(v->flags & 0xDFFB);
	if (const auto abode = AbodeOf(villager); abode != entt::null)
	{
		abode_villagers::LeaveHome(abode);
	}
}

// ---- going home --------------------------------------------------------------------------------------------------

uint32_t GoHome(entt::entity villager)
{
	// 0x760270: DoGoingHome(0x25, 0xEE)
	return DoGoingHome(villager, VillagerStates::ArrivesHome, VillagerStates::SleepInTent);
}

uint32_t GoHomeState(LivingAction& action)
{
	return GoHome(Entities().ToEntity(action));
}

uint32_t DoGoingHome(entt::entity villager, VillagerStates arrive, VillagerStates tent)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// 0x760289..0x7602A5: IsDancing (vt +0x978) -> RemoveFromDance((flags >> 1) & 1) (vt +0xB08). TODO(baile):
	// openblack's villagers dance only at the worship site (milagros2), not in a DanceGroup that GO_HOME leaves
	// 0x7602AD: GetAbode
	const auto abode = AbodeOf(villager);
	if (abode != entt::null)
	{
		// 0x7602B8..0x7602C5: inside (flags & 4) -> SetTopState(38 AT_HOME)
		if (Inside(villager))
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
		TraceIf(villager, fmt::format("home 36: to the door {} -> {}", Xz(door), static_cast<uint32_t>(arrive)));
		SetupMoveToOnFootpath(villager, abode, door, arrive);
		return 1;
	}
	// 0x760310..0x76031B: no town -> SetTopState(130 VAGRANT_START)
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		TraceIf(villager, "home 36: no abode -> vagrant 130");
		SetTopState(villager, VillagerStates::VagrantStart);
		return 1;
	}
	const auto me = Me(villager);
	const auto townPos = tq::PosOf(town);
	// 0x760343..0x76035B: GetDistanceInMetres (0x74CD50) > 100 (test ah, 0x41; jne: <= 100 or unordered -> near)
	if (tq::GetDistanceInMetres(me, townPos) > k_FarFromTown)
	{
		// 0x76036B..0x7603EC: the TOP (+0x8C) read first; a = (GameFloatRand(pi/2) (0x127) - pi/4) + Get3DAngleFromXZ(town,
		// me); d = GameFloatRand(25) (0x128) + 10; pos = town + GetPosFromAngle(a, d); SetupMoveToWithHug(pos, TOP)
		const auto top = TopOf(villager);
		const float spread = GameFloatRand(glm::half_pi<float>()) - glm::quarter_pi<float>();
		const float angle = tq::Get3DAngleFromXZ(townPos, me) + spread;
		const float distance = GameFloatRand(25.0f) + 10.0f;
		const auto pos = townPos + tq::GetPosFromAngle(angle, distance);
		TraceIf(villager, fmt::format("home 36: no abode -> far(town d={:.1f}) {}", tq::GetDistanceInMetres(me, townPos), Xz(pos)));
		MoveTo(villager, pos, top);
		return 1;
	}
	// 0x7603F1..0x76043D: d = GameFloatRand(8) (0x12C) + 2, a = GameFloatRand(2 pi) (0x12C): pos = me + GetPosFromAngle
	const float near = GameFloatRand(8.0f) + 2.0f;
	const float around = GameFloatRand(glm::two_pi<float>());
	auto pos = me + tq::GetPosFromAngle(around, near);
	// 0x760442..0x760450: GetTentPos(pos) -> SetupMoveToWithHug(pos, tent)
	if (GetTentPos(villager, pos))
	{
		TraceIf(villager, fmt::format("home 36: no abode -> tent {}", Xz(pos)));
		MoveTo(villager, pos, tent);
		return 1;
	}
	// 0x760452..0x7604B9: the TOP read; d = GameFloatRand(20) (0x130) + 10, a = GameFloatRand(2 pi) (0x130); pos +=
	// GetPosFromAngle(a, d); SetupMoveToWithHug(pos, TOP): a stroll, then 36 again
	const auto top = TopOf(villager);
	const float strollDistance = GameFloatRand(20.0f) + 10.0f;
	const float strollAngle = GameFloatRand(glm::two_pi<float>());
	pos += tq::GetPosFromAngle(strollAngle, strollDistance);
	TraceIf(villager, fmt::format("home 36: no abode -> wander {}", Xz(pos)));
	MoveTo(villager, pos, top);
	return 1;
}

void SetupMoveToOnFootpath(entt::entity villager, entt::entity object, glm::ivec2 pos, VillagerStates final)
{
	// 0x5EDD39..0x5EDD63: me == object.GetArrivePos() && pos != me -> SetupMoveToWithHug(pos, final)
	const auto me = Me(villager);
	if (me == abode_queries::GetArrivePos(object) && pos != me)
	{
		SetupMoveToWithHug(villager, tq::ToMetres(pos), final);
		return;
	}
	// 0x5EDD72..0x5EDD7D: object.UseFootpathIfNecessary(this, pos, final) (vt +0x80): MultiMapFixed 0x52EEC0 /
	// GameThingWithPos 0x570350 with a GetFootpathLink -> GFootpathLink::UseFootpathIfNecessary 0x5362E0; without one
	// SetupMoveToWithHug(pos, final) (0x57037E). (aproximado, P-9) the footpath walk is not ported: always the direct walk
	SetupMoveToWithHug(villager, tq::ToMetres(pos), final);
}

uint32_t ArrivesHome(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// 0x760938..0x760941: no abode -> SetTopState(0x81 129 HOMELESS_START); 0
	const auto abode = AbodeOf(villager);
	if (abode == entt::null)
	{
		TraceIf(villager, "home 37: no abode -> 129");
		SetTopState(villager, VillagerStates::HomelessStart);
		return 0;
	}
	const auto door = abode_queries::GetArrivePos(abode);
	const auto in = [villager, abode]() {
		// 0x760A91..0x760A9E: ArriveHome; SetTopState(0x26 38)
		ArriveHome(villager);
		TraceIf(villager, fmt::format("home 37: arrive (present {})", abode_villagers::PresentAtHome(abode)));
		SetTopState(villager, VillagerStates::AtHome);
		return 1u;
	};
	// 0x760947..0x760965: AreWeThere(door, 0) (vt +0x85C) -> else SetupMoveToOnFootpath(abode, door, 0x25 37): the literal
	// 37, also from 249
	if (!AreWeThere(villager, tq::ToMetres(door), 0.0f))
	{
		TraceIf(villager, "home 37: not there");
		SetupMoveToOnFootpath(villager, abode, door, VillagerStates::ArrivesHome);
		return 1;
	}
	// 0x76096B..0x760985: IsBuilt (vt +0x890) && IsRepaired (vt +0x88C: Abode 0x4016A0, the life (vt +0x884) not below 1)
	if (abode_queries::IsBuilt(abode) && !(life::LifeOf(abode) < 1.0f))
	{
		return in();
	}
	const auto& info = InfoOf(villager);
	// 0x76098B..0x7609A3: life < DamageThresholdToGoHome (+0x35C; test ah, 1)
	if (life::LifeOf(villager) < info.damageThresholdToGoHome)
	{
		// 0x7609A9..0x7609B5: a functional abode -> in
		if (abode_queries::IsFunctional(abode))
		{
			return in();
		}
		// 0x7609BB..0x760A2D: a = Get3DAngleFromXZ(abode, me) + (pi/8 - GameFloatRand(pi/4) (0x1D1)); d =
		// GameFloatRand(5) (0x1D2) + 5; pos = me + GetPosFromAngle(a, d)
		const auto me = Me(villager);
		const float spread = k_EighthPi - GameFloatRand(glm::quarter_pi<float>());
		const float angle = tq::Get3DAngleFromXZ(tq::PosOf(abode), me) + spread;
		const float distance = GameFloatRand(5.0f) + 5.0f;
		auto pos = me + tq::GetPosFromAngle(angle, distance);
		// 0x760A2D..0x760A4D: GetTentPos -> SetupMoveToWithHug(pos, 0xEE 238), the literal 238 (also from 249); 1
		if (GetTentPos(villager, pos))
		{
			TraceIf(villager, fmt::format("home 37: tent {}", Xz(pos)));
			MoveTo(villager, pos, VillagerStates::SleepInTent);
		}
		return 1;
	}
	// 0x760A5E..0x760A72: food < HungryForFood (+0x2C0; test ah, 1: strict)
	if (v->food < info.hungryForFood)
	{
		// 0x760A74..0x760A8B: not functional -> SetTopState(0xA3 163) and on to ArriveHome / 38 (no jump between: the
		// 163 is overwritten in the same turn, literal)
		if (!abode_queries::IsFunctional(abode))
		{
			TraceIf(villager, "home 37: hungry 163+arrive");
			SetTopState(villager, VillagerStates::DecideWhatToDo);
		}
		return in();
	}
	// 0x760AB0..0x760ABB: SetupBuildingObject(abode) == 1 -> 1 (TODO(V7/V11): 0)
	if (SetupBuildingObject(villager, abode) == 1)
	{
		return 1;
	}
	TraceIf(villager, "home 37: repair TODO(V7)");
	return in();
}

uint32_t ArrivesHomeState(LivingAction& action)
{
	ArrivesHome(Entities().ToEntity(action));
	return 1;
}

// ---- at home -----------------------------------------------------------------------------------------------------

uint32_t AtHome(LivingAction& action)
{
	// 0x760B10: HomeDecideWhatToDo; 1
	HomeDecideWhatToDo(Entities().ToEntity(action));
	return 1;
}

uint32_t HomeDecideWhatToDo(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// 0x75FEA3..0x75FED5: an abode, a town, and the town's emergency -> SetTopState(0x77 119); 1 (it hides in bed)
	const auto town = TownOf(villager);
	if (AbodeOf(villager) != entt::null && town != entt::null && tq::IsInStateOfEmergency(Entities().Get<const Town>(town)))
	{
		TraceIf(villager, "home 38: emergency -> 119");
		SetTopState(villager, VillagerStates::GotoBedAtHome);
		return 1;
	}
	// 0x75FEDC..0x75FEE8: CheckNeedsAtHome == 1 -> 1
	if (CheckNeedsAtHome(villager) == 1)
	{
		return 1;
	}
	// 0x75FEEA..0x75FF57: a disciple (flags & 0x200) whose type ignores the needs
	if ((v->flags & Villager::k_FlagDisciple) != 0 && DiscipleIgnoresNeeds(v->discipleType))
	{
		// 0x75FF1D..0x75FF46: a BREEDER (5) with Sleep (16) first in the town's order 1 and CheckSatisfySleep -> 1
		if (v->discipleType == k_DiscipleBreeder && town != entt::null &&
		    town_desire::GetSortedDesires(town).at(0).index == static_cast<uint32_t>(TownDesireInfo::ForSleep) &&
		    CheckSatisfySleep(villager) != 0)
		{
			return 1;
		}
		// 0x75FF4D..0x75FF51: DecideWhatToDo (vt +0x8C8): its result
		TraceIf(villager, "home 38: disciple");
		return DecideWhatToDo(Entities().Get<LivingAction>(villager));
	}
	// 0x75FF59..0x75FF65: CheckNeededForSomething == 1 -> 1
	if (CheckNeededForSomething(villager) == 1)
	{
		TraceIf(villager, "home 38: something");
		return 1;
	}
	// 0x75FF67..0x75FF6E: HomeNothingToDo; 0
	HomeNothingToDo(villager);
	return 0;
}

uint32_t CheckNeedsAtHome(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// 0x760114..0x76013E: a woman: WomanSpecial == 1 -> 1; pregnant -> 1 (she stays at home doing nothing)
	if (IsWoman(villager))
	{
		if (WomanSpecial(villager) == 1)
		{
			return 1;
		}
		if (IsPregnant(villager))
		{
			return 1;
		}
	}
	// 0x76013F..0x760204: t = the larger of GetLifeDesireFromLife(D) and POWER(F) (POWER not below it keeps POWER):
	// a disciple that ignores the needs (D, F) = (+0x35C, +0x2C4), else (+0x360, +0x2C0)
	const auto& info = InfoOf(villager);
	const bool ignores = (v->flags & Villager::k_FlagDisciple) != 0 && DiscipleIgnoresNeeds(v->discipleType);
	const float d = ignores ? info.damageThresholdToGoHome : info.damageThresholdToSleepUntil;
	const float f = ignores ? info.starvingForFood : info.hungryForFood;
	const float lifeDesire = GetLifeDesireFromLife(villager, d);
	const float foodDesire = Power(f);
	const float t = foodDesire < lifeDesire ? lifeDesire : foodDesire;
	// 0x760204..0x760219: CheckSatisfyOwnDesire(t x 0.9) == 1 -> 1
	const float trigger = t * k_NeedsAtHome;
	if (CheckSatisfyOwnDesire(villager, trigger) == 1)
	{
		TraceIf(villager, fmt::format("home 38: needs(t={:.6f})", trigger));
		return 1;
	}
	// 0x76021E..0x760238: a child: CheckChildActivity 0x757F00 (= ChildDecideWhatToDo; 1) == 1 -> 1
	if (IsChild(villager))
	{
		ChildDecideWhatToDo(villager);
		return 1;
	}
	return 0;
}

uint32_t HomeNothingToDo(entt::entity villager)
{
	// 0x75FFB3..0x75FFE1: inside: GameRand(4) (VillagerHome.cpp 0x4B) == 0 -> counter (+0x58) = 0, SetTopState(0x77 119)
	if (Inside(villager))
	{
		const auto r = GameRand(4);
		if (r == 0)
		{
			TraceIf(villager, "home 38: nothing r4=0 -> 119");
			Counter(villager) = 0;
			SetTopState(villager, VillagerStates::GotoBedAtHome);
			return 1;
		}
		TraceIf(villager, fmt::format("home 38: nothing r4={}", r));
	}
	// 0x75FFE8: SetupNothingToDo
	SetupNothingToDo(villager);
	return 1;
}

uint32_t ExitAtHome(LivingAction& action, VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	// 0x761B40..0x761B5F: Infos[next] (0xDB9F38 + 0x114 next: memory +0xD0 = file 0xC0, StaysAtHomeOnExit) == 0 ->
	// LeaveHome
	const bool stays = state_info::StaysAtHomeOnExit(StateInfo(next));
	const bool wasInside = Inside(villager);
	if (!stays)
	{
		LeaveHome(villager);
	}
	if (TraceOn(villager) && wasInside)
	{
		const auto abode = AbodeOf(villager);
		Trace(villager, fmt::format("exit-home {} -> {} ({}, present {})", action.states.at(0), static_cast<uint32_t>(next),
		                            stays ? "stay" : "leave",
		                            abode != entt::null ? abode_villagers::PresentAtHome(abode) : 0));
	}
	return 1;
}

uint32_t GotoBedAtHome(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// 0x760B33..0x760B47: SetTopState(0x78 120), then +0x58 = info +0x24C RestAtHomeTime
	SetTopState(villager, VillagerStates::SleepingAtHome);
	if (Entities().AllOf<LivingAction>(villager))
	{
		Counter(villager) = static_cast<uint16_t>(InfoOf(villager).restAtHomeTime);
	}
	return 1;
}

uint32_t SleepingAtHome(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// 0x760D73..0x760D7A: no town -> nothing (it sleeps for ever, literal)
	if (TownOf(villager) == entt::null)
	{
		return 1;
	}
	// 0x760D7C..0x760D85: --counter (u16); not 0 -> 1
	--action.turnsUntilStateChange;
	if (action.turnsUntilStateChange != 0)
	{
		return 1;
	}
	// 0x760D87..0x760D9D: DoSleeping(1) == 0 -> SetTopState(0x26 38)
	if (DoSleeping(villager, 1.0f) == 0)
	{
		TraceIf(villager, fmt::format("sleep 120: life {:.6f} -> wake", life::LifeOf(villager)));
		SetTopState(villager, VillagerStates::AtHome);
	}
	else
	{
		TraceIf(villager, fmt::format("sleep 120: life {:.6f} -> keep", life::LifeOf(villager)));
	}
	return 1;
}

uint32_t DoSleeping(entt::entity villager, float f)
{
	// 0x760DB6..0x760DBE: poisoned (vt +0x4A4) -> 0 (no sleep, no healing)
	if (life::IsPoisoned(villager))
	{
		return 0;
	}
	const auto& info = InfoOf(villager);
	// 0x760DC4..0x760DF9: life < info +0x128 (life) -> IncreaseLife(f x +0x250 RestAtHomeRestoresLifeBy) (Villager
	// 0x753460 -> Object 0x637870)
	if (life::LifeOf(villager) < info.life)
	{
		life::IncreaseLife(villager, f * info.restAtHomeRestoresLifeBy);
	}
	// 0x760DFB..0x760E2E: (a town and GetSortedDesire(0).index == 16) or life < +0x360 DamageThresholdToSleepUntil ->
	// +0x58 = RestAtHomeTime; 1
	const auto town = TownOf(villager);
	const bool sleepFirst = town != entt::null &&
	                        town_desire::GetSortedDesires(town).at(0).index == static_cast<uint32_t>(TownDesireInfo::ForSleep);
	if (sleepFirst || life::LifeOf(villager) < info.damageThresholdToSleepUntil)
	{
		Counter(villager) = static_cast<uint16_t>(info.restAtHomeTime);
		return 1;
	}
	return 0;
}

uint32_t WakeUpAtHome(LivingAction& action)
{
	// 0x760E50: jmp GoHome
	return GoHome(Entities().ToEntity(action));
}

uint32_t CheckWhenGoingToBed(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// 0x760B64..0x760B70: flags & 0x2000 -> 1; 0x760B76: flags |= 0x2000 (LeaveHome clears it)
	if ((v->flags & Villager::k_FlagGoingToBed) != 0)
	{
		return 1;
	}
	v->flags = static_cast<uint16_t>(v->flags | Villager::k_FlagGoingToBed);
	// 0x760B82..0x760B8E: CheckDeathFromOldAge -> 0
	if (CheckDeathFromOldAge(villager))
	{
		return 0;
	}
	// 0x760B90..0x760BC9: no town, GetRawDesire(16) < 1 (test ah, 1) or not sexually active -> 1
	const auto town = TownOf(villager);
	if (town == entt::null || town_desire::GetRawDesire(town, TownDesireInfo::ForSleep) < 1.0f || !IsSexuallyActive(villager))
	{
		return 1;
	}
	const auto abode = AbodeOf(villager);
	const auto& list = abode_villagers::VillagersOf(abode);
	const auto sex = InfoOf(villager).sex;
	if (sex == SexType::Female)
	{
		// 0x760BDD..0x760C1F: the first man of the abode's list that is inside -> CheckGetPregnantAtHome (hers); 1
		for (const auto other : list)
		{
			if (InfoOf(other).sex == SexType::Male && Inside(other))
			{
				CheckGetPregnantAtHome(villager);
				return 1;
			}
		}
		return 1;
	}
	if (sex == SexType::Male)
	{
		// 0x760C26..0x760C6D: each woman of the list that is inside -> her CheckGetPregnantAtHome
		const auto copy = list;
		for (const auto other : copy)
		{
			if (InfoOf(other).sex == SexType::Female && Inside(other))
			{
				CheckGetPregnantAtHome(other);
			}
		}
	}
	return 1;
}

uint32_t CheckGetPregnantAtHome(entt::entity villager)
{
	// TODO(V14): 0x760C80 = WillHousewifeGetPregnant(0) 0x7624C0 -> HousewifeGetsPregnant(0) 0x762570 (+0xF8 =
	// TimePregnantFor 999, GoHome if outside). Neutral (P-2): the birth (110 -> 111 -> ChildBorn) is V14, and a
	// pregnancy without it would keep the woman at home for ever (CheckNeedsAtHome)
	TraceIf(villager, "home: CheckGetPregnantAtHome 0x760C80 TODO(V14)");
	return 0;
}

// ---- the tent ----------------------------------------------------------------------------------------------------

uint32_t SleepInTent(entt::entity villager)
{
	if (!Entities().AllOf<LivingAction>(villager))
	{
		return 1;
	}
	auto& counter = Counter(villager);
	// 0x761AE3..0x761AE8: counter != 0 -> --counter; 1
	if (counter != 0)
	{
		--counter;
		return 1;
	}
	// 0x761AEA..0x761AF6: DoSleeping(1) != 0 -> 1 (the counter was reset)
	if (DoSleeping(villager, 1.0f) != 0)
	{
		return 1;
	}
	// 0x761AF8..0x761B0C: no abode and CheckHomelessMoveIntoAbode -> 1
	if (AbodeOf(villager) == entt::null && CheckHomelessMoveIntoAbode(villager) != 0)
	{
		return 1;
	}
	// 0x761B0E..0x761B30: r = HomeDecideWhatToDo; r == 0 or TOP still 238 -> counter = RestAtHomeTime, then --counter
	const auto r = HomeDecideWhatToDo(villager);
	if (r == 0 || TopOf(villager) == VillagerStates::SleepInTent)
	{
		if (Entities().AllOf<LivingAction>(villager))
		{
			auto& again = Counter(villager);
			again = static_cast<uint16_t>(InfoOf(villager).restAtHomeTime);
			--again;
		}
	}
	return 1;
}

uint32_t SleepInTentState(LivingAction& action)
{
	return SleepInTent(Entities().ToEntity(action));
}

bool GetTentPos(entt::entity villager, glm::ivec2& pos)
{
	// 0x7604F9..0x760547: the nearest tree to me (+0x14) within 50 and fn_0074C650 -> pos = its point; 1
	const auto me = Me(villager);
	if (const auto tree = FindNearestTree(me, k_TreeRadius); tree != entt::null)
	{
		glm::ivec2 out(0);
		if (TentNextToTree(tree, villager, out))
		{
			TraceIf(villager, fmt::format("tent: tree {} {}", object_index::Of(tree), Xz(out)));
			pos = out;
			return true;
		}
	}
	// 0x76056D..0x7606B1: tmp = pos; three tries
	auto tmp = pos;
	for (int attempt = 0; attempt < k_TentTries; ++attempt)
	{
		// 0x760584..0x760591: MapCoords::Collide(0x19) != 0 -> on to the next try's move
		if ((CollideAt(tmp) & k_TentCollide) == 0)
		{
			// 0x760597..0x76064A: free = 1; 9 cells from tmp's, tmp moved one spiral step after each
			bool free = true;
			map_coords::Spiral spiral;
			for (int cell = 0; cell < k_TentCells; ++cell)
			{
				const map_coords::MapCoords at {tmp.x, tmp.y, 0.0f};
				// 0x7605A6..0x76062B: the cell's objects (the fixed list, then the mobile one): a villager (vt +0x2C8)
				// within 5 m of tmp (fn_00605CD0; test ah, 1) whose TOP is 238 -> free = 0, on to the next cell
				for (const auto object : tq::ObjectsInCell(map_coords::Cell(at)))
				{
					if (!IsVillagerObject(object))
					{
						continue;
					}
					if (!(tq::GetDistanceInMetres(tmp, tq::PosOf(object)) < k_TentNeighbour))
					{
						continue;
					}
					if (TopOf(object) == VillagerStates::SleepInTent)
					{
						free = false;
						break;
					}
				}
				// 0x76062D..0x760644: GUtils::Spiral, tmp += the step
				AddCell(tmp, spiral.Next());
			}
			// 0x760650..0x7606CC: free -> pos = tmp (9 spiral steps from the cell tried); 1
			if (free)
			{
				TraceIf(villager, fmt::format("tent: spiral try {} {}", attempt, Xz(tmp)));
				pos = tmp;
				return true;
			}
		}
		// 0x760654..0x7606A0: tmp += GetPosFromAngle(GameFloatRand(2 pi), GameFloatRand(5) + 3) (0x16D: the 5 is drawn
		// first, it is the last argument pushed)
		const float distance = GameFloatRand(5.0f) + 3.0f;
		const float angle = GameFloatRand(glm::two_pi<float>());
		tmp += tq::GetPosFromAngle(angle, distance);
	}
	TraceIf(villager, "tent: fail");
	return false;
}

entt::entity FindNearestTree(glm::ivec2 pos, float radius)
{
	if (g_NearestTreeForTests)
	{
		return g_NearestTreeForTests(pos, radius);
	}
	// fn_00604AF0 (MapCoords::FindNearest with the filter Villager::FUN_00761BC0 = jmp [vt +0x338] IsTree, nothing
	// excluded)
	return map_cells::FindNearestInSpiral(map_coords::MapCoords {pos.x, pos.y, 0.0f}, &IsTree, radius, entt::null);
}

bool TentNextToTree(entt::entity tree, entt::entity villager, glm::ivec2& out)
{
	const auto treePos = tq::PosOf(tree);
	// 0x74C655..0x74C77E: 9 spiral cells from the tree's
	glm::ivec2 walk = treePos;
	map_coords::Spiral spiral;
	std::optional<glm::ivec2> occupant;
	for (int cell = 0; cell < k_TreeCells; ++cell)
	{
		const map_coords::MapCoords at {walk.x, walk.y, 0.0f};
		for (const auto object : tq::ObjectsInCell(map_coords::Cell(at)))
		{
			// 0x74C6AF..0x74C6DA: fn_00605CD0(tree, object) - Get2DRadius (vt +0x64) < 4 (test ah, 1)
			const float d = tq::GetDistanceInMetres(treePos, tq::PosOf(object));
			if (!(d - tq::Get2DRadius(object) < k_TreeOccupant))
			{
				continue;
			}
			// 0x74C6DC..0x74C6FC: a villager in 238, or a non-villager with +0x24 & 2 (the MultiMapFixed bit,
			// MultiMapFixed ctor 0x52E1F0)
			const bool counts = IsVillagerObject(object) ? TopOf(object) == VillagerStates::SleepInTent
			                                             : map_cells::IsMultiMapFixedClass(object);
			if (!counts)
			{
				continue;
			}
			// 0x74C6FE..0x74C71E: a second occupant -> 0; the first one's position kept
			if (occupant.has_value())
			{
				return false;
			}
			occupant = tq::PosOf(object);
		}
		AddCell(walk, spiral.Next());
	}
	// 0x74C784..0x74C7D7: out = the tree; += GetPosFromAngle(a, 2), a = Get3DAngleFromXZ(occupant, tree) (the other
	// side) or Get3DAngleFromXZ(tree, villager)
	const float angle = occupant.has_value() ? tq::Get3DAngleFromXZ(*occupant, treePos)
	                                         : tq::Get3DAngleFromXZ(treePos, Me(villager));
	out = treePos + tq::GetPosFromAngle(angle, k_TentFromTree);
	return true;
}

// ---- the homeless and the vagrants -------------------------------------------------------------------------------

uint32_t HomelessStart(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// 0x761323..0x76134D: CheckHungry == 1 (the direct call resets LastCheckTurn: the periodic check's phase moves),
	// CheckNeededForSomething == 1, CheckHomelessMoveIntoAbode != 0 -> 1; else SetupNothingToDo
	if (CheckHungry(villager, CurrentTurn()))
	{
		return 1;
	}
	if (CheckNeededForSomething(villager) == 1)
	{
		return 1;
	}
	if (CheckHomelessMoveIntoAbode(villager) != 0)
	{
		return 1;
	}
	SetupNothingToDo(villager);
	return 1;
}

uint32_t VagrantStart(entt::entity villager)
{
	const auto me = Me(villager);
	// 0x76A8D8..0x76A918: MapCoords::GetNearestTown(200) 0x6020E0; of my tribe (Town::GetTribe 0x73C840 = the GTribeInfo
	// of +0x5B8, Villager::GetTribe 0x751EE0 = the info's +0x1F4) and AddVillagerToTown -> SetTopState(0xA3 163); 1
	if (const auto town = town_villagers::GetNearestTown(me, k_VagrantTownRadius); town != entt::null)
	{
		const auto* tribe = Entities().TryGet<const Tribe>(town);
		if (tribe != nullptr && *tribe == InfoOf(villager).tribeType && town_villagers::AddVillagerToTown(town, villager))
		{
			TraceIf(villager, fmt::format("vagrant 130: joins town {}", Entities().Get<const Town>(town).id));
			SetTopState(villager, VillagerStates::DecideWhatToDo);
			return 1;
		}
	}
	// 0x76A92A..0x76A942: life < DamageThresholdToGoHome (+0x35C; test ah, 1)
	if (life::LifeOf(villager) < InfoOf(villager).damageThresholdToGoHome)
	{
		// 0x76A944..0x76A9B2: d = GameFloatRand(5), a = GameFloatRand(2 pi) (VillagerStates.cpp 0x27F, in that order);
		// pos = me + GetPosFromAngle(a, d); GetTentPos -> SetupMoveToWithHug(pos, 0xEE 238)
		const float distance = GameFloatRand(5.0f);
		const float angle = GameFloatRand(glm::two_pi<float>());
		auto pos = me + tq::GetPosFromAngle(angle, distance);
		if (GetTentPos(villager, pos))
		{
			MoveTo(villager, pos, VillagerStates::SleepInTent);
		}
		return 1;
	}
	// 0x76A9C3..0x76AA4E: a = GetYAngle + (GameFloatRand(pi/4) (0x287) - pi/8); d = GameFloatRand(20) (0x288) + 10; pos =
	// me + GetPosFromAngle(a, d); InBounds -> SetupMoveToWithHug(pos, 0x82 130)
	const float spread = GameFloatRand(glm::quarter_pi<float>()) - k_EighthPi;
	const float angle = YAngleOf(villager) + spread;
	const float distance = GameFloatRand(20.0f) + 10.0f;
	const auto pos = me + tq::GetPosFromAngle(angle, distance);
	if (map_coords::InBounds(map_coords::MapCoords {pos.x, pos.y, 0.0f}))
	{
		TraceIf(villager, fmt::format("vagrant 130: stroll {}", Xz(pos)));
		MoveTo(villager, pos, VillagerStates::VagrantStart);
	}
	return 1;
}

uint32_t VagrantStartState(LivingAction& action)
{
	return VagrantStart(Entities().ToEntity(action));
}

uint32_t CheckHomelessMoveIntoAbode(entt::entity villager)
{
	// 0x761366..0x76136D: a town
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	// 0x76136F..0x76137B: FindAbodeWithSpaceInTown(me, 0)
	const auto abode = town_villagers::FindAbodeWithSpaceInTown(town, villager, 0.0f);
	if (abode == entt::null)
	{
		return 0;
	}
	// 0x76137D..0x7613BE: out of the homeless list (if there); 0x7613C8 AddVillagerToAbode; 0x7613D0 SetTopState(0x24 36)
	town_villagers::RemoveFromHomelessList(town, villager);
	TraceIf(villager, fmt::format("homeless: into abode {} (score {:.6f})", object_index::Of(abode),
	                              abode_villagers::CalculateScoreForAddingVillagerToAbode(abode, villager)));
	abode_villagers::AddVillagerToAbode(abode, villager);
	SetTopState(villager, VillagerStates::GoHome);
	return 1;
}

bool MakeHomeless(entt::entity villager)
{
	// 0x761224 MakeHomelessNoStateChange; 0x76122B SetTopState(0x81 129); its result
	const bool made = MakeHomelessNoStateChange(villager);
	SetTopState(villager, VillagerStates::HomelessStart);
	return made;
}

bool MakeHomelessNoStateChange(entt::entity villager)
{
	// 0x761246: town = GetTown()
	const auto town = TownOf(villager);
	// 0x76124D..0x761271: an abode -> RemoveAliveVillagerFromAbode, SetAbode(0), SetTown(town)
	if (const auto abode = AbodeOf(villager); abode != entt::null)
	{
		abode_villagers::RemoveAliveVillagerFromAbode(abode, villager);
		SetAbode(villager, entt::null);
		SetTown(villager, town);
	}
	// 0x761276..0x76128F: no town -> 0; already in its list -> 0
	if (town == entt::null || town_villagers::IsVillagerInHomelessList(town, villager))
	{
		return false;
	}
	// 0x761290..0x7612EF: out of the vagrants; 0x7612F9..0x761312: the head of +0x768, ++ +0x76C; 1
	town_villagers::RemoveFromVagrants(villager);
	town_villagers::AddToHomelessList(town, villager);
	TraceIf(villager, "homeless: list");
	return true;
}

void HomeDeleted(entt::entity villager)
{
	// 0x7611F3..0x7611FD: +0x60 == GetAbode -> +0x60 = 0. TODO: Living +0x60 is not identified in openblack
	// 0x761206..0x761218: an abode -> MakeHomeless; else TownDeleted 0x750B50 (TODO(V12))
	if (AbodeOf(villager) != entt::null)
	{
		MakeHomeless(villager);
	}
}

uint32_t CheckNeedNewAbode(entt::entity villager)
{
	// 0x757F96..0x757F9E: a child -> 0 (also from 114 without a mother nor an abode: it stays there, literal)
	if (IsChild(villager))
	{
		return 0;
	}
	// 0x757FA4..0x757FBF: an abode that is not too crowded -> 0
	const auto abode = AbodeOf(villager);
	if (abode != entt::null && !abode_villagers::IsTooCrowded(abode))
	{
		return 0;
	}
	// 0x757FC5..0x757FCE / 0x758060: no town -> VagrantStart; 1
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		VagrantStart(villager);
		return 1;
	}
	// 0x757FD4..0x75800E: s = the abode's score for me (0 without one); FindAbodeWithSpaceInTown(me, s)
	const float score = abode != entt::null ? abode_villagers::CalculateScoreForAddingVillagerToAbode(abode, villager) : 0.0f;
	const auto better = town_villagers::FindAbodeWithSpaceInTown(town, villager, score);
	// 0x758010..0x758036: found and MoveVillagerToAbode == 1 -> IsVillagerAvailable ? SetTopState(0x24 36); 1
	if (better != entt::null && MoveVillagerToAbode(villager, better) == 1)
	{
		TraceIf(villager, fmt::format("abode: moves to {} (score {:.6f})", object_index::Of(better), score));
		if (IsVillagerAvailable(villager))
		{
			SetTopState(villager, VillagerStates::GoHome);
		}
		return 1;
	}
	// 0x75803E..0x758058: not in the town's homeless list -> MakeHomeless; 1
	if (const auto now = TownOf(villager); now == entt::null || !town_villagers::IsVillagerInHomelessList(now, villager))
	{
		TraceIf(villager, "abode: too crowded, none better -> homeless 129");
		MakeHomeless(villager);
	}
	return 1;
}

uint32_t MoveVillagerToAbode(entt::entity villager, entt::entity abode)
{
	// 0x758086..0x7580B6: a child GetRoomLeftForChildren, an adult GetRoomLeftForAdults; > 0 (jg) -> Force; 1
	const int32_t room = IsChild(villager) ? abode_villagers::GetRoomLeftForChildren(abode)
	                                       : abode_villagers::GetRoomLeftForAdults(abode);
	if (room <= 0)
	{
		return 0;
	}
	ForceMoveVillagerToAbode(villager, abode);
	return 1;
}

void ForceMoveVillagerToAbode(entt::entity villager, entt::entity abode)
{
	// 0x756248..0x75625C: my town and the abode's
	const auto mine = TownOf(villager);
	const auto theirs = abode_villagers::TownOf(abode);
	if (mine == theirs)
	{
		// 0x7562AA AddVillagerToAbode
		abode_villagers::AddVillagerToAbode(abode, villager);
		return;
	}
	// 0x75625E..0x756265: my town's RemoveVillager
	if (mine != entt::null)
	{
		town_villagers::RemoveVillager(mine, villager);
	}
	// 0x75626A..0x756299: the abode's GetPercentAbodeFullWithChildren (vt +0x8A0) for a child, WithAdults (+0x89C) for an
	// adult, below 1 (test ah, 1; jne) -> AddVillagerToAbode; else its town's AddVillagerToTown
	const float full = IsChild(villager) ? abode_villagers::GetPercentAbodeFullWithChildren(abode)
	                                     : abode_villagers::GetPercentAbodeFullWithAdults(abode);
	if (full < 1.0f)
	{
		abode_villagers::AddVillagerToAbode(abode, villager);
		return;
	}
	if (theirs != entt::null)
	{
		town_villagers::AddVillagerToTown(theirs, villager);
	}
}

// ---- growing up --------------------------------------------------------------------------------------------------

uint32_t GoHomeAndChange(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto abode = AbodeOf(villager);
	if (abode != entt::null)
	{
		// 0x761820..0x76184A: door = GetArrivePos; not AreWeThere(door, 0) -> SetupMoveToWithHug(door, 0xEA 234); 1
		const auto door = abode_queries::GetArrivePos(abode);
		if (!AreWeThere(villager, tq::ToMetres(door), 0.0f))
		{
			MoveTo(villager, door, VillagerStates::GoHomeAndChange);
			return 1;
		}
		// 0x76185A..0x761877: SetTopState(inside ? 38 : 37)
		SetTopState(villager, Inside(villager) ? VillagerStates::AtHome : VillagerStates::ArrivesHome);
	}
	else
	{
		// 0x76186D: SetTopState(0xA3 163)
		SetTopState(villager, VillagerStates::DecideWhatToDo);
	}
	// 0x76187D..0x7618A1: GetScale < 0.95 (test ah, 1) -> SetScaleForAge(GetAge())
	if (const auto* t = Entities().TryGet<const Transform>(villager); t != nullptr && t->scale.x < k_GrownUpScale)
	{
		SetScaleForAge(villager, GetAge(villager));
	}
	return 1;
}

uint32_t ExitGoHomeAndChange(LivingAction& action, VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	// 0x761988..0x761997: IsStateExitFunctionSameAs(next) (vt +0x96C) -> nothing
	if (!IsStateExitFunctionSameAs(villager, next))
	{
		// 0x76199B..0x7619B1: the town's tribe (+0x5B8) or, without one, the info's (+0x1F4)
		const auto town = TownOf(villager);
		const auto* townTribe = town != entt::null ? Entities().TryGet<const Tribe>(town) : nullptr;
		const auto tribe = townTribe != nullptr ? *townTribe : InfoOf(villager).tribeType;
		// 0x7619B7..0x7619D4: ChangeTribeIfRequired(tribe, Infos[next] 0xC0 == 0)
		const bool leaving = !state_info::StaysAtHomeOnExit(StateInfo(next));
		ChangeTribeIfRequired(villager, tribe, leaving);
	}
	// 0x7619D9..0x7619EA: discipleType 10 CHANGE_HOUSE -> SetVillagerDisciple(0, 0, 0) 0x756000. TODO(V14): disciples
	if (const auto* v = VillagerOf(villager); v != nullptr && v->discipleType == k_DiscipleChangeHouse)
	{
		TraceIf(villager, "234: SetVillagerDisciple(0) 0x756000 TODO(V14)");
	}
	return 1;
}

void ChangeTribeIfRequired(entt::entity villager, Tribe tribe, bool leaving)
{
	const auto& info = InfoOf(villager);
	// 0x7618C8..0x7618D3: KeepMeshWhenChangeTown (+0x388) != 0 -> nothing
	if (info.keepMeshWhenChangeTown != 0)
	{
		return;
	}
	// 0x7618D9..0x7618E5: GVillagerInfo::Find(tribe, +0x1FC my number) 0x752650
	const auto* found = FindVillagerInfo(tribe, info.villagerNumber);
	if (found == nullptr)
	{
		return; // (openblack, guard) the original passes a null info to ChangeInfo
	}
	// 0x7618EA..0x761940: with a town, +0x6F4 += new +0x2D8 - old +0x2D8 (recomputed with the TownStats each
	// Town::Process, V3: nothing to add here)
	// 0x761942 ChangeInfo
	ChangeInfo(villager, *found);
	// 0x76194A..0x761964: leaving -> CreateSmokyStuff(1, 1.0 [0x3F800000], colour -1) 0x63A810: the object's point half
	// its height up ((inferido) dev\tmp_dis\animals\misc.md §3), SmokyStuff::Create(point, 1, 1.0, -1)
	if (leaving)
	{
		if (const auto* t = Entities().TryGet<const Transform>(villager))
		{
			const float half = object::ObjectGetHeight(villager) * 0.5f;
			smoky_stuff::Create(t->position + glm::vec3(0.0f, half, 0.0f), 1, 1.0f, 0xFFFFFFFFu);
		}
	}
}

uint32_t ChangeInfo(entt::entity villager, const GVillagerInfo& info)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// 0x761A0B: +0x28 = the info (openblack finds it by tribe and number: VillagerInfoOf)
	v->tribe = info.tribeType;
	v->number = info.villagerNumber;
	v->sex = info.sex == SexType::Female ? Villager::Sex::FEMALE : Villager::Sex::MALE;
	// 0x761A0E..0x761AD1: a child: the three meshes = +0x204 ChildMeshHigh (rareza: SetAge uses +0x20C / +0x208 /
	// +0x204); an adult: GetDetailMesh(2 / 1 / 0) (vt +0x60C)
	SetVillagerMeshes(villager, info, IsChild(villager), true);
	TraceIf(villager, fmt::format("234: ChangeInfo -> {} {} ({})", static_cast<int>(info.tribeType),
	                              static_cast<int>(info.villagerNumber), IsChild(villager) ? "child mesh" : "adult mesh"));
	return 1;
}

void SetVillagerMeshes(entt::entity villager, const GVillagerInfo& info, bool child, bool childHighOnly)
{
	auto& registry = Entities();
	const auto meshId = child ? (childHighOnly ? info.childMeshHigh : detail_meshes::Villager(info, true))
	                          : detail_meshes::Villager(info, false);
	const auto id = resources::HashIdentifier(meshId);
	if (auto* mesh = registry.TryGet<Mesh>(villager))
	{
		mesh->id = id;
		return;
	}
	// not drawn now (a -4 clip): the mesh it gets back when it is drawn again (VillagerAnimations' hiddenMesh)
	if (auto* animation = registry.TryGet<SkeletalAnimation>(villager); animation != nullptr && animation->hiddenMesh != 0)
	{
		animation->hiddenMesh = id;
	}
}

const GVillagerInfo* FindVillagerInfo(Tribe tribe, VillagerNumber number)
{
	// 0x752659..0x75267F: _VillagerInfos 0xDA6BE8, stride 0x3A4: the first with +0x1F4 == tribe and +0x1FC == number
	for (const auto& info : Locator::infoConstants::value().villager)
	{
		if (info.tribeType == tribe && info.villagerNumber == number)
		{
			return &info;
		}
	}
	return nullptr;
}

glm::ivec2 FindPosOutsideAbode(entt::entity villager, entt::entity abode)
{
	// 0x753474..0x753481: abode 0 -> mine
	if (abode == entt::null)
	{
		abode = AbodeOf(villager);
	}
	if (abode == entt::null)
	{
		return Me(villager); // (openblack, guard) the original dereferences the null abode
	}
	// 0x753483..0x75349C: Get3DAngleFromXZ(abode +0x14, door), stored
	const auto door = abode_queries::GetArrivePos(abode);
	const float toDoor = tq::Get3DAngleFromXZ(tq::PosOf(abode), door);
	// 0x7534A0..0x7534C9: d = GameFloatRand(1.5) (Villager.cpp 0xA96) + 1.5, stored
	const float distance = GameFloatRand(1.5f) + 1.5f;
	// 0x7534CD..0x7534E5: a = (pi/8 - GameFloatRand(pi/4) (0xA97)) + toDoor
	const float spread = k_EighthPi - GameFloatRand(glm::quarter_pi<float>());
	const float angle = spread + toDoor;
	// 0x7534EF..0x75350E: door + GetPosFromAngle(a, d)
	return door + tq::GetPosFromAngle(angle, distance);
}

uint32_t SetupBuildingObject([[maybe_unused]] entt::entity villager, [[maybe_unused]] entt::entity abode)
{
	// TODO(V7/V11): Villager::SetupBuildingObject 0x758530: AddBuildingSite 0x73B8E0 (the repair site) for an abode that
	// is not built or repaired, and the walk to it. Neutral: openblack has no building sites
	return 0;
}

bool IsSexuallyActive(entt::entity villager)
{
	// 0x761099..0x7610BA: GetAge >= +0x228 (jb) and GetAge < +0x22C (jae)
	const auto& info = InfoOf(villager);
	const uint32_t age = GetAge(villager);
	return !(age < info.startHavingSexAge) && age < info.stopHavingSexAge;
}

// ---- test hooks --------------------------------------------------------------------------------------------------

void SetTentQueriesForTests(std::function<entt::entity(glm::ivec2, float)> nearestTree,
                            std::function<uint32_t(glm::ivec2)> collide)
{
	g_NearestTreeForTests = std::move(nearestTree);
	g_CollideForTests = std::move(collide);
}
} // namespace openblack::ecs::villager
