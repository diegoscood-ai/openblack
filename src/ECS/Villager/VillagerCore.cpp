/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerCore.h"

#include <algorithm>
#include <cmath>

#include <array>
#include <vector>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Common/RandomNumberManager.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Life.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Villager/VillagerOriginalFns.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerSpeed.h"
#include "Game.h"
#include "InfoConstants.h"
#include "GameClock.h"
#include "Locator.h"

// Villager.cpp / Living.cpp of runblack.exe W120: the villager's turn and its state changes (VillagerCore.h; the
// disassembly is in dev\tmp_dis\aldeanos\core\d_process.txt, d_setstate.txt, d_callfns.txt, d_decide.txt, d_pause.txt,
// d_ctor.txt and v1\).

namespace openblack::ecs::villager
{
using namespace components;
using state_info::StateInfo;

namespace
{
std::optional<uint32_t> g_TurnForTests;
std::function<uint32_t(uint32_t)> g_RandForTests;
std::function<float(float)> g_FloatRandForTests;

/// The villagers VillagerDead marked this turn (FlushDeaths kills them after the turn)
struct PendingDeath
{
	entt::entity villager;
	DeathReason reason;
};
std::vector<PendingDeath> g_Deaths;
/// Whether CheckEveryTime's hurt rule enters 36 GO_HOME (on since V2: 36 walks to the door; the tests may turn it off)
bool g_GoHomeEnabled = true;

/// g_DiscipleInfos 0x99A1F8 (.rdata, 13 x 0x1C) +0xC, read from runblack.exe (the same in bw1-decomp Villager.cpp:31):
/// NONE 0, FARMER 1, FORESTER 1, FISHERMAN 1, BUILDER 1, BREEDER 1, PROTECTION 1, MISSIONARY 0, CRAFTSMAN 1, TRADER 1,
/// CHANGE_HOUSE 0, WORSHIP 0, FROM_VORTEX 0
constexpr std::array<uint32_t, 13> k_DiscipleIgnoresNeeds = {0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0};

/// DEATH_REASON's names (Enums.h DeathReason)
constexpr std::array<const char*, static_cast<size_t>(DeathReason::_COUNT)> k_DeathReasonNames = {
    "NONE", "STARVING", "SPELL", "ANIMAL", "CHANT", "PLAYER_INTERACTION", "PLAYER_INTERACTION_DROWN", "SACRIFICE",
    "EXHAUSTION", "OLD_AGE"};

const char* DeathName(DeathReason reason)
{
	return k_DeathReasonNames.at(std::min<size_t>(static_cast<size_t>(reason), k_DeathReasonNames.size() - 1));
}

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

systems::LivingActionSystemInterface& System()
{
	return Locator::livingActionSystem::value();
}

LivingAction* ActionOf(entt::entity villager)
{
	return Entities().TryGet<LivingAction>(villager);
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

VillagerStates Raw(const LivingAction& action, Index index)
{
	return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
}

uint32_t Number(VillagerStates state)
{
	return static_cast<uint32_t>(state);
}

std::string StateText(VillagerStates state)
{
	const auto n = std::min<size_t>(Number(state), k_VillagerStateStrings.size() - 1);
	return fmt::format("{} {}", Number(state), k_VillagerStateStrings.at(n));
}

/// Villager::GetTown 0x751F00 (vt +0x48): +0x12C
Town* TownOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr || v->town == entt::null || !Entities().Valid(v->town))
	{
		return nullptr;
	}
	return Entities().TryGet<Town>(v->town);
}

/// Town +0x5E8, read by the disciple check of CheckEveryTime (0x750643). TODO(V3): openblack's towns have no such field
/// yet: 0
uint32_t TownField0x5E8([[maybe_unused]] const Town& town)
{
	return 0;
}

/// Villager +0xE0 & 0x2: at the worship site. Its writers (AddVillagerToWorshipSite 0x76C3F0 / RemoveVillagerFromWorshipSite
/// 0x76C440) are Milagros' and keep it in WorshipVillager::atSite (team_apis.md (C)) until it moves into `flags`
bool AtWorshipSite(entt::entity villager, const Villager& v)
{
	if ((v.flags & Villager::k_FlagAtWorshipSite) != 0)
	{
		return true;
	}
	const auto* worship = Entities().TryGet<const WorshipVillager>(villager);
	return worship != nullptr && worship->atSite;
}

/// fn_00436A80 -> UniqueKeyHeap::GetUniqueIdFromAddress 0x7E19A0: the heap key of the object. (aproximado) openblack
/// has no UniqueKeyHeap: the creation index (Object +0x3C) stands for it
uint32_t UniqueId(entt::entity villager)
{
	return static_cast<uint32_t>(std::max<int64_t>(object_index::Of(villager), 0));
}
} // namespace

// ---- clock and random --------------------------------------------------------------------------------------------

uint32_t CurrentTurn()
{
	if (g_TurnForTests)
	{
		return *g_TurnForTests;
	}
	return game_clock::Turn();
}

void SetTurnForTests(std::optional<uint32_t> turn)
{
	g_TurnForTests = turn;
}

uint32_t GameRand(uint32_t n)
{
	if (g_RandForTests)
	{
		return g_RandForTests(n);
	}
	if (n == 0 || !Locator::rng::has_value())
	{
		return 0;
	}
	return Locator::rng::value().NextValue<uint32_t>(0, n - 1);
}

