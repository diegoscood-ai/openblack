/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedScenarioRunner.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <ranges>
#include <system_error>
#include <type_traits>
#include <variant>

#include <MindFile.h>
#include <SDL_events.h>
#include <bgfx/bgfx.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/FlatLand.h"
#include "3D/LandIslandInterface.h"
#include "3D/SkyInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/LeashOwnership.h"
#include "Creature/LeashRules.h"
#include "Debug/GesturesModel.h"
#include "Debug/MiraclesModel.h"
#include "Debug/TestbedDemoPoints.h"
#include "Debug/WeatherModel.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/FieldArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/InputStateInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Weather/Storms.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Input/FixedMouse.h"
#include "Input/GameActionMapInterface.h"
#include "Input/GameCursor.h"
#include "Input/HandDemo.h"
#include "Input/RealInput.h"
#include "Locator.h"
#include "Magic/Gestures/GestureInput.h"
#include "Magic/Gestures/GestureTemplates.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Particles/ParticleTypes.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

#include "../Profiler.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using openblack::ecs::archetypes::CreatureArchetype;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureAnimation;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::CreatureNeeds;
using openblack::ecs::components::Transform;

namespace
{
using Kind = Command::Kind;
using Pace = ecs::systems::CreatureLocomotionSystemInterface::Pace;

/// The lines of the log of commands kept
constexpr size_t k_LogLines = 8;
/// A creature follows another this far behind, for a creature of size 1
constexpr float k_FollowDistance = 25.0f;
/// The size of a creature whose scenario doesn't give one
constexpr float k_DefaultSize = 1.0f;
/// The least the overview takes in either way of the middle of what it frames
constexpr float k_MinOverviewHalfSize = 25.0f;
/// The live results of a benchmark are summed up again every so many frames
constexpr uint32_t k_FramesPerSummary = 30;
/// What the store of a crowd's town starts with
constexpr uint32_t k_CrowdFood = 2000;
constexpr uint32_t k_CrowdWood = 2000;
/// The town a scenario's buildings and fields belong to, its id kept clear of the crowds' towns, and what its buildings
/// hold
constexpr uint32_t k_ScenarioTownId = 900;
constexpr uint32_t k_ScenarioTownFood = 500;
constexpr uint32_t k_ScenarioTownWood = 500;
/// The hand's place on the screen is logged for this long after a button changes
constexpr float k_HandWatchSeconds = 0.5f;

/// The scenario's town, a Celtic town of the player's in the middle of the map, made when first needed
uint32_t ScenarioTown(glm::vec2 middle)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& towns = registry.Context().towns;
	if (const auto found = towns.find(k_ScenarioTownId); found != towns.end())
	{
		if (registry.Valid(found->second))
		{
			return k_ScenarioTownId;
		}
		towns.erase(found);
	}
	const glm::vec3 position {middle.x, Locator::terrainSystem::value().GetHeightAt(middle), middle.y};
	ecs::archetypes::TownArchetype::Create(static_cast<int>(k_ScenarioTownId), position, PlayerNames::PLAYER_ONE,
	                                       Tribe::CELTIC);
	return k_ScenarioTownId;
}

/// The profiler's stages as benchmarks name them: the drawing of the reflection and of the main pass told apart, and
/// everything from drawing the scene on counted as rendering
std::vector<benchmark::StageInfo> BenchmarkStages()
{
	static const std::vector<std::string> k_Names = [] {
		std::vector<std::string> names;
		for (size_t i = 0; i < Profiler::k_StageNames.size(); ++i)
		{
			const auto stage = static_cast<Profiler::Stage>(i);
			std::string name(Profiler::k_StageNames.at(i));
			if (stage > Profiler::Stage::ReflectionPass && stage <= Profiler::Stage::ReflectionDrawSprites)
			{
				name = "Reflection " + name;
			}
			else if (stage > Profiler::Stage::MainPass && stage <= Profiler::Stage::MainPassDrawSprites)
			{
				name = "Main " + name;
			}
			names.push_back(std::move(name));
		}
		return names;
	}();
	std::vector<benchmark::StageInfo> stages;
	for (size_t i = 0; i < k_Names.size(); ++i)
	{
		// the scene's draw up to the renderer's frame, and the draw's preparation; every other stage is update work
		const auto stage = static_cast<Profiler::Stage>(i);
		const bool render = (stage >= Profiler::Stage::SceneDraw && stage <= Profiler::Stage::RendererFrame) ||
		                    (stage >= Profiler::Stage::PreDraw && stage <= Profiler::Stage::DrawUpload);
		stages.push_back({.name = k_Names.at(i), .render = render});
	}
	return stages;
}

/// The kind of build a benchmark's results were measured in
#ifdef NDEBUG
constexpr std::string_view k_Build = "optimised";
#else
constexpr std::string_view k_Build = "debug";
#endif

double Milliseconds(std::chrono::system_clock::duration duration)
{
	return std::chrono::duration<double, std::milli>(duration).count();
}

/// The time a stage took in a frame: its last run, if it ran within the frame, as the profiler's own summary counts it
double StageMilliseconds(const Profiler::Entry& entry, const Profiler::Scope& stage)
{
	if (!stage.finalized || stage.start < entry.frameStart || stage.end > entry.frameEnd)
	{
		return 0.0;
	}
	return Milliseconds(stage.end - stage.start);
}

std::string_view MoveResultName(ecs::systems::CreatureLocomotionSystemInterface::MoveResult result)
{
	using MoveResult = ecs::systems::CreatureLocomotionSystemInterface::MoveResult;
	switch (result)
	{
	case MoveResult::InvalidDestination:
		return "nowhere to stand there";
	case MoveResult::Busy:
		return "it can't walk";
	case MoveResult::Started:
	default:
		return "on its way";
	}
}

/// Where a creature faces on the land; its mesh looks back along +z
glm::vec2 AheadOf(const Transform& transform)
{
	const auto ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	return {ahead.x, ahead.z};
}

/// The debug windows leave the mouse alone while a scenario drives it, so that the real pointer resting on one of them
/// doesn't take the hand's place
void KeepDebugWindowsOffTheMouse(bool off)
{
	if (ImGui::GetCurrentContext() == nullptr)
	{
		return;
	}
	auto& io = ImGui::GetIO();
	io.ConfigFlags = off ? (io.ConfigFlags | ImGuiConfigFlags_NoMouse) : (io.ConfigFlags & ~ImGuiConfigFlags_NoMouse);
}

/// Whether a command sends its creature somewhere, or has it face, throw or point somewhere
bool HasPoint(Kind kind)
{
	return kind == Kind::WalkTo || kind == Kind::RunTo || kind == Kind::FleeFrom || kind == Kind::TurnToFace ||
	       kind == Kind::ThrowAt || kind == Kind::PointAt;
}

/// Whether a command is the player's alone, given before a creature is looked for
bool IsPlayerCommand(Kind kind)
{
	return kind == Kind::HoldSeed || kind == Kind::DrawGesture || kind == Kind::SummonSeed || kind == Kind::PressKey ||
	       kind == Kind::HandTakeFireBall;
}

std::string_view Started(bool started)
{
	return started ? "started" : "can't";
}

/// What a command whose part of the game this tree doesn't have yet says
std::string NotYet(Kind kind)
{
	return fmt::format("not in this tree yet: {}", Name(kind));
}

/// The camera's fields of view across and up and down, in radians
glm::vec2 FieldsOfView(const Camera& camera)
{
	const auto horizontal = camera.GetHorizontalFieldOfView();
	const auto aspect = Locator::windowing::has_value() ? Locator::windowing::value().GetAspectRatio() : 1.0f;
	return {horizontal, 2.0f * std::atan(std::tan(horizontal * 0.5f) / std::max(aspect, 0.1f))};
}

/// Why the player's leash was refused, for the readout
std::string RefusedText(const ecs::systems::LeashSystemInterface& leashes, PlayerNames player)
{
	const auto refused = leashes.LastRefusal(player);
	return refused.has_value() ? fmt::format("refused: {}", creature_leash::Describe(refused->why)) : "can't";
}

/// The hour of the day jumps, as a script sets it: the clock goes on from there and the sky follows at once
void SetHour(float hour)
{
	if (!Locator::dayNightClock::has_value())
	{
		return;
	}
	auto& clock = Locator::dayNightClock::value().Clock();
	clock.SetScriptTime(hour);
	if (Locator::skySystem::has_value())
	{
		Locator::skySystem::value().SetTime(clock.GetScriptTime());
	}
}

/// A villager's or animal's life and poison as the scenario gives them
void SetLifeAndPoison(entt::entity entity, const ObjectSetup& object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (entity == entt::null || !registry.Valid(entity) ||
	    !registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(entity))
	{
		return;
	}
	if (object.life.has_value())
	{
		ecs::life::SetLife(entity, std::clamp(*object.life, 0.0f, 1.0f));
	}
	if (object.poisoned)
	{
		ecs::life::SetPoisoned(entity, true);
	}
}

glm::ivec2 WindowSize()
{
	return Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(1);
}

/// The records of the game's hand demo of that name, read through the byte cache as its playback reads them; none when
/// it cannot be read
std::vector<hand_demo::Record> ReadHandDemo(std::string_view name)
{
	if (!Locator::filesystem::has_value() || !Locator::resources::has_value())
	{
		return {};
	}
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto& bytes = resources::LoadBlob(
		    Locator::resources::value().GetBlobs(),
		    fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "HandDemo" / (std::string(name) + ".hnd")));
		return hand_demo::ParseRecords(bytes);
	}
	catch (const std::exception&)
	{
		return {};
	}
}

