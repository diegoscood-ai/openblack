/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "ECS/GameEndRules.h"
#include "ECS/Systems/GameEndSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class GameEndSystem final: public GameEndSystemInterface
{
public:
	/// In the game's own world
	GameEndSystem();
	/// In a world of its own, and whether the game is a skirmish, as tests give them
	GameEndSystem(std::unique_ptr<game_end::World> world, bool skirmish);
	~GameEndSystem() override;

	void ProcessTurn() override;
	[[nodiscard]] bool IsDecided() const override { return _state.decided; }
	[[nodiscard]] std::vector<game_end::Event> TakeEvents() override;
	[[nodiscard]] std::optional<PlayerNames> LastGodIn() override;
	void Reset() override;

private:
	[[nodiscard]] bool Skirmish() const;

	std::unique_ptr<game_end::World> _world;
	/// None: the game's own, the registry's context
	std::optional<bool> _skirmish;
	game_end::State _state;
	std::vector<game_end::Event> _events;
};

} // namespace openblack::ecs::systems
