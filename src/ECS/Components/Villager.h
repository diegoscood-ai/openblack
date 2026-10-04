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

#include <array>
#include <string>
#include <string_view>
#include <tuple>

#include <entt/entity/entity.hpp>
#include <entt/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

struct Villager
{
	/// Originally VillagerTasks
	enum class Task : uint8_t
	{
		IDLE,

		_COUNT
	};
	static constexpr std::array<std::string_view, static_cast<uint8_t>(Task::_COUNT)> k_TaskStrs = {
	    "IDLE", //
	};

	/// Originally VillagerSex
	enum class Sex : uint8_t
	{
		MALE,
		FEMALE,

		_COUNT
	};
	static constexpr std::array<std::string_view, static_cast<uint8_t>(Sex::_COUNT)> k_SexStrs = {
	    "MALE",   //
	    "FEMALE", //
	};

	/// Originally VillagerLifeStage
	enum class LifeStage : uint8_t
	{
		Child,
		Adult,

		_COUNT
	};
	static constexpr std::array<std::string_view, static_cast<uint8_t>(LifeStage::_COUNT)> k_LifeStageStrs = {
	    "Child", //
	    "Adult", //
	};

	using Type = std::tuple<Tribe, Villager::LifeStage, Villager::Sex, VillagerNumber>;

	/// Villager +0xE0 bits (docs/bw1-notes/villagers.md, section Flags): 0x1 after a tap on its abode, 0x2 at the worship site,
	/// 0x4 inside its home, 0x8 a child (SetAge), 0x10 on the way to the worship site / a fire, 0x20 in the hand, 0x80
	/// football / script, 0x200 / 0x400 disciple / disciple follower, 0x2000 going to bed, 0xC000 the tree type of the wood
	/// carried. 0x800 / 0x1000 (into / out-of clip) live in SkeletalAnimation::transitionFlags.
	enum Flags : uint16_t
	{
		k_FlagAfterTapOnAbode = 0x1,
		k_FlagAtWorshipSite = 0x2,
		k_FlagAtHome = 0x4,
		k_FlagChild = 0x8,
		k_FlagOnWayToWorshipSite = 0x10,
		k_FlagInHand = 0x20,
		/// out of the world population g_game +0x205A54: set by Villager::SetDying 0x76A558 (once), read by ~Villager
		/// 0x74FBCD (openblack counts the entities instead: magic::players::WorldPopulation skips these)
		k_FlagCountedOut = 0x40,
		k_FlagFootball = 0x80,
		k_FlagDisciple = 0x200,
		k_FlagDiscipleFollower = 0x400,
		k_FlagGoingToBed = 0x2000,
		/// bits 14-15: the tree type (CarriedTreeType 0..3) of the wood carried, PickupResource 0x751460..0x751480 (`and
		/// 0x3FFF; or (t & 3) << 14`), read by GetWoodCarriedObject 0x7502A0
		k_FlagTreeTypeMask = 0xC000,
	};
	static constexpr uint16_t k_TreeTypeShift = 14; ///< +0xE0 >> 14 (GetWoodCarriedObject 0x7502A9)

	/// Living +0xB4 (u16) bits a villager keeps here (docs/bw1-notes/villagers.md, section Death): 0x1 dead
	/// (Villager::SetDying 0x76A4F1, Living::IsDead 0x417270), 0x30 the landType of the last landing (Villager::EndPhysics
	/// 0x5F0A60: 0 on its feet, 1 / 2 on a side, 3 none; SetDying 0x76A4FF ors in 3), 0x40 a skeleton (SET_SKELETON,
	/// Villager::SetSkeleton 0x7562C0; Living::IsSkeleton 0x416FF0), 0x400 special (EndPhysics 0x5F0D11 -> TestSpecial).
	/// The other bits are elsewhere: 0x2 poisoned (components::Poisoned), 0x80 downed (components::DownedVillager)
	enum Status : uint16_t
	{
		k_StatusDead = 0x1,
		k_StatusLandTypeMask = 0x30,
		k_StatusSkeleton = 0x40,
		k_StatusSpecial = 0x400,
	};
	static constexpr uint16_t k_LandTypeShift = 4; ///< (+0xB4 & 0x30) >> 4

