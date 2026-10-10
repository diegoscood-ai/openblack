/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedScenarios.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

#include <fmt/format.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include "3D/FlatLand.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"
#include "CreatureSpawnerModel.h"
#include "DebugGuiInterface.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/DispenserRules.h"
#include "MagicModel.h"
#include "TestbedScenariosModel.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::testbed_scenarios;
using openblack::debug::creature_spawner::SpeciesName;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureLocomotion;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::CreatureNeeds;

namespace
{
/// The game's speeds to pick from: how many times longer a turn takes, and what to call it
struct Speed
{
	float multiplier;
	std::string_view name;
};
constexpr std::array<Speed, 4> k_Speeds {{{2.0f, "x0.5"}, {1.0f, "x1"}, {0.5f, "x2"}, {0.25f, "x4"}}};
/// The speeds the creatures' body time can run at
constexpr std::array<float, 5> k_BodyTimes {1.0f, 10.0f, 100.0f, 1000.0f, 3600.0f};

const ImVec4 k_RunColour {0.25f, 0.60f, 0.30f, 1.0f};
const ImVec4 k_StopColour {0.85f, 0.30f, 0.25f, 1.0f};
/// The costliest profiler stages a benchmark shows as it runs
constexpr size_t k_TopStages = 12;
/// A frame this long or less makes the target of 100 frames a second
constexpr float k_TargetFrameMs = 10.0f;
/// How far the close up on a creature's head stands from it, for a creature of size 1
constexpr float k_HeadDistance = 1.4f;

std::string_view MotionName(CreatureLocomotion::Motion motion)
{
	constexpr std::array<std::string_view, 6> k_Names {"standing", "planning",     "confused",
	                                                   "turning",  "stepping off", "walking"};
	return k_Names.at(static_cast<size_t>(motion));
}

std::string Label(const Scenario& scenario)
{
	return fmt::format("{}: {}", Name(scenario.facet), scenario.name);
}
} // namespace

TestbedScenarios::TestbedScenarios(Window& spawner) noexcept
    : Window(std::string(k_TestbedScenariosWindow), ImVec2(640.0f, 620.0f))
    , _spawner(spawner)
{
}

void TestbedScenarios::SetHost(TestbedHost* host) noexcept
{
	if (_runner != nullptr)
	{
		_runner->Stop();
	}
	_host = host;
	_runner = host != nullptr ? std::make_unique<Runner>(*host) : nullptr;
}

void TestbedScenarios::UpdateAlways() noexcept
{
	if (_host == nullptr || _runner == nullptr)
	{
		return;
	}
	// Scenarios run in the game's time, a frame's step at a time: not while it is paused, and faster as the game is
	const float seconds = ScenarioSeconds(_host->FrameSeconds(), _host->IsPaused(), _host->GetGameSpeed());
	RunRequested();
	_runner->Update(seconds);
}

void TestbedScenarios::RunRequested() noexcept
{
	const auto request = _host->TakeScenarioRequest();
	if (!request.has_value())
	{
		return;
	}
	const auto all = All();
	const auto run = ResolveRequest(all, *request);
	if (!run.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "No testbed scenario {}; there are:{}", request->id, KnownIds(all));
		_host->RequestQuit();
		return;
	}
	const auto& scenario = all[run->index];
	_picked = run->index;
	_facet = scenario.facet;
	_runner->SetBenchmarkSettings(run->settings);
	_focus = 0;
	_runner->Start(scenario);
}

void TestbedScenarios::RunFromWindow(const Scenario& scenario) noexcept
{
	_runner->SetBenchmarkSettings(ForRunFromWindow(_runner->GetBenchmarkSettings()));
	_runner->Start(scenario);
}

void TestbedScenarios::Draw() noexcept
{
	if (_host == nullptr || _runner == nullptr)
	{
		ImGui::TextDisabled("The scenarios run only in the game");
		return;
	}
	const auto picked = PickedIn(All(), _picked);
	if (!picked.has_value())
	{
		ImGui::TextDisabled("There are no scenarios");
		return;
	}
	_picked = *picked;
	DrawPicker();
	DrawControls();
	DrawBenchmark();
	DrawTime();
	DrawCamera();
	DrawCreatures();
	DrawFixtures();
}

