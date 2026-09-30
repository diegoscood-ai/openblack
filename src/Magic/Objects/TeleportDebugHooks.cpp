/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// OPENBLACK_TEST_TELEPORT="x0,z0,x1,z1[,player[,mode]]": two TELEPORT casts as the script's SPELL_AT_POS (stone B at
// x1,z1 first, then stone A at x0,z0), by the neutral player or by `player` (0 = PLAYER_ONE: its chants run down).
// mode:
//   walk (default) - two turns before the casts the villager nearest A starts walking to B; A's REACT_TO_TELEPORT then
//                    reaches it, it goes to A and comes out at B (GO_TOWARDS_TELEPORT_REACTION / TELEPORT_REACTION);
//   drop           - one second after the casts that villager is applied to stone A as the hand's drop does
//                    (fn_005FC4F0: forced jump);
//   none           - only the stones.
// OPENBLACK_TEST_TELEPORT_TURN=<n>: start at game turn n (the camera fly of a screenshot takes ~160 turns).
// OPENBLACK_TELEPORT_TRACE=1 (or OPENBLACK_SPELL_TRACE) logs the stones, the reaction, the jumps and the chants.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <limits>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellCreator.h"
#include "Magic/Script/CHLSpells.h"
#include "MagicTeleport.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

namespace
{
enum class Phase
{
	Start,
	Casting,
	Dropping,
	Done,
};

struct Test
{
	bool parsed {false};
	bool valid {false};
	glm::vec2 a {0.0f};
	glm::vec2 b {0.0f};
	int player {-1};
	char mode[16] = "walk";
	Phase phase {Phase::Start};
	unsigned int castTurn {0};
	unsigned int startTurn {0};
	entt::entity villager {entt::null};
	entt::entity stoneA {entt::null};
};
Test g_Test;

entt::entity NearestVillager(const glm::vec2& at)
{
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity best = entt::null;
	float bestDistance = std::numeric_limits<float>::max();
	registry.Each<const Villager, const Transform, const LivingAction>(
	    [&](entt::entity entity, const Villager&, const Transform& transform, const LivingAction&) {
		    const float d = glm::distance(glm::vec2(transform.position.x, transform.position.z), at);
		    if (d < bestDistance)
		    {
			    bestDistance = d;
			    best = entity;
		    }
	    });
	return best;
}

entt::entity Cast(const glm::vec2& at)
{
	const float y = Locator::terrainSystem::value().GetHeightAt(at);
	const glm::vec3 target(at.x, y, at.y);
	ecs::components::SpellCreator creator;
	if (g_Test.player >= 0 && g_Test.player < static_cast<int>(PlayerNames::_COUNT))
	{
		creator = creator::OfPlayer(static_cast<PlayerNames>(g_Test.player));
	}
	return script::CastSpellAtPos(target, MagicType::Teleport, target + glm::vec3(0.0f, 30.0f, 0.0f), creator, false, 0.0f,
	                              -1.0f, 0.0f, glm::vec3(0.0f));
}
} // namespace

void teleport::RunDebugHooks()
{
	if (!g_Test.parsed)
	{
		g_Test.parsed = true;
		const char* value = std::getenv("OPENBLACK_TEST_TELEPORT");
		if (value != nullptr)
		{
			const int n = std::sscanf(value, "%f,%f,%f,%f,%d,%15s", &g_Test.a.x, &g_Test.a.y, &g_Test.b.x, &g_Test.b.y,
			                          &g_Test.player, g_Test.mode);
			g_Test.valid = n >= 4;
			if (const char* turn = std::getenv("OPENBLACK_TEST_TELEPORT_TURN"); turn != nullptr)
			{
				g_Test.startTurn = static_cast<unsigned int>(std::atoi(turn));
			}
			if (!g_Test.valid)
			{
				SPDLOG_LOGGER_WARN(spdlog::get("game"), "Teleport test: OPENBLACK_TEST_TELEPORT=\"{}\" not understood", value);
			}
		}
	}
	if (!g_Test.valid || g_Test.phase == Phase::Done || !Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto turn = CurrentTurn();
	switch (g_Test.phase)
	{
	case Phase::Start:
		if (turn < g_Test.startTurn)
		{
			break;
		}
		g_Test.villager = NearestVillager(g_Test.a);
		if (std::strcmp(g_Test.mode, "walk") == 0 && g_Test.villager != entt::null)
		{
			// the villager walks towards B (MOVE_TO_POS, then deciding again)
			auto* wallHug = registry.TryGet<WallHug>(g_Test.villager);
			auto* action = registry.TryGet<LivingAction>(g_Test.villager);
			if (wallHug != nullptr && action != nullptr)
			{
				wallHug->goal = g_Test.b;
				wallHug->step = glm::vec2(0.0f);
				registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
				                MoveStateFinalStepTag, MoveStateArrivedTag>(g_Test.villager);
				registry.Remove<WallHugObjectReference>(g_Test.villager);
				registry.Assign<MoveStateLinearTag>(g_Test.villager);
				auto& system = Locator::livingActionSystem::value();
				system.VillagerSetState(*action, LivingAction::Index::Final, VillagerStates::DecideWhatToDo, true);
				system.VillagerSetState(*action, LivingAction::Index::Top, VillagerStates::MoveToPos, true);
			}
		}
		if (g_Test.villager != entt::null)
		{
			const auto& p = registry.Get<const Transform>(g_Test.villager).position;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Teleport test: villager {} at ({:.1f}, {:.1f}), mode {}",
			                   static_cast<uint32_t>(g_Test.villager), p.x, p.z, g_Test.mode);
		}
		g_Test.castTurn = turn + 2;
		g_Test.phase = Phase::Casting;
		break;
	case Phase::Casting:
	{
		if (turn < g_Test.castTurn)
		{
			break;
		}
		const auto spellB = Cast(g_Test.b);
		const auto spellA = Cast(g_Test.a);
		const auto& stones = g_Test.player >= 0 ? StonesOf(static_cast<PlayerNames>(g_Test.player)) : StonesOf(PlayerNames::NEUTRAL);
		g_Test.stoneA = stones.empty() ? entt::null : stones.front();
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Teleport test: spells {} (B at {:.1f}, {:.1f}) and {} (A at {:.1f}, {:.1f}) player {}: {} stones; "
		                   "CanCastAt(A) now {}",
		                   spellB == entt::null ? -1 : static_cast<int>(spellB), g_Test.b.x, g_Test.b.y,
		                   spellA == entt::null ? -1 : static_cast<int>(spellA), g_Test.a.x, g_Test.a.y, g_Test.player,
		                   stones.size(), !AnyMultiMapFixedNear(glm::vec3(g_Test.a.x, 0.0f, g_Test.a.y), k_Radius));
		g_Test.castTurn = turn + 10;
		g_Test.phase = std::strcmp(g_Test.mode, "drop") == 0 ? Phase::Dropping : Phase::Done;
		break;
	}
	case Phase::Dropping:
		if (turn < g_Test.castTurn)
		{
			break;
		}
		if (g_Test.villager != entt::null && registry.Valid(g_Test.villager) && g_Test.stoneA != entt::null)
		{
			const bool valid = ValidToApplyVillagerDirectly(g_Test.stoneA, g_Test.villager);
			const int result = ApplyVillagerDirectly(g_Test.stoneA, g_Test.villager);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Teleport test: villager {} dropped on stone {}: valid {} -> {}",
			                   static_cast<uint32_t>(g_Test.villager), static_cast<uint32_t>(g_Test.stoneA), valid, result);
		}
		g_Test.phase = Phase::Done;
		break;
	case Phase::Done:
		break;
	}
}