float GameFloatRand(float x)
{
	if (g_FloatRandForTests)
	{
		return g_FloatRandForTests(x);
	}
	if (x == 0.0f || !Locator::rng::has_value())
	{
		return 0.0f;
	}
	const float value = Locator::rng::value().NextValue(0.0f, std::abs(x));
	return x < 0.0f ? -value : value;
}

void SetGoHomeEnabledForTests(bool enabled)
{
	g_GoHomeEnabled = enabled;
}

void SetRandForTests(std::function<uint32_t(uint32_t)> rand, std::function<float(float)> floatRand)
{
	g_RandForTests = std::move(rand);
	g_FloatRandForTests = std::move(floatRand);
}

// ---- data --------------------------------------------------------------------------------------------------------

const GVillagerInfo& InfoOf(entt::entity villager)
{
	if (const auto* info = ecs::VillagerInfoOf(villager); info != nullptr)
	{
		return *info;
	}
	return Locator::infoConstants::value().villager.at(0);
}

VillagerStates GetState(entt::entity villager, Index index)
{
	const auto* action = ActionOf(villager);
	return action != nullptr ? Raw(*action, index) : VillagerStates::InvalidState;
}

VillagerStates GetFinalState(entt::entity villager)
{
	// 0x751DD0: Infos[+0x8C].isFinal (0xDB9E84) ? +0x8C : +0x8D
	const auto top = GetState(villager, Index::Top);
	return state_info::IsFinal(StateInfo(top)) ? top : GetState(villager, Index::Final);
}

uint32_t GetAge(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr ? AgeFromBirthTurn(v->birthTurn, CurrentTurn()) : 0;
}

void SetAgeBirthTurn(entt::entity villager, uint32_t age, uint32_t turn)
{
	if (auto* v = VillagerOf(villager))
	{
		v->birthTurn = BirthTurnForAge(age, turn);
	}
}

bool IsChild(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr && (v->flags & Villager::k_FlagChild) != 0;
}

bool IsWoman(entt::entity villager)
{
	// 0x752620: GVillagerInfo +0x1F8 == 1 (FEMALE) && !IsChild
	return InfoOf(villager).sex == SexType::Female && !IsChild(villager);
}

bool IsHungry(entt::entity villager)
{
	// 0x752600: fcomp food, info +0x2C0; test ah, 0x41 -> food <= hungryForFood
	const auto* v = VillagerOf(villager);
	return v != nullptr && v->food <= InfoOf(villager).hungryForFood;
}

uint32_t GetGameTurnsSinceLastChecked(entt::entity villager, uint32_t turn)
{
	// 0x750670: g_game +0x205A40 - +0xEC (unsigned)
	const auto* v = VillagerOf(villager);
	return v != nullptr ? turn - v->lastCheckTurn : 0;
}

void SetGameTurnLastChecked(entt::entity villager, uint32_t turn)
{
	// 0x7506A0: +0xEC = g_game +0x205A40
	if (auto* v = VillagerOf(villager))
	{
		v->lastCheckTurn = turn;
	}
}

bool IsScriptControlled(entt::entity villager)
{
	// +0x24 & 0x400: written by GameThingWithPos::SetControlledByScript 0x402240 (script_held: AddScriptThing, the
	// release, and the vortex's fn_005FE3B0 0x5FE474)
	return script_held::IsControlledByScript(villager);
}

float Power(float x)
{
	// POWER 0x75BB60: m = x < 1 ? x : 1 (0x75BB64); m * m * m (the loop of 2 fmul, 0x75BB81..0x75BB8B); 1 - that
	const float m = x < 1.0f ? x : 1.0f;
	return 1.0f - m * m * m;
}

float GetDesireForFood(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return Power(v != nullptr ? v->food : 1.0f);
}

bool DiscipleIgnoresNeeds(uint8_t discipleType)
{
	// cmp [0x99A204 + 0x1C * t], 1 (0x75061E). Types past the table: (aproximado) no
	return discipleType < k_DiscipleIgnoresNeeds.size() && k_DiscipleIgnoresNeeds.at(discipleType) == 1;
}

// ---- creation ----------------------------------------------------------------------------------------------------

bool RollSpecialVillager()
{
	// 0x74FBF0: GameRand(10) <= 1 -> SpecialVillager::Create 0x71F1A0; a special one that comes back without +0xA & 1
	// is the result (0x74FC1F). TODO(V14): SpecialVillager is not ported: a normal villager is always made
	return GameRand(10) <= 1;
}

uint32_t SetAge(entt::entity villager, const GVillagerInfo& info, uint32_t age, uint32_t turn,
                const std::function<void(uint32_t age)>& meshesAndScale)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return age;
	}
	// 0x7528C0: below grownUpAge (GLivingInfo +0x138) a child, flags |= 8; else max(age, 18) and flags &= ~8
	if (age < info.grownUpAge)
	{
		v->flags = static_cast<uint16_t>(v->flags | Villager::k_FlagChild);
		v->lifeStage = Villager::LifeStage::Child;
	}
	else
	{
		age = std::max<uint32_t>(age, 18);
		v->flags = static_cast<uint16_t>(v->flags & ~Villager::k_FlagChild);
		v->lifeStage = Villager::LifeStage::Adult;
	}
	// the meshes (DetailMeshes) and InitialiseScale + SetScaleForAge (FloatRand)
	if (meshesAndScale)
	{
		meshesAndScale(age);
	}
	// Living::SetAge 0x5ED2C0
	SetAgeBirthTurn(villager, age, turn);
	return age;
}

