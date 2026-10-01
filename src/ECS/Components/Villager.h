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
	/// football / script, 0x200 / 0x400 disciple / disciple follower, 0x2000 going to bed. 0x800 / 0x1000 (into /
	/// out-of clip) live in SkeletalAnimation::transitionFlags.
	enum Flags : uint16_t
	{
		k_FlagAfterTapOnAbode = 0x1,
		k_FlagAtWorshipSite = 0x2,
		k_FlagAtHome = 0x4,
		k_FlagChild = 0x8,
		k_FlagOnWayToWorshipSite = 0x10,
		k_FlagInHand = 0x20,
		k_FlagFootball = 0x80,
		k_FlagDisciple = 0x200,
		k_FlagDiscipleFollower = 0x400,
		k_FlagGoingToBed = 0x2000,
	};

	float life; ///< Object +0x48, 0..1 (Object::GetLife; ecs::life has the setters)
	/// Living +0xA0: the turn it was born (Living::SetAge 0x5ED2C0: turn - age * 1500); its age is
	/// ecs::villager::GetAge (Living::GetAge 0x5ECAF0)
	int32_t birthTurn {0};
	uint16_t flags {0};           ///< +0xE0, the Flags above
	float food {0.0f};            ///< +0xE8, the food in its belly (the constructor: 0.5 .. 1.1)
	uint32_t lastCheckTurn {0};   ///< +0xEC, the turn of the last periodic check (CheckHungry resets it)
	uint8_t foodSpeedUp {0};      ///< +0xF0, IsFoodSpeedUp 0x55C980; ProcessFoodSpeedup 0x753430
	uint8_t discipleType {0};     ///< +0xF2, VillagerDisciple (g_DiscipleInfos 0x99A1F8)
	std::array<int16_t, 2> resourceHeld {}; ///< +0xF4 / +0xF6, FOOD and WOOD carried
	int16_t pregnancy {0};        ///< +0xF8, turns left of a pregnancy (0 none)
	entt::entity mother {entt::null};      ///< +0x100
	entt::entity targetThing {entt::null}; ///< +0x118 TargetThing (bw1-decomp Villager.h), what the jobs work on
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