/// Whether the mouse's own events are dropped before the game sees them, as in the test runs
bool MouseEventsDropped()
{
	return input::IgnoreRealInput() || input::FixedMouse().has_value();
}

/// Sends a move of the pointer to where it is, by so much, with the buttons held, as the mouse does, for the camera's
/// drags; the hand follows the fixed cursor instead
void PushMotion(glm::ivec2 position, uint32_t buttons, glm::ivec2 moved)
{
	SDL_Event event {};
	event.type = SDL_MOUSEMOTION;
	event.motion.windowID = Locator::windowing::has_value() ? Locator::windowing::value().GetID() : 0;
	event.motion.state = buttons;
	event.motion.x = position.x;
	event.motion.y = position.y;
	event.motion.xrel = moved.x;
	event.motion.yrel = moved.y;
	SDL_PushEvent(&event);
}
} // namespace

void Runner::Start(const Scenario& scenario)
{
	Stop();
	// The camera is the player's again before the scenario frames it, and the cave closes
	if (Locator::creatureModeSystem::has_value())
	{
		Locator::creatureModeSystem::value().Leave();
	}
	if (Locator::creatureCaveSystem::has_value() && Locator::creatureCaveSystem::value().IsOpen())
	{
		Locator::creatureCaveSystem::value().Close();
	}
	_scenario = &scenario;
	_running = true;
	_seconds = 0.0f;
	_timeline = {};
	_creatures.clear();
	_objects.clear();
	_presses.clear();
	_particles.clear();
	_started.clear();
	_log.clear();
	_sweep.reset();
	_handWatchSeconds = 0.0f;
	_holdPointer = false;
	_shot.reset();
	_crowdCreatures.clear();
	_village = {};
	_crowdNext = 0;
	_crowdAbodes.clear();
	_crowdEntities.clear();
	_crowdProgress = {};
	_settledFrames = 0;
	_recorder.reset();
	_liveResults = {};
	_framesSinceSummary = 0;
	_measured = false;
	if (scenario.crowd.has_value())
	{
		const auto& crowd = *scenario.crowd;
		if (crowd.kind == Crowd::Kind::Creatures)
		{
			_crowdCreatures = LayOutCreatures(crowd.count, crowd.seed);
			_crowdProgress.total = _crowdCreatures.size();
		}
		else
		{
			_village = LayOutVillagers(crowd.count, crowd.seed);
			_crowdProgress.total = _village.towns.size() + _village.abodes.size() + _village.villagers.size();
		}
		_recorder = std::make_unique<benchmark::Recorder>(BenchmarkStages(), _benchmark.frames);
	}

	// The scenario's hand demo is read before the land is made: one recorded with its camera close to the ground is
	// played over a lower plane
	_demoRecords.clear();
	_demoStarted = false;
	auto planeAltitude = flat_land::k_Altitude;
	if (const auto& demo = scenario.fixtures.handDemo; demo.has_value())
	{
		_demoRecords = ReadHandDemo(demo->name);
		if (_demoRecords.empty())
		{
			Log(fmt::format("Problem: the hand demo {} could not be read", demo->name));
		}
		planeAltitude =
		    demo->planeAltitude.value_or(testbed_demo::PlaneAltitudeUnder(_demoRecords).value_or(flat_land::k_Altitude));
	}

	// A scenario that writes into the game's folder runs only on a copy of the game's data: checked before anything is
	// changed, so that nothing is left to undo
	if (!MayRunHere(scenario))
	{
		_running = false;
		return;
	}

	// A fresh testbed: the last one's creatures, objects, footprints, weather and scripts all go with it
	_host.LoadTestbed(planeAltitude);
	if (!Locator::terrainSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		_running = false;
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	_middle = (land.GetExtent().minimum + land.GetExtent().maximum) * 0.5f;
	_land = &land;
	_presses = DemoPresses(planeAltitude);

	SetUpEnvironment(scenario.environment);
	PlaceObjects(scenario, _middle);
	PlaceCreatures(scenario, _middle);
	PlaceFixtures(scenario);
	for (size_t i = 0; i < scenario.particles.size(); ++i)
	{
		_particles.push_back({StartParticle(i), 0.0f});
	}
	Frame(scenario.framing.shot, scenario.framing.creature, scenario.framing.distance);
	Log(fmt::format("Started {}", scenario.name));

	// What the scenario sets up that this tree has no way to yet
	std::vector<std::string_view> missing;
	if (!scenario.dispensers.empty())
	{
		missing.emplace_back("dispensers");
	}
	if (!scenario.miracles.empty())
	{
		missing.emplace_back("miracles");
	}
	if (scenario.hand.has_value())
	{
		missing.emplace_back("the hand held still");
	}
	if (scenario.tribalPower.has_value())
	{
		missing.emplace_back("tribal power");
	}
	if (scenario.environment.prayer.has_value())
	{
		missing.emplace_back("prayer power");
	}
	if (scenario.logMiraclesEvery.has_value())
	{
		missing.emplace_back("the miracles' positions");
	}
	if (std::ranges::any_of(scenario.objects, [](const ObjectSetup& object) {
		    return object.walkTo.has_value() || object.dropOnStoneSeconds.has_value() || object.worshipAt.has_value();
	    }))
	{
		missing.emplace_back("villagers' walks and teleport stones");
	}
	if (std::ranges::any_of(scenario.particles, [](const ParticleSetup& particle) { return particle.player != 0; }))
	{
		missing.emplace_back("particle colours by player");
	}
	if (!missing.empty())
	{
		Log(fmt::format("not in this tree yet: {}", fmt::join(missing, ", ")));
	}
	// The camera's drags and the wheel come as the mouse's own events, which the test runs drop
	if (MouseEventsDropped() && std::ranges::any_of(scenario.commands, [](const Command& command) {
		    return command.kind == Kind::PointerSweep || command.kind == Kind::WheelTurn;
	    }))
	{
		Log("Problem: the mouse's events are dropped while the real input is ignored or the mouse is fixed; the "
		    "camera won't see this scenario's drags and wheel");
	}
}

void Runner::Stop()
{
	if (!_running)
	{
		return;
	}
	_running = false;
	RestoreSettings();
	Log("Stopped");
}

void Runner::RestoreSettings()
{
	// The mouse is the player's again
	ReleasePointer();
	// Its particle effects die away
	if (Locator::particleSystem::has_value())
	{
		for (const auto& particle : _particles)
		{
			Locator::particleSystem::value().CloseDown(particle.effect);
		}
	}
	_particles.clear();
	if (Locator::creaturePhysiologySystem::has_value())
	{
		auto& physiology = Locator::creaturePhysiologySystem::value();
		physiology.SetTimeScale(1.0f);
		physiology.SetFaintingEnabled(true);
	}
	if (Locator::footprintSystem::has_value())
	{
		Locator::footprintSystem::value().SetAprilFoolsOverride(std::nullopt);
	}
	if (Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().SetAngerStartsFights(true);
	}
	if (Locator::dayNightClock::has_value())
	{
		Locator::dayNightClock::value().Clock().SetRunning(true);
	}
}

void Runner::SetIslandWeather(Weather kind)
{
	if (kind == Weather::Clear || !Locator::weatherSystem::has_value())
	{
		return;
	}
	namespace window = debug::weather_window;
	// Rain and thunderstorms as the Weather window's presets; snow and blizzards as cold rainless storms that snow
	window::StormSettings settings;
	switch (kind)
	{
	case Weather::Rain:
		window::ApplyPreset(window::k_Presets.at(2), settings);
		break;
	case Weather::Thunderstorm:
		window::ApplyPreset(window::k_Presets.at(3), settings);
		break;
	case Weather::Snow:
	case Weather::Blizzard:
		settings.rain = 0;
		settings.overcast = 100;
		settings.temperature = static_cast<int8_t>(kind == Weather::Snow ? -5 : -10);
		settings.windStrength = kind == Weather::Snow ? 5 : 30;
		break;
	case Weather::Clear:
		break;
	}
	auto storm = window::IslandStorm(settings);
	if (kind == Weather::Snow || kind == Weather::Blizzard)
	{
		storm.weather.snow = static_cast<int8_t>(kind == Weather::Snow ? 60 : 100);
	}
	Locator::weatherSystem::value().SetStormCreationEnabled(false);
	weather::storms::Create(storm);
	Log(fmt::format("Weather over the island: {}", Name(kind)));
}

void Runner::PlaceByHand(const testbed_fixtures::Fixtures& fixtures)
{
	if (!Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	const auto middle = (land.GetExtent().minimum + land.GetExtent().maximum) * 0.5f;
	for (const auto& problem : testbed_fixtures::Problems(fixtures))
	{
		Log(fmt::format("Problem: {}", problem));
	}
	testbed_fixtures::Place(
	    fixtures, middle, {}, [this](size_t index) { return ObjectAt(index); },
	    [this](std::string line) { Log(std::move(line)); });
}

std::vector<glm::vec2> Runner::DemoPresses(uint8_t planeAltitude)
{
	std::vector<glm::vec2> points;
	if (_demoRecords.empty())
	{
		return points;
	}
	// The hand's ray as the game casts it: the camera's field of view across, the window's shape, the mouse to a pixel
	const testbed_demo::Lens lens {
	    .xFovDegrees = Locator::config::has_value() ? Locator::config::value().cameraXFov : 70.0f,
	    .aspect = Locator::windowing::has_value() ? Locator::windowing::value().GetAspectRatio() : 1.0f,
	};
	const auto height = testbed_demo::PlaneHeight(planeAltitude);
	for (const auto& press : testbed_demo::Presses(_demoRecords))
	{
		auto onScreen = press;
		onScreen.mouse = testbed_demo::RoundToPixel(press.mouse, WindowSize());
		const auto point = testbed_demo::PointOnPlane(onScreen, lens, height);
		if (!point.has_value())
		{
			// The presses after one that misses the land are not counted, so that a press's number stays its own
			Log(fmt::format("Problem: the hand demo's press {} does not reach the land", points.size()));
			break;
		}
		points.emplace_back(point->x, point->z);
		Log(fmt::format("Hand demo press {} at ({:.1f}, {:.1f})", points.size() - 1, point->x, point->z));
	}
	return points;
}

void Runner::StartHandDemo()
{
	if (_scenario == nullptr || _demoStarted || !_scenario->fixtures.handDemo.has_value())
	{
		return;
	}
	const auto& demo = *_scenario->fixtures.handDemo;
	if (_seconds < demo.startSeconds)
	{
		return;
	}
	_demoStarted = true;
	// The demo moves the camera, the cursor and the buttons from here, as the tutorial plays it
	ReleaseCamera();
	ReleasePointer();
	const auto played = hand_demo::Play(demo.name, 0, false, false);
	Log(played ? fmt::format("Hand demo {} playing", demo.name)
	           : fmt::format("Problem: the hand demo {} did not start", demo.name));
}

bool Runner::MayRunHere(const Scenario& scenario)
{
	// The copy of the game's data carries its marker
	if (scenario.fixtures.writesGameData)
	{
		const bool onCopy =
		    Locator::filesystem::has_value() && Locator::filesystem::value().Exists(Locator::filesystem::value().GetGamePath() /
		                                                                            testbed_fixtures::k_GameDataCopyMarker);
		if (!onCopy)
		{
			Log(fmt::format("Problem: {} writes into the game's folder, so it runs only on a copy of the game's data (one "
			                "with {} in it)",
			                scenario.id, testbed_fixtures::k_GameDataCopyMarker));
			return false;
		}
	}
	return true;
}

void Runner::PlaceFixtures(const Scenario& scenario)
{
	const auto& fixtures = scenario.fixtures;
	if (testbed_fixtures::Empty(fixtures))
	{
		return;
	}
	for (const auto& problem : testbed_fixtures::Problems(
	         fixtures, fixtures.handDemo.has_value() ? std::optional<size_t>(_presses.size()) : std::nullopt))
	{
		Log(fmt::format("Problem: {}", problem));
	}
	// The creature fixtures are the scenario's creatures too, numbered after its own, so that its commands reach them
	testbed_fixtures::Place(
	    fixtures, _middle, _presses, [this](size_t index) { return ObjectAt(index); },
	    [this](std::string line) { Log(std::move(line)); },
	    [this](entt::entity creature) {
		    _creatures.push_back(creature);
		    _started.push_back(false);
	    });
}

void Runner::SetUpEnvironment(const Environment& environment)
{
	SetHour(environment.hour);
	if (Locator::dayNightClock::has_value())
	{
		Locator::dayNightClock::value().Clock().SetRunning(environment.clockRuns);
	}
	SetIslandWeather(environment.weather);
	if (Locator::creaturePhysiologySystem::has_value())
	{
		auto& physiology = Locator::creaturePhysiologySystem::value();
		physiology.SetTimeScale(environment.bodyTimeScale);
		physiology.SetFaintingEnabled(environment.fainting);
	}
	if (Locator::footprintSystem::has_value())
	{
		Locator::footprintSystem::value().SetAprilFoolsOverride(environment.aprilFools);
	}
	if (Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().SetAngerStartsFights(environment.angerStartsFights);
	}
	if (Locator::alignmentSystem::has_value())
	{
		Locator::alignmentSystem::value().SetPlayerAlignment(PlayerNames::PLAYER_ONE,
		                                                     environment.playerAlignment.value_or(0.0f));
	}
	// The cursor, and so the hand, put at a place on the screen; held there until the scenario stops, as no mouse moves
	// it in a test run
	if (environment.cursor.has_value() && Locator::inputState::has_value() && Locator::windowing::has_value())
	{
		const auto size = WindowSize();
		_pointer.MoveTo(glm::ivec2(*environment.cursor * glm::vec2(size)), size);
		// The debug windows still take the mouse, so the scenario can be stopped
		_holdPointer = true;
	}
}

void Runner::PlaceObjects(const Scenario& scenario, glm::vec2 middle)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	for (const auto& object : scenario.objects)
	{
		const auto point = MapPoint(middle, object.offset);
		const glm::vec3 position {point.x, land.GetHeightAt(point), point.y};
		const auto yaw = glm::radians(object.yawDegrees);
		_objects.push_back(std::visit(
		    [&]<typename T>(T type) -> entt::entity {
			    if constexpr (std::is_same_v<T, MobileObjectInfo>)
			    {
				    return ecs::archetypes::MobileObjectArchetype::Create(position, type, yaw, object.scale);
			    }
			    else if constexpr (std::is_same_v<T, TreeInfo>)
			    {
				    return ecs::archetypes::TreeArchetype::Create(object.forest, position, type, true, yaw,
				                                                  object.fullSize.value_or(object.scale), object.scale);
			    }
			    else if constexpr (std::is_same_v<T, VillagerInfo>)
			    {
				    constexpr uint32_t k_AdultAge = 30;
				    const auto villager = ecs::archetypes::VillagerArchetype::Create(position, position, type, k_AdultAge);
				    if (object.joinTown)
				    {
					    const auto town = Locator::entitiesRegistry::value().Context().towns.at(ScenarioTown(middle));
					    ecs::town_villagers::AddVillagerToTown(town, villager);
				    }
				    return villager;
			    }
			    else if constexpr (std::is_same_v<T, PotInfo>)
			    {
				    return ecs::archetypes::PotArchetype::Create(position, yaw, type, object.amount);
			    }
			    else if constexpr (std::is_same_v<T, AbodeInfo>)
			    {
				    return ecs::archetypes::AbodeArchetype::Create(ScenarioTown(middle), position, type, yaw, object.scale,
				                                                   k_ScenarioTownFood, k_ScenarioTownWood);
			    }
			    else if constexpr (std::is_same_v<T, FieldTypeInfo>)
			    {
				    return ecs::archetypes::FieldArchetype::Create(static_cast<int>(ScenarioTown(middle)), position, type, yaw);
			    }
			    else if constexpr (std::is_same_v<T, AnimalInfo>)
			    {
				    // An animal of no town, with a flock of its own and its age by chance; it faces as it is made
				    return ecs::archetypes::AnimalArchetype::Create(position, type, 0, 0);
			    }
			    else
			    {
				    return ecs::archetypes::FeatureArchetype::Create(position, type, yaw, object.scale);
			    }
		    },
		    object.type));
		SetLifeAndPoison(_objects.back(), object);
	}
}

void Runner::PlaceCreatures(const Scenario& scenario, glm::vec2 middle)
{
	const auto& land = Locator::terrainSystem::value();
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& setup : scenario.creatures)
	{
		const auto point = MapPoint(middle, setup.offset);
		const glm::vec3 position {point.x, land.GetHeightAt(point), point.y};
		auto body = CreatureArchetype::StartBody(setup.species);
		body.alignment = setup.alignment.value_or(body.alignment);
		body.fatness = setup.fatness.value_or(body.fatness);
		body.strength = setup.strength.value_or(body.strength);
		const auto size = setup.size.value_or(k_DefaultSize);
		const auto entity =
		    CreatureArchetype::Create(position, setup.owner, setup.species, 0, glm::radians(setup.facingDegrees), size, body);
		if (auto* mind = registry.TryGet<CreatureMindState>(entity))
		{
			mind->paused = setup.pauseMind;
			if (setup.phase.has_value())
			{
				mind->developmentPhase = *setup.phase;
			}
		}
		if (!setup.mindFile.empty())
		{
			LoadMindFile(entity, setup.mindFile);
		}
		if (Locator::leashSystem::has_value())
		{
			// Made for trying things out, it knows every leash, as creatures made by the original's debug tools do
			auto& leashes = Locator::leashSystem::value();
			for (const auto type : creature_leash::k_Types)
			{
				leashes.SetKnown(entity, type, true);
			}
			if (setup.leashable.has_value())
			{
				leashes.SetLeashable(entity, *setup.leashable);
			}
		}
		if (Locator::creatureSkinSystem::has_value())
		{
			auto& skins = Locator::creatureSkinSystem::value();
			for (size_t slot = 0; slot < setup.tattoos.size(); ++slot)
			{
				skins.SetTattoo(entity, slot, setup.tattoos[slot]);
			}
			for (const auto& wound : setup.wounds)
			{
				skins.AddWound(entity, wound);
			}
			for (const auto& drop : setup.blood)
			{
				skins.AddBlood(entity, drop);
			}
		}
		_creatures.push_back(entity);
		_started.push_back(false);
	}
}

