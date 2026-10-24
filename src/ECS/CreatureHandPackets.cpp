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

#include "Creature/CreatureFeedback.h"
#include "Creature/LeashKeys.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureNeeds.h"
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

void creature_hand_packets::ApplyHardSlap(std::optional<entt::entity> playersCreature)
{
	if (!playersCreature.has_value() || !Locator::entitiesRegistry::has_value() || !ecs::IsAvailable(*playersCreature))
	{
		return;
	}
	// (not ported) the bruise drawn on its skin
	if (auto* needs = Locator::entitiesRegistry::value().TryGet<components::CreatureNeeds>(*playersCreature))
	{
		needs->needs.life = creature_feedback::LifeAfterBruise(needs->needs.life);
	}
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
	// At its take with something in its hand (the thing taken, or what it held all along) it goes on as it was
	const bool takingWithSomethingHeld = hands != nullptr && minds != nullptr && minds->IsTakingFromHand(packet.object) &&
	                                     hands->IsSetOnTakingFromHand(packet.object) &&
	                                     hands->GetHeld(packet.object).has_value();
	// Reaching for it, its hand plays on to the moment of taking and closes on nothing there
	if (hands != nullptr && hands->GetTakingFromHand(packet.object).has_value())
	{
		hands->CallOffTake(packet.object);
	}
	if (minds != nullptr && !takingWithSomethingHeld)
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