void Construct(entt::entity villager, const GVillagerInfo& info, uint32_t age, uint32_t turn, bool inWater,
               const std::function<void(uint32_t age)>& meshesAndScale)
{
	auto* v = VillagerOf(villager);
	auto* action = ActionOf(villager);
	if (v == nullptr || action == nullptr)
	{
		return;
	}
	// 0x74F966..0x74F9D2 (+0xE4, +0xFC, +0x10C..+0x124) and SetToZero 0x74FB20: flags, food, foodSpeedUp, resourceHeld,
	// +0xFC, abode, discipleType, town, lastCheckTurn, mother, +0x104, +0x108 = 0
	v->flags = 0;
	v->food = 0.0f;
	v->foodSpeedUp = 0;
	v->resourceHeld = {};
	v->abode = entt::null;
	v->discipleType = 0;
	v->town = entt::null;
	v->lastCheckTurn = 0;
	v->mother = entt::null;
	v->targetThing = entt::null;
	v->pregnancy = 0;
	// 0x74F9E9: age < grownUpAge -> mother = 0 again (already 0 after SetToZero)
	if (age < info.grownUpAge)
	{
		v->mother = entt::null;
	}
	// 0x74F9F5 SetAge
	SetAge(villager, info, age, turn, meshesAndScale);
	// 0x74FA02 foodSpeedUp = 0; 0x74FA08: a woman (info +0x1F8 == 1): pregnancy (+0xF8) = 0
	v->foodSpeedUp = 0;
	if (info.sex == SexType::Female)
	{
		v->pregnancy = 0;
	}
	// 0x74FA18..0x74FA67: min(1, GameFloatRand(0.6) + hungryForFood (+0x2C0)) is a macro that evaluates twice: a first
	// draw below 1 draws again and keeps the second unclamped (0.5 .. 1.1). 0x3F19999A = 0.6
	const float first = GameFloatRand(0.6f) + info.hungryForFood;
	if (first < 1.0f)
	{
		const float second = GameFloatRand(0.6f);
		v->food = second + info.hungryForFood;
	}
	else
	{
		v->food = 1.0f;
	}
	// 0x74FA6D..0x74FACD: lastCheckTurn = turn - (GameRand(processChecksEvery +0x2DC) < turn ? GameRand(...) : turn)
	uint32_t back = turn;
	if (GameRand(info.processChecksEvery) < turn)
	{
		back = GameRand(info.processChecksEvery);
	}
	v->lastCheckTurn = turn - back;
	// 0x74FAB0..0x74FADF: the state counter (Object +0x58) = GameRand(500) + 1, before the water test
	action->turnsUntilStateChange = static_cast<uint16_t>(GameRand(500) + 1);
	// 0x74FADC..0x74FAF5: MapCoords::IsWater 0x6035B0 -> SetState(0, 16 DROWNING), else SetState(0, 85 CREATED):
	// Villager::SetState only (no entry, clips or speed; nothing of the water's: Villager::Drowning does the rest)
	SetState(villager, Index::Top, inWater ? VillagerStates::Drowning : VillagerStates::Created);
	// 0x74FAFF ++g_game +0x205A54 (the villager count). TODO(V12): openblack keeps no such counter
	// 0x74FB0C SetSkeleton(arg 4) 0x7562C0. TODO(V12): no villager skeletons
	if (TraceOn(villager))
	{
		const auto* transform = Entities().TryGet<const Transform>(villager);
		const glm::vec3 at = transform != nullptr ? transform->position : glm::vec3(0.0f);
		Trace(villager, fmt::format("created at ({:.1f}, {:.1f}, {:.1f}): age {} food {:.4f} lastCheckTurn {} counter {} state {}{}",
		                            at.x, at.y, at.z, GetAge(villager), v->food, v->lastCheckTurn,
		                            action->turnsUntilStateChange, StateText(GetState(villager, Index::Top)),
		                            inWater ? " (born in water)" : ""));
	}
}

// ---- the turn ----------------------------------------------------------------------------------------------------

void ProcessReaction([[maybe_unused]] entt::entity villager)
{
	// TODO(reactions, Milagros M-5): Living::ProcessReaction 0x5F1270 for the villagers (+0x94 reaction, +0xBC its
	// object, the per-type turns of 0xC09CF0). Milagros' handler (VillagerReactions.cpp) applies them meanwhile.
}

void ProcessFoodSpeedup(entt::entity villager, uint32_t turn)
{
	// 0x753430: IsFoodSpeedUp (vt +0x87C, 0x55C980: +0xF0 != 0) && turn % 10 == 0 -> --foodSpeedUp
	auto* v = VillagerOf(villager);
	if (v != nullptr && v->foodSpeedUp != 0 && turn % 10 == 0)
	{
		--v->foodSpeedUp;
	}
}

uint32_t ProcessState(entt::entity villager, uint32_t turn)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return 0;
	}
	// 0x74FF76: ++Living +0x90 (u16)
	++action->turnsSinceStateChange;
	// 0x74FF7E
	ProcessFoodSpeedup(villager, turn);
	// 0x74FF83..0x750011: the validate slot (+0x80, 0xD09218) of TOP and of the raw FINAL, if any; the result is unused
	System().VillagerCallValidate(*action, Index::Top);
	System().VillagerCallValidate(*action, Index::Final);
	// 0x750013..0x750046: flags & 0x800 -> IsReadyForNewAnimation(1) ? FinishedIntoOutOfAnimation; return 1 (no
	// CheckEveryTime, no state). The flags 0x800 / 0x1000 live in SkeletalAnimation::transitionFlags
	if (ecs::VillagerWaitsForTransition(villager, action->turnsSinceStateChange))
	{
		return 1;
	}
	// 0x750049 (the result is unused)
	CheckEveryTime(villager, turn);
	// (aproximado until V12) the original keeps the villager after VillagerDead (SetDying -> 13 SET_DYING) and calls
	// CallState anyway; openblack kills it after the turn, so its state does not run again
	if (IsDying(villager) || !Entities().Valid(villager))
	{
		return 1;
	}
	// 0x750050 CallState 0x7521D0: the TOP's slot +0x00
	auto* again = ActionOf(villager);
	if (again == nullptr)
	{
		return 1;
	}
	return System().VillagerCallState(*again, Index::Top);
}

