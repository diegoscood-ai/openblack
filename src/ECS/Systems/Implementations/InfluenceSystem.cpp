/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "InfluenceSystem.h"

#include <utility>

#include <spdlog/spdlog.h>

#include "ECS/Components/VirtualInfluence.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The player whose interface openblack has: the local one
PlayerNames InterfacePlayer()
{
	return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
}

/// The local player's: the first time it is asked, their entity gets one as the game starts it (full strength, switched
/// on). None without a local player's entity
virtual_influence::State* LocalVirtualInfluence()
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto carrier = std::as_const(registry).Front<ecs::components::VirtualInfluence>(); carrier != entt::null)
	{
		return &registry.Get<ecs::components::VirtualInfluence>(carrier).state;
	}
	const auto player = magic::players::EntityOf(InterfacePlayer());
	return player == entt::null ? nullptr : &registry.AssignState<ecs::components::VirtualInfluence>(player).state;
}

/// A map position as the influence code takes it, in metres on the ground
glm::vec3 PointOf(const map_coords::MapCoords& place)
{
	const auto metres = map_coords::ToMetres(place);
	return {metres.x, 0.0f, metres.y};
}
} // namespace

void InfluenceSystem::ProcessTurn([[maybe_unused]] uint32_t turn)
{
	influence::ProcessTurn();
}

void InfluenceSystem::UpdateBorders()
{
	influence::Update3DInfluence();
}

void InfluenceSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	// The hand's point is tested against the circles only while the game runs, once the hand has a point
	if (!game_clock::IsPaused())
	{
		if (const auto& hands = Locator::handSystem::value().GetPlayerHandPositions(); hands[0].has_value())
		{
			influence::ProcessHandCrossing(*hands[0]);
		}
	}
	// Every ripple ages by the frame's game time, which is none while paused. The frame's whole milliseconds come back
	// exactly from the float
	influence::UpdateRipples(static_cast<uint32_t>(gameTime.count()));
}

float InfluenceSystem::PlayerInfluence(PlayerNames player, const glm::vec3& position) const
{
	return influence::CalculatePlayerInfluence(player, position);
}

float InfluenceSystem::PlayerInfluence(PlayerNames player, const glm::vec3& position, influence::CalcType type,
                                       bool includeAllies) const
{
	return influence::CalculatePlayerInfluence(player, position, type, includeAllies);
}

bool InfluenceSystem::IsInAntiInfluence(PlayerNames player, const glm::vec3& position) const
{
	return influence::IsInAntiInfluence(player, position);
}

std::span<const influence::Circle> InfluenceSystem::GetCircles() const
{
	return influence::Circles();
}

bool InfluenceSystem::IsBorderShown(PlayerNames player) const
{
	return influence::BoundaryShown(player);
}

std::span<const influence::Ripple> InfluenceSystem::GetRipples() const
{
	return influence::Ripples();
}

void InfluenceSystem::SetVirtualInfluence(PlayerNames player, bool enabled)
{
	// Every interface of the player is switched: a player without one has nothing to switch
	if (player != InterfacePlayer())
	{
		return;
	}
	if (auto* state = LocalVirtualInfluence(); state != nullptr)
	{
		virtual_influence::SetEnabled(*state, enabled);
	}
}

void InfluenceSystem::ProcessVirtualInfluence(const map_coords::MapCoords& handPlace)
{
	auto* state = LocalVirtualInfluence();
	if (state == nullptr)
	{
		return;
	}
	const auto player = InterfacePlayer();
	const auto hand = PointOf(handPlace);
	const auto& citadel = Locator::infoConstants::value().citadel;
	// Asked in the game's order: an anti-influence ring first, which leaves the player's own influence unasked
	const bool shielded = influence::IsInAntiInfluence(player, hand);
	const bool inside = !shielded && influence::IsInPlayerRawInfluence(player, hand);
	const auto tenthBefore = static_cast<int>(state->fraction * 10.0f);
	virtual_influence::ProcessTurn(*state,
	                               {
	                                   .hand = handPlace,
	                                   .turn = game_clock::Turn(),
	                                   .handShielded = shielded,
	                                   .handInInfluence = inside,
	                                   .influencePower = influence::InfluencePower(player),
	                                   // (pending) the chants the player's worship sites spare for it, and their use
	                                   .chants = 0.0f,
	                               },
	                               {
	                                   .maxDistance = citadel.virtualInfluenceMaxDistance,
	                                   .maxTurns = citadel.virtualInfluenceMaxGameTicks,
	                                   .chantsToDouble = citadel.virtualInfluenceChantsToDouble,
	                               });
	// The log follows the strength a tenth at a time
	if (static_cast<int>(state->fraction * 10.0f) != tenthBefore)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "The hand {} its player's influence at ({:.1f}, {:.1f}): strength {:.2f}",
		                    inside ? "is in" : (shielded ? "is shielded from" : "is out of"), hand.x, hand.z, state->fraction);
	}
}

const virtual_influence::State* InfluenceSystem::GetVirtualInfluence(PlayerNames player) const
{
	if (player != InterfacePlayer() || !Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto carrier = registry.Front<ecs::components::VirtualInfluence>();
	return carrier == entt::null ? nullptr : &registry.Get<const ecs::components::VirtualInfluence>(carrier).state;
}
