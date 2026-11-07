/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OtherPlayerSparkles.h"

#include <cstdint>

#include <utility>

#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/Components/ComputerHand.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Transform.h"
#include "ECS/PlayerColours.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Particles/PlayerSparkles.h"
#include "Worship/Citadel.h"

namespace openblack::ecs::other_player_sparkles
{

namespace
{
/// The interface's action state while its hand directs a creature fight
constexpr int32_t k_CreatureFightState = 0x10;
/// The players walked, from the first, the neutral one ending the walk earlier when it comes first
constexpr uint32_t k_WalkedPlayers = 7;

/// Who plays the player, by its type
player_sparkles::Played PlayedOf(PlayerNames player)
{
	const auto type = magic::players::ReadMagicOf(player).playerType;
	switch (type)
	{
	case 1:
		return player_sparkles::Played::Person;
	case 2:
		return player_sparkles::Played::Computer;
	case 3:
		return player_sparkles::Played::Neutral;
	default:
		return player_sparkles::Played::Nobody;
	}
}

/// Whether the player's computer mind plays
bool MindOn(const Registry& registry, entt::entity entity)
{
	const auto* mind =
	    entity != entt::null && registry.Valid(entity) ? registry.TryGet<const components::ComputerPlayer>(entity) : nullptr;
	return mind != nullptr && mind->switchedOn;
}

/// The local player's hand directs a creature fight
bool DirectsFight()
{
	return Locator::handSystem::has_value() && Locator::handSystem::value().GetActionStateNumber() == k_CreatureFightState;
}

/// The player at this machine
PlayerNames LocalPlayer()
{
	return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
}

/// Whether another god shows sparkles now; openblack has no network players, so no person but the local one is in the
/// game
bool ShowsNow(const Registry& registry, PlayerNames player, player_sparkles::Played played)
{
	return player != LocalPlayer() &&
	       player_sparkles::Shows(played, MindOn(registry, magic::players::EntityOf(player)), /*personInGame=*/false);
}
} // namespace

std::optional<glm::vec3> ReportedCamera(PlayerNames player)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto entity = magic::players::EntityOf(player);
	if (entity != entt::null && registry.Valid(entity))
	{
		if (const auto* hand = registry.TryGet<const components::ComputerHand>(entity);
		    hand != nullptr && hand->reportedCamera.has_value())
		{
			return hand->reportedCamera;
		}
	}
	const auto citadel = worship::citadel::Of(player);
	if (citadel == entt::null)
	{
		return std::nullopt;
	}
	// the citadel's place on the map grid at its height there
	const auto& place = registry.Get<const components::Transform>(citadel).position;
	return player_sparkles::CameraOverCitadel(map_coords::ToWorld(map_coords::FromWorld(place)));
}

void AddForTurn()
{
	if (!Locator::dynamicsSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	if (game_clock::SequenceMode() != game_clock::k_SequenceModeNone || DirectsFight())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	auto& dynamics = Locator::dynamicsSystem::value();
	for (uint32_t number = 0; number < k_WalkedPlayers; ++number)
	{
		const auto player = static_cast<PlayerNames>(number);
		const auto played = PlayedOf(player);
		if (played == player_sparkles::Played::Neutral)
		{
			break;
		}
		if (!ShowsNow(registry, player, played))
		{
			continue;
		}
		const auto camera = ReportedCamera(player);
		if (!camera.has_value())
		{
			continue;
		}
		const auto tint = game_random::LocalRand(player_sparkles::k_Tints);
		const auto colour = player_sparkles::Colour(player_colours::Colour(player, player_colours::LandNumber()), tint);
		const auto kind = player_sparkles::KindOf(game_random::LocalRand(2));
		if (dynamics.PlayerPuffCount(player) == 0)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Player sparkles: player {} shows at ({:.2f}, {:.2f}, {:.2f})", number,
			                   camera->x, camera->y, camera->z);
		}
		dynamics.AddPlayerPuff(player, *camera, player_sparkles::k_Size, colour, kind);
		dynamics.SetPlayerPuffPoint(player, *camera);
	}
}

bool SymbolShown(PlayerNames player)
{
	if (!Locator::entitiesRegistry::has_value() || DirectsFight())
	{
		return false;
	}
	return ShowsNow(std::as_const(Locator::entitiesRegistry::value()), player, PlayedOf(player));
}

void OnGoInsideCitadel()
{
	if (Locator::dynamicsSystem::has_value())
	{
		Locator::dynamicsSystem::value().ClearPlayerPuffs();
	}
}

} // namespace openblack::ecs::other_player_sparkles