	float life; ///< Object +0x48, 0..1 (Object::GetLife; ecs::life has the setters)
	/// Living +0xA0: the turn it was born (Living::SetAge 0x5ED2C0: turn - age * 1500); its age is
	/// ecs::villager::GetAge (Living::GetAge 0x5ECAF0)
	int32_t birthTurn {0};
	uint16_t flags {0};           ///< +0xE0, the Flags above
	uint16_t status {0};          ///< Living +0xB4, the Status bits above
	float food {0.0f};            ///< +0xE8, the food in its belly (the constructor: 0.5 .. 1.1)
	uint32_t lastCheckTurn {0};   ///< +0xEC, the turn of the last periodic check (CheckHungry resets it)
	uint8_t foodSpeedUp {0};      ///< +0xF0, IsFoodSpeedUp 0x55C980; ProcessFoodSpeedup 0x753430
	uint8_t discipleType {0};     ///< +0xF2, VillagerDisciple (g_DiscipleInfos 0x99A1F8)
	std::array<int16_t, 2> resourceHeld {}; ///< +0xF4 / +0xF6, FOOD and WOOD carried
	int16_t pregnancy {0};        ///< +0xF8, turns left of a pregnancy (0 none)
	/// +0xFC: the BuildingSite (its entity, components::BuildingSite) the villager builds (V7, VillagerBuild.cpp):
	/// written by GotoBuildingSite 0x758A74, GotoStoragePitForBuildingMaterials 0x758912, cleared by ExitBuilding
	/// 0x7597FC and Building 0x758DF8; 0 in the constructor 0x74F96C / SetToZero 0x74FB43
	entt::entity buildingSite {entt::null};
	entt::entity mother {entt::null};      ///< +0x100
	entt::entity targetThing {entt::null}; ///< +0x118 TargetThing (bw1-decomp Villager.h), what the jobs work on
	/// +0x118 while in a building state: the index 0..127 of the site's ring point (GotoBuildingSite 0x758A68,
	/// ReenterBuildingState 0x758FA4, Building 0x758DB2, read by ArrivesAtBuildingSite 0x758B12). The original's union
	/// with TargetThing (SaveBuilding 0x754A23 saves it as 4 bytes); kept apart: every builder read follows a builder
	/// write on the same path (V7_spec §2.1)
	int32_t buildPosIndex {0};
	/// +0x118 (u8) too: the death reason VillagerDead 0x750929 writes after SetDying (GetDeathReason 0x55CB10; SaveDead
	/// 0x754CC0). (inferred) a union with the low byte of TargetThing; kept apart, nothing reads TargetThing after death
	DeathReason deathReason {DeathReason::None};
	// +0x11C (a union: Football* / TradeTown / WanderArea, bw1-decomp Villager.h) is left out until a job reads it;
	// the scripts use its first two dwords:
	/// +0x11C: SET_SCRIPT_ULONG's clip (GScript::SetScriptUlong 0x6F8855), SCRIPT_PLAY_ANIM's (ScriptAnimation 0x768A00)
	uint32_t scriptAnim {0};
	/// +0x120: how many times SCRIPT_PLAY_ANIM plays it (0x6F884D; ScriptPlayAnim 0x768970 counts it down)
	uint32_t scriptAnimLoops {0};
	/// mirrors flags & k_FlagChild (DetailMeshes, the drawing, the sounds read it)
	LifeStage lifeStage;
	Sex sex;
	Tribe tribe;
	VillagerNumber number;
	Task task;
	entt::entity town;
	entt::entity abode;
};
} // namespace openblack::ecs::components