uint32_t CheckEveryTime(entt::entity villager, uint32_t turn)
{
	auto* action = ActionOf(villager);
	auto* v = VillagerOf(villager);
	if (action == nullptr || v == nullptr)
	{
		return 1;
	}
	// 0x750425: GameThingWithPos +0x25 & 4 (controlled by a script) -> 1
	if (IsScriptControlled(villager))
	{
		return 1;
	}
	const auto& info = InfoOf(villager);
	const GVillagerStateTableInfo* st = &StateInfo(Raw(*action, Index::Top));
	// the life's wear (Object::ReduceLife vt +0x5B8, 0x637810)
	if (state_info::IsMoving(*st))
	{
		// 0x750439..0x75046F: a moving TOP (+0x24 memory) wears the TOP's drain (+0x108), and from here on `st` is the raw
		// FINAL's row (+0x8D, not GetFinalState)
		life::ReduceLife(villager, state_info::LifeDrainPerTurn(*st));
		st = &StateInfo(Raw(*action, Index::Final));
	}
	else
	{
		// 0x750471..0x75049F: GetFinalState's drain (0xDB9F70); `st` stays the TOP's
		life::ReduceLife(villager, state_info::LifeDrainPerTurn(StateInfo(GetFinalState(villager))));
	}
	// 0x7504A1: the periodic checks of the state (+0xF4 memory)
	if (state_info::DoPeriodicChecks(*st))
	{
		// 0x7504B9: life == 0 exactly (fcomp 0; test ah, 0x40)
		if (v->life == 0.0f)
		{
			// 0x7504D0..0x7504EA: final 248, 249, 250 (from worship) or flags & 2 (0x7504DC, at the worship site) ->
			// CHANT, else EXHAUSTION
			const auto final = GetFinalState(villager);
			const bool chant = final == VillagerStates::GoHomeFromWorship || final == VillagerStates::ArrivesHomeFromWorship ||
			                   final == VillagerStates::SleepInTentFromWorship || AtWorshipSite(villager, *v);
			// 0x7504EF..0x7504FE: VillagerDead(reason, GetPlayer(), 0.0, 1). TODO(V12): GetPlayer (vt +0x1C)
			VillagerDead(villager, chant ? DeathReason::Chant : DeathReason::Exhaustion, PlayerNames::NEUTRAL, 0.0f, 1);
			return 1;
		}
		// 0x75050E..0x75051E: turns since the last check > processChecksEvery (+0x2DC), strictly (jbe)
		const uint32_t turns = GetGameTurnsSinceLastChecked(villager, turn);
		if (turns > info.processChecksEvery)
		{
			if (TraceOn(villager))
			{
				Trace(villager, fmt::format("check: turn {} t {} life {:.6f} food {:.4f} top {} final {}", turn, turns, v->life,
				                            v->food, StateText(Raw(*action, Index::Top)),
				                            StateText(Raw(*action, Index::Final))));
			}
			// 0x750524..0x750546: (turn + UniqueId) % 800 < t (unsigned div by 0x320)
			if ((turn + UniqueId(villager)) % 800 < turns)
			{
				// 0x75054A
				if (CheckDeathFromOldAge(villager))
				{
					return 1;
				}
			}
			// 0x750557..0x7505C3: hurt -> GO_HOME, and on: life < damageThresholdToGoHome (+0x35C; fcomp, test ah, 1),
			// not at home (flags & 4), the state goes home when hurt (+0xF8 memory), not downed (Living::status +0xB4 &
			// 0x80: components::DownedVillager), the state does not forbid it (+0xE8 memory), and not reacting to food
			// (rows 19 / 20: 0xDBB2E4 / 0xDBB3F8) unless food > hungryForFood (test ah, 0x41)
			const auto& table = Locator::infoConstants::value().villagerStateTable;
			const bool foodReaction = st == &table.at(static_cast<size_t>(VillagerStates::GotoFoodReaction)) ||
			                          st == &table.at(static_cast<size_t>(VillagerStates::ArrivesAtFoodReaction));
			if (v->life < info.damageThresholdToGoHome && (v->flags & Villager::k_FlagAtHome) == 0 &&
			    state_info::GoHomeWhenHurt(*st) && !Entities().AllOf<DownedVillager>(villager) &&
			    !state_info::NoGoHomeWhenHurt(*st) && (!foodReaction || v->food > info.hungryForFood))
			{
				// 0x7505C3 SetTopState(36 GO_HOME): Villager::GoHome 0x760270 walks to the abode's door (VillagerHome.cpp);
				// TODO(V4): its arrival 37 and AT_HOME 38. The tests may switch the rule off (SetGoHomeEnabledForTests)
				if (g_GoHomeEnabled)
				{
					if (TraceOn(villager))
					{
						Trace(villager, fmt::format("hurt (life {:.4f}): GO_HOME", v->life));
					}
					SetTopState(villager, VillagerStates::GoHome);
				}
				else if (TraceOn(villager))
				{
					Trace(villager, fmt::format("hurt (life {:.4f}): GO_HOME off", v->life));
				}
			}
			// 0x7505C9..0x7505D9 (vt +0xAF8)
			if (IsChild(villager))
			{
				CheckChildGrownUp(villager);
			}
			// 0x7505E0..0x7505EC
			if (IsWoman(villager))
			{
				WomanSpecial(villager);
			}
			// 0x7505F3
			return CheckHungry(villager, turn) ? 1 : 0;
		}
	}
	// 0x7505FC..0x750656: a disciple (flags & 0x200) of a type that ignores the needs, with the raw FINAL 221
	// DISCIPLE_NOTHING_TO_DO, and a town whose +0x5E8 is set -> DECIDE_WHAT_TO_DO. TODO(V3): openblack's towns have no
	// +0x5E8 (it is 0), so this never fires yet
	if ((v->flags & Villager::k_FlagDisciple) != 0 && DiscipleIgnoresNeeds(v->discipleType) &&
	    Raw(*action, Index::Final) == VillagerStates::DiscipleNothingToDo)
	{
		if (const auto* town = TownOf(villager); town != nullptr && TownField0x5E8(*town) != 0)
		{
			SetTopState(villager, VillagerStates::DecideWhatToDo);
		}
	}
	return 1;
}

