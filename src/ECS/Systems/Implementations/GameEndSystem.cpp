/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameEndSystem.h"

#include <utility>

#include <spdlog/spdlog.h>

#include "ECS/Components/PlayerMagic.h"
#include "ECS/CreatureRemoval.h"
#include "ECS/Registry.h"
#include "ECS/SkirmishGame.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Game/GameStats.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Worship/Citadel.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using openblack::ecs::components::GameStanding;
using openblack::ecs::components::PlayerMagic;

namespace
{
/// The type of a player that a human plays
constexpr int32_t k_HumanPlayerType = 1;

/// The end of the game's world: the players' entities, their temples, creatures, statistics and computer minds
class GameWorld final: public game_end::World
{
public:
	[[nodiscard]] std::vector<PlayerNames> ActiveGods() const override
	{
		// The players the game walks as its active players: the seven slots before the neutral player that a human or a
		// computer mind plays. Only a player that has an entity on the land keeps its standing
		std::vector<PlayerNames> gods;
		for (auto index = 0u; index < static_cast<uint32_t>(PlayerNames::NEUTRAL); ++index)
		{
			const auto player = static_cast<PlayerNames>(index);
			if (magic::players::EntityOf(player) == entt::null)
			{
				continue;
			}
			const auto type = magic::players::ReadMagicOf(player).playerType;
			if (type == k_HumanPlayerType || type == PlayerMagic::k_ComputerPlayerType)
			{
				gods.push_back(player);
			}
		}
		return gods;
	}

	[[nodiscard]] bool IsHuman(PlayerNames god) const override
	{
		return magic::players::ReadMagicOf(god).playerType == k_HumanPlayerType;
	}

	[[nodiscard]] bool HasTemple(PlayerNames god) const override { return worship::citadel::Of(god) != entt::null; }

	[[nodiscard]] GameStanding& Standing(PlayerNames god) override
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = magic::players::EntityOf(god);
		if (auto* standing = registry.TryGet<GameStanding>(entity); standing != nullptr)
		{
			return *standing;
		}
		return registry.AssignState<GameStanding>(entity);
	}

	void RemoveCreature(PlayerNames god) override
	{
		if (!Locator::playerSystem::has_value())
		{
			return;
		}
		if (const auto creature = Locator::playerSystem::value().GetPrimaryCreature(god); creature.has_value())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game's end: player {} is out, and its creature goes",
			                   static_cast<int>(god));
			ecs::creature_removal::RemoveFromGame(*creature);
		}
	}

	void CountGodDefeated(PlayerNames god) override { game_stats::GodDefeated(static_cast<size_t>(god)); }

	void TakeNextPlace(PlayerNames god) override { game_stats::TakeNextPlace(static_cast<size_t>(god)); }

	void StopMind(PlayerNames god) override
	{
		// Only a god that a computer mind plays has one going
		if (Locator::computerPlayerSystem::has_value() &&
		    magic::players::ReadMagicOf(god).playerType == PlayerMagic::k_ComputerPlayerType)
		{
			Locator::computerPlayerSystem::value().SwitchOn(god, false);
		}
	}
};
} // namespace

GameEndSystem::GameEndSystem()
    : _world(std::make_unique<GameWorld>())
{
}

GameEndSystem::GameEndSystem(std::unique_ptr<game_end::World> world, bool skirmish)
    : _world(std::move(world))
    , _skirmish(skirmish)
{
}

GameEndSystem::~GameEndSystem() = default;

bool GameEndSystem::Skirmish() const
{
	if (_skirmish.has_value())
	{
		return *_skirmish;
	}
	return ecs::IsSkirmishGame();
}

void GameEndSystem::ProcessTurn()
{
	auto events = game_end::ProcessTurn(*_world, Skirmish(), _state);
	for (const auto& event : events)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game's end: player {} is out{}", static_cast<int>(event.god),
		                   event.kind == game_end::Event::Kind::GodOutAndDecided ? ", and the game is decided" : "");
	}
	_events.insert(_events.end(), events.begin(), events.end());
}

std::vector<game_end::Event> GameEndSystem::TakeEvents()
{
	return std::exchange(_events, {});
}

std::optional<PlayerNames> GameEndSystem::LastGodIn()
{
	for (const auto god : _world->ActiveGods())
	{
		if (game_end::CanLose(god) && !_world->Standing(god).out)
		{
			return god;
		}
	}
	return std::nullopt;
}

void GameEndSystem::Reset()
{
	_state = {};
	_events.clear();
}
