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
#include <cstdint>

#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>

#include "BenchmarkRecorder.h"
#include "Input/HandDemo.h"
#include "TestbedGesture.h"
#include "TestbedHost.h"
#include "TestbedPointer.h"
#include "TestbedScenarioRegistry.h"

namespace openblack
{
class LandIslandInterface;
}

namespace openblack::testbed_scenarios
{

/// How far spawning a scenario's crowd has got
struct CrowdProgress
{
	size_t spawned {0};
	size_t total {0};
	/// The time spent spawning, apart from the rest of the frames it took
	double spawnMs {0.0};
	uint32_t spawnFrames {0};

	[[nodiscard]] bool Done() const { return spawned >= total; }
};

/// How a crowd's frames are measured once it has all spawned
struct BenchmarkSettings
{
	/// Frames left to settle first, as the crowd's minds and routes start
	uint32_t warmUpFrames {120};
	/// The last so many frames are measured
	uint32_t frames {600};
	/// The game quits once that many frames are measured, for running a benchmark from the command line
	bool quitWhenMeasured {false};
	/// The command line's --benchmark-out: the results are written there, with .json and .csv after it, once the frames
	/// are measured, and by the window's Save button. Without it nothing is written.
	std::optional<std::filesystem::path> resultsBase;
};

/// Plays a scenario on the testbed: loads the testbed afresh, which clears away everything on it, sets the time of
/// day and the creatures' body time, puts down the objects and creatures as the scenario has them, frames the camera,
/// then gives the commands in turn as their time comes. Everything goes through the game's systems; the game itself is
/// reached only through the host.
class Runner
{
public:
	explicit Runner(TestbedHost& host)
	    : _host(host)
	{
	}

	/// Loads the testbed and starts the scenario on it, in place of any other
	void Start(const Scenario& scenario);
	/// Stops giving commands and puts back the settings the scenario changed: the body time, fainting, the footprints'
	/// smileys, the clock, whether anger starts fights and the mouse. What is on the land stays, to look at or carry on
	/// with by hand.
	void Stop();
	/// Once a frame, by the scenario seconds the frame moves on: the game's fixed step, so that a scenario plays the same
	/// whatever the frame rate. It must run after the game reads the mouse's buttons and before the hand reads them, as
	/// the debug windows' loop does.
	void Update(float seconds);

	[[nodiscard]] bool IsRunning() const { return _running; }
	/// The scenario running, or last run
	[[nodiscard]] const Scenario* GetScenario() const { return _scenario; }
	[[nodiscard]] float GetSeconds() const { return _seconds; }
	[[nodiscard]] const Timeline& GetTimeline() const { return _timeline; }
	/// The scenario's creatures by their place in it, null where one is gone
	[[nodiscard]] std::span<const entt::entity> GetCreatures() const { return _creatures; }
	/// What became of the last commands, the newest last
	[[nodiscard]] const std::deque<std::string>& GetLog() const { return _log; }

	/// The scenario's crowd as it spawns, if it has one
	[[nodiscard]] std::optional<CrowdProgress> GetCrowdProgress() const;
	void SetBenchmarkSettings(BenchmarkSettings settings) { _benchmark = settings; }
	[[nodiscard]] const BenchmarkSettings& GetBenchmarkSettings() const { return _benchmark; }
	/// The frames still to settle before the crowd's frames are measured, and how many have been
	[[nodiscard]] uint32_t GetWarmUpLeft() const;
	[[nodiscard]] size_t GetMeasuredFrames() const { return _recorder ? _recorder->Count() : 0; }
	/// The frames measured so far, summed up every so often as they are
	[[nodiscard]] const benchmark::Results& GetLiveResults() const { return _liveResults; }
	[[nodiscard]] std::span<const benchmark::StageInfo> GetStages() const;
	/// How many entities there are of each kind
	[[nodiscard]] std::vector<std::pair<std::string, size_t>> EntityCounts() const;
	/// Sets fixtures out on the loaded land at once, as the window's buttons ask, logging what came of it; the land is
	/// not loaded afresh
	void PlaceByHand(const testbed_fixtures::Fixtures& fixtures);
	/// Writes the results of the frames measured so far to the benchmark settings' results path, with .json and .csv
	/// after it; the JSON file's path, or none when there is no path, nothing measured or the files couldn't be written
	std::optional<std::filesystem::path> SaveResults();