// ---- state changes -----------------------------------------------------------------------------------------------

bool IsStateExitFunctionSameAs(entt::entity villager, VillagerStates next)
{
	// 0x752535: GetFinalState (vt +0xB04); 0x752540..0x752583: its row's exit (+0x20 of the 0x90-byte row, 0xD091B8) and
	// next's, all four dwords equal (the first 0 -> equal at once, 0x75256B) -> 1
	const auto exitOf = [](VillagerStates s) {
		const auto i = static_cast<size_t>(s);
		return i < k_OriginalStateFns.size() ? k_OriginalStateFns.at(i).exit : 0u;
	};
	if (exitOf(GetFinalState(villager)) == exitOf(next))
	{
		return true;
	}
	// 0x752585..0x7525A5: Infos[next] final (0xDB9E84, file 0x0C) -> 0, else 1
	return !state_info::IsFinal(StateInfo(next));
}

bool CanPauseForASecond(entt::entity villager, VillagerStates state)
{
	// 0x752120: TOP != 239 && Infos[s].canPause (0xDB9F3C) && !(+0x25 & 4)
	return GetState(villager, Index::Top) != VillagerStates::PauseForASecond &&
	       state_info::CanPauseForASecond(StateInfo(state)) && !IsScriptControlled(villager);
}

uint32_t SetTopState(entt::entity villager, VillagerStates state)
{
	if (ActionOf(villager) == nullptr)
	{
		return 0;
	}
	const auto before = GetState(villager, Index::Top);
	if (CanPauseForASecond(villager, state))
	{
		// 0x752026..0x75205C: x = 1 - life (life * 0.5 if poisoned, Living::IsPoisoned vt +0x4A4); x^3
		const auto* v = VillagerOf(villager);
		float x = v != nullptr ? v->life : 0.0f;
		if (Entities().AllOf<Poisoned>(villager))
		{
			x *= 0.5f;
		}
		x = 1.0f - x;
		const float x3 = x * x * x;
		// 0x752073..0x752092: GameFloatRand(1) - 0.5 * x^3 < pauseForASecondChance (+0x368)
		const float r = GameFloatRand(1.0f);
		const float threshold = InfoOf(villager).pauseForASecondChance;
		if (r - 0.5f * x3 < threshold)
		{
			if (TraceOn(villager))
			{
				Trace(villager, fmt::format("pause 239 -> {} (rand {:.4f}, threshold {:.4f})", StateText(state), r,
				                            threshold + 0.5f * x3));
			}
			// 0x7520A2: SetupPauseForASecond; 0 -> on to Living::SetTopState
			if (SetupPauseForASecond(villager, state) != 0)
			{
				return k_Done;
			}
		}
	}
	// 0x7520B8
	const auto result = LivingSetTopState(villager, state);
	if (result == k_EntryRefused)
	{
		// 0x7520C4: CallEntryStateFunction(163) (vt +0x90C), its result unused
		CallEntryStateFunction(villager, VillagerStates::DecideWhatToDo);
	}
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("SetTopState {} -> {} = {:#x}", StateText(before), StateText(state), result));
	}
	return result;
}

uint32_t LivingSetTopState(entt::entity villager, VillagerStates state)
{
	// 0x5F28F1: CallExitStateFunction (vt +0x904) -> 0: 0x2E, nothing changed
	if (CallExitStateFunction(villager, state) == 0)
	{
		return k_ExitRefused;
	}
	// 0x5F2900: CallOutofAnimationFunction (vt +0x8E4): may set the flags 0x1800 already
	const auto out = ecs::VillagerCallOutOfAnimation(villager, state);
	// 0x5F290D: CallEntryStateFunction (vt +0x90C) -> 0: 0x2F (the flags and the out-of clip stay touched: literal)
	if (CallEntryStateFunction(villager, state) == 0)
	{
		return k_EntryRefused;
	}
	// 0x5F291B SetStateSpeed, 0x5F2921..0x5F295C the clips
	ecs::VillagerApplyStateClips(villager, state, out);
	return k_Done;
}

