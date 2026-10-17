/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHandPackets.h"

#include <utility>

#include <glm/vec2.hpp>

#include "Creature/LeashKeys.h"
#include "ECS/Components/Creature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"
#include "Magic/Gestures/PowerUpSystem.h"

using namespace openblack;
using namespace openblack::ecs;

void creature_hand_packets::ApplyFeedback(systems::CreatureMindSystemInterface* minds, const game_packets::Packet& packet)
{
	if (minds == nullptr || packet.object == entt::null || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!ecs::IsAvailable(packet.object) || !registry.AllOf<components::Creature>(packet.object))
	{
		return;
	}
	minds->ReceiveFeedback(packet.object, packet.data[0]);
}

bool creature_hand_packets::ApplyGive(systems::CreatureMindSystemInterface* minds, const game_packets::Packet& packet)
{
	if (minds == nullptr || packet.object == entt::null || !Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto held = static_cast<entt::entity>(static_cast<uint32_t>(packet.value));
	if (!ecs::IsAvailable(packet.object) || !registry.AllOf<components::Creature>(packet.object) || !ecs::IsAvailable(held))
	{
		return false;
	}
	return minds->TakeFromHand(packet.object, held, packet.data[0] != 0.0f, glm::vec2(packet.data[1], packet.data[2]));
}

void creature_hand_packets::ApplyGiveCallOff(systems::CreatureMindSystemInterface* minds,
                                             systems::CreatureObjectActionSystemInterface* hands,
                                             const game_packets::Packet& packet)
{
	if (packet.object == entt::null)
	{
		return;
	}
	if (hands != nullptr && hands->GetTakingFromHand(packet.object).has_value())
	{
		hands->Cancel(packet.object);
	}
	if (minds != nullptr)
	{
		minds->AbandonAction(packet.object);
	}
}

void creature_hand_packets::ApplyLeashClick(systems::LeashSystemInterface* leash, PlayerNames player,
                                            magic::gestures::LeashSelection* picker)
{
	if (picker != nullptr)
	{
		picker->LeashClickArrived();
	}
	if (leash != nullptr)
	{
		(void)leash->PressKey(player, creature_leash::LeashKey::Leash);
	}
}
