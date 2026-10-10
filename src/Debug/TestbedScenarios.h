/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <memory>
#include <optional>
#include <string>

#include <glm/vec2.hpp>

#include "TestbedHost.h"
#include "TestbedScenarioRegistry.h"
#include "TestbedScenarioRunner.h"
#include "Window.h"

namespace openblack::debug::gui
{

/// Ready-made scenarios on the creature testbed, one facet of the creatures at a time: pick a facet and a scenario, read
/// what it sets up and what to look for, and run, restart or stop it. The game's speed and the creatures' body time
/// can be changed, the camera put on the scenario's creatures, and their activities, needs, desires and speed read as
/// they go. A running scenario carries on with the window closed. It reaches the game only through its host, and does
/// nothing until it has one.
class TestbedScenarios final: public Window
{
public:
	/// The spawner is the creature spawner's window, which a creature's button opens
	explicit TestbedScenarios(Window& spawner) noexcept;

	/// The game the scenarios run in, or none once it has gone; a scenario running is stopped first
	void SetHost(testbed_scenarios::TestbedHost* host) noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override {}
	void UpdateAlways() noexcept override;
	void ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept override {}
	void ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept override {}

private:
	void DrawPicker() noexcept;
	void DrawControls() noexcept;
	/// The crowd of a benchmark as it spawns, and its frames as they are measured
	void DrawBenchmark() noexcept;
	/// Runs the scenario the command line asked for, once
	void RunRequested() noexcept;
	/// Starts a scenario picked in the window: measured as the command line asked, but the game carries on after
	void RunFromWindow(const testbed_scenarios::Scenario& scenario) noexcept;
	void DrawTime() noexcept;
	void DrawCamera() noexcept;
	void DrawCreatures() noexcept;
	/// Each fixture set out by hand, where the controls put it
	void DrawFixtures() noexcept;

	Window& _spawner;
	testbed_scenarios::TestbedHost* _host {nullptr};
	std::unique_ptr<testbed_scenarios::Runner> _runner;
	/// The facet the list is narrowed to, or every facet
	std::optional<testbed_scenarios::Facet> _facet;
	size_t _picked {0};
	/// The scenario's creature the camera shots are of
	size_t _focus {0};
	/// Where the last benchmark's results were written
	std::string _savedTo;
	/// The fixtures section's controls: where, from the middle of the map, and what
	struct FixtureControls
	{
		glm::vec2 at {0.0f, 40.0f};
		int magic {0};
		int weather {1};
		bool lightning {true};
		int species {0};
		int hold {0};
		int huts {4};
		int villagers {8};
		int trees {5};
	} _fixtureControls;
};

} // namespace openblack::debug::gui
