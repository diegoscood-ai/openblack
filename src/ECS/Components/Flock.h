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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// A flock of animals (Flock, 0x90 bytes, list g_game+0x205C44; invisible, only simulation data). Made by CREATE_FLOCK
/// (0x71634A -> Flock::Flock 0x52F780) or, for an animal created without one, its own (Flock::Flock(Living*)
/// 0x52F950 in fn_00419C20). The herd's AI is in ECS/AnimalAI.h: the leader is the first member.
struct Flock
{
	/// +0x8C: the script's id (CREATE_ANIMAL / CREATE_NEW_ANIMAL look flocks up by it); an animal's own flock has none
	int32_t id {-1};
	glm::vec3 domainCentre {0.0f};      ///< +0x14, SetDomainCentrePos 0x52FC20 (CREATE_FLOCK's second position)
	glm::vec3 savedDomainCentre {0.0f}; ///< +0x60 (and +0x6C): the position it was created at
	uint16_t domainRadius {0x50};       ///< +0x50 (GetDomainRadius)
	uint16_t flockDistance {0x1E};      ///< +0x52 (GetFlockDistance; an animal's own flock: (int)info.flockDistance)
	entt::entity town {entt::null};     ///< +0x34
	std::vector<entt::entity> members;  ///< +0x3C/+0x48 (fn_0052FA50 keeps it sorted by the living's +0xD4, not ported)
	uint32_t maxMembers {0};            ///< +0x88: the most members it has had
	/// the birds' flight (Dove, dev\tmp_dis\animals\birds_ai.md): +0x54 the flock altitude (0: info.altitudeNormal),
	/// +0x78 the followers' state, +0x7C the state after a leg, +0x80 the follow mode (2 next member, 3 formation, 7 landing)
	float altitude {0.0f};
	uint8_t followState {43};
	uint8_t afterMove {0};
	uint8_t followMode {0};
	/// +0x4C: the leader's turns since it last moved the herd (Animal::ProcessNeeds; KeepLeaderWithinDomain vs stayTime)
	uint32_t leaderTurns {0};
};

} // namespace openblack::ecs::components
