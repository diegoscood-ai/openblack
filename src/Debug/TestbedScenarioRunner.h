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

#include <array>
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
#include <glm/vec3.hpp>

#include "BenchmarkRecorder.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "Input/HandDemo.h"
#include "TestbedGesture.h"
#include "TestbedHeldKeys.h"
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
	/// Plays the scenarios' commands on the land the game has loaded, one scenario after the other, as a land entry's
	/// timeline= asks, in place of any other: the land is not loaded afresh and nothing is set out on it, the points
	/// are map coordinates, creature 0 is the player's creature, and the commands carry on through a change of land
	void StartOnLand(std::vector<const Scenario*> chapters);
	/// Whether the scenario running is played on the game's own land
	[[nodiscard]] bool IsOnLand() const { return _onLand; }
	/// Stops giving commands and puts back the settings the scenario changed: the body time, fainting, the footprints'
	/// smileys, the clock, whether anger starts fights and the mouse. What is on the land stays, to look at or carry on
	/// with by hand.
	void Stop();
	/// Once a frame, by the scenario seconds the frame moves on: the game's fixed step, so that a scenario plays the same
	/// whatever the frame rate. It must run after the game reads the mouse's buttons and before the hand reads them, as
	/// the debug windows' loop does.
	void Update(float seconds);

	[[nodiscard]] bool IsRunning() const { return _running; }
	/// Whether the running scenario's clock goes on while the game is paused (Scenario::clockWhilePaused)
	[[nodiscard]] bool ClockRunsWhilePaused() const { return _scenario != nullptr && _scenario->clockWhilePaused; }
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
	/// The scenario's run state, as it starts: its clock, its timeline, and none of the last one's things
	void BeginRun(const Scenario& scenario);
	/// Starts the land timeline's scenario of that place
	void BeginChapter(size_t chapter);
	/// The land timeline's next scenario once this one's commands are all given, and a line once the last one's are
	void NextLandChapter();
	/// A land timeline's creature 0: the player's creature, or none until they have one
	void RefreshLandCreature();
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
	/// Lays the scenario's footpaths and links them to the objects they lead from
	void PlaceFootpaths(const Scenario& scenario, glm::vec2 middle);
	/// Runs the scenario's land script lines, their places from the middle of the map
	void RunLandScript(const Scenario& scenario, glm::vec2 middle);
	void PlaceBirds(const Scenario& scenario, glm::vec2 middle);
	/// The scenario's town's belief in the player, as the map script's SET_TOWN_BELIEF sets it
	void SetTownBelief(float belief);
	void PlaceBareTowns(const Scenario& scenario, glm::vec2 middle);
	void PlaceCreatures(const Scenario& scenario, glm::vec2 middle);
	/// The testbed's rule, not the game's, once the scenario's creatures are all made: the first creature of each owner
	/// who leads creatures is the one they lead (Scenario::firstCreatureLeads); then each creature's own leashable, in
	/// the order they were made, has the last word
	void LeadFirstCreatures(const Scenario& scenario);
	/// The same rule for the crowd's creatures once the crowd is all made, for the owners the scenario has no creature
	/// of
	void LeadFirstCrowdCreatures();
	/// The scenario's dispensers and lone bubbles
	void PlaceDispensers(const Scenario& scenario);
	/// Casts the scenario's miracles as their times come, and lets go of held ones when theirs are up; those cast through
	/// the hand move the hand by the turn's seconds
	void UpdateMiracles(float seconds);
	/// A miracle cast through the hand: its seed into the hand, the hand over its point, and the button's press timed
	/// (a storm's or shield's circle is drawn once the button is down)
	void StartHandCast(size_t index);
	/// What the hand sees from where it is, looking down at the miracle's point (or creature), for the magic system
	[[nodiscard]] std::optional<ecs::systems::MagicSystemInterface::HandFrame> HandFrameFor(size_t index, glm::vec3 hand) const;
	/// Casts the scenario's miracle of that index; the running miracle, or none
	entt::entity CastMiracle(size_t index);
	/// Every miracle's position and the land's height under it, for a scenario that logs them
	void LogMiracles();
	/// Every building's life and repair site, for a scenario that logs them
	void LogBuildings();
	/// The scenario's villagers set walking, and dropped on teleport stones, as their times come
	void UpdateVillagerWalks();
	/// Spawns the next batch of the crowd
	void SpawnCrowd();
	void SpawnCrowdMember(size_t index);
	/// Takes the last frame's times from the profiler, once the crowd has spawned and settled
	void Measure();
	/// Starts the scenario's particle effect of that index
	[[nodiscard]] uint32_t StartParticle(size_t index) const;
	void UpdateParticles(float seconds);
	/// The scenario's objects thrown when their throws are due
	void UpdateThrows();
	/// The needs and desires go on once the body and mind have started, and every frame for those that hold them
	void ApplyStates();
	void Give(const Command& command);
	/// Changes the land through the host, which ends the scenario
	void ChangeLand(std::string_view landScript);
	/// The commands on things, of the player's hand and of the leashes; each returns what came of it
	std::string GiveObjectCommand(entt::entity creature, const Command& command);
	std::string GiveHandCommand(entt::entity creature, const Command& command);
	/// A script's guide or Ogre native on the creature, or on the creature the scenario last made by one
	std::string GiveGuideOgreNative(entt::entity creature, const Command& command, glm::vec2 point);
	std::string GiveLeashCommand(entt::entity creature, const Command& command);
	/// The commands of the player alone, such as a key pressed
	std::string GivePlayerCommand(const Command& command);
	/// The commands on the scripts: their globals set and logged, a script started, the texts clicked on, and what a
	/// wait waited for; each returns what came of it
	std::string GiveScriptsCommand(const Command& command);
	/// Whether what a wait waits for has come: its global, its script's task, its text or the camera near its point
	[[nodiscard]] bool WaitIsOver(const Command& command);
	/// A command's point of the land, at the land's height there
	[[nodiscard]] glm::vec3 GroundPoint(glm::vec2 point) const;
	/// The commands on the camera, on a thing of the land and on the hand over the land; each returns what came of it
	std::string GiveLandCommand(const Command& command);
	/// The thing a command acts on: the land's thing it picks (Command::thing), else the scenario's object of its place
	[[nodiscard]] std::optional<entt::entity> ThingOf(const Command& command) const;
	/// The second thing a command acts on: the land's thing it picks (Command::other), else the scenario's object of the
	/// place its value gives
	[[nodiscard]] std::optional<entt::entity> OtherThingOf(const Command& command) const;
	/// The land's thing nearest a point of a kind, as a script finds one, without the script taking it up
	[[nodiscard]] std::optional<entt::entity> FindThing(const testbed_land_timeline::ThingNear& pick) const;
	/// The camera kept behind and above the thing it follows
	void FollowThing();
	/// A text waiting for the player's click clicked on, while the scenario asks for its texts to be
	void ClickWaitingText();
	/// The hand starts taking hold of the newest fireball it can see: an empty hand with a click (or a press held past
	/// the grab's wait, value 1), a hand with a seed by resting over it
	std::string StartFireBallTake(const Command& command);
	/// Each frame: the pointer kept on the ball, the button pressed and let go, and what came of it logged once the ball
	/// is gone or the time is up
	void UpdateFireBallTake(float seconds);
	/// The commands to the scenario's villagers and things: a villager's walk, a store taking a thing
	std::string GiveVillagerCommand(const Command& command);
	/// The commands of fights, and of being knocked out and brought round
	std::string GiveFightCommand(entt::entity creature, const Command& command);
	/// Creature Mode's and the Creature Cave's commands, as the player's keys and clicks give them
	std::string GiveCreatureModeCommand(entt::entity creature, const Command& command);
	/// A script opening or closing the scenery, and the player's hand laying a gate stone in the plinth
	void GiveSceneryCommand(const Command& command);
	[[nodiscard]] std::string GiveAdvisorCommand(const Command& command) const;
	/// A land vortex made, or one the scenario made fading in or out, as a script does it
	void GiveVortexCommand(const Command& command);
	/// A key (an SDL scancode) goes down or up in the game's input, as the window gives a real key's
	void GiveKey(uint32_t key, bool down) const;
	/// Once every vortex the scenario made is gone, or 30 seconds in with the ones still there named, logs the heights of
	/// the cells the first two levelled squares share
	void LogVortexLand();
	/// The player's mouse: presses, moves and the wheel
	std::string GivePointerCommand(const Command& command);
	/// Moves the mouse on along a sweep, gives the hand the buttons held, and lets go of the mouse once the scenario's
	/// commands are done
	/// The cursor kept over the point of the land a scenario holds the hand over, wherever the camera has gone
	void HoldHand();
	void UpdatePointer(float seconds);
	/// Creature Mode's double click and held camera keys, as the window gives the game a player's: the cursor kept over
	/// the creature until the double click is given, and the keys let go once their time is up
	void UpdateCreatureModeInput(float seconds);
	/// The camera keys held are let go, as the window lets go of real keys
	void LetGoOfCameraKeys();
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
	/// Logs the hand's light from the land's colour of the frame, when the scenario asks and the colour has changed
	void LogHandLight();
	/// Logs which of the sun glare's samples the land hides from the camera, when the scenario asks and that has changed
	void LogSunGlare();
	void UpdateCamera();
	[[nodiscard]] bool IsFree(size_t creature) const;
	/// Whether a villager of the scenario's objects is walking round its object of that index, or the object is gone
	[[nodiscard]] bool IsWalkedRound(size_t object) const;
	/// The scenario's creatures by their place in it, while they are still about: none for one taken out of the game,
	/// whose entity is no longer valid, though it keeps its place in the list
	[[nodiscard]] std::optional<entt::entity> CreatureAt(size_t index) const;
	/// The scenario's objects by their place in it, while they are still about
	[[nodiscard]] std::optional<entt::entity> ObjectAt(size_t index) const;
	void Log(std::string line);

	TestbedHost& _host;
	const Scenario* _scenario {nullptr};
	bool _running {false};
	/// The scenarios a land timeline plays in turn, the one playing, and whether the end of the last has been logged
	bool _onLand {false};
	std::vector<const Scenario*> _chapters;
	size_t _chapter {0};
	bool _landDoneLogged {false};
	float _seconds {0.0f};
	Timeline _timeline;
	glm::vec2 _middle {0.0f};
	std::vector<entt::entity> _creatures;
	std::vector<entt::entity> _objects;
	/// When each of the scenario's throws is next due, in seconds; none once done
	std::vector<std::optional<float>> _throwsDue;
	/// Where the scenario's hand demo presses meet the land, in order; none without a demo
	std::vector<glm::vec2> _presses;
	/// The testbed's land the scenario was set out on; another land in its place ends the scenario. A land timeline's
	/// land, as last seen
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
	/// The scenario's miracles: when each is next cast, and the one held until when
	struct RunningMiracle
	{
		std::optional<float> nextAt;
		std::optional<float> letGoAt;
		entt::entity spell {entt::null};
		/// A shield is broken then
		std::optional<float> breakAt;
		/// Through the hand: where the hand is, and when the button goes down and comes up, and the sweep ends
		glm::vec3 hand {0.0f};
		std::optional<float> pressAt;
		std::optional<float> releaseAt;
		std::optional<float> sweepUntil;
	};
	std::vector<RunningMiracle> _miracles;
	/// When the miracles' positions are next logged, for a scenario that logs them
	float _nextMiracleLog {0.0f};
	/// When the buildings are next logged, for a scenario that logs them
	float _nextBuildingLog {0.0f};
	/// For each object, when it next walks, whether it next walks out or back, and whether it has been dropped yet
	struct ObjectWalk
	{
		std::optional<float> nextAt;
		bool outwards {true};
		bool dropped {false};
	};
	std::vector<ObjectWalk> _walks;
	/// Whether each creature's needs and desires have been set as it started
	std::vector<bool> _started;

	std::deque<std::string> _log;

	/// The mouse as the scenario drives it, and the sweep it is moving along
	testbed_pointer::Driver _pointer;
	std::optional<testbed_pointer::Sweep> _sweep;
	/// The scenario put the cursor somewhere as it started, which holds until it stops
	bool _holdPointer {false};
	/// The debug Footpaths box as it was before a scenario that draws the footpaths turned it on
	std::optional<bool> _drawFootpathsBefore;
	/// The point of the land the hand is held over, by the cursor, for the whole scenario
	std::optional<glm::vec3> _handHoldPoint;
	/// The hand taking hold of a fireball: the ball, the time and frames since it started, how it takes it, when it
	/// pressed the Action button and whether it let go, and the seed it held as it started
	struct FireBallTake
	{
		entt::entity ball {entt::null};
		float seconds {0.0f};
		uint32_t frames {0};
		bool feed {false};
		bool holdPastGrab {false};
		std::optional<float> pressedAt;
		bool released {false};
		entt::entity seed {entt::null};
		float seedPower {1.0f};
	};
	std::optional<FireBallTake> _fireBallTake;
	/// The object the last ScriptDance made, as the scripts know it (0 for none), which ScriptDanceAttach attaches to
	uint32_t _lastDance {0};
	/// The script interface level before the scenario's first ScriptInterfaceLevel, put back once it stops
	std::optional<int32_t> _interfaceLevelBefore;
	/// The hand's place on the screen is logged every frame for a while after the mouse's buttons change
	float _handWatchSeconds {0.0f};
	/// The gesture the hand is drawing, and the recogniser's cooldown when last read, which tells when it takes one
	std::optional<testbed_gesture::Drawing> _drawing;
	float _gestureCooldown {0.0f};
	/// Whether the frame after a drawing still looks for the gesture being taken
	bool _watchGestureOnce {false};
	/// The land's colour the hand's light was last logged for
	std::optional<uint32_t> _handLightColour;
	/// The sun glare's line last logged
	std::optional<std::string> _sunGlareLine;
	/// The vortices the scenario made, in order, with the point each was made at; and whether the land they levelled
	/// has been logged
	struct MadeVortex
	{
		entt::entity entity {entt::null};
		glm::vec2 point {0.0f};
	};
	std::vector<MadeVortex> _vortices;
	bool _vortexLandLogged {false};
	/// Whether the scenario has faded the screen as a script does, put back to normal when it stops
	bool _faded {false};
	/// Whether the scenario showed a text in the dialogue, whose wait for a click its key commands log
	bool _textShown {false};
	/// The hand took hold of a creature for the scenario as its own pick does, which it lets go of once the scenario
	/// stops
	bool _handGrabbed {false};
	/// The creature the scenario's last script native made from another, which the desire natives then act on
	std::optional<entt::entity> _scriptMade;
	/// The keys the scenario has put down, let go when it stops
	HeldKeys _keys;
	/// The texts that wait for a click are clicked on (ClickTexts), until the scenario stops
	bool _clickTexts {false};
	/// The thing the camera follows (CameraFollow), and how far back
	std::optional<entt::entity> _followed;
	float _followDistance {1.0f};
	/// The command a wait for a task was last asked for, by its place, and whether a task of its script was seen running
	/// since it began waiting
	std::optional<size_t> _taskWaitAt;
	bool _taskSeenRunning {false};
	/// A double click on a creature: the cursor kept over it until the hand sees it under itself, then the double click's
	/// second press down, and up again the next frame
	struct CreatureDoubleClick
	{
		entt::entity creature {entt::null};
		float seconds {0.0f};
		bool down {false};
	};
	std::optional<CreatureDoubleClick> _doubleClick;
	/// The camera keys held for Creature Mode, and when they are let go
	struct HeldCameraKeys
	{
		std::array<SDL_Scancode, 2> keys {};
		float until {0.0f};
	};
	std::optional<HeldCameraKeys> _cameraKeys;

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