std::optional<entt::entity> Runner::CreatureAt(size_t index) const
{
	if (index >= _creatures.size() || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto entity = _creatures[index];
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<Creature>(entity))
	{
		return std::nullopt;
	}
	return entity;
}

void Runner::ApplyStates()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (size_t i = 0; i < _creatures.size(); ++i)
	{
		const auto entity = CreatureAt(i);
		if (!entity.has_value())
		{
			continue;
		}
		// a creature fixture has no setup of its own to hold
		if (i >= _scenario->creatures.size())
		{
			continue;
		}
		const auto& setup = _scenario->creatures[i];
		auto* needs = registry.TryGet<CreatureNeeds>(*entity);
		auto* mind = registry.TryGet<CreatureMindState>(*entity);
		const auto* creature = registry.TryGet<const Creature>(*entity);
		// The body and the mind start from the species' tables on their first turn, over anything set before
		if (needs == nullptr || mind == nullptr || creature == nullptr || !needs->started || !mind->desires.has_value())
		{
			continue;
		}
		if (_started[i] && !setup.hold)
		{
			continue;
		}
		auto overrides = setup.needs;
		if (_started[i])
		{
			// Its age is only set as it starts, so it still grows older
			overrides.age.reset();
		}
		Apply(overrides, needs->needs, creature->size);
		Apply(setup.desires, *mind->desires);
		if (!_started[i] && !setup.desires.empty())
		{
			// Its first thought was had with the species' desires; it thinks again with the scenario's
			mind->idle = {};
		}
		_started[i] = true;
	}
}

