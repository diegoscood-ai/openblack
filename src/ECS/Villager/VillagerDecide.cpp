/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerDecide.h"

#include <cstdint>

#include <unordered_map>
#include <unordered_set>

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>

#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Life.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerWorship.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "InfoConstants.h"
#include "Locator.h"

// Villager.cpp / VillagerStates.cpp of runblack.exe W120 (VillagerDecide.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;
namespace aq = abode_queries;

namespace
{
std::function<bool(entt::entity)> g_WorshipCheckForTests;
std::function<void(const char*)> g_DecideLog;
std::unordered_map<entt::entity, uint32_t> g_ForcedNothingRolls;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

void Log(const char* step)
{
	if (g_DecideLog)
	{
		g_DecideLog(step);
	}
}

/// Villager::GetTown 0x751F00 (vt +0x48): +0x12C, the town entity (entt::null without one)
entt::entity TownEntityOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr || v->town == entt::null || !Entities().Valid(v->town) || !Entities().AllOf<Town>(v->town))
	{
		return entt::null;
	}
	return v->town;
}

Town* TownOf(entt::entity villager)
{
	const auto town = TownEntityOf(villager);
	return town != entt::null ? &Entities().Get<Town>(town) : nullptr;
}

/// Villager::GetAbode 0x752160: +0x128 (entt::null without one, or when the abode is gone)
entt::entity AbodeOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr || v->abode == entt::null || !Entities().Valid(v->abode))
	{
		return entt::null;
	}
	return v->abode;
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

/// Living::SetupMoveToWithHug 0x5F2890 on a MapCoords goal
uint32_t MoveTo(entt::entity villager, glm::ivec2 goal, VillagerStates final)
{
	return SetupMoveToWithHug(villager, tq::ToMetres(goal), final);
}

const GTownInfo& TownInfo()
{
	return Locator::infoConstants::value().town;
}
} // namespace

// ---- the state functions -----------------------------------------------------------------------------------------

uint32_t DecideWhatToDo(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// 0x7515C5..0x7515ED: the town's emergency -> SetTopState(242 GOTO_CONGREGATE_IN_TOWN_AFTER_EMERGENCY)
	if (const auto* town = TownOf(villager); town != nullptr && tq::IsInStateOfEmergency(*town))
	{
		Log("emergency");
		TraceIf(villager, "decide: emergency -> 242");
		SetTopState(villager, VillagerStates::GotoCongregateInTownAfterEmergency);
		return 1;
	}
	// 0x7515F4..0x751680: a disciple (0x200) or a disciple follower (0x400)
	if ((v->flags & Villager::k_FlagDisciple) != 0 || (v->flags & Villager::k_FlagDiscipleFollower) != 0)
	{
		Log("disciple");
		if (DiscipleDecideWhatToDo(villager) == 1)
		{
			TraceIf(villager, "decide: disciple");
			// 0x751618: a follower loses its disciple type
			if ((v->flags & Villager::k_FlagDiscipleFollower) != 0)
			{
				v->discipleType = 0;
				return 1;
			}
			// 0x75162A..0x75165F: g_DiscipleInfos[type] +4 (0x99A1FC) -> Reaction::CreateReaction(this, 0x18,
			// GetPlayer(), 1). TODO(V14): unreachable while DiscipleDecideWhatToDo is neutral
			return 1;
		}
		// 0x751666..0x751680: a follower that has nothing to do stops following
		if ((v->flags & Villager::k_FlagDiscipleFollower) != 0)
		{
			v->flags = static_cast<uint16_t>(v->flags & 0xF9FF);
			v->discipleType = 0;
		}
	}
	// 0x751687: SetTopState(163) (vt +0x8E8; 163 does not pause, FINAL is cleared)
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	// 0x751696..0x7516AB: a child
	if (IsChild(villager))
	{
		Log("child");
		return ChildDecideWhatToDo(villager);
	}
	// 0x7516AD
	Log("something");
	if (CheckNeededForSomething(villager) == 1)
	{
		return 1;
	}
	// 0x7516B9
	Log("resources");
	if (CheckTakeResourcesToStoragePit(villager) != 0)
	{
		return 1;
	}
	// 0x7516C4..0x7516D3: SetupNothingToDo always gives 1, so the SetTopState(36) after it is dead code
	Log("nothing");
	if (SetupNothingToDo(villager) == 0)
	{
		SetTopState(villager, VillagerStates::GoHome);
	}
	return 1;
}