uint32_t SetCurrentAndDestinationState(entt::entity villager, VillagerStates current, VillagerStates destination)
{
	const auto before = GetState(villager, Index::Top);
	// 0x7520F0
	const auto result = LivingSetCurrentAndDestinationState(villager, current, destination);
	if (result == k_EntryRefused)
	{
		// 0x752105: CallEntryStateFunction(163) (vt +0x90C)
		CallEntryStateFunction(villager, VillagerStates::DecideWhatToDo);
	}
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("SetCurrentAndDestinationState {} -> {}, {} = {:#x}", StateText(before), StateText(current),
		                            StateText(destination), result));
	}
	return result;
}

uint32_t LivingSetCurrentAndDestinationState(entt::entity villager, VillagerStates current, VillagerStates destination)
{
	if (ActionOf(villager) == nullptr)
	{
		return 0;
	}
	// 0x5F298B: the exit told d (ebx = arg 2)
	if (CallExitStateFunction(villager, destination) == 0)
	{
		return k_ExitRefused;
	}
	// 0x5F299C: the out-of clip for d
	const auto out = ecs::VillagerCallOutOfAnimation(villager, destination);
	// 0x5F29A5..0x5F29B1: CallEntryStateFunction(c, d) (vt +0x908)
	if (CallEntryStateFunction(villager, current, destination) == 0)
	{
		return k_EntryRefused;
	}
	// 0x5F29BB..: SetStateSpeed and the clips, the into function told d (0x5F29EB push ebx)
	ecs::VillagerApplyStateClips(villager, destination, out);
	return k_Done;
}

void SetState(entt::entity villager, Index index, VillagerStates state)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return;
	}
	// 0x753695..0x7536BD: PREVIOUS never keeps a state with Infos +0x20 (file 0x10) set
	if (index == Index::Previous && state_info::NotStoredAsPrevious(StateInfo(state)))
	{
		return;
	}
	const auto old = Raw(*action, index);
	// 0x7536DC..0x7536F0: the old one of any index, if final (0xDB9E84) and not 0, leaves the town's modifiers
	if (state_info::IsFinal(StateInfo(old)) && old != VillagerStates::InvalidState)
	{
		AdjustTownModifier(villager, old, false);
	}
	// 0x753700..0x753714: the new one, if final and not 0, enters them. Also for PREVIOUS (literal: the original counts
	// a stored state as served)
	if (state_info::IsFinal(StateInfo(state)) && state != VillagerStates::InvalidState)
	{
		AdjustTownModifier(villager, state, true);
	}
	// 0x75371E: setting TOP clears FINAL first (SetState(1, 0), vt +0x938, with its own adjustment)
	if (index == Index::Top)
	{
		SetState(villager, Index::Final, VillagerStates::InvalidState);
	}
	// 0x75372F Living::SetState 0x5F2A80 -> LivingAction::SetState 0x5ECC90: states[index] = s; index 0 -> +0x90 = 0
	action->states.at(static_cast<size_t>(index)) = static_cast<uint8_t>(state);
	if (index == Index::Top)
	{
		action->turnsSinceStateChange = 0;
	}
	if (TraceOn(villager) && old != state)
	{
		Trace(villager, fmt::format("SetState {} {} -> {}", LivingAction::k_IndexStrings.at(static_cast<size_t>(index)),
		                            StateText(old), StateText(state)));
	}
}

void AdjustTownModifier(entt::entity villager, VillagerStates state, bool entering)
{
	// 0x753560: GetTown (vt +0x48)
	auto* town = TownOf(villager);
	if (town == nullptr)
	{
		return;
	}
	// 0x75358C: Infos +0x14 (file 0x04), -1 none
	const auto& row = StateInfo(state);
	const int desire = state_info::ServedDesire(row);
	if (desire == -1)
	{
		return;
	}
	// (aproximado) a guard: info.dat has desires 0..16 only
	if (desire < 0 || static_cast<size_t>(desire) >= town->desire.doingNow.size())
	{
		return;
	}
	// 0x7535A8..0x7535D9: k = entering ? 1 : -1; +0x510[d] += k * Infos +0x18; +0x554[d] += k
	const float k = entering ? 1.0f : -1.0f;
	town->desire.doingNow.at(static_cast<size_t>(desire)) += k * state_info::ServedDesireAmount(row);
	town->desire.doingNowCount.at(static_cast<size_t>(desire)) += k;
	// 0x7535E0: the fprintf under the flag 0xCD3C74 -> the trace
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("AdjustTownModifier {} desire {} {:+.0f}: doingNow {:.3f} count {:.0f}", StateText(state),
		                            desire, k, town->desire.doingNow.at(static_cast<size_t>(desire)),
		                            town->desire.doingNowCount.at(static_cast<size_t>(desire))));
	}
}

uint32_t CallExitStateFunction(entt::entity villager, VillagerStates next)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return 0;
	}
	// 0x752328..0x75233B: the raw TOP and GetFinalState, read first
	const auto top = Raw(*action, Index::Top);
	const auto final = GetFinalState(villager);
	// 0x75234E..0x75236E: exit[top](next) (slot +0x20, 0xD091B8), else 1
	const auto a = System().VillagerCallExit(*action, top, next);
	// 0x752370..0x75239E: and exit[final](next) when final != top
	uint32_t b = 1;
	if (top != final)
	{
		if (auto* still = ActionOf(villager); still != nullptr)
		{
			b = System().VillagerCallExit(*still, final, next);
		}
	}
	// 0x7523A0..0x7523A7: 1 only if both are 1
	return a == 1 && b == 1 ? 1 : 0;
}