std::optional<entt::entity> Runner::ObjectAt(size_t index) const
{
	if (index >= _objects.size() || !Locator::entitiesRegistry::has_value() ||
	    !Locator::entitiesRegistry::value().Valid(_objects[index]))
	{
		return std::nullopt;
	}
	return _objects[index];
}

std::string Runner::GiveObjectCommand(entt::entity creature, const Command& command)
{
	if (!Locator::creatureObjectActionSystem::has_value())
	{
		return "no hands";
	}
	auto& hands = Locator::creatureObjectActionSystem::value();
	const auto& land = Locator::terrainSystem::value();
	const auto point = MapPoint(_middle, command.point);
	const glm::vec3 onLand {point.x, land.GetHeightAt(point), point.y};
	const auto object = ObjectAt(command.object);
	switch (command.kind)
	{
	case Kind::PickUp:
		return object.has_value() ? std::string(Started(hands.PickUp(creature, *object))) : "it is gone";
	case Kind::PutDown:
		return std::string(Started(hands.PutDown(creature)));
	case Kind::Discard:
		return std::string(Started(hands.Discard(creature)));
	case Kind::Lob:
		return std::string(Started(hands.Lob(creature)));
	case Kind::EatHeld:
		return std::string(Started(hands.EatHeld(creature)));
	case Kind::Examine:
		return std::string(Started(hands.Keep(creature, creature_object_actions::k_FirstKeepAnimation + command.value)));
	case Kind::ThrowAt:
		// At about the height of a creature's middle
		return std::string(Started(hands.Throw(creature, onLand + glm::vec3(0.0f, CreatureHeight(1.0f) * 0.5f, 0.0f))));
	case Kind::KnockDown:
		return object.has_value() ? std::string(Started(hands.Destroy(creature, *object))) : "it is gone";
	case Kind::PointAt:
		return std::string(Started(hands.PointAt(creature, onLand)));
	default:
		return {};
	}
}

std::string Runner::GiveLeashCommand(entt::entity creature, const Command& command)
{
	if (!Locator::leashSystem::has_value())
	{
		return "no leashes";
	}
	auto& leashes = Locator::leashSystem::value();
	switch (command.kind)
	{
	case Kind::PutOnLeash:
		// It must know the learning leash before any, and the leash itself
		leashes.SetKnown(creature, LeashType::Rope, true);
		leashes.SetKnown(creature, command.leash, true);
		if (leashes.IsLeashed(creature))
		{
			return leashes.ChangeType(creature, command.leash) ? "changed" : "can't";
		}
		return leashes.PutOn(creature, command.leash) ? "on" : RefusedText(leashes, command.player);
	case Kind::MakeLeashable:
		return leashes.SetLeashable(creature, true) ? "leashable" : RefusedText(leashes, command.player);
	case Kind::HandTapLeash:
		// As the player's Action button tapping it does
		return leashes.TapCreature(command.player, creature) ? "on" : RefusedText(leashes, command.player);
	case Kind::LeashShake:
		// The game shakes a leash off with a scribble drawn with the empty hand, which comes with the gestures
		return NotYet(command.kind);
	case Kind::LeashKey:
	{
		// As the player pressing the key does, through the same call the controls make
		constexpr std::array k_Keys {creature_leash::LeashKey::Leash, creature_leash::LeashKey::PreviousLeash,
		                             creature_leash::LeashKey::NextLeash};
		const auto before = leashes.TypeOf(creature);
		if (!leashes.PressKey(command.player, k_Keys.at(command.value)))
		{
			return RefusedText(leashes, command.player);
		}
		const auto after = leashes.TypeOf(creature);
		return after == LeashType::None ? "off" : before == after ? "same" : creature_leash::Name(after);
	}
	case Kind::TieLeash:
		if (const auto object = ObjectAt(command.object))
		{
			return leashes.TieTo(creature, *object) ? "tied" : "can't";
		}
		return "it is gone";
	case Kind::UntieLeash:
		leashes.UntieToHand(creature);
		return {};
	case Kind::TakeOffLeash:
		leashes.TakeOff(creature);
		return {};
	case Kind::ConfineToHome:
		leashes.ConfineToHome(creature, command.radius);
		return {};
	default:
		return {};
	}
}

std::string Runner::GivePlayerCommand(const Command& command)
{
	switch (command.kind)
	{
	case Kind::PressKey:
		if (!Locator::gameActionSystem::has_value())
		{
			return "no keys";
		}
		Locator::gameActionSystem::value().QueuePress(static_cast<input::BindableActionMap>(command.value));
		return {};
	case Kind::HoldSeed:
	{
		// As a bubble's seed comes into the hand, the way the Miracles window puts one there: at its base level
		if (!Locator::magicSystem::has_value())
		{
			return "no miracles";
		}
		const auto seed = Locator::magicSystem::value().GiveSeedToHand(
		    command.player, static_cast<SpellSeedType>(command.value), debug::miracles::k_BasePowerUpLevel, 1.0f);
		return seed == entt::null ? "the hand can't take it" : fmt::format("seed {} in the hand", static_cast<uint32_t>(seed));
	}
	case Kind::SummonSeed:
	{
		// As the miracle selection asks the player's worship for it: the best icon of the seed charges it from the
		// prayer power, and the seed comes into the hand once it is full
		auto* icons = Locator::magicSystem::has_value() ? magic::gestures::GetIconProvider() : nullptr;
		const auto seed = static_cast<int>(command.value);
		if (icons == nullptr)
		{
			return "the player has no worship";
		}
		if (!icons->IconValidForRequest(seed))
		{
			return "the player's worship can't give it";
		}
		icons->RequestSpell(seed);
		return "asked for";
	}
	case Kind::DrawGesture:
		return DrawGesture(command);
	case Kind::HandTakeFireBall:
		// The hand held over a fireball in flight comes with the hand's own testbed set up
		return NotYet(command.kind);
	default:
		return {};
	}
}

std::string Runner::DrawGesture(const Command& command)
{
	namespace gestures = magic::gestures;
	if (!Locator::inputState::has_value() || !Locator::windowing::has_value() || !Locator::magicSystem::has_value())
	{
		return "no mouse";
	}
	if (_drawing.has_value())
	{
		return "the hand is still drawing";
	}
	const auto gesture = static_cast<gestures::Gesture>(command.value);
	const auto name = debug::gestures_window::GestureName(gesture);
	const auto size = WindowSize();
	auto drawing = testbed_gesture::StartDrawing(testbed_gesture::StrokeOf(gestures::Templates(), gesture, size), gesture);
	if (!drawing.has_value())
	{
		return fmt::format("no template of {}", name);
	}
	if (!_pointer.IsTaken())
	{
		_pointer.Take();
	}
	KeepDebugWindowsOffTheMouse(true);
	// The stroke reaches the hand through the fixed cursor alone: no mouse motion goes out, so no drag of the camera can
	// wipe the gesture as it is drawn
	_pointer.MoveTo(drawing->pixels.front(), size);
	if (drawing->holdAction)
	{
		// The hand is given the button at the end of this frame's update, and the stroke starts the next frame
		_pointer.Press(testbed_pointer::ButtonOf(3), true);
	}
	_gestureCooldown = gestures::State().cooldown;
	auto line = fmt::format("{}, {} mouse messages{}", name, drawing->pixels.size(),
	                        drawing->holdAction ? " with the Action button held" : "");
	_drawing = std::move(drawing);
	return line;
}