uint32_t NothingToDo([[maybe_unused]] LivingAction& action)
{
	// 0x760000
	return 1;
}

uint32_t GoAndChilloutOutsideHome(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// 0x76B3F8..0x76B410: no abode or no town -> SetTopState(163)
	const auto abode = AbodeOf(villager);
	const auto townEntity = TownEntityOf(villager);
	if (abode == entt::null || townEntity == entt::null)
	{
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// 0x76B416: R = the town's info +0x144 (maxDistanceFromHouseThatPeopleChillOut)
	const float r = TownInfo().maxDistanceFromHouseThatPeopleChillOut;
	// 0x76B42C: the door (GetArrivePos, vt +0x104)
	const auto door = aq::GetArrivePos(abode);
	// 0x76B432..0x76B47F: T = door + GetPosFromAngle(Get3DAngleFromXZ(abode +0x14, door), R x 10): out of the door
	const auto lookAt = door + tq::GetPosFromAngle(tq::Get3DAngleFromXZ(tq::PosOf(abode), door), r * 10.0f);
	// 0x76B484..0x76B4B0
	GetMeToMyChillOutPos(villager, &GetPosOutsideMyHouse, door, r, &lookAt);
	return 1;
}

uint32_t SitAndChillout(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// 0x76B4E3..0x76B4F3: old = +0x58; +0x58 = old - 1; (int16) old > 0 (jg) -> 1
	const auto old = static_cast<int16_t>(action.turnsUntilStateChange);
	action.turnsUntilStateChange = static_cast<uint16_t>(old - 1);
	if (old > 0)
	{
		return 1;
	}
	// 0x76B4F9
	action.turnsUntilStateChange = 0;
	// 0x76B4FF..0x76B525: the town's emergency -> SetTopState(242)
	if (const auto* town = TownOf(villager); town != nullptr && tq::IsInStateOfEmergency(*town))
	{
		TraceIf(villager, "sit 246: check -> emergency 242");
		SetTopState(villager, VillagerStates::GotoCongregateInTownAfterEmergency);
		return 1;
	}
	// 0x76B529: CheckNeededForSomething (worship, civic, own desires)
	if (CheckNeededForSomething(villager) != 0)
	{
		TraceIf(villager, "sit 246: check -> something");
		return 1;
	}
	// 0x76B532..0x76B551: GameRand(10) (VillagerStates.cpp 0x3FF) == 0 -> SetupNothingToDo, without going through 163
	const auto r = GameRand(10);
	if (r == 0)
	{
		TraceIf(villager, "sit 246: check -> nothing(r10=0)");
		SetupNothingToDo(villager);
		return 1;
	}
	// 0x76B558: the counter = subsequentChillOutTime (+0x396)
	TraceIf(villager, fmt::format("sit 246: check -> again (r10={})", r));
	action.turnsUntilStateChange = InfoOf(villager).subsequentChillOutTime;
	return 1;
}

uint32_t EnterSitAndChillOut(LivingAction& action, [[maybe_unused]] VillagerStates final,
                             [[maybe_unused]] VillagerStates next)
{
	// 0x76B570: +0x58 = info +0x394 (u16); 1
	action.turnsUntilStateChange = InfoOf(Entities().ToEntity(action)).initialChillOutTime;
	return 1;
}

uint32_t GoAndChilloutInTown(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// 0x76B599..0x76B5A2: no town -> SetTopState(163)
	const auto townEntity = TownEntityOf(villager);
	if (townEntity == entt::null)
	{
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// 0x76B5AB..0x76B5E1: c = GetCongregationPos(); GetMeToMyChillOutPos(GetChillOutPos, c, info +0x140, c)
	const auto c = tq::GetCongregationPos(townEntity);
	GetMeToMyChillOutPos(villager, &GetChillOutPos, c, TownInfo().maxDistanceFromCongreationPosThatPeopleChillOut, &c);
	return 1;
}

uint32_t ChildFollowsMother(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// 0x7578C6..0x757904: CheckChild, CheckNeededForTownDesire == 1, ChildGotoCreche -> 1
	if (CheckChild(villager) != 0 || CheckNeededForTownDesire(villager) == 1 || ChildGotoCreche(villager) != 0)
	{
		return 1;
	}
	// 0x75790A..0x757965: the mother (+0x100) if available (vt +0x2C), else the abode; neither: CheckNeedNewAbode
	glm::ivec2 pos(0);
	const char* around = "mother";
	const auto* v = VillagerOf(villager);
	if (v != nullptr && v->mother != entt::null && Entities().Valid(v->mother))
	{
		pos = tq::PosOf(v->mother);
	}
	else if (const auto abode = AbodeOf(villager); abode != entt::null)
	{
		pos = tq::PosOf(abode);
		around = "home";
	}
	else
	{
		// Until V4 (CheckNeedNewAbode 0x757F90 is neutral) a child with no mother and no abode stands still here
		static std::unordered_set<int64_t> traced;
		if (TraceOn(villager) && traced.insert(object_index::Of(villager)).second)
		{
			Trace(villager, "child 114: no mother, no abode -> CheckNeedNewAbode 0x757F90 (TODO V4: stands still)");
		}
		CheckNeedNewAbode(villager);
		return 1;
	}
	// 0x757969..0x7579AA: pos += GetPosFromAngle(GameFloatRand(2 pi) (VillagerChild.cpp 0x39), 0x99A934 (10, read only
	// here) x 0.5)
	const float angle = GameFloatRand(glm::two_pi<float>());
	pos += tq::GetPosFromAngle(angle, 10.0f * 0.5f);
	// 0x7579AF MapCoords::IsNavigable 0x603840: Collide() & 2 (dry land) and not & 8. (aproximado) openblack's Collide
	// (ecs::animal_ai::detail::Collides) knows water / land only: bit 8 is never set
	// (the tests have no land: navigable)
	const auto metres = tq::ToMetres(pos);
	if (Locator::terrainSystem::has_value() &&
	    (!animal_ai::detail::Collides(metres, 2) || animal_ai::detail::Collides(metres, 8)))
	{
		return 1;
	}
	// 0x7579BC..0x7579C5: SetupMoveToWithHug(pos, 114)
	TraceIf(villager, fmt::format("child 114: around {} -> {}", around, Xz(pos)));
	MoveTo(villager, pos, VillagerStates::ChildFollowsMother);
	return 1;
}

// ---- DecideWhatToDo's checks ---------------------------------------------------------------------------------------

uint32_t CheckNeededForSomething(entt::entity villager)
{
	// 0x75FF83..0x75FF9C: homeless and CheckHomelessMoveIntoAbode -> 1
	if (AbodeOf(villager) == entt::null && CheckHomelessMoveIntoAbode(villager) != 0)
	{
		TraceIf(villager, "decide: homeless");
		return 1;
	}
	// 0x75FFA0..0x75FFAA: CheckNeededForSpecial == 1 (dec, neg, sbb, inc)
	return CheckNeededForSpecial(villager) == 1 ? 1 : 0;
}

uint32_t CheckHomelessMoveIntoAbode([[maybe_unused]] entt::entity villager)
{
	// TODO(V4): 0x761360 (Town::FindAbodeWithSpaceInTown 0x73B370, the homeless list +0x768 / +0xE4 / +0x76C,
	// Abode::AddVillagerToAbode 0x404060, SetTopState(36)). Neutral
	return 0;
}

uint32_t CheckNeededForSpecial(entt::entity villager)
{
	// 0x760013: CheckNeededForWorship 0x76BA60 (Milagros: VillagerWorship.cpp) == 1
	Log("worship");
	const bool worship =
	    g_WorshipCheckForTests ? g_WorshipCheckForTests(villager) : villager_worship::CheckNeededForWorship(villager);
	if (worship)
	{
		TraceIf(villager, "decide: worship");
		return 1;
	}
	// 0x760021: CheckNeededForCivic == 1
	Log("civic");
	if (CheckNeededForCivic(villager) == 1)
	{
		TraceIf(villager, "decide: civic");
		return 1;
	}
	// 0x76002D..0x760043: CheckSatisfyOwnDesire(info +0x38C ownDesireThreshold) == 1
	Log("own");
	return CheckSatisfyOwnDesire(villager, InfoOf(villager).ownDesireThreshold) == 1 ? 1 : 0;
}

uint32_t CheckNeededForCivic(entt::entity villager)
{
	// 0x758185..0x75819C: a town and fn_7581A0 == 1
	if (TownEntityOf(villager) == entt::null)
	{
		return 0;
	}
	return CheckNeededForTownDesire(villager) == 1 ? 1 : 0;
}

uint32_t CheckNeededForTownDesire(entt::entity villager)
{
	// fn_7581A0: no town -> 0 (0x7581DB)
	if (TownEntityOf(villager) == entt::null)
	{
		return 0;
	}
	// 0x7581AF: the trigger
	const float trigger = GetOwnDesiresTrigger(villager);
	// 0x7581CA: town +0x34 TownDesire::CheckVillagerNeededForTownDesire(this, trigger) 0x745FF0, the jobs' share-out
	// (V3, ECS/Town/TownDesire). Void in the PDB (QAEX), but it leaves 0 or 1 in eax (0x7460EB / 0x7460F7), which the
	// caller compares with 1 (P-11). Pending V4: at night Sleep (16) is first and CheckSatisfySleep sends the villagers
	// with an abode to 36; 37 ARRIVES_HOME is V4, so they stop at the door until then (P-1, accepted for V3)
	const uint32_t result = town_desire::CheckVillagerNeededForTownDesire(TownEntityOf(villager), villager, trigger);
	// 0x7581CF: flags &= ~1 (the tap on its abode is forgotten), always with a town
	if (auto* v = VillagerOf(villager))
	{
		v->flags = static_cast<uint16_t>(v->flags & 0xFFFE);
	}
	return result;
}

float GetOwnDesiresTrigger(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0.0f;
	}
	// 0x7581E6: flags & 1 (after a tap on its abode) -> 0
	if ((v->flags & Villager::k_FlagAfterTapOnAbode) != 0)
	{
		return 0.0f;
	}
	// 0x7581FC..0x758212: f = IsHungry ? GetDesireForFood : 0
	const float f = IsHungry(villager) ? GetDesireForFood(villager) : 0.0f;
	// 0x75821A..0x75824E: l = GetDesireForLife - info +0x38C when > 0 (test ah, 0x41), else 0
	const float threshold = InfoOf(villager).ownDesireThreshold;
	const float lifeDesire = GetDesireForLife(villager) - threshold;
	const float l = lifeDesire > 0.0f ? lifeDesire : 0.0f;
	// 0x758256..0x758295: max(f, l) + 0.5 x min(f, l)
	const float high = f > l ? f : l;
	const float low = f < l ? f : l;
	const float t = low * 0.5f + high;
	// 0x75829B..0x7582BE: a child gets at least 0.11 (0x8CA280; 0x3DE147AE)
	if (IsChild(villager) && t <= 0.11f)
	{
		return 0.11f;
	}
	// 0x7582C7..0x7582E1: min(t, 1)
	return t < 1.0f ? t : 1.0f;
}

float GetDesireForLife(entt::entity villager)
{
	// 0x75BBA0: GetLifeDesireFromLife(GetLife()) (vt +0x11C)
	return GetLifeDesireFromLife(villager, life::LifeOf(villager));
}

float GetLifeDesireFromLife(entt::entity villager, float life)
{
	// 0x75BBC0: D = info +0x35C damageThresholdToGoHome; m = D < life ? D : life (fcomp, test ah, 1)
	const float d = InfoOf(villager).damageThresholdToGoHome;
	const float m = d < life ? d : life;
	// 0x75BBE0..0x75BBF6: x = (life - m) / (1 - D); 1 - x^2
	const float x = (life - m) / (1.0f - d);
	return 1.0f - x * x;
}

uint32_t CheckSatisfyOwnDesire(entt::entity villager, float trigger)
{
	// 0x760054..0x76006C: dF = GetDesireForFood - t, dL = GetDesireForLife - t
	const float dF = GetDesireForFood(villager) - trigger;
	const float dL = GetDesireForLife(villager) - trigger;
	TraceIf(villager, fmt::format("decide: own(dF {:.4f}, dL {:.4f})", dF, dL));
	// 0x760070..0x76007D: dF > dL (test ah, 0x41) and dF > 0: food first
	if (dF > dL && dF > 0.0f)
	{
		if (CheckSatisfyOwnFoodDesire(villager) != 0)
		{
			return 1;
		}
		// 0x7600A5..0x7600B8
		return dL > 0.0f ? CheckSatisfySleep(villager) : 0;
	}
	// 0x7600C2..0x760102: (dF <= dL, or dF <= 0) life first
	if (dL > 0.0f)
	{
		if (CheckSatisfySleep(villager) != 0)
		{
			return 1;
		}
		return dF > 0.0f ? CheckSatisfyOwnFoodDesire(villager) : 0;
	}
	return 0;
}

uint32_t CheckSatisfyOwnFoodDesire(entt::entity villager)
{
	// 0x75BF03: IsHungry -> ChangeStateToFindFoodToEat
	if (IsHungry(villager))
	{
		return ChangeStateToFindFoodToEat(villager);
	}
	return 0;
}

uint32_t ChangeStateToFindFoodToEat(entt::entity villager)
{
	// TODO(V4): Villager::ChangeStateToFindFoodToEat 0x75B990 (the storage pit, the field, the food at home). Neutral
	TraceIf(villager, "own food: ChangeStateToFindFoodToEat TODO(V4)");
	return 0;
}

uint32_t CheckSatisfySleep(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// 0x761493..0x7614B4: flags & 1 and life >= D (fcomp, test ah, 1) -> 0
	if ((v->flags & Villager::k_FlagAfterTapOnAbode) != 0 &&
	    !(life::LifeOf(villager) < InfoOf(villager).damageThresholdToGoHome))
	{
		return 0;
	}
	// 0x7614B6..0x7614DB: inside its home (flags & 4): CheckWhenGoingToBed -> SetTopState(119 GOTO_BED_AT_HOME); 1
	if ((v->flags & Villager::k_FlagAtHome) != 0)
	{
		if (CheckWhenGoingToBed(villager) != 0)
		{
			SetTopState(villager, VillagerStates::GotoBedAtHome);
		}
		return 1;
	}
	// 0x7614DD..0x7614F7: an abode -> SetTopState(36 GO_HOME); 1
	if (AbodeOf(villager) != entt::null)
	{
		TraceIf(villager, "decide: own sleep -> 36");
		SetTopState(villager, VillagerStates::GoHome);
		return 1;
	}
	// 0x7614F9..0x761507: TOP 238 SLEEP_IN_TENT -> 1
	return GetState(villager, Index::Top) == VillagerStates::SleepInTent ? 1 : 0;
}

uint32_t CheckWhenGoingToBed([[maybe_unused]] entt::entity villager)
{
	// TODO(V4): Villager::CheckWhenGoingToBed 0x760B60. Neutral
	return 0;
}

uint32_t CheckTakeResourcesToStoragePit(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// 0x7516E3..0x7516FF: movsx; cmp with info +0x26C / +0x270; jg
	const auto& info = InfoOf(villager);
	const int32_t wood = v->resourceHeld.at(1);
	const int32_t food = v->resourceHeld.at(0);
	if (wood > static_cast<int32_t>(info.minWoodToShowGraphic) || food > static_cast<int32_t>(info.minFoodToShowGraphic))
	{
		// 0x751704: SetTopState(31 GOTO_STORAGE_PIT_FOR_DROP_OFF). TODO(V5): 31 is not ported
		TraceIf(villager, fmt::format("decide: resources (wood {}, food {}) -> 31", wood, food));
		SetTopState(villager, VillagerStates::GotoStoragePitForDropOff);
		return 1;
	}
	return 0;
}

uint32_t DiscipleDecideWhatToDo([[maybe_unused]] entt::entity villager)
{
	// TODO(V14): Villager::DiscipleDecideWhatToDo 0x751720. Neutral
	return 0;
}

uint32_t ChildDecideWhatToDo(entt::entity villager)
{
	// 0x757EC3: CheckChild == 1
	if (CheckChild(villager) == 1)
	{
		TraceIf(villager, "decide: child CheckChild");
		return 1;
	}
	// 0x757ECF: CheckNeededForTownDesire (0x757C80 = jmp fn_7581A0) == 1
	if (CheckNeededForTownDesire(villager) == 1)
	{
		return 1;
	}
	// 0x757EDB: ChildGotoCreche
	if (ChildGotoCreche(villager) != 0)
	{
		return 1;
	}
	// 0x757EE4..0x757EEA: SetTopState(114 CHILD_FOLLOWS_MOTHER). TODO(V14): 114 is not ported
	TraceIf(villager, "decide: child -> 114");
	SetTopState(villager, VillagerStates::ChildFollowsMother);
	return 1;
}

uint32_t CheckChild(entt::entity villager)
{
	// 0x757E85..0x757E96: not a child -> GoHome (its result)
	if (!IsChild(villager))
	{
		return GoHome(villager);
	}
	// 0x757E98..0x757EA1: IsMotherAlive == 0 -> mother (+0x100) = 0
	if (IsMotherAlive(villager) == 0)
	{
		if (auto* v = VillagerOf(villager))
		{
			v->mother = entt::null;
		}
	}
	// 0x757EA9..0x757EB9: hungry -> GoHome
	if (IsHungry(villager))
	{
		return GoHome(villager);
	}
	return 0;
}

uint32_t IsMotherAlive([[maybe_unused]] entt::entity villager)
{
	// TODO(V14): 0x757F40 (the mother +0x100 available, of the same tribe +0x1F4, IsAMother 0x751110 and Living +0xB4).
	// Neutral: the mother is left as it is
	return 1;
}

uint32_t ChildGotoCreche([[maybe_unused]] entt::entity villager)
{
	// TODO(V14): 0x7579F0 (the town's creche +0x744, functional -> SetupMoveToOnFootpath(creche, door, 113)). openblack
	// does not link creches to their town: 0
	return 0;
}

uint32_t CheckNeedNewAbode([[maybe_unused]] entt::entity villager)
{
	// TODO(V4): Villager::CheckNeedNewAbode 0x757F90. Neutral
	return 0;
}

// ---- the idle branch ---------------------------------------------------------------------------------------------

uint32_t SetupNothingToDo(entt::entity villager)
{
	// 0x753B5A..0x753B74: GetTown, GetAbode, GameRand(9) (Villager.cpp 0xB46)
	const auto townEntity = TownEntityOf(villager);
	const auto abode = AbodeOf(villager);
	uint32_t r = GameRand(9);
	if (const auto forced = g_ForcedNothingRolls.find(villager); forced != g_ForcedNothingRolls.end())
	{
		r = forced->second;
		g_ForcedNothingRolls.erase(forced);
	}
	// 0x753B7C..0x753B8D: the jump table 0x753C64 = {0, 1, 1, 1, 2, 2, 2, 2, 2} (> 8: SetTopState(36))
	static constexpr uint8_t k_Branch[9] = {0, 1, 1, 1, 2, 2, 2, 2, 2};
	const int branch = r <= 8 ? k_Branch[r] : -1;
	std::string roll = fmt::format("decide: nothing r={}", r);
	if (branch == 0)
	{
		// 0x753B94..0x753BA4: a functional abode (vt +0xD4) -> 36
		if (abode != entt::null && aq::IsFunctional(abode))
		{
			TraceIf(villager, roll + " -> 36");
			SetTopState(villager, VillagerStates::GoHome);
			return 1;
		}
		// 0x753BA6..0x753BBD: GameRand(100) (0xB4B) < 10 -> 36; else on to branch 1
		const auto r100 = GameRand(100);
		roll += fmt::format(" r100={}", r100);
		if (r100 < 10)
		{
			TraceIf(villager, roll + " -> 36");
			SetTopState(villager, VillagerStates::GoHome);
			return 1;
		}
	}
	if (branch == 0 || branch == 1)
	{
		// 0x753BD7..0x753BEC: an abode (functional or not) -> 245; else on to branch 2
		if (abode != entt::null)
		{
			TraceIf(villager, roll + " -> 245");
			SetTopState(villager, VillagerStates::GoAndChilloutOutsideHome);
			return 1;
		}
	}
	if (branch >= 0)
	{
		// 0x753BF6..0x753C35: a town and GetChillOutPos -> SetupMoveToWithHug(pos, 246); 1 whatever it returns
		glm::ivec2 pos(0);
		if (townEntity != entt::null && GetChillOutPos(villager, pos))
		{
			TraceIf(villager, roll + " -> 246" + Xz(pos));
			MoveTo(villager, pos, VillagerStates::SitAndChillout);
			return 1;
		}
	}
	// 0x753C3F..0x753C4D: SetTopState(36)
	TraceIf(villager, roll + " -> 36");
	SetTopState(villager, VillagerStates::GoHome);
	return 1;
}

bool GetChillOutPos(entt::entity villager, glm::ivec2& out)
{
	// 0x753C79..0x753C80
	const auto townEntity = TownEntityOf(villager);
	if (townEntity == entt::null)
	{
		return false;
	}
	// 0x753C8D: c = GetCongregationPos
	const auto c = tq::GetCongregationPos(townEntity);
	// 0x753C92..0x753CAA: R = info +0x140 x 0.1 (0x8AC404)
	const float r = TownInfo().maxDistanceFromCongreationPosThatPeopleChillOut * 0.1f;
	// 0x753CAE: Get3DAngleFromXZ(c, me)
	const float toMe = tq::Get3DAngleFromXZ(c, tq::PosOf(villager));
	// 0x753CB7..0x753CE3: GameFloatRand(pi / 4) (0x3F490FDB, Villager.cpp 0xB7C) - pi / 8 (0x8C6CA0) + that
	const float angle = GameFloatRand(glm::quarter_pi<float>()) - 0.39269909f + toMe;
	// 0x753CE7..0x753D08: GameFloatRand(9 R) (0x8FFEC8, 0xB7D) + R
	const float distance = GameFloatRand(r * 9.0f) + r;
	// 0x753D0D..0x753D36: c + GetPosFromAngle(angle, distance)
	out = c + tq::GetPosFromAngle(angle, distance);
	return true;
}

bool GetPosOutsideMyHouse(entt::entity villager, glm::ivec2& out)
{
	// 0x753D58..0x753D5D: a town
	if (TownEntityOf(villager) == entt::null)
	{
		return false;
	}
	// 0x753D5F..0x753D70: R' = info +0x144 x 0.5
	const float r = TownInfo().maxDistanceFromHouseThatPeopleChillOut * 0.5f;
	// 0x753D74..0x753D7B: an abode
	const auto abode = AbodeOf(villager);
	if (abode == entt::null)
	{
		return false;
	}
	// 0x753D7D..0x753DAF: GetPosOutside(3, R', R')
	out = aq::GetPosOutside(abode, 3.0f, r, r);
	return true;
}

void GetMeToMyChillOutPos(entt::entity villager, ChillOutPosFn pmf, glm::ivec2 a, float r, const glm::ivec2* c)
{
	const auto me = tq::PosOf(villager);
	// 0x76B618..0x76B633: dist = GetDistanceInMetres(A, me) (fn_00605CD0); dist <= R (test ah, 0x41) -> near
	const float distance = tq::GetDistanceInMetres(a, me);
	if (!(distance <= r))
	{
		// 0x76B635..0x76B66C: far: pmf(tmp) -> SetupMoveToWithHug(tmp, GetFinalState (vt +0xB04)); else nothing
		glm::ivec2 tmp(0);
		if (pmf(villager, tmp))
		{
			TraceIf(villager, fmt::format("chill {}: far d={:.2f} R={:.2f} -> {}", static_cast<uint32_t>(GetFinalState(villager)),
			                              distance, r, Xz(tmp)));
			MoveTo(villager, tmp, GetFinalState(villager));
		}
		return;
	}
	// 0x76B67B..0x76B6A2: tmp = me; radius = Get2DRadius (vt +0x64) x 1.2 (0x8C6C98)
	glm::ivec2 tmp = me;
	const float radius = tq::Get2DRadius(villager) * 1.2f;
	// 0x76B6A6..0x76B6C6: CheckForClearArea(tmp, radius, IsObject (FUN_00761BB0 -> vt +0x460), this)
	entt::entity blocker = entt::null;
	if (tq::CheckForClearArea(tmp, radius, &tq::IsObject, villager, &blocker))
	{
		// 0x76B6D2..0x76B6DF: C -> LookAtPos(C, 2)
		if (c != nullptr)
		{
			LookAtPos(villager, *c, 2);
		}
		// 0x76B6E4..0x76B6ED: SetTopState(246)
		TraceIf(villager, fmt::format("chill {}: near clear -> 246", static_cast<uint32_t>(GetFinalState(villager))));
		SetTopState(villager, VillagerStates::SitAndChillout);
		return;
	}
	// 0x76B6FD..0x76B741: R + 5 < dist (test ah, 1) -> tmp += GetPosFromAngle(Get3DAngleFromXZ(tmp, A), 5). Dead code:
	// this branch only runs with dist <= R. (The original reads A here, 0x76B712 [esp + 0x44], not C)
	if (r + 5.0f < distance)
	{
		tmp += tq::GetPosFromAngle(tq::Get3DAngleFromXZ(tmp, a), 5.0f);
	}
	// 0x76B746..0x76B794: FindClearArea(tmp, tmp, 5, 1, radius, IsObject, this) -> SetupMoveToWithHug(tmp, final)
	if (TraceOn(villager) && blocker != entt::null)
	{
		const auto& registry = Entities();
		const char* kind = registry.AllOf<Villager>(blocker) ? "villager" : registry.AllOf<Abode>(blocker) ? "abode" : "object";
		const auto at = tq::ToMetres(tq::PosOf(blocker));
		Trace(villager, fmt::format("chill: blocker {} at ({:.1f}, {:.1f})", object_index::Of(blocker), at.x, at.y));
		const float door = registry.AllOf<Abode>(blocker) ? tq::GetDistanceInMetres(tq::PosOf(blocker), aq::GetArrivePos(blocker)) : -1.0f;
		Trace(villager, fmt::format("chill {}: near blocked by {} {} (d {:.2f}, radius {:.2f}, its door {:.2f}, mine x 1.2 {:.2f}, R {:.2f})",
		                            static_cast<uint32_t>(GetFinalState(villager)), kind, object_index::Of(blocker),
		                            tq::GetDistanceInMetres(tq::PosOf(blocker), me), tq::Get2DRadius(blocker), door, radius, r));
	}
	if (tq::FindClearArea(tmp, tmp, 5.0f, 1.0f, radius, &tq::IsObject, villager))
	{
		TraceIf(villager, fmt::format("chill {}: near blocked -> {}", static_cast<uint32_t>(GetFinalState(villager)), Xz(tmp)));
		MoveTo(villager, tmp, GetFinalState(villager));
		return;
	}
	// 0x76B7A3..0x76B7C9: pmf(tmp) -> SetupMoveToWithHug(tmp, final)
	if (pmf(villager, tmp))
	{
		TraceIf(villager, fmt::format("chill {}: near blocked, no clear area -> {}", static_cast<uint32_t>(GetFinalState(villager)),
		                              Xz(tmp)));
		MoveTo(villager, tmp, GetFinalState(villager));
	}
}

// ---- test hooks --------------------------------------------------------------------------------------------------

void SetWorshipCheckForTests(std::function<bool(entt::entity)> check)
{
	g_WorshipCheckForTests = std::move(check);
}

void ForceNextNothingRoll(entt::entity villager, uint32_t r)
{
	g_ForcedNothingRolls[villager] = r;
}

void SetDecideLogForTests(std::function<void(const char*)> log)
{
	g_DecideLog = std::move(log);
	if (!g_DecideLog)
	{
		g_ForcedNothingRolls.clear();
	}
}
} // namespace openblack::ecs::villager