uint32_t CallEntryStateFunction(entt::entity villager, VillagerStates state)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return 0;
	}
	// 0x7523D7: GetFinalState first
	const auto final = GetFinalState(villager);
	// 0x7523F2..0x75240A: entry[s](final, s) (slot +0x10, 0xD091A8), else 1
	const auto result = System().VillagerCallEntry(*action, state, final, state);
	if (result == 1)
	{
		// 0x752411: SetState(0, s)
		SetState(villager, Index::Top, state);
		return 1;
	}
	// 0x752429..0x75242E: 0x23 -> 1 (the function set the states), else 0
	return result == k_EntryNoSet ? 1 : 0;
}

uint32_t CallEntryStateFunction(entt::entity villager, VillagerStates current, VillagerStates destination)
{
	// 0x752447: GetFinalState before the first entry
	const auto final = GetFinalState(villager);
	// 0x75245A: CallEntryStateFunction(c) (vt +0x90C) -> 0: 0
	if (CallEntryStateFunction(villager, current) == 0)
	{
		return 0;
	}
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return 0;
	}
	// 0x752475..0x75248D: entry[d](final, d), else 1
	const auto result = System().VillagerCallEntry(*action, destination, final, destination);
	if (result == 1)
	{
		// 0x752494: SetState(1, d)
		SetState(villager, Index::Final, destination);
		return 1;
	}
	// 0x7524AC: 0x23 -> 1; else 0 (TOP stays c, FINAL 0: the Villager wrapper's 0x2F then enters 163)
	return result == k_EntryNoSet ? 1 : 0;
}

uint32_t SetupPauseForASecond(entt::entity villager, VillagerStates state)
{
	// 0x76B090: SetCurrentAndDestinationState(239, s) (vt +0x8DC) == 1 (dec, neg, sbb, inc)
	return SetCurrentAndDestinationState(villager, VillagerStates::PauseForASecond, state) == 1 ? 1 : 0;
}

void SetTopStateToFinal(entt::entity villager)
{
	// 0x5ECA80: SetTopState(+0x8D) (vt +0x8E8)
	SetTopState(villager, GetState(villager, Index::Final));
}

uint32_t SetupWaitForCounter(entt::entity villager, uint16_t turns, VillagerStates final)
{
	// 0x76B06E: SetCurrentAndDestinationState(57 WAIT_FOR_COUNTER, final) (vt +0x8DC); anything but 1 -> 0 with the
	// counter untouched (0x76B086)
	if (SetCurrentAndDestinationState(villager, VillagerStates::WaitForCounter, final) != 1)
	{
		return 0;
	}
	// 0x76B07E: +0x58 = the count (the Object u16 state counter)
	if (auto* action = Entities().TryGet<LivingAction>(villager); action != nullptr)
	{
		action->turnsUntilStateChange = turns;
	}
	return 1;
}

uint32_t WaitForCounter(LivingAction& action)
{
	// Living::WaitForCounter 0x5EC310: --+0x58 and, once it is not above 0 (a signed `jg`), SetTopStateToFinal
	// 0x5ECA80. Always 1
	--action.turnsUntilStateChange;
	if (static_cast<int16_t>(action.turnsUntilStateChange) <= 0)
	{
		SetTopStateToFinal(Entities().ToEntity(action));
	}
	return 1;
}

uint32_t SetupMoveToWithHug(entt::entity villager, const glm::vec2& goal, VillagerStates final)
{
	auto& registry = Entities();
	if (!registry.AllOf<LivingAction, WallHug>(villager))
	{
		return 0;
	}
	// 0x5F2894..0x5F28AF: SetCurrentAndDestinationState (vt +0x8DC) with the byte at GLivingInfo +0x124 (moveState; 1
	// MOVE_TO_POS in all 63 villager rows of info.dat) and final first (TOP, whose SetState clears FINAL, then FINAL:
	// 0x752440)
	const auto moveState = static_cast<VillagerStates>(static_cast<uint8_t>(InfoOf(villager).moveState));
	if (SetCurrentAndDestinationState(villager, moveState, final) != 1)
	{
		return 0;
	}
	// MobileWallHug::SetupMobileMoveToPos(pos, 0xC): the goal, a fresh step and a LINEAR move (openblack's WallHug)
	auto& wallHug = registry.Get<WallHug>(villager);
	wallHug.goal = goal;
	wallHug.step = glm::vec2(0.0f);
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	registry.Remove<WallHugObjectReference>(villager);
	registry.Assign<MoveStateLinearTag>(villager);
	return 1;
}