	/// Puts the camera on a shot of one of the scenario's creatures, or of all of them; following and close ups keep
	/// up with the creature until the camera is let go
	void Frame(Shot shot, size_t creature, float distance = 1.0f);
	/// The camera is the player's again
	void ReleaseCamera() { _shot.reset(); }
	[[nodiscard]] std::optional<Shot> GetShot() const { return _shot; }

private:
	void SetUpEnvironment(const Environment& environment);
	/// The weather over the whole island, as the Weather window forces it, with the climates breeding no storms
	void SetIslandWeather(Weather kind);
	/// False when the scenario may not run here: it writes into the game's folder and the game is not running on a copy
	/// of its data
	bool MayRunHere(const Scenario& scenario);
	/// The scenario's fixtures
	void PlaceFixtures(const Scenario& scenario);
	/// Puts back what a scenario changed: the pointer, its particles, the body time, fainting, the footprints' smileys,
	/// anger starting fights and the clock. Also when a scenario ends on its own, as when another land is loaded
	void RestoreSettings();
	/// Where the hand demo's presses meet the plane at that altitude, as the hand's ray from the recorded camera finds them
	[[nodiscard]] std::vector<glm::vec2> DemoPresses(uint8_t planeAltitude);
	/// Plays the scenario's hand demo once its time comes
	void StartHandDemo();
	void PlaceObjects(const Scenario& scenario, glm::vec2 middle);
	void PlaceCreatures(const Scenario& scenario, glm::vec2 middle);
	/// Spawns the next batch of the crowd
	void SpawnCrowd();
	void SpawnCrowdMember(size_t index);
	/// Takes the last frame's times from the profiler, once the crowd has spawned and settled
	void Measure();
	/// Starts the scenario's particle effect of that index
	[[nodiscard]] uint32_t StartParticle(size_t index) const;
	void UpdateParticles(float seconds);
	/// The needs and desires go on once the body and mind have started, and every frame for those that hold them
	void ApplyStates();
	void Give(const Command& command);
	/// Changes the land through the host, which ends the scenario
	void ChangeLand(std::string_view landScript);
	/// The commands on things, of the player's hand and of the leashes; each returns what came of it
	std::string GiveObjectCommand(entt::entity creature, const Command& command);
	std::string GiveLeashCommand(entt::entity creature, const Command& command);
	/// The commands of the player alone, such as a key pressed
	std::string GivePlayerCommand(const Command& command);
	/// The commands of fights, and of being knocked out and brought round
	std::string GiveFightCommand(entt::entity creature, const Command& command);
	/// Creature Mode's and the Creature Cave's commands, as the player's keys and clicks give them
	std::string GiveCreatureModeCommand(entt::entity creature, const Command& command);
	/// The player's mouse: presses, moves and the wheel
	std::string GivePointerCommand(const Command& command);
	/// Moves the mouse on along a sweep, gives the hand the buttons held, and lets go of the mouse once the scenario's
	/// commands are done
	void UpdatePointer(float seconds);
	/// The mouse is the player's again
	void ReleasePointer();
	/// The hand starts drawing a gesture (value): the pointer along its stroke, through the recogniser
	std::string DrawGesture(const Command& command);
	/// Moves the drawing on: the stroke started, the pointer following it, the Action button let go at its end, and what
	/// the recogniser took logged
	void UpdateDrawing(float seconds);
	/// The drawing stops where it is, the stroke player with it
	void StopDrawing();
	/// Where the cursor and the hand are on the screen, for the log
	[[nodiscard]] std::string HandOnScreen() const;
	/// Sets a desire or the stage of growing up, or rewards what the creature last did by the kind of thing it was to
	std::string TeachMind(entt::entity entity, const Command& command);
	/// Loads a mind file named as a scenario names it into a creature
	void LoadMindFile(entt::entity entity, std::string_view name);
	void UpdateCamera();
	[[nodiscard]] bool IsFree(size_t creature) const;
	[[nodiscard]] std::optional<entt::entity> CreatureAt(size_t index) const;
	/// The scenario's objects by their place in it, while they are still about
	[[nodiscard]] std::optional<entt::entity> ObjectAt(size_t index) const;
	void Log(std::string line);

	TestbedHost& _host;
	const Scenario* _scenario {nullptr};
	bool _running {false};
	float _seconds {0.0f};
	Timeline _timeline;
	glm::vec2 _middle {0.0f};
	std::vector<entt::entity> _creatures;
	std::vector<entt::entity> _objects;
	/// Where the scenario's hand demo presses meet the land, in order; none without a demo
	std::vector<glm::vec2> _presses;
	/// The testbed's land the scenario was set out on; another land in its place ends the scenario
	const LandIslandInterface* _land {nullptr};
	/// The scenario's hand demo, and whether it has been started
	std::vector<hand_demo::Record> _demoRecords;
	bool _demoStarted {false};
	/// The scenario's particle effects, and the seconds since each was last started
	struct RunningParticle
	{
		uint32_t effect;
		float seconds;
	};
	std::vector<RunningParticle> _particles;
	/// Whether each creature's needs and desires have been set as it started
	std::vector<bool> _started;

	std::deque<std::string> _log;

	/// The mouse as the scenario drives it, and the sweep it is moving along
	testbed_pointer::Driver _pointer;
	std::optional<testbed_pointer::Sweep> _sweep;
	/// The scenario put the cursor somewhere as it started, which holds until it stops
	bool _holdPointer {false};
	/// The hand's place on the screen is logged every frame for a while after the mouse's buttons change
	float _handWatchSeconds {0.0f};
	/// The gesture the hand is drawing, and the recogniser's cooldown when last read, which tells when it takes one
	std::optional<testbed_gesture::Drawing> _drawing;
	float _gestureCooldown {0.0f};
	/// Whether the frame after a drawing still looks for the gesture being taken
	bool _watchGestureOnce {false};

	/// The crowd laid out, the next of it to spawn, its homes and towns as they have spawned, and how long it took
	std::vector<CrowdCreature> _crowdCreatures;
	VillageLayout _village;
	size_t _crowdNext {0};
	std::vector<entt::entity> _crowdAbodes;
	std::vector<entt::entity> _crowdEntities;
	CrowdProgress _crowdProgress;

	BenchmarkSettings _benchmark;
	uint32_t _settledFrames {0};
	std::unique_ptr<benchmark::Recorder> _recorder;
	benchmark::Results _liveResults;
	uint32_t _framesSinceSummary {0};
	/// The frames have all been measured once, and said so
	bool _measured {false};

	std::optional<Shot> _shot;
	size_t _shotCreature {0};
	float _shotDistance {1.0f};
};

} // namespace openblack::testbed_scenarios
