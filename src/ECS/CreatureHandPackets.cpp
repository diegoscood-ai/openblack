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
#include <spdlog/spdlog.h>

#include "Creature/CreatureFeedback.h"
#include "Creature/LeashKeys.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
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

game_packets::Packet creature_hand_packets::HardSlapPacket(const Bruise& bruise)
{
	game_packets::Packet packet {game_packets::Type::CreatureHardSlap};
	// whether there is a spot, its primitive, its three corners, how far along its two edges, then the column
	packet.data[7] = static_cast<float>(bruise.column);
	if (!bruise.spot.has_value())
	{
		return packet;
	}
	packet.data[0] = 1.0f;
	packet.data[1] = static_cast<float>(bruise.spot->primitive);
	for (size_t k = 0; k < bruise.spot->corners.size(); ++k)
	{
		packet.data.at(2 + k) = static_cast<float>(bruise.spot->corners.at(k));
	}
	packet.data[5] = bruise.spot->u;
	packet.data[6] = bruise.spot->v;
	return packet;
}

creature_hand_packets::Bruise creature_hand_packets::BruiseOf(const game_packets::Packet& packet)
{
	Bruise bruise {.spot = std::nullopt, .column = static_cast<uint8_t>(packet.data[7])};
	if (packet.data[0] != 0.0f)
	{
		creature_skin::MeshSpot spot {
		    .primitive = static_cast<uint32_t>(packet.data[1]), .corners = {}, .u = packet.data[5], .v = packet.data[6]};
		for (size_t k = 0; k < spot.corners.size(); ++k)
		{
			spot.corners.at(k) = static_cast<uint32_t>(packet.data.at(2 + k));
		}
		bruise.spot = spot;
	}
	return bruise;
}

creature_marks::Mark creature_hand_packets::BruiseMark(const creature_skin::SkinPlace& place, uint8_t column)
{
	return {.u = place.u, .v = place.v, .skin = place.skin, .age = 0, .type = 0, .column = column};
}

void creature_hand_packets::ApplyHardSlap(std::optional<entt::entity> playersCreature, const game_packets::Packet& packet)
{
	if (!playersCreature.has_value() || !Locator::entitiesRegistry::has_value() || !ecs::IsAvailable(*playersCreature))
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* needs = registry.TryGet<components::CreatureNeeds>(*playersCreature))
	{
		const float before = needs->needs.life;
		needs->needs.life = creature_feedback::LifeAfterBruise(needs->needs.life);
		// (debug readout) the bruise's life
		if (auto log = spdlog::get("game"); log != nullptr)
		{
			log->info("Creature hand: the third hard slap bruises creature {}: life {:.4f} to {:.4f}",
			          entt::to_integral(*playersCreature), before, needs->needs.life);
		}
	}
	// The bruise goes on its own skin where the spot the slap met is on its own mesh, the slapped creature's or not
	const auto bruise = BruiseOf(packet);
	if (!bruise.spot.has_value() || !Locator::creatureSkinSystem::has_value())
	{
		return;
	}
	if (const auto place = creature_skin::PlaceOf(std::as_const(registry), *playersCreature, *bruise.spot))
	{
		const auto mark = BruiseMark(*place, bruise.column);
		Locator::creatureSkinSystem::value().AddWound(*playersCreature, mark);
		// (debug readout) the bruise's mark on the skin
		if (auto log = spdlog::get("game"); log != nullptr)
		{
			log->info("Creature hand: the bruise is marked on creature {}'s skin at ({}, {}) of skin {}, column {}",
			          entt::to_integral(*playersCreature), static_cast<int>(mark.u), static_cast<int>(mark.v),
			          static_cast<int>(mark.skin), static_cast<int>(mark.column));
		}
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
		// it starts again as an action that failed, the stronger for a duel it is in
		if (Locator::creatureFightSystem::has_value())
		{
			Locator::creatureFightSystem::value().FailsWhileDueling(packet.object);
		}
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