void Runner::UpdateDrawing(float seconds)
{
	namespace gestures = magic::gestures;
	// The frame after a drawing ends is watched too: the gesture may be taken as the button is let go
	if ((!_drawing.has_value() && !_watchGestureOnce) || !Locator::magicSystem::has_value())
	{
		return;
	}
	const auto& state = gestures::State();
	if (testbed_gesture::TookAGesture(_gestureCooldown, state.cooldown))
	{
		// A sizing circle is kept as the circle; the last match may have been tried against other gestures since
		const auto taken = state.circlePending ? state.circleGesture : state.result.gesture;
		auto line = fmt::format("{:.2f}s: recognised {} (template {}{})", _seconds, debug::gestures_window::GestureName(taken),
		                        state.result.templateIndex, state.result.reversed ? ", mirrored" : "");
		if (state.circlePending)
		{
			line += fmt::format(": a circle at ({:.1f}, {:.1f}, {:.1f}), size {:.1f}", state.circlePosition.x,
			                    state.circlePosition.y, state.circlePosition.z, state.circleSize);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed {}", line);
		Log(line);
	}
	_gestureCooldown = state.cooldown;
	if (!_drawing.has_value())
	{
		_watchGestureOnce = false;
		return;
	}
	const auto step = testbed_gesture::Step(*_drawing, seconds, gestures::sampling::PlayingStroke());
	if (step.startStroke)
	{
		gestures::sampling::PlayStroke(_drawing->pixels);
	}
	if (step.pointer.has_value())
	{
		_pointer.MoveTo(*step.pointer, WindowSize());
	}
	if (step.releaseAction)
	{
		_pointer.Press(testbed_pointer::ButtonOf(3), false);
	}
	if (step.done)
	{
		const auto line = fmt::format("{:.2f}s: drawn {}", _seconds, debug::gestures_window::GestureName(_drawing->gesture));
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed {}", line);
		Log(line);
		_drawing.reset();
		_watchGestureOnce = true;
	}
}

void Runner::StopDrawing()
{
	if (!_drawing.has_value())
	{
		return;
	}
	_drawing.reset();
	_watchGestureOnce = false;
	// An empty stroke ends the one playing
	if (Locator::magicSystem::has_value() && magic::gestures::sampling::PlayingStroke())
	{
		magic::gestures::sampling::PlayStroke({});
	}
}

std::string Runner::GiveFightCommand(entt::entity creature, const Command& command)
{
	if (!Locator::creatureFightSystem::has_value())
	{
		return "no fights";
	}
	auto& fights = Locator::creatureFightSystem::value();
	switch (command.kind)
	{
	case Kind::StartFight:
		if (const auto opponent = CreatureAt(command.value))
		{
			constexpr std::array<std::string_view, 4> k_Results {"started", "no opponent", "busy", "too weak"};
			return std::string(k_Results.at(static_cast<size_t>(fights.StartFight(creature, *opponent))));
		}
		return "it is gone";
	case Kind::FightBlow:
	{
		constexpr std::array k_Bands {creature_fight::Band::High, creature_fight::Band::Mid, creature_fight::Band::Low};
		// As a click held for the charge, then let go
		const bool queued = fights.QueueMove(creature, creature_fight::AttackMove(k_Bands.at(command.value)), true);
		fights.ReleaseCharge(creature, command.chargeMs);
		return queued ? fmt::format("{:.0f} ms", command.chargeMs) : "not fighting";
	}
	case Kind::FightBlock:
		return fights.QueueMove(creature, creature_fight::BlockMove(), true) ? "" : "not fighting";
	case Kind::FightStep:
		return fights.QueueMove(creature, creature_fight::StepMove(static_cast<creature_fight::Step>(command.value)), true)
		           ? ""
		           : "not fighting";
	case Kind::FightSpecial:
		return fights.QueueMove(creature, {.kind = creature_fight::Move::Kind::Special}, true) ? "" : "not fighting";
	case Kind::FightAuto:
		fights.SetAutoFighting(creature, command.value != 0);
		return fights.IsFighting(creature) ? "" : "not fighting";
	case Kind::KnockOut:
		fights.KnockOut(creature);
		return {};
	case Kind::BringRound:
		fights.Resurrect(creature);
		return {};
	case Kind::TieLeashToCreature:
		if (const auto other = CreatureAt(command.value); other.has_value() && Locator::leashSystem::has_value())
		{
			return Locator::leashSystem::value().TieTo(creature, *other) ? "tied" : "can't";
		}
		return "it is gone";
	default:
		return {};
	}
}

bool Runner::IsFree(size_t creature) const
{
	const auto entity = CreatureAt(creature);
	if (!entity.has_value())
	{
		// A creature that is gone holds nothing up
		return true;
	}
	if (Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(*entity))
	{
		return false;
	}
	const auto* animation = Locator::entitiesRegistry::value().TryGet<const CreatureAnimation>(*entity);
	return animation == nullptr || !creature_layers::IsPlaying(animation->body);
}

void Runner::ChangeLand(std::string_view landScript)
{
	Log(fmt::format("{:.1f}s: {} {}", _seconds, Name(Kind::ChangeLand), landScript));
	// The testbed goes with the land, so the scenario ends here, as when another land is loaded over it
	_running = false;
	_shot.reset();
	RestoreSettings();
	const bool loaded = _host.ChangeLand(landScript);
	Log(loaded ? fmt::format("Changed to the land of {}", landScript)
	           : fmt::format("Problem: the land of {} could not be loaded", landScript));
}

void Runner::Give(const Command& command)
{
	if (IsPlayerCommand(command.kind))
	{
		const auto result = GivePlayerCommand(command);
		Log(fmt::format("{:.1f}s: {}{}{}", _seconds, Name(command.kind), result.empty() ? "" : ": ", result));
		return;
	}
	if (IsPointerCommand(command.kind))
	{
		const auto result = GivePointerCommand(command);
		const auto line = fmt::format("{:.2f}s: {}: {}", _seconds, Name(command.kind), result);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed {}", line);
		Log(line);
		return;
	}
	if (command.kind == Kind::SetHour)
	{
		SetHour(command.hour);
		Log(fmt::format("{:.1f}s: the hour is {:.1f}", _seconds, command.hour));
		return;
	}
	if (command.kind == Kind::SetAlignment)
	{
		if (Locator::alignmentSystem::has_value())
		{
			Locator::alignmentSystem::value().SetPlayerAlignment(command.player, command.alignment);
		}
		Log(fmt::format("{:.1f}s: the alignment is {:.2f}", _seconds, command.alignment));
		return;
	}
	if (command.kind == Kind::ChangeLand)
	{
		ChangeLand(command.landScript);
		return;
	}
	const auto entity = CreatureAt(command.creature);
	if (!entity.has_value() || !Locator::creatureLocomotionSystem::has_value() || !Locator::creatureMindSystem::has_value())
	{
		return;
	}
	// a creature fixture, numbered after the scenario's own creatures, has no label
	const auto who = command.creature < _scenario->creatures.size() && !_scenario->creatures[command.creature].label.empty()
	                     ? std::string(_scenario->creatures[command.creature].label)
	                     : fmt::format("creature {}", command.creature);
	// Lying out cold, it does nothing it is told until it comes round
	if (const auto* needs = Locator::entitiesRegistry::value().TryGet<const CreatureNeeds>(*entity);
	    needs != nullptr && needs->rest == CreatureNeeds::Rest::Unconscious && command.kind != Kind::BringRound)
	{
		Log(fmt::format("{:.1f}s: {} {}: out cold", _seconds, who, Name(command.kind)));
		return;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	auto& minds = Locator::creatureMindSystem::value();
	const auto point = MapPoint(_middle, command.point);
	std::string result;
	switch (command.kind)
	{
	case Kind::WalkTo:
	case Kind::RunTo:
		result =
		    MoveResultName(locomotion.MoveTo(*entity, point, command.kind == Kind::RunTo ? Pace::Run : Pace::Walk, 0.0f, 1.0f));
		break;
	case Kind::Follow:
		if (const auto leader = CreatureAt(command.value))
		{
			const auto size = Locator::entitiesRegistry::value().Get<Creature>(*entity).size;
			result = MoveResultName(locomotion.Follow(*entity, *leader, k_FollowDistance * std::max(size, 0.5f), Pace::Walk));
		}
		break;
	case Kind::FleeFrom:
		result = MoveResultName(locomotion.FleeFrom(*entity, point));
		break;
	case Kind::TurnToFace:
		result = locomotion.TurnToFace(*entity, point) ? "turning" : "can't";
		break;
	case Kind::FaceCamera:
		result = locomotion.TurnToFace(*entity, glm::xz(Locator::camera::value().GetOrigin())) ? "turning" : "can't";
		break;
	case Kind::Stop:
		locomotion.Stop(*entity);
		break;
	case Kind::PlayAction:
		result = minds.PlayAction(*entity, command.value) ? "playing" : "busy";
		break;
	case Kind::PlayGesture:
		result = minds.PlayGesture(*entity, command.value) ? "playing" : "busy";
		break;
	case Kind::PullFace:
		minds.PullFace(*entity, command.value);
		break;
	case Kind::ShowFeeling:
		if (const auto face = minds.ShowFeeling(*entity, static_cast<creature_face::Cue>(command.value)))
		{
			result = creature_face::Name(face->face);
		}
		break;
	case Kind::SitDown:
		result = minds.SitDown(*entity) ? "sitting" : "busy";
		break;
	case Kind::StandUp:
		minds.StandUp(*entity);
		break;
	case Kind::Sleep:
		result = minds.Sleep(*entity) ? "started" : "can't";
		break;
	case Kind::Wake:
		minds.Wake(*entity);
		break;
	case Kind::Eat:
		result = minds.Eat(*entity, std::nullopt) ? "started" : "nothing to eat";
		break;
	case Kind::Drink:
		result = minds.Drink(*entity) ? "started" : "no water near";
		break;
	case Kind::Poo:
		result = minds.Poo(*entity) ? "started" : "can't";
		break;
	case Kind::Puke:
		result = minds.Puke(*entity) ? "started" : "can't";
		break;
	case Kind::Faint:
		result = minds.Faint(*entity) ? "started" : "can't";
		break;
	case Kind::Stroke:
	case Kind::Slap:
		// As a whole session of the hand on the creature would, a full reward or punishment as the hand lets go
		minds.ReceiveFeedback(*entity, command.kind == Kind::Stroke ? 1.0f : -1.0f);
		break;
	case Kind::PickUp:
	case Kind::PutDown:
	case Kind::Discard:
	case Kind::Lob:
	case Kind::EatHeld:
	case Kind::Examine:
	case Kind::ThrowAt:
	case Kind::KnockDown:
	case Kind::PointAt:
		result = GiveObjectCommand(*entity, command);
		break;
	case Kind::HandStroke:
	case Kind::HandSlap:
	case Kind::HandLetGo:
		// The player's hand held to a creature by a command comes with the hand's own testbed set up
		result = NotYet(command.kind);
		break;
	case Kind::PutOnLeash:
	case Kind::TieLeash:
	case Kind::UntieLeash:
	case Kind::TakeOffLeash:
	case Kind::ConfineToHome:
	case Kind::MakeLeashable:
	case Kind::HandTapLeash:
	case Kind::LeashKey:
	case Kind::LeashShake:
		result = GiveLeashCommand(*entity, command);
		break;
	case Kind::StartFight:
	case Kind::FightBlow:
	case Kind::FightBlock:
	case Kind::FightStep:
	case Kind::FightSpecial:
	case Kind::FightAuto:
	case Kind::KnockOut:
	case Kind::BringRound:
	case Kind::TieLeashToCreature:
		result = GiveFightCommand(*entity, command);
		break;
	case Kind::CreatureKey:
	case Kind::DoubleClick:
	case Kind::CameraKeys:
	case Kind::ClearCameraView:
	case Kind::LeaveCreatureMode:
	case Kind::OpenCreatureCave:
	case Kind::ApplyTattoo:
	case Kind::RemoveTattoo:
		result = GiveCreatureModeCommand(*entity, command);
		break;
	case Kind::SetHour:
	case Kind::HoldSeed:
	case Kind::DrawGesture:
	case Kind::SummonSeed:
	case Kind::PressKey:
	case Kind::HandTakeFireBall:
	case Kind::SetAlignment:
	case Kind::ChangeLand:
	// The mouse commands are given before a creature is looked for
	case Kind::PointerTo:
	case Kind::PointerPress:
	case Kind::PointerRelease:
	case Kind::PointerSweep:
	case Kind::WheelTurn:
		break;
	case Kind::SetDesire:
	case Kind::SetPhase:
	case Kind::RewardIf:
		result = TeachMind(*entity, command);
		break;
	case Kind::SeeSkill:
		minds.SeeSkill(Locator::entitiesRegistry::value().Get<Transform>(*entity).position, command.value);
		break;
	case Kind::SeeMiracle:
		minds.SeeMiracle(Locator::entitiesRegistry::value().Get<Transform>(*entity).position, command.value);
		break;
	case Kind::KnowMiracle:
		minds.KnowMiracle(*entity, command.value);
		break;
	case Kind::CastMiracle:
	{
		const auto target = command.atCreature.has_value() ? CreatureAt(*command.atCreature) : ObjectAt(command.object);
		result = !target.has_value()                                                       ? "nothing to cast at"
		         : minds.TellCast(*entity, static_cast<MagicType>(command.value), *target) ? ""
		                                                                                   : "can't";
		break;
	}
	case Kind::PlayerDid:
	{
		const auto& land = Locator::terrainSystem::value();
		minds.PlayerDid(command.player, command.value, glm::vec3(point.x, land.GetHeightAt(point), point.y), std::nullopt);
		break;
	}
	}
	Log(fmt::format("{:.1f}s: {} {}{}{}", _seconds, who, Name(command.kind), result.empty() ? "" : ": ", result));
}

std::string Runner::GiveCreatureModeCommand([[maybe_unused]] entt::entity creature, const Command& command)
{
	if (!Locator::creatureModeSystem::has_value() || !Locator::creatureCaveSystem::has_value())
	{
		return "no creature mode";
	}
	auto& mode = Locator::creatureModeSystem::value();
	auto& cave = Locator::creatureCaveSystem::value();
	const auto following = [&mode] {
		const auto followed = mode.GetCreature();
		return followed.has_value() ? fmt::format("following entity {}", static_cast<uint32_t>(*followed))
		                            : std::string("the camera is the player's");
	};
	switch (command.kind)
	{
	case Kind::CreatureKey:
		mode.PressCreatureKey();
		return following();
	case Kind::DoubleClick:
	case Kind::CameraKeys:
		// The double click and the held keys made by a command come with Creature Mode's own testbed set up
		return NotYet(command.kind);
	case Kind::ClearCameraView:
		mode.ClearView();
		if (const auto view = mode.GetView(); view.has_value())
		{
			return fmt::format("heading {:.2f}, pitch {:.2f}", view->yaw, view->pitch);
		}
		return "not in Creature Mode";
	case Kind::LeaveCreatureMode:
		mode.Leave();
		return following();
	case Kind::OpenCreatureCave:
		cave.Open();
		cave.GetScreen().page = static_cast<creature_cave::Page>(command.value);
		cave.GetScreen().pageRequested = true;
		return cave.GetCreature().has_value() ? "" : "the player has no creature";
	case Kind::ApplyTattoo:
		return cave.ApplyTattoo(static_cast<uint8_t>(command.bodyPart), static_cast<uint8_t>(command.value),
		                        glm::u8vec3(180, 30, 30))
		           ? ""
		           : "can't";
	case Kind::RemoveTattoo:
		return cave.RemoveTattoo(static_cast<uint8_t>(command.bodyPart)) ? "" : "nothing there";
	default:
		return {};
	}
}

std::string Runner::TeachMind(entt::entity entity, const Command& command)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(entity);
	if (mind == nullptr)
	{
		return "no mind";
	}
	switch (command.kind)
	{
	case Kind::SetDesire:
		if (mind->desires.has_value() && command.value < creature_desires::k_DesireCount)
		{
			auto& state = mind->desires->desires.at(command.value);
			state.activated = true;
			state.suppressedTurns = 0;
			state.value = command.amount * std::max(state.max, 0.0f);
			return fmt::format("{} {:.2f}", creature_desires::Name(static_cast<creature_desires::Desire>(command.value)),
			                   state.value);
		}
		return "no desires yet";
	case Kind::SetPhase:
		mind->developmentPhase = static_cast<uint32_t>(command.value);
		return fmt::format("stage {}", command.value);
	case Kind::RewardIf:
		// From now on each thing it does to something is judged as soon as it is done
		mind->trainer = static_cast<uint32_t>(command.value);
		return fmt::format("trained: stroked for a {}, slapped for anything else",
		                   creature_tree::BeliefName(static_cast<uint32_t>(command.value)));
	default:
		return {};
	}
}

void Runner::LoadMindFile(entt::entity entity, std::string_view name)
{
	constexpr std::string_view k_Game = "game:";
	// The file last opened with the spawner's file dialog isn't kept in this tree, so it is always the game's own mind
	// of that name, which it falls back to
	constexpr std::string_view k_Chosen = "chosen:";
	std::string file;
	if (name.starts_with(k_Game))
	{
		file = name.substr(k_Game.size());
	}
	else if (name.starts_with(k_Chosen))
	{
		file = name.substr(k_Chosen.size());
	}
	if (file.empty() || !Locator::filesystem::has_value() || !Locator::resources::has_value())
	{
		Log(fmt::format("mind file {}: {}", name, creaturemind::ResultToStr(creaturemind::MindResult::ErrCantOpen)));
		return;
	}
	// Through the resource cache, by the same name the spawner's list loads it with
	const auto path = Locator::filesystem::value().GetPath<filesystem::Path::CreatureMind>(true) / file;
	auto& minds = Locator::resources::value().GetCreatureMinds();
	const auto id = minds.Load(file, resources::CreatureMindLoader::FromDiskTag {}, path).first->first;
	const auto handle = minds.Handle(id);
	const auto result = handle ? handle->result : creaturemind::MindResult::ErrCantOpen;
	if (handle && handle->Loaded() && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().LoadMind(entity, std::make_shared<const creaturemind::MindFileData>(handle->data));
	}
	Log(fmt::format("mind file {}: {}", file, creaturemind::ResultToStr(result)));
}

void Runner::Frame(Shot shot, size_t creature, float distance)
{
	_shot = shot;
	_shotCreature = creature;
	_shotDistance = distance;
	UpdateCamera();
}

void Runner::UpdateCamera()
{
	if (!_shot.has_value() || _scenario == nullptr || !Locator::terrainSystem::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	// Creature Mode takes the camera from the scenario's shot for good
	if (Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().IsActive())
	{
		_shot.reset();
		return;
	}
	auto& camera = Locator::camera::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto& land = Locator::terrainSystem::value();
	std::optional<CameraPlacement> placement;
	switch (*_shot)
	{
	case Shot::Testbed:
		// As the testbed leaves it: 120 units south of the middle and 60 up, looking at the middle
		placement = CameraPlacement {
		    .origin = {_middle.x, land.GetHeightAt(_middle) + 60.0f, _middle.y - 120.0f},
		    .focus = {_middle.x, land.GetHeightAt(_middle), _middle.y},
		};
		break;
	case Shot::Overview:
	{
		std::vector<glm::vec2> points;
		// the scenario's own creatures only: a creature fixture is framed through the shot's points
		for (size_t i = 0; i < std::min(_creatures.size(), _scenario->creatures.size()); ++i)
		{
			if (const auto entity = CreatureAt(i))
			{
				points.push_back(glm::xz(registry.Get<Transform>(*entity).position));
			}
		}
		for (const auto& object : _scenario->objects)
		{
			points.push_back(MapPoint(_middle, object.offset));
		}
		for (const auto& particle : _scenario->particles)
		{
			points.push_back(MapPoint(_middle, particle.offset));
		}
		for (const auto& extra : _scenario->framing.include)
		{
			points.push_back(MapPoint(_middle, extra));
		}
		// And everywhere the creatures are sent
		for (const auto& command : _scenario->commands)
		{
			if (HasPoint(command.kind))
			{
				points.push_back(MapPoint(_middle, command.point));
			}
		}
		const auto bounds = BoundsOf(points, glm::vec2(k_MinOverviewHalfSize));
		placement = Overview({bounds.centre.x, land.GetHeightAt(bounds.centre), bounds.centre.y}, bounds.halfSize,
		                     FieldsOfView(camera), _shotDistance);
		break;
	}
	case Shot::Placed:
	{
		const auto eye = MapPoint(_middle, {_scenario->framing.eye.x, _scenario->framing.eye.z});
		const auto look = MapPoint(_middle, {_scenario->framing.look.x, _scenario->framing.look.z});
		placement = CameraPlacement {
		    .origin = {eye.x, land.GetHeightAt(eye) + _scenario->framing.eye.y, eye.y},
		    .focus = {look.x, land.GetHeightAt(look) + _scenario->framing.look.y, look.y},
		};
		break;
	}
	case Shot::Follow:
	case Shot::Head:
		if (const auto entity = CreatureAt(_shotCreature))
		{
			const auto& transform = registry.Get<Transform>(*entity);
			const auto height = CreatureHeight(registry.Get<Creature>(*entity).size);
			placement = *_shot == Shot::Follow ? Follow(transform.position, height, _shotDistance)
			                                   : Head(transform.position, AheadOf(transform), height, _shotDistance);
		}
		break;
	}
	if (placement.has_value())
	{
		camera.SetOrigin(placement->origin).SetFocus(placement->focus);
	}
	// A shot of everything is taken once; following and close ups keep up with the creature
	if (*_shot == Shot::Overview || *_shot == Shot::Testbed || *_shot == Shot::Placed)
	{
		_shot.reset();
	}
}

void Runner::Update(float seconds)
{
	if (_scenario == nullptr)
	{
		return;
	}
	UpdateCamera();
	if (!_running)
	{
		return;
	}
	// Another land was loaded over the testbed: a new land, or the scenario's creatures or crowd gone with the registry
	const auto gone = [](entt::entity entity) { return !Locator::entitiesRegistry::value().Valid(entity); };
	const auto* land = Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
	if (land != _land ||
	    (!_creatures.empty() && std::ranges::none_of(std::views::iota(size_t {0}, _creatures.size()),
	                                                 [this](size_t i) { return CreatureAt(i).has_value(); })) ||
	    (!_crowdEntities.empty() && gone(_crowdEntities.front())))
	{
		_running = false;
		_shot.reset();
		RestoreSettings();
		Log("Its creatures are gone");
		return;
	}
	_seconds += seconds;
	StartHandDemo();
	Measure();
	SpawnCrowd();
	UpdateParticles(seconds);
	ApplyStates();
	UpdatePointer(seconds);
	UpdateDrawing(seconds);
	const auto due = Advance(_timeline, _scenario->commands, _scenario->repeatFrom, seconds, [this](const Command& command) {
		// A command to the hand waits for the gesture it is drawing
		if (command.kind == Kind::HoldSeed || command.kind == Kind::DrawGesture || command.kind == Kind::SummonSeed)
		{
			return !_drawing.has_value();
		}
		return IsFree(command.creature);
	});
	for (const auto index : due)
	{
		// A land change ends the scenario, and the commands due after it with it
		if (!_running)
		{
			break;
		}
		Give(_scenario->commands[index]);
	}
	// The buttons held reach the hand this frame: this runs after the game has copied the mouse's own buttons into
	// what the hand reads, and before the hand reads them
	_pointer.WriteButtons();
}

uint32_t Runner::StartParticle(size_t index) const
{
	if (!Locator::particleSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return ecs::systems::ParticleSystemInterface::k_NoEffect;
	}
	const auto& particle = _scenario->particles.at(index);
	const auto point = MapPoint(_middle, particle.offset);
	const glm::vec3 position {point.x, Locator::terrainSystem::value().GetHeightAt(point) + particle.height, point.y};
	auto& effects = Locator::particleSystem::value();
	// A type plays its own particle file, as the debug window's spawner starts one
	const auto file = particle.file.empty() ? particles::ParticleTypeFile(particle.type) : particle.file;
	if (file.empty())
	{
		return ecs::systems::ParticleSystemInterface::k_NoEffect;
	}
	const auto effect = effects.Start(file, position, particle.magnitude);
	if (effect == ecs::systems::ParticleSystemInterface::k_NoEffect)
	{
		return effect;
	}
	effects.SetDrawPath(effect, particle.path);
	if (particle.targetsCreatures)
	{
		for (const auto creature : _creatures)
		{
			effects.AddTarget(effect, creature);
		}
	}
	return effect;
}

void Runner::UpdateParticles(float seconds)
{
	if (!Locator::particleSystem::has_value())
	{
		return;
	}
	auto& effects = Locator::particleSystem::value();
	for (size_t i = 0; i < _particles.size(); ++i)
	{
		auto& running = _particles.at(i);
		const auto restart = _scenario->particles.at(i).restartSeconds;
		running.seconds += seconds;
		// An effect that has ended, or whose time is up, starts again
		if (effects.Find(running.effect) == nullptr || (restart > 0.0f && running.seconds >= restart))
		{
			effects.CloseDown(running.effect);
			running = {StartParticle(i), 0.0f};
		}
	}
}

std::string Runner::GivePointerCommand(const Command& command)
{
	if (!Locator::inputState::has_value() || !Locator::windowing::has_value())
	{
		return "no mouse";
	}
	const auto size = WindowSize();
	if (!_pointer.IsTaken())
	{
		// The pointer holds still where the cursor is from now on, until the scenario moves it
		_pointer.Take();
		_pointer.MoveTo(_pointer.GetPosition(), size);
	}
	KeepDebugWindowsOffTheMouse(true);
	// The camera's drags and the wheel come as the mouse's own events, which are dropped in the test runs
	const std::string_view dropped = MouseEventsDropped() ? " (the mouse's events are dropped here)" : "";
	switch (command.kind)
	{
	case Kind::PointerTo:
	{
		const auto to = testbed_pointer::PixelAtShare(command.point, size);
		const auto moved = to - _pointer.GetPosition();
		_pointer.MoveTo(to, size);
		PushMotion(to, _pointer.ButtonMask(), moved);
		break;
	}
	case Kind::PointerPress:
	case Kind::PointerRelease:
		// The hand is given the buttons at the end of this frame's update
		_pointer.Press(testbed_pointer::ButtonOf(command.value), command.kind == Kind::PointerPress);
		_handWatchSeconds = k_HandWatchSeconds;
		break;
	case Kind::PointerSweep:
		_sweep = testbed_pointer::StartSweep(command.point, command.amount, size);
		return HandOnScreen() + std::string(dropped);
	case Kind::WheelTurn:
	{
		SDL_Event event {};
		event.type = SDL_MOUSEWHEEL;
		event.wheel.windowID = Locator::windowing::value().GetID();
		event.wheel.y = static_cast<int32_t>(command.value) * (command.ctrl ? -1 : 1);
		event.wheel.preciseY = static_cast<float>(event.wheel.y);
		SDL_PushEvent(&event);
		return HandOnScreen() + std::string(dropped);
	}
	default:
		break;
	}
	return HandOnScreen();
}

void Runner::UpdatePointer(float seconds)
{
	if (!_pointer.IsTaken())
	{
		return;
	}
	// Once the commands are done, the buttons let go and the gesture drawn, the mouse is the player's again
	if (!_holdPointer && !_pointer.AnyHeld() && !_sweep.has_value() && !_drawing.has_value() && _timeline.done &&
	    _handWatchSeconds <= 0.0f)
	{
		ReleasePointer();
		return;
	}
	if (_handWatchSeconds > 0.0f || _sweep.has_value())
	{
		_handWatchSeconds -= seconds;
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Testbed {:.3f}s: {}", _seconds, HandOnScreen());
	}
	if (_sweep.has_value())
	{
		const auto size = WindowSize();
		const auto step = testbed_pointer::Advance(*_sweep, seconds);
		auto position = _pointer.GetPosition();
		// While the camera turns with the mouse its pointer is held, and only the movement comes through
		const bool frozen = Locator::gameActionSystem::has_value() && Locator::gameActionSystem::value().IsCursorFrozen();
		if (!frozen)
		{
			position = testbed_pointer::ClampToWindow(position + step.moved, size);
		}
		_pointer.MoveTo(position, size);
		PushMotion(position, _pointer.ButtonMask(), step.moved);
		if (step.done)
		{
			_sweep.reset();
			const auto line = fmt::format("{:.2f}s: moved: {}", _seconds, HandOnScreen());
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed {}", line);
			Log(line);
		}
	}
}

void Runner::ReleasePointer()
{
	StopDrawing();
	_sweep.reset();
	_handWatchSeconds = 0.0f;
	_holdPointer = false;
	if (_pointer.IsTaken() && Locator::inputState::has_value())
	{
		_pointer.Release();
		KeepDebugWindowsOffTheMouse(false);
	}
}

std::string Runner::HandOnScreen() const
{
	if (!Locator::handSystem::has_value() || !Locator::camera::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return {};
	}
	const auto cursor = input::GameCursor();
	const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(hand);
	if (transform == nullptr)
	{
		return {};
	}
	const auto& position = transform->position;
	const auto size = glm::vec2(WindowSize());
	const auto& camera = Locator::camera::value();
	glm::vec3 screen {0.0f};
	camera.ProjectWorldToScreen(position, {0.0f, 0.0f, size.x, size.y}, screen);
	const auto cues = camera.GetModel().GetHandCues();
	constexpr std::array<std::string_view, 4> k_DragModes {"pan", "edge rotate", "pitch", "pitch from the top"};
	const auto drag = !cues.dragging              ? std::string_view("none")
	                  : cues.dragMode.has_value() ? k_DragModes.at(static_cast<size_t>(*cues.dragMode))
	                                              : std::string_view("undecided");
	const bool held = Locator::gameActionSystem::has_value() && Locator::gameActionSystem::value().IsCursorFrozen();
	return fmt::format("cursor ({}, {}), hand ({:.0f}, {:.0f}) at ({:.1f}, {:.1f}, {:.1f}), {:.1f} from the camera, scale "
	                   "{:.3f}, hints {:#x}, drag {}, camera heading {:.3f} pitch {:.3f}{}",
	                   cursor.x, cursor.y, screen.x, screen.y, position.x, position.y, position.z,
	                   glm::distance(position, camera.GetOrigin()), transform->scale.y, cues.tricons, drag,
	                   camera.GetRotation().y, camera.GetRotation().x, held ? ", held" : "");
}

void Runner::Log(std::string line)
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed: {}", line);
	_log.push_back(std::move(line));
	while (_log.size() > k_LogLines)
	{
		_log.pop_front();
	}
}

std::optional<CrowdProgress> Runner::GetCrowdProgress() const
{
	if (_scenario == nullptr || !_scenario->crowd.has_value())
	{
		return std::nullopt;
	}
	return _crowdProgress;
}

std::span<const benchmark::StageInfo> Runner::GetStages() const
{
	if (_recorder == nullptr)
	{
		return {};
	}
	return _recorder->Stages();
}

uint32_t Runner::GetWarmUpLeft() const
{
	return _settledFrames >= _benchmark.warmUpFrames ? 0 : _benchmark.warmUpFrames - _settledFrames;
}

void Runner::SpawnCrowd()
{
	if (_scenario == nullptr || !_scenario->crowd.has_value() || _crowdProgress.Done() ||
	    !Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto start = std::chrono::steady_clock::now();
	const auto batch = std::min(_scenario->crowd->perFrame, _crowdProgress.total - _crowdNext);
	for (size_t i = 0; i < batch; ++i)
	{
		SpawnCrowdMember(_crowdNext++);
	}
	_crowdProgress.spawned = _crowdNext;
	_crowdProgress.spawnMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	++_crowdProgress.spawnFrames;
	if (_crowdProgress.Done())
	{
		Log(fmt::format("Spawned {} in {:.0f} ms over {} frames", _crowdProgress.total, _crowdProgress.spawnMs,
		                _crowdProgress.spawnFrames));
	}
}

void Runner::SpawnCrowdMember(size_t index)
{
	const auto& land = Locator::terrainSystem::value();
	const auto onLand = [&land, this](glm::vec2 offset) {
		const auto point = MapPoint(_middle, offset);
		return glm::vec3(point.x, land.GetHeightAt(point), point.y);
	};
	if (!_crowdCreatures.empty())
	{
		const auto& member = _crowdCreatures.at(index);
		_crowdEntities.push_back(CreatureArchetype::Create(onLand(member.offset), member.owner, member.species, 0,
		                                                   glm::radians(member.facingDegrees), k_DefaultSize,
		                                                   CreatureArchetype::StartBody(member.species)));
		return;
	}
	// The towns first, then their homes and stores, then the villagers who live in them
	const auto towns = _village.towns.size();
	const auto abodes = _village.abodes.size();
	if (index < towns)
	{
		const auto& town = _village.towns.at(index);
		_crowdEntities.push_back(
		    ecs::archetypes::TownArchetype::Create(static_cast<int>(index), onLand(town.offset), town.owner, town.tribe));
		return;
	}
	if (index < towns + abodes)
	{
		const auto& abode = _village.abodes.at(index - towns);
		_crowdAbodes.push_back(ecs::archetypes::AbodeArchetype::Create(static_cast<uint32_t>(abode.town), onLand(abode.offset),
		                                                               abode.type, glm::radians(abode.yawDegrees), 1.0f,
		                                                               k_CrowdFood, k_CrowdWood));
		return;
	}
	const auto& member = _village.villagers.at(index - towns - abodes);
	const auto& home = _village.abodes.at(member.abode);
	const auto entity =
	    ecs::archetypes::VillagerArchetype::Create(onLand(home.offset), onLand(member.offset), member.type, member.age);
	_crowdEntities.push_back(entity);
	// It lives in the home laid out for it, rather than the first in its town with room; the home's town becomes its
	// town
	auto& registry = Locator::entitiesRegistry::value();
	const auto abode = _crowdAbodes.at(member.abode);
	if (abode != entt::null && registry.Valid(abode))
	{
		ecs::abode_villagers::AddVillagerToAbode(abode, entity);
	}
}

void Runner::Measure()
{
	if (_recorder == nullptr || !_crowdProgress.Done() || !Locator::profiler::has_value())
	{
		return;
	}
	if (_settledFrames < _benchmark.warmUpFrames)
	{
		++_settledFrames;
		return;
	}
	// The last frame, whole: it ended as this one started
	const auto& profiler = Locator::profiler::value();
	const auto& entry = profiler.GetEntries().at(profiler.GetEntryIndex(-1));
	const auto frame = entry.frameEnd - entry.frameStart;
	const auto& draw = entry.stages.at(static_cast<size_t>(Profiler::Stage::SceneDraw));
	const bool drawn = draw.finalized && draw.start >= entry.frameStart && draw.start <= entry.frameEnd;
	const auto update = drawn ? draw.start - entry.frameStart : frame;
	std::array<float, static_cast<size_t>(Profiler::Stage::_count)> stages {};
	for (size_t i = 0; i < stages.size(); ++i)
	{
		stages.at(i) = static_cast<float>(StageMilliseconds(entry, entry.stages.at(i)));
	}
	// What the renderer last drew, the frame before
	const auto* renderStats = bgfx::getStats();
	_recorder->Add(static_cast<float>(Milliseconds(frame)), static_cast<float>(Milliseconds(update)),
	               static_cast<float>(Milliseconds(frame - update)), stages,
	               renderStats != nullptr ? static_cast<float>(renderStats->numDraw) : 0.0f);
	if (++_framesSinceSummary >= k_FramesPerSummary || _recorder->Count() == _recorder->Capacity())
	{
		_framesSinceSummary = 0;
		_liveResults = _recorder->Summarise();
	}
	if (!_measured && _recorder->Count() >= _recorder->Capacity())
	{
		_measured = true;
		_liveResults = _recorder->Summarise();
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Benchmark {}: frame mean {:.2f} ms p95 {:.2f} max {:.2f}", _scenario->id,
		                   _liveResults.frame.average, _liveResults.frame.p95, _liveResults.frame.max);
		if (_benchmark.resultsBase.has_value())
		{
			const auto written = SaveResults();
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Benchmark {}: results {}", _scenario->id,
			                   written.has_value() ? written->generic_string() : "not written");
		}
		if (_benchmark.quitWhenMeasured)
		{
			_host.RequestQuit();
		}
	}
}

