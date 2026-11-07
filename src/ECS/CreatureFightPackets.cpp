/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFightPackets.h"

#include "3D/MapCoords.h"
#include "Creature/CreatureFight.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::ecs;

void creature_fight_packets::ApplyGroundPress(systems::CreatureFightSystemInterface* fights,
                                              systems::ParticleSystemInterface* particles,
                                              std::optional<entt::entity> playersCreature, const game_packets::Packet& packet,
                                              bool replace)
{
	if (fights == nullptr)
	{
		return;
	}
	// The point: the map position's metres, at its altitude above the land there
	const auto point = map_coords::ToWorld(packet.coords);
	const bool fighting =
	    playersCreature.has_value() && ecs::IsAvailable(*playersCreature) && fights->IsDuelling(*playersCreature);
	if (!fighting)
	{
		if (particles != nullptr)
		{
			particles->StartSpotVisual(SpotVisualType::CommandFail, point, std::nullopt, playersCreature.value_or(entt::null),
			                           1.0f);
		}
		return;
	}
	fights->GroundPress(*playersCreature, point, replace);
}

void creature_fight_packets::ApplyBodyPress(systems::CreatureFightSystemInterface* fights,
                                            std::optional<entt::entity> playersCreature, const game_packets::Packet& packet,
                                            bool replace)
{
	if (fights == nullptr || packet.object == entt::null || !ecs::IsAvailable(packet.object) || !playersCreature.has_value())
	{
		return;
	}
	if (const auto move = creature_fight::ScriptMove(static_cast<uint32_t>(packet.value)))
	{
		fights->QueueMove(*playersCreature, *move, replace);
	}
}

void creature_fight_packets::ApplyCharge(systems::CreatureFightSystemInterface* fights,
                                         std::optional<entt::entity> playersCreature, const game_packets::Packet& packet)
{
	if (fights == nullptr || !playersCreature.has_value())
	{
		return;
	}
	fights->ReleaseCharge(*playersCreature, static_cast<float>(static_cast<uint32_t>(packet.value)));
}

void creature_fight_packets::ApplyFightSpell(systems::CreatureFightSystemInterface* fights, const game_packets::Packet& packet)
{
	if (fights == nullptr || packet.object == entt::null)
	{
		return;
	}
	fights->QueueMove(
	    packet.object,
	    creature_fight::Move {.kind = creature_fight::Move::Kind::Spell, .value = static_cast<uint32_t>(packet.value)}, false);
}