uint32_t LookAtPos(entt::entity villager, glm::ivec2 pos, uint32_t mode)
{
	auto& registry = Entities();
	auto* wallHug = registry.TryGet<WallHug>(villager);
	auto* transform = registry.TryGet<Transform>(villager);
	if (wallHug == nullptr || transform == nullptr)
	{
		return 0;
	}
	// 0x5EC554..0x5EC57A: the step: mode 0 -> 0x40, 1 -> 0x80, 2 -> 0x100, else the mode itself (0x5EC57A mov ebp,
	// [esp+0x18] = entry [esp+8], arg 2 after the four pushes; the function takes two arguments, ret 8)
	const int32_t maxStep = mode == 0 ? 0x40 : mode == 1 ? 0x80 : mode == 2 ? 0x100 : static_cast<int32_t>(mode);
	// 0x5EC57E..0x5EC587: GetAngleFromXZ(me (+0x14), pos)
	const auto me = town_queries::PosOf(villager);
	const int32_t target = town_queries::GetAngleFromXZ(me, pos);
	// 0x5EC58E: the current angle (+0x5C, u16). (aproximado) WallHug::yAngle in radians
	const auto current = static_cast<int32_t>(
	    static_cast<uint32_t>(std::lround(static_cast<double>(wallHug->yAngle) * 2048.0 / (2.0 * 3.14159265358979323846))) &
	    0x7FF);
	const auto setGameAngle = [&](int32_t angle) {
		// MobileWallHug::SetGameAngle 0x60DA90: the angle and the drawn rotation (PathfindingSystem's convention)
		const auto a = static_cast<uint16_t>(angle & 0x7FF);
		wallHug->yAngle = static_cast<float>(a) * 0.0030679617f;
		animal_ai::detail::FaceAngle(*transform, a);
	};
	// 0x5EC594..0x5EC5BF: d = target - current; |d| < step -> SetGameAngle(target); 1
	const int32_t d = target - current;
	if (std::abs(d) < maxStep)
	{
		setGameAngle(target);
		return 1;
	}
	// 0x5EC5C2..0x5EC5F0: d > 0: d < 0x400 (unsigned) -> +step, else -step; d <= 0: |d| < 0x400 -> -step, else +step;
	// & 0x7FF; 0
	bool up;
	if (d > 0)
	{
		up = static_cast<uint32_t>(d) < 0x400;
	}
	else
	{
		up = !(static_cast<uint32_t>(std::abs(d)) < 0x400);
	}
	setGameAngle(up ? current + maxStep : current - maxStep);
	return 0;
}

uint32_t VillagerCreated(LivingAction& action)
{
	// 0x753DD0: v = +0x58; +0x58 = v - 1 (u16); v == 0 -> +0x58 = 0, SetTopState(163) (vt +0x8E8). Returns 1
	const auto v = action.turnsUntilStateChange;
	action.turnsUntilStateChange = static_cast<uint16_t>(v - 1);
	if (v == 0)
	{
		action.turnsUntilStateChange = 0;
		SetTopState(Entities().ToEntity(action), VillagerStates::DecideWhatToDo);
	}
	return 1;
}

uint32_t PauseForASecond(LivingAction& action)
{
	// 0x76B0B0: IsReadyForNewAnimation(1) 0x5EC960 -> SetTopStateToFinal 0x5ECA80. Returns 1
	const auto villager = Entities().ToEntity(action);
	if (ecs::VillagerAnimationDone(villager, action.turnsSinceStateChange))
	{
		SetTopStateToFinal(villager);
	}
	return 1;
}

// ---- checks that are neutral until their milestone ---------------------------------------------------------------

bool CheckHungry(entt::entity villager, uint32_t turn)
{
	// 0x75BCC0: 0 turns since the last check -> 0 (0x75BCD0). TODO(V4): eating, starving, the hunger states
	// (0x75BCD6..0x75BEDA). Every other way ends in SetGameTurnLastChecked (0x75BEDF): the periodic check's clock
	if (GetGameTurnsSinceLastChecked(villager, turn) == 0)
	{
		return false;
	}
	SetGameTurnLastChecked(villager, turn);
	// the one half that does not need the hunger states: 0x75BD92..0x75BD9E, a poisoned villager takes the hunger
	// damage even when it is not hungry (ecs::life::ProcessPoison; the food side comes with V4)
	ecs::life::ProcessPoison(villager);
	return false;
}

bool CheckChildGrownUp([[maybe_unused]] entt::entity villager)
{
	// TODO(V4): Villager::CheckChildGrownUp 0x751050 (the grown-up check and the rescale every 375 turns)
	return false;
}

bool WomanSpecial([[maybe_unused]] entt::entity villager)
{
	// TODO(V4): Villager::WomanSpecial 0x752240 (pregnancy)
	return false;
}

bool CheckDeathFromOldAge([[maybe_unused]] entt::entity villager)
{
	// TODO(V4): Villager::CheckDeathFromOldAge 0x760CA0 (read in dev\tmp_dis\aldeanos\core\d_age.txt)
	return false;
}

// ---- death (provisional until V12) -------------------------------------------------------------------------------

void VillagerDead(entt::entity villager, DeathReason reason, [[maybe_unused]] PlayerNames player,
                  [[maybe_unused]] float amount, [[maybe_unused]] int flag)
{
	// TODO(V12, villager death): Villager::VillagerDead 0x7506C0 (the alignment, the town's counts, the texts,
	// SetDying -> 13). Meanwhile it is marked and killed at the end of the turn (FlushDeaths)
	if (IsDying(villager))
	{
		return;
	}
	g_Deaths.push_back({villager, reason});
	const auto text = fmt::format("died ({})", DeathName(reason));
	if (TraceOn(villager))
	{
		Trace(villager, text);
	}
	else if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Villager {} {}", object_index::Of(villager), text);
	}
}

bool IsDying(entt::entity villager)
{
	return std::any_of(g_Deaths.begin(), g_Deaths.end(), [villager](const auto& d) { return d.villager == villager; });
}

std::optional<DeathReason> PendingDeathReason(entt::entity villager)
{
	const auto it =
	    std::find_if(g_Deaths.begin(), g_Deaths.end(), [villager](const auto& d) { return d.villager == villager; });
	return it != g_Deaths.end() ? std::optional<DeathReason>(it->reason) : std::nullopt;
}

void ForgetDeathsForTests()
{
	g_Deaths.clear();
}

void FlushDeaths()
{
	auto deaths = std::move(g_Deaths);
	g_Deaths.clear();
	auto& registry = Entities();
	for (const auto& death : deaths)
	{
		if (registry.Valid(death.villager))
		{
			life::Kill(death.villager, DeathName(death.reason));
		}
	}
}
} // namespace openblack::ecs::villager
