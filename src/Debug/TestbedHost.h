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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "TestbedOptions.h"

namespace openblack::gui
{
class GameInterface;
}

namespace openblack::testbed_scenarios
{

/// What the scenario runner and its window need of the game around it: a fresh testbed, the game's speed, pause and
/// frame step, the scenario the command line asked for, quitting, and the in-game interface the tattoo editor's dialog
/// is part of.
/// The game implements it; the runner only sees this, so it never reaches the game object itself.
class TestbedHost
{
public:
	virtual ~TestbedHost() = default;

	/// Loads the flat testbed afresh, which clears away everything on the land; its plane at that altitude
	virtual void LoadTestbed(uint8_t planeAltitude) = 0;
	/// The turn length multiplier, as the game's: 2 runs at half speed, 0.5 at double
	[[nodiscard]] virtual float GetGameSpeed() const = 0;
	virtual void SetGameSpeed(float multiplier) = 0;
	[[nodiscard]] virtual bool IsPaused() const = 0;
	/// The seconds the game's frame moves on by: the fixed step when the game runs on one, else the frame's own time
	[[nodiscard]] virtual float FrameSeconds() const = 0;
	/// The scenario the command line asked for, given out once
	[[nodiscard]] virtual std::optional<ScenarioRequest> TakeScenarioRequest() = 0;
	/// The game ends after the frame it is in
	virtual void RequestQuit() = 0;
	/// Changes to the land of a script in the game's Scripts folder, such as "Land2.txt", through the game's own land
	/// change, as a map script asks for one: the player's creature is saved to the profile first. Whether it loaded
	virtual bool ChangeLand(std::string_view landScript) = 0;
	/// The scenarios a land entry's timeline= names, given out once the land is loaded: their commands are played on that
	/// land in turn. None for a game with no land entry or no timeline
	[[nodiscard]] virtual std::optional<std::vector<std::string>> TakeLandTimeline() { return std::nullopt; }
	/// A quest run's scripts (a land entry's quest=), taken once, as the timeline is; none for any other run
	[[nodiscard]] virtual std::optional<std::vector<testbed_land_timeline::QuestScript>> TakeLandQuest()
	{
		return std::nullopt;
	}
	/// The in-game interface (its menus and dialogs), none before the game makes it
	[[nodiscard]] virtual gui::GameInterface* GetInterface() const { return nullptr; }
};

} // namespace openblack::testbed_scenarios