std::vector<std::pair<std::string, size_t>> Runner::EntityCounts() const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return {};
	}
	auto& registry = Locator::entitiesRegistry::value();
	return {
	    {"placed", registry.Size<Transform>()},
	    {"creatures", registry.Size<Creature>()},
	    {"villagers", registry.Size<ecs::components::Villager>()},
	    {"abodes", registry.Size<ecs::components::Abode>()},
	    {"towns", registry.Size<ecs::components::Town>()},
	};
}

// Tool output, not game data: written only when the command line gave --benchmark-out, to that path alone, so a game
// run without the flag never writes a file here
std::optional<std::filesystem::path> Runner::SaveResults()
{
	if (!_benchmark.resultsBase.has_value() || _scenario == nullptr || _recorder == nullptr || _recorder->Count() == 0)
	{
		return std::nullopt;
	}
	benchmark::RunInfo run {
	    .scenarioId = std::string(_scenario->id),
	    .scenarioName = std::string(_scenario->name),
	    .build = std::string(k_Build),
	    .crowd = _crowdProgress.total,
	    .spawnMs = _crowdProgress.spawnMs,
	    .spawnFrames = _crowdProgress.spawnFrames,
	    .warmUpFrames = _benchmark.warmUpFrames,
	    .entityCounts = EntityCounts(),
	};
	if (Locator::windowing::has_value())
	{
		const auto size = Locator::windowing::value().GetSize();
		run.width = static_cast<uint32_t>(size.x);
		run.height = static_cast<uint32_t>(size.y);
	}
	const auto& base = *_benchmark.resultsBase;
	std::error_code error;
	if (base.has_parent_path())
	{
		std::filesystem::create_directories(base.parent_path(), error);
	}
	const auto files = benchmark::ResultPaths(base);
	std::ofstream json(files.json, std::ios::binary);
	std::ofstream csv(files.csv, std::ios::binary);
	if (!json || !csv)
	{
		Log(fmt::format("Couldn't write {}", files.json.generic_string()));
		return std::nullopt;
	}
	const auto results = _recorder->Summarise();
	json << benchmark::ToJson(run, results, _recorder->Stages());
	csv << benchmark::ToCsv(run, results, _recorder->Stages());
	Log(fmt::format("Saved {}", files.json.generic_string()));
	return files.json;
}