void TestbedScenarios::DrawFixtures() noexcept
{
	if (!ImGui::CollapsingHeader("Fixtures"))
	{
		return;
	}
	namespace fixtures = testbed_fixtures;
	auto& controls = _fixtureControls;
	ImGui::TextWrapped("Sets one thing out on the land at once, at the point from the middle of the map (x east, y north)");
	ImGui::InputFloat2("At", &controls.at.x, "%.0f");
	const fixtures::Where at {controls.at};
	const auto place = [this](fixtures::Fixtures what) { _runner->PlaceByHand(what); };

	// A dispenser of any miracle a dispenser can give
	const auto miracles = magic::DispensableMiracles();
	if (!miracles.empty())
	{
		controls.magic = std::clamp(controls.magic, 0, static_cast<int>(miracles.size()) - 1);
		const auto magicName = [&miracles](int index) {
			const auto type = miracles[static_cast<size_t>(index)];
			return Locator::infoConstants::has_value() ? debug::magic_window::MagicName(Locator::infoConstants::value(), type)
			                                           : fmt::format("Magic {}", static_cast<int>(type));
		};
		if (ImGui::BeginCombo("Miracle", magicName(controls.magic).c_str()))
		{
			for (int i = 0; i < static_cast<int>(miracles.size()); ++i)
			{
				if (ImGui::Selectable(magicName(i).c_str(), i == controls.magic))
				{
					controls.magic = i;
				}
			}
			ImGui::EndCombo();
		}
		const auto magic = miracles[static_cast<size_t>(controls.magic)];
		if (ImGui::Button("Dispenser"))
		{
			place({.dispensers = {{.magic = magic, .at = at}}});
		}
		ImGui::SameLine();
		if (ImGui::Button("Seed in the hand"))
		{
			place({.handSeed = magic});
		}
	}

	// A storm at the point
	ImGui::Combo("Weather", &controls.weather, "Rain\0Snow\0Rain and snow\0");
	ImGui::SameLine();
	ImGui::Checkbox("Lightning", &controls.lightning);
	if (ImGui::Button("Storm"))
	{
		fixtures::Storm storm {.at = at};
		storm.rain = controls.weather == 1 ? 0.0f : 1.0f;
		storm.snow = controls.weather == 0 ? 0.0f : 1.0f;
		storm.temperature = controls.weather == 0 ? 10.0f : -5.0f;
		if (controls.lightning)
		{
			storm.forkSeconds = glm::vec2 {2.0f, 8.0f};
			storm.sheetSeconds = glm::vec2 {3.0f, 15.0f};
		}
		place({.storms = {storm}});
	}

	// The player's creature, free, leashed or penned
	controls.species = std::clamp(controls.species, 0, static_cast<int>(debug::creature_spawner::k_SpeciesCount) - 1);
	const auto species = debug::creature_spawner::SpeciesAt(static_cast<size_t>(controls.species));
	if (ImGui::BeginCombo("Species", std::string(SpeciesName(species)).c_str()))
	{
		for (int i = 0; i < static_cast<int>(debug::creature_spawner::k_SpeciesCount); ++i)
		{
			const auto name = std::string(SpeciesName(debug::creature_spawner::SpeciesAt(static_cast<size_t>(i))));
			if (ImGui::Selectable(name.c_str(), i == controls.species))
			{
				controls.species = i;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::Combo("Held", &controls.hold, "Free\0Leashed\0Penned\0");
	if (ImGui::Button("Creature"))
	{
		place({.creatures = {{.species = species, .at = at, .hold = static_cast<fixtures::Hold>(controls.hold)}}});
	}

	// A village, a fire, trees and a pile
	ImGui::InputInt("Huts", &controls.huts);
	ImGui::InputInt("Villagers", &controls.villagers);
	controls.huts = std::clamp(controls.huts, 1, 20);
	controls.villagers = std::clamp(controls.villagers, 0, 100);
	if (ImGui::Button("Village"))
	{
		place({.villages = {{.at = at,
		                     .huts = static_cast<size_t>(controls.huts),
		                     .villagers = static_cast<size_t>(controls.villagers)}}});
	}
	ImGui::SameLine();
	if (ImGui::Button("Fire"))
	{
		place({.fires = {{.what = at}}});
	}
	ImGui::InputInt("Trees", &controls.trees);
	controls.trees = std::clamp(controls.trees, 1, 50);
	if (ImGui::Button("Trees"))
	{
		place({.trees = {{.at = at, .count = static_cast<size_t>(controls.trees), .spread = 20.0f}}});
	}
	ImGui::SameLine();
	if (ImGui::Button("Wood pile"))
	{
		place({.piles = {{.type = PotInfo::WoodPile_1, .at = at}}});
	}
}

void TestbedScenarios::DrawPicker() noexcept
{
	const auto all = All();
	if (ImGui::BeginCombo("Facet", _facet.has_value() ? std::string(Name(*_facet)).c_str() : "Every facet"))
	{
		if (ImGui::Selectable("Every facet", !_facet.has_value()))
		{
			_facet.reset();
		}
		// Facets with no scenarios yet are left out
		for (const auto facet : ListedFacets(all))
		{
			if (ImGui::Selectable(std::string(Name(facet)).c_str(), _facet == facet))
			{
				_facet = facet;
				_picked = PickAfterFacetChange(all, _facet, _picked);
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::BeginCombo("Scenario", Label(all[_picked]).c_str(), ImGuiComboFlags_HeightLarge))
	{
		for (const auto index : Listed(all, _facet))
		{
			if (ImGui::Selectable(Label(all[index]).c_str(), index == _picked))
			{
				_picked = index;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", std::string(all[index].description).c_str());
			}
		}
		ImGui::EndCombo();
	}
	const auto& scenario = all[_picked];
	ImGui::TextWrapped("%s", std::string(scenario.description).c_str());
	ImGui::TextWrapped("Look for: %s", std::string(scenario.expected).c_str());
	const auto& environment = scenario.environment;
	ImGui::TextDisabled("%.1f o'clock%s, %s, body time x%.0f, %zu creature%s, %zu object%s, %zu command%s",
	                    static_cast<double>(environment.hour), environment.clockRuns ? "" : " (clock stopped)",
	                    std::string(Name(environment.weather)).c_str(), static_cast<double>(environment.bodyTimeScale),
	                    scenario.creatures.size(), scenario.creatures.size() == 1 ? "" : "s", scenario.objects.size(),
	                    scenario.objects.size() == 1 ? "" : "s", scenario.commands.size(),
	                    scenario.commands.size() == 1 ? "" : "s");
}

void TestbedScenarios::DrawControls() noexcept
{
	const auto& chosen = All()[_picked];
	const auto* current = _runner->GetScenario();
	const bool running = _runner->IsRunning();

	ImGui::PushStyleColor(ImGuiCol_Button, k_RunColour);
	if (ImGui::Button("Run", ImVec2(90.0f, 0.0f)))
	{
		_focus = 0;
		RunFromWindow(chosen);
	}
	ImGui::PopStyleColor();
	ImGui::SetItemTooltip("Loads the testbed afresh and sets this scenario up on it");
	ImGui::SameLine();
	ImGui::BeginDisabled(current == nullptr);
	if (ImGui::Button("Restart", ImVec2(90.0f, 0.0f)) && current != nullptr)
	{
		RunFromWindow(*current);
	}
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("Runs the last scenario again from the start, on a fresh testbed");
	ImGui::SameLine();
	ImGui::BeginDisabled(!running);
	ImGui::PushStyleColor(ImGuiCol_Button, k_StopColour);
	if (ImGui::Button("Stop", ImVec2(90.0f, 0.0f)))
	{
		_runner->Stop();
	}
	ImGui::PopStyleColor();
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("Gives no more commands and puts back the body time, fainting, footprints and clock; the "
	                      "creatures stay");
	ImGui::SameLine();
	if (ImGui::Button("Empty testbed"))
	{
		_runner->Stop();
		_host->LoadTestbed(flat_land::k_Altitude);
	}
	ImGui::SetItemTooltip("Loads the testbed afresh, with nothing on it");

	// The scenario run last, which may have been stopped
	if (current != nullptr)
	{
		const auto status = StatusLine(running, *current, _runner->GetSeconds(), _runner->GetTimeline());
		ImGui::TextUnformatted(status.c_str());
		if (!_runner->GetLog().empty() && ImGui::TreeNode("Log"))
		{
			for (const auto& line : _runner->GetLog())
			{
				ImGui::TextUnformatted(line.c_str());
			}
			ImGui::TreePop();
		}
	}
}

void TestbedScenarios::DrawBenchmark() noexcept
{
	const auto progress = _runner->GetCrowdProgress();
	if (!progress.has_value())
	{
		return;
	}
	ImGui::SeparatorText("Benchmark");
	std::string counts;
	for (const auto& [name, count] : _runner->EntityCounts())
	{
		counts += fmt::format("{}{} {}", counts.empty() ? "" : ", ", count, name);
	}
	ImGui::TextUnformatted(counts.c_str());

	if (!progress->Done())
	{
		const auto fraction = static_cast<float>(progress->spawned) / static_cast<float>(std::max<size_t>(progress->total, 1));
		ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f),
		                   fmt::format("Spawning {} of {}", progress->spawned, progress->total).c_str());
		return;
	}
	ImGui::Text("Spawned %zu in %.0f ms over %u frames", progress->total, progress->spawnMs, progress->spawnFrames);
	ImGui::SetItemTooltip("The time spent creating the crowd's entities, apart from the rest of those frames");
	if (const auto warmUp = _runner->GetWarmUpLeft(); warmUp > 0)
	{
		ImGui::Text("Settling: %u frames before measuring", warmUp);
		return;
	}

	const auto& settings = _runner->GetBenchmarkSettings();
	const auto& results = _runner->GetLiveResults();
	ImGui::Text("Measured the last %zu of up to %u frames", _runner->GetMeasuredFrames(), settings.frames);
	const auto frame = results.frame;
	const auto colour = frame.average <= k_TargetFrameMs ? ImVec4(0.4f, 0.85f, 0.4f, 1.0f) : ImVec4(0.95f, 0.45f, 0.35f, 1.0f);
	ImGui::TextColored(colour, "Frame: mean %.2f ms (%.0f FPS), p95 %.2f, max %.2f", static_cast<double>(frame.average),
	                   frame.average > 0.0f ? 1000.0 / static_cast<double>(frame.average) : 0.0, static_cast<double>(frame.p95),
	                   static_cast<double>(frame.max));
	ImGui::Text("Update: mean %.2f ms, p95 %.2f   Render: mean %.2f ms, p95 %.2f", static_cast<double>(results.update.average),
	            static_cast<double>(results.update.p95), static_cast<double>(results.render.average),
	            static_cast<double>(results.render.p95));
	ImGui::Text("Draw calls: mean %.0f, max %.0f", static_cast<double>(results.draws.average),
	            static_cast<double>(results.draws.max));

	const auto stages = _runner->GetStages();
	if (!results.stages.empty() &&
	    ImGui::BeginTable("Benchmark stages", 6,
	                      ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("Stage");
		ImGui::TableSetupColumn("");
		ImGui::TableSetupColumn("Mean");
		ImGui::TableSetupColumn("p95");
		ImGui::TableSetupColumn("Max");
		ImGui::TableSetupColumn("When run");
		ImGui::TableHeadersRow();
		for (size_t i = 0; i < std::min(k_TopStages, results.stages.size()); ++i)
		{
			const auto& stage = results.stages[i];
			if (stage.stage >= stages.size())
			{
				continue;
			}
			const auto& info = stages[stage.stage];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(info.name.data(), info.name.data() + info.name.size());
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%s", info.render ? "render" : "update");
			ImGui::TableNextColumn();
			ImGui::Text("%.2f", static_cast<double>(stage.perFrame.average));
			ImGui::TableNextColumn();
			ImGui::Text("%.2f", static_cast<double>(stage.perFrame.p95));
			ImGui::TableNextColumn();
			ImGui::Text("%.2f", static_cast<double>(stage.perFrame.max));
			ImGui::TableNextColumn();
			ImGui::Text("%.2f in %zu", static_cast<double>(stage.meanWhenRun), stage.framesRun);
		}
		ImGui::EndTable();
	}

	// The results are written only where the command line's --benchmark-out says, so the button is there only with it
	if (!settings.resultsBase.has_value())
	{
		return;
	}
	ImGui::BeginDisabled(_runner->GetMeasuredFrames() == 0);
	if (ImGui::Button("Save results"))
	{
		const auto path = _runner->SaveResults();
		_savedTo = path.has_value() ? path->generic_string() : "couldn't write them";
	}
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("Writes the frames measured so far to the --benchmark-out path, as JSON and CSV, to compare "
	                      "with other runs");
	if (!_savedTo.empty())
	{
		ImGui::SameLine();
		ImGui::TextDisabled("%s", _savedTo.c_str());
	}
}

void TestbedScenarios::DrawTime() noexcept
{
	ImGui::SeparatorText("Time");
	ImGui::TextUnformatted("Game");
	for (const auto& speed : k_Speeds)
	{
		ImGui::SameLine();
		if (ImGui::RadioButton(std::string(speed.name).c_str(), _host->GetGameSpeed() == speed.multiplier))
		{
			_host->SetGameSpeed(speed.multiplier);
		}
	}
	if (Locator::creaturePhysiologySystem::has_value())
	{
		auto& physiology = Locator::creaturePhysiologySystem::value();
		ImGui::SameLine(0.0f, 24.0f);
		ImGui::TextUnformatted("Body");
		for (const auto scale : k_BodyTimes)
		{
			ImGui::SameLine();
			if (ImGui::RadioButton(fmt::format("x{:.0f}##body", scale).c_str(), physiology.GetTimeScale() == scale))
			{
				physiology.SetTimeScale(scale);
			}
		}
		ImGui::SetItemTooltip("Game turns of every creature's body that pass each game turn, to watch them grow and tire");
	}
}

void TestbedScenarios::DrawCamera() noexcept
{
	ImGui::SeparatorText("Camera");
	const auto creatures = _runner->GetCreatures();
	ImGui::BeginDisabled(creatures.empty());
	const auto shot = _runner->GetShot();
	if (ImGui::Button(shot == Shot::Follow ? "Following" : "Follow"))
	{
		_runner->Frame(Shot::Follow, _focus);
	}
	ImGui::SetItemTooltip("Behind and above the creature picked below, keeping up with it");
	ImGui::SameLine();
	if (ImGui::Button(shot == Shot::Head ? "Close up on the head" : "Head and eyes"))
	{
		_runner->Frame(Shot::Head, _focus, k_HeadDistance);
	}
	ImGui::SetItemTooltip("In front of the picked creature's face, keeping up with it");
	ImGui::SameLine();
	if (ImGui::Button("Overview"))
	{
		_runner->Frame(Shot::Overview, _focus);
	}
	ImGui::SetItemTooltip("Above and to the south of all of the scenario, once");
	ImGui::SameLine();
	if (ImGui::Button("Testbed view"))
	{
		_runner->Frame(Shot::Testbed, _focus);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!shot.has_value());
	if (ImGui::Button("Let go"))
	{
		_runner->ReleaseCamera();
	}
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("The camera is yours again");
}

void TestbedScenarios::DrawCreatures() noexcept
{
	const auto* scenario = _runner->GetScenario();
	const auto creatures = _runner->GetCreatures();
	if (scenario == nullptr || creatures.empty() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	ImGui::SeparatorText("Its creatures");
	auto& registry = Locator::entitiesRegistry::value();
	if (!ImGui::BeginTable("Scenario creatures", 8,
	                       ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
	                           ImGuiTableFlags_SizingFixedFit))
	{
		return;
	}
	ImGui::TableSetupScrollFreeze(2, 1);
	ImGui::TableSetupColumn("");
	ImGui::TableSetupColumn("Creature");
	ImGui::TableSetupColumn("Doing");
	ImGui::TableSetupColumn("Speed");
	ImGui::TableSetupColumn("Needs");
	ImGui::TableSetupColumn("Size");
	ImGui::TableSetupColumn("Strongest desire");
	ImGui::TableSetupColumn("");
	ImGui::TableHeadersRow();
	for (size_t i = 0; i < creatures.size() && i < scenario->creatures.size(); ++i)
	{
		const auto entity = creatures[i];
		ImGui::PushID(static_cast<int>(i));
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		auto focus = static_cast<int>(_focus);
		if (ImGui::RadioButton("##focus", &focus, static_cast<int>(i)))
		{
			_focus = i;
			if (const auto shot = _runner->GetShot(); shot == Shot::Follow || shot == Shot::Head)
			{
				_runner->Frame(*shot, _focus, *shot == Shot::Head ? k_HeadDistance : 1.0f);
			}
		}
		const auto* creature = registry.Valid(entity) ? registry.TryGet<const Creature>(entity) : nullptr;
		const auto& setup = scenario->creatures[i];
		ImGui::TableNextColumn();
		const auto name = setup.label.empty() ? std::string(SpeciesName(setup.species))
		                                      : fmt::format("{}, {}", SpeciesName(setup.species), setup.label);
		ImGui::TextUnformatted(name.c_str());
		if (creature == nullptr)
		{
			ImGui::TableNextColumn();
			ImGui::TextDisabled("gone");
			ImGui::PopID();
			continue;
		}
		const auto* mind = registry.TryGet<const CreatureMindState>(entity);
		const auto* needs = registry.TryGet<const CreatureNeeds>(entity);
		const auto* locomotion = registry.TryGet<const CreatureLocomotion>(entity);
		ImGui::TableNextColumn();
		const auto doing = mind != nullptr ? std::string(creature_mind::Name(mind->idle.activity)) : std::string("-");
		ImGui::Text("%s%s", doing.c_str(), mind != nullptr && mind->paused ? " (paused)" : "");
		if (locomotion != nullptr)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("%s", std::string(MotionName(locomotion->motion)).c_str());
		}
		ImGui::TableNextColumn();
		ImGui::Text("%.1f", locomotion != nullptr ? static_cast<double>(locomotion->speed) : 0.0);
		ImGui::TableNextColumn();
		if (needs != nullptr)
		{
			const auto& body = needs->needs;
			ImGui::Text("E %.2f T %.2f X %.2f P %.2f W %+.2f", static_cast<double>(body.energy),
			            static_cast<double>(body.dehydration), static_cast<double>(body.exhaustion),
			            static_cast<double>(body.poo), static_cast<double>(body.warmth));
			ImGui::SetItemTooltip("Energy, thirst, exhaustion, poo and warmth");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%.2f", static_cast<double>(creature->size));
		ImGui::TableNextColumn();
		if (mind != nullptr && mind->desires.has_value())
		{
			if (const auto strongest = StrongestDesire(*mind->desires))
			{
				ImGui::Text("%s %.2f", std::string(creature_desires::Name(*strongest)).c_str(),
				            static_cast<double>((*mind->desires)[*strongest].value));
			}
		}
		ImGui::TableNextColumn();
		// The spawner has no way to be handed a creature, so it opens for the creature to be picked in its list
		if (ImGui::SmallButton("Spawner"))
		{
			_spawner.Open();
		}
		ImGui::SetItemTooltip("Opens the creature spawner, to pick this creature in its list for its body, mind, "
		                      "movement and sounds");
		ImGui::PopID();
	}
	ImGui::EndTable();
}
