/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSpawner.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL_events.h>
#include <SDL_mouse.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureSkin.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureTurnPose.h"
#include "Creature/LeashRules.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreaturePose.h"
#include "ECS/CreatureRemoval.h"
#include "ECS/MobileDrawing.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "MiraclesModel.h"
#include "RoutePlanner/RouteFollower.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::debug::creature_spawner;
using openblack::ecs::archetypes::CreatureArchetype;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureAnimation;
using openblack::ecs::components::CreatureDrawPose;
using openblack::ecs::components::CreatureLeash;
using openblack::ecs::components::CreatureLocomotion;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::CreatureMorph;
using openblack::ecs::components::CreatureNeeds;
using openblack::ecs::components::CreatureTattoos;
using openblack::ecs::components::Transform;

namespace
{
const ImVec4 k_PlacingColour {0.85f, 0.30f, 0.25f, 1.0f};
const ImVec4 k_StartColour {0.25f, 0.60f, 0.30f, 1.0f};
/// A click picks out the thing to pick up or knock down nearest it, this close at most
constexpr float k_PickRadius = 12.0f;
/// The player's creature section lists this many of its strongest desires
constexpr size_t k_TopDesires = 5;

const GCreatureInfo* SpeciesInfo(CreatureType species)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& creatures = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(species);
	return row < creatures.size() ? &creatures.at(row) : nullptr;
}

/// An animation's place in the creature spec and its name
std::string AnimationLabel(size_t animation)
{
	const auto name = creature_layers::animations::Name(animation);
	return name.empty() ? fmt::format("{}", animation) : fmt::format("{} {}", animation, name);
}

std::string_view LookKindName(creature_look::Interest kind)
{
	constexpr std::array<std::string_view, 8> k_Names {"dove",     "citadel", "creature", "animal",
	                                                   "villager", "abode",   "tree",     "fixed object"};
	return k_Names.at(static_cast<size_t>(kind));
}

std::string_view MotionName(CreatureLocomotion::Motion motion)
{
	constexpr std::array<std::string_view, 6> k_Names {"Standing", "Planning a route", "Confused",
	                                                   "Turning",  "Stepping off",     "Walking"};
	return k_Names.at(static_cast<size_t>(motion));
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

bool IsLeftClick(const SDL_Event& event)
{
	return (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_LEFT;
}

std::string ActionName(const creature_mind_tables::Tables* tables, uint32_t action)
{
	return tables != nullptr && action < tables->actions.size() ? tables->actions[action].name : fmt::format("#{}", action);
}

/// Where the player's hand is, on the land
std::optional<glm::vec3> HandOnLand()
{
	if (!Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	using Side = ecs::systems::HandSystemInterface::Side;
	const auto positions = Locator::handSystem::value().GetPlayerHandPositions();
	auto point = debug::miracles::HandPoint(positions.at(static_cast<size_t>(Side::Left)),
	                                        positions.at(static_cast<size_t>(Side::Right)));
	if (point.has_value() && Locator::terrainSystem::has_value())
	{
		*point = OnGround(*point, Locator::terrainSystem::value().GetHeightAt(glm::xz(*point)));
	}
	return point;
}
} // namespace

CreatureSpawner::CreatureSpawner() noexcept
    : Window("Creature Spawner", ImVec2(460.0f, 620.0f))
{
}

void CreatureSpawner::Close() noexcept
{
	_placing = false;
	_commanding = false;
	_clicked = false;
	Window::Close();
}

void CreatureSpawner::Draw() noexcept
{
	if (!Locator::entitiesRegistry::has_value())
	{
		ImGui::TextUnformatted("No land loaded");
		return;
	}
	DrawPlayersCreature();
	if (!ImGui::BeginTabBar("CreatureSpawnerTabs"))
	{
		return;
	}
	if (ImGui::BeginTabItem("Spawn"))
	{
		DrawSpawnMind();
		DrawSettings();
		ImGui::Separator();
		DrawPlacing();
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Creatures"))
	{
		DrawCreatures();
		ImGui::EndTabItem();
	}
	// Selecting a creature brings its tab to the front, once
	const bool justSelected = _selected.has_value() && _selected != _tabFor;
	_tabFor = _selected;
	if (_selected.has_value() &&
	    ImGui::BeginTabItem("Selected", nullptr, justSelected ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None))
	{
		DrawSelected();
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
}

void CreatureSpawner::DrawPlayersCreature() noexcept
{
	if (!ImGui::CollapsingHeader("Player's creature", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	if (!Locator::leashSystem::has_value())
	{
		ImGui::TextUnformatted("No leash system, so no player's creature");
		return;
	}
	auto& leashes = Locator::leashSystem::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto& view = std::as_const(registry);
	const auto found = leashes.PlayersCreature(PlayerNames::PLAYER_ONE);
	const auto* creature = found.has_value() ? view.TryGet<const Creature>(*found) : nullptr;
	const auto* transform = found.has_value() ? view.TryGet<const Transform>(*found) : nullptr;
	if (creature == nullptr || transform == nullptr)
	{
		ImGui::TextUnformatted("Player one has no creature");
		return;
	}
	const auto entity = *found;
	const auto* pose = view.TryGet<const CreatureDrawPose>(entity);
	const auto* mind = view.TryGet<const CreatureMindState>(entity);
	const auto* leash = view.TryGet<const CreatureLeash>(entity);
	const auto* needs = view.TryGet<const CreatureNeeds>(entity);
	const auto summary = Summarise(
	    {
	        .entity = entity,
	        .species = creature->species,
	        .size = creature->size,
	        .alignment = creature->alignment,
	        .leashable = creature->leashable,
	        .position = transform->position,
	        .ownScale = transform->scale,
	        .poseScale = pose != nullptr ? pose->scale : std::nullopt,
	        .developmentPhase = mind != nullptr ? std::optional(mind->developmentPhase) : std::nullopt,
	        .home = leash != nullptr ? leash->home : std::nullopt,
	        .wornLeash = leash != nullptr && leash->worn.has_value() ? std::optional(leash->worn->type) : std::nullopt,
	        .knownLeashes = leash != nullptr ? leash->known : decltype(leash->known) {},
	        .desires = mind != nullptr && mind->desires.has_value() ? &*mind->desires : nullptr,
	        .plan = mind != nullptr ? mind->planner.current : std::nullopt,
	        .needs = needs != nullptr ? &needs->needs : nullptr,
	    },
	    k_TopDesires);

	ImGui::Text("%s, entity %u, at %.1f, %.1f, %.1f", SpeciesName(summary.species).data(), entt::to_integral(entity),
	            static_cast<double>(summary.position.x), static_cast<double>(summary.position.y),
	            static_cast<double>(summary.position.z));
	ImGui::SameLine();
	if (ImGui::SmallButton("Select"))
	{
		_selected = entity;
	}
	ImGui::Text("Size %.2f, drawn at %.3f%s", static_cast<double>(summary.size), static_cast<double>(summary.drawnScale.x),
	            pose != nullptr && pose->inPen ? " (in its pen)" : "");
	ImGui::Text("Alignment %+.2f", static_cast<double>(summary.alignment));
	if (summary.developmentPhase.has_value())
	{
		ImGui::Text("Stage of growing up %u of %d", *summary.developmentPhase, ecs::player_creature::k_LastDevelopmentStage);
		if (mind != nullptr && mind->scriptStage.has_value())
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(stage %u waits for its mind file)", *mind->scriptStage);
		}
	}
	if (summary.home.has_value())
	{
		ImGui::Text("Home at %.1f, %.1f", static_cast<double>(summary.home->x), static_cast<double>(summary.home->z));
	}
	else
	{
		ImGui::TextUnformatted("No home");
	}
	if (leash != nullptr && leash->confinementRadius > 0.0f)
	{
		ImGui::SameLine();
		ImGui::Text(", kept within %.1f of it", static_cast<double>(leash->confinementRadius));
	}
	ImGui::Text("In a lesson script: %s", creature->inDevScript ? "yes" : "no");
	// the angle it is turned by, as it is made with (a new creature's is 0)
	ImGui::Text("Facing %.1f degrees",
	            static_cast<double>(glm::degrees(-ecs::creature_pose::ReadHeading(transform->rotation))));
	{
		// The planner's rule for sleeping on the spot, on what the creature's home, temple and leash are now
		bool hasTemple = false;
		view.Each<const ecs::components::Temple>([&](entt::entity /*unused*/, const ecs::components::Temple& temple) {
			hasTemple = hasTemple || temple.owner == creature->owner;
		});
		std::optional<float> homeDistance;
		if (const auto home = ecs::creature_home::HomeOf(view, entity))
		{
			homeDistance = gutils::GetDistanceInMetres(glm::vec2(transform->position.x, transform->position.z),
			                                           glm::vec2(home->x, home->z));
		}
		const bool leashed = leashes.IsLeashed(entity);
		const auto away = homeDistance.has_value() ? fmt::format("{:.0f} m from home", *homeDistance) : std::string("no home");
		ImGui::Text("Sleeps on the spot: %s (%s, %s, %s)",
		            creature_mind::MaySleepOnTheSpot(leashed, hasTemple, homeDistance) ? "may" : "not now",
		            leashed ? "leashed" : "no leash", hasTemple ? "its player has a temple" : "no temple", away.c_str());
	}
	ImGui::Text("Leash: %s, knows %s, %s",
	            summary.wornLeash.has_value() ? creature_leash::Name(*summary.wornLeash) : "none worn",
	            summary.knownLeashes.c_str(), summary.leashable ? "leashable" : "not leashable");

	std::string desires;
	if (mind != nullptr && mind->desires.has_value())
	{
		for (const auto desire : summary.desires)
		{
			desires += fmt::format("{}{} {:.2f}", desires.empty() ? "" : ", ", creature_desires::Name(desire),
			                       (*mind->desires)[desire].value);
		}
	}
	ImGui::TextWrapped("Desires: %s", desires.empty() ? "none yet" : desires.c_str());
	if (summary.plan.has_value())
	{
		const auto* tables =
		    Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
		ImGui::Text("Plan: %s for %s, priority %.1f", ActionName(tables, summary.plan->action).c_str(),
		            creature_desires::Name(summary.plan->desire).data(), static_cast<double>(summary.plan->priority));
		if (summary.plan->object.has_value())
		{
			// What the plan is done to; a worship site's altar is a temple part of its own
			const auto object = static_cast<entt::entity>(*summary.plan->object);
			ImGui::Text("  on entity %u%s", *summary.plan->object,
			            view.Valid(object) && view.AllOf<ecs::components::WorshipTotem>(object) ? ", a worship site's altar"
			                                                                                    : "");
		}
	}
	else if (mind != nullptr)
	{
		ImGui::Text("Doing: %s", creature_mind::Name(mind->idle.activity).data());
	}
	if (summary.needs.has_value())
	{
		const auto& body = *summary.needs;
		ImGui::Text("Energy %.2f, exhaustion %.2f, thirst %.2f, poo %.2f, life %.2f, age %u", static_cast<double>(body.energy),
		            static_cast<double>(body.exhaustion), static_cast<double>(body.dehydration), static_cast<double>(body.poo),
		            static_cast<double>(body.life), body.age);
	}
	if (Locator::creatureHandSystem::has_value())
	{
		const auto& hand = Locator::creatureHandSystem::value();
		const auto held = hand.GetCreature();
		const auto under = hand.CreatureUnderHand();
		ImGui::Text("Hand: %s, under it %s",
		            held.has_value() ? fmt::format("holding creature {}", entt::to_integral(*held)).c_str()
		                             : "holding no creature",
		            under.has_value() ? fmt::format("creature {}", entt::to_integral(*under)).c_str() : "no creature");
	}

	if (ImGui::Button("Bring to the hand"))
	{
		if (const auto hand = HandOnLand())
		{
			if (auto* moved = Find<Transform>(registry, entity))
			{
				moved->position = *hand;
				ecs::NotifyTeleported(entity);
				_lastPlayers = fmt::format("Brought to {:.0f}, {:.0f}", hand->x, hand->z);
			}
		}
		else
		{
			_lastPlayers = "The hand is not on the land";
		}
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.0f);
	ImGui::SliderInt("##phase", &_phase, 0, ecs::player_creature::k_LastDevelopmentStage);
	ImGui::SameLine();
	if (ImGui::Button("Set stage"))
	{
		const auto phase = ClampPhase(_phase, ecs::player_creature::k_LastDevelopmentStage);
		ecs::player_creature::SetDevelopmentStage(registry, entity, phase);
		_lastPlayers = fmt::format("Stage of growing up set to {}", phase);
	}
	ImGui::SameLine();
	if (ImGui::Button("Start growing up"))
	{
		// As the tutorial's script does: the first stage, kept near its home
		auto* minds = Locator::creatureMindSystem::has_value() ? &Locator::creatureMindSystem::value() : nullptr;
		ecs::player_creature::DevFunction(leashes, minds, 1, PlayerNames::PLAYER_ONE);
		_lastPlayers = "Started growing up";
	}
	ImGui::TextUnformatted("Knows");
	for (const auto type : creature_leash::k_Types)
	{
		ImGui::SameLine();
		bool known = leashes.Knows(entity, type);
		if (ImGui::Checkbox(fmt::format("{}##players", creature_leash::Name(type)).c_str(), &known))
		{
			leashes.SetKnown(entity, type, known);
		}
	}
	ImGui::SameLine();
	bool leashable = leashes.IsLeashable(entity);
	if (ImGui::Checkbox("Leashable##players", &leashable))
	{
		leashes.SetLeashable(entity, leashable);
	}
	if (!_lastPlayers.empty())
	{
		ImGui::TextUnformatted(_lastPlayers.c_str());
	}
}

void CreatureSpawner::UseSpeciesDefaults() noexcept
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const auto body = CreatureArchetype::StartBody(_species);
	_alignment = body.alignment;
	_fatness = body.fatness;
	_strength = body.strength;
	_scale = CreatureArchetype::StartScale(_species);
	_defaultsFor = _species;
}

void CreatureSpawner::DrawSettings() noexcept
{
	if (_defaultsFor != _species)
	{
		UseSpeciesDefaults();
	}

	ImGui::SeparatorText("Creature");

	if (ImGui::BeginCombo("Species", SpeciesName(_species).data()))
	{
		for (size_t i = 0; i < k_SpeciesCount; ++i)
		{
			const auto species = SpeciesAt(i);
			if (ImGui::Selectable(SpeciesName(species).data(), species == _species))
			{
				_species = species;
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::BeginCombo("Owner", OwnerName(_owner).data()))
	{
		for (size_t i = 0; i < static_cast<size_t>(PlayerNames::_COUNT); ++i)
		{
			const auto owner = static_cast<PlayerNames>(i);
			if (ImGui::Selectable(OwnerName(owner).data(), owner == _owner))
			{
				_owner = owner;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SeparatorText("Body");
	ImGui::SliderFloat("Alignment", &_alignment, -1.0f, 1.0f,
	                   _alignment < 0.0f ? "Evil %.2f" : (_alignment > 0.0f ? "Good %.2f" : "Neutral %.2f"));
	ImGui::SliderFloat("Fatness", &_fatness, 0.0f, 1.0f, "%.2f");
	ImGui::SliderFloat("Strength", &_strength, 0.0f, 1.0f, "%.2f");
	ImGui::SliderFloat("Size", &_scale, creature_morph::k_MinScale, creature_morph::k_MaxScale, "%.2f",
	                   ImGuiSliderFlags_Logarithmic);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Creatures grow by themselves up to size %.1f", static_cast<double>(creature_morph::k_MaxGrownScale));
	}
	if (ImGui::Button("Species defaults"))
	{
		UseSpeciesDefaults();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("As a new creature of the species is: neutral, with its size, fatness and strength");
	}

	const auto* info = SpeciesInfo(_species);
	const auto morph = creature_morph::FromAttributes(
	    _alignment, _fatness, _strength, info != nullptr ? info->strength : creature_morph::k_UnknownSpeciesStrength);
	ImGui::Text("Evil-good %+.2f, thin-fat %+.2f, weak-strong %+.2f", static_cast<double>(morph.evilGood),
	            static_cast<double>(morph.thinFat), static_cast<double>(morph.weakStrong));
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("How far the body is pulled from its base mesh towards each axis' mesh, -1 to 1");
	}

	ImGui::SeparatorText("Facing");
	ImGui::Checkbox("Random", &_randomFacing);
	if (!_randomFacing)
	{
		ImGui::SliderFloat("Degrees", &_facingDegrees, 0.0f, 360.0f, "%.0f");
	}
}

void CreatureSpawner::DrawSelected() noexcept
{
	if (!_selected.has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto* creature = Find<Creature>(registry, *_selected);
	auto* transform = Find<Transform>(registry, *_selected);
	if (creature == nullptr || transform == nullptr)
	{
		_selected.reset();
		return;
	}

	ImGui::SeparatorText("Selected creature");
	ImGui::Text("%s at %.0f, %.0f", SpeciesName(creature->species).data(), static_cast<double>(transform->position.x),
	            static_cast<double>(transform->position.z));
	ImGui::SliderFloat("Its alignment", &creature->alignment, -1.0f, 1.0f, "%.2f");
	ImGui::SliderFloat("Its fatness", &creature->fatness, 0.0f, 1.0f, "%.2f");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Its body follows its fatness by at most %.2f a game turn, after what it did, within 0 and 1",
		                  static_cast<double>(creature_morph::k_MaxFatnessStep));
	}
	ImGui::SliderFloat("Its strength", &creature->strength, 0.0f, 1.0f, "%.2f");
	if (ImGui::SliderFloat("Its size", &creature->size, creature_morph::k_MinScale, creature_morph::k_MaxScale, "%.2f",
	                       ImGuiSliderFlags_Logarithmic))
	{
		creature->size = creature_morph::ClampScale(creature->size);
		transform->scale = glm::vec3(CreatureArchetype::DrawnScale(creature->species, creature->size));
		registry.SetDirty();
	}
	if (auto* morph = Find<CreatureMorph>(registry, *_selected); morph != nullptr)
	{
		ImGui::Text("Drawn: evil-good %+.2f, thin-fat %+.2f, weak-strong %+.2f", static_cast<double>(morph->drawn.evilGood),
		            static_cast<double>(morph->drawn.thinFat), static_cast<double>(morph->drawn.weakStrong));
		const auto skinWeight = creature_skin::BlendWeight(morph->drawn.evilGood);
		ImGui::Text("Skin %u of %u towards %s", static_cast<uint32_t>(skinWeight),
		            static_cast<uint32_t>(creature_skin::k_MaxWeight), morph->drawn.evilGood < 0.0f ? "evil" : "good");
		ImGui::Text("Fatness shown %.2f", static_cast<double>(morph->shownFatness));
		ImGui::SameLine();
		if (ImGui::SmallButton("Show now"))
		{
			morph->shownFatness = creature->fatness;
		}
	}
	if (ImGui::Button("Deselect"))
	{
		_selected.reset();
	}
	if (!_selected.has_value() || !ImGui::BeginTabBar("SelectedCreatureTabs"))
	{
		return;
	}
	const auto entity = *_selected;
	const std::array<std::pair<const char*, void (CreatureSpawner::*)(entt::entity) noexcept>, 9> k_Tabs {{
	    {"Looks", &CreatureSpawner::DrawAppearance},
	    {"Audio", &CreatureSpawner::DrawAudio},
	    {"Movement", &CreatureSpawner::DrawMovement},
	    {"Hands", &CreatureSpawner::DrawHands},
	    {"Mind", &CreatureSpawner::DrawMind},
	    {"Learning", &CreatureSpawner::DrawLearning},
	    {"Body", &CreatureSpawner::DrawBody},
	    {"Leash", &CreatureSpawner::DrawLeash},
	    {"Fight", &CreatureSpawner::DrawFight},
	}};
	for (const auto& [label, draw] : k_Tabs)
	{
		if (ImGui::BeginTabItem(label))
		{
			(this->*draw)(entity);
			ImGui::EndTabItem();
		}
	}
	ImGui::EndTabBar();
}

void CreatureSpawner::DrawMovement(entt::entity entity) noexcept
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* self = registry.TryGet<const CreatureLocomotion>(entity);
	if (self == nullptr || !Locator::creatureLocomotionSystem::has_value())
	{
		return;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();

	ImGui::SeparatorText("Movement");
	ImGui::Text("%s%s", MotionName(self->motion).data(), self->failed ? ", the last move failed" : "");
	ImGui::Text("Speed %.1f of walk %.1f, run %.1f units/s", static_cast<double>(self->speed),
	            static_cast<double>(self->speeds.walk), static_cast<double>(self->speeds.run));
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Asked for %.2f of its top speed, %.1f units/s", static_cast<double>(self->fraction),
		                  static_cast<double>(creature_locomotion::TargetSpeed(self->fraction, self->speeds.run)));
	}
	ImGui::Text("Heading %.0f degrees, radius %.1f, scale %.3f", static_cast<double>(glm::degrees(self->heading)),
	            static_cast<double>(self->radius), static_cast<double>(self->scale));
	if (self->follower != nullptr)
	{
		const auto& follower = *self->follower;
		if (follower.HasRoute())
		{
			ImGui::Text("Route of %zu legs ahead, %.1f units left", follower.GetRouteAhead().size(),
			            static_cast<double>(follower.RemainingLength()));
		}
		else if (!self->routeReady && self->destination.has_value())
		{
			ImGui::Text("Planning, %d plans so far", follower.GetPlanCount());
		}
		// Stopped on its way as what it was doing ended: how much of its route is left before it stands
		if (self->braking)
		{
			ImGui::Text("Braking to a stop, %.2f units left", static_cast<double>(follower.RemainingLength()));
		}
	}
	std::string legs;
	for (const auto& track : self->tracks)
	{
		legs += fmt::format("{}{} {:.2f}", legs.empty() ? "" : ", ", AnimationLabel(track.animation), track.weight);
	}
	ImGui::TextWrapped("Legs: %s", legs.empty() ? "as the body plays" : legs.c_str());
	// What the body is drawn from this frame: each slot's animation, weight and time, and a turn's or a step's pair
	// drawn as one, with the morph its animations follow
	if (const auto* body = registry.TryGet<const CreatureAnimation>(entity); body != nullptr)
	{
		std::string drawn;
		for (const auto& slot : body->slots)
		{
			drawn += fmt::format("{}{} {:.3f} at {:.0f} ms{}", drawn.empty() ? "" : ", ", AnimationLabel(slot.animation),
			                     slot.weight, slot.timeMs, slot.mirrored ? " mirrored" : "");
			if (slot.pair.has_value())
			{
				drawn += fmt::format(" (one animation with {} by {:.3f}, {:.0f} of {} ms)", AnimationLabel(slot.pair->to),
				                     slot.pair->weight, slot.pair->timeMs, slot.pair->durationMs);
			}
		}
		ImGui::TextWrapped("Drawn from: %s", drawn.empty() ? "the body's own" : drawn.c_str());
		if (const auto* morph = registry.TryGet<const CreatureMorph>(entity); morph != nullptr)
		{
			ImGui::Text("Animations pulled %.3f towards %s, %.3f towards %s",
			            static_cast<double>(std::abs(morph->drawn.evilGood)), morph->drawn.evilGood < 0.0f ? "evil" : "good",
			            static_cast<double>(std::abs(morph->drawn.thinFat)), morph->drawn.thinFat < 0.0f ? "thin" : "fat");
		}
	}

	const auto colour = _commanding ? k_PlacingColour : k_StartColour;
	ImGui::PushStyleColor(ImGuiCol_Button, colour);
	if (ImGui::Button(_commanding ? "Stop commanding" : "Command it", ImVec2(-1.0f, 0.0f)))
	{
		_commanding = !_commanding;
		_clicked = false;
		if (_commanding)
		{
			_placing = false;
		}
	}
	ImGui::PopStyleColor();
	if (_commanding)
	{
		ImGui::TextWrapped("Left click on the land:");
		auto order = static_cast<int>(_order);
		ImGui::RadioButton("Walk here", &order, static_cast<int>(Order::Walk));
		ImGui::SameLine();
		ImGui::RadioButton("Run here", &order, static_cast<int>(Order::Run));
		ImGui::SameLine();
		ImGui::RadioButton("Flee from", &order, static_cast<int>(Order::Flee));
		ImGui::SameLine();
		ImGui::RadioButton("Face", &order, static_cast<int>(Order::Face));
		ImGui::RadioButton("Pick up", &order, static_cast<int>(Order::PickUp));
		ImGui::SameLine();
		ImGui::RadioButton("Throw at", &order, static_cast<int>(Order::Throw));
		ImGui::SameLine();
		ImGui::RadioButton("Knock down", &order, static_cast<int>(Order::Destroy));
		ImGui::SameLine();
		ImGui::RadioButton("Point at", &order, static_cast<int>(Order::Point));
		_order = static_cast<Order>(order);
		if (!_lastOrder.empty())
		{
			ImGui::TextUnformatted(_lastOrder.c_str());
		}
	}
	if (ImGui::Button("Stop"))
	{
		locomotion.Stop(entity);
	}
	ImGui::SameLine();
	if (ImGui::Button("Face the camera") && Locator::camera::has_value())
	{
		locomotion.TurnToFace(entity, glm::xz(Locator::camera::value().GetOrigin()));
	}
	ImGui::SameLine();
	ImGui::Checkbox("Show route", &_showRoute);
	if (_showRoute)
	{
		DrawRoute(entity);
	}
}

void CreatureSpawner::DrawRoute(entt::entity entity) noexcept
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* self = registry.TryGet<const CreatureLocomotion>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (self == nullptr || transform == nullptr || self->follower == nullptr || !self->follower->HasRoute() ||
	    !Locator::terrainSystem::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	const auto& camera = Locator::camera::value();
	const auto display = ImGui::GetIO().DisplaySize;
	const glm::vec4 viewport {0.0f, 0.0f, display.x, display.y};
	auto* drawList = ImGui::GetBackgroundDrawList();
	const auto project = [&](glm::vec2 point) -> std::optional<ImVec2> {
		glm::vec3 screen;
		// A little above the ground, so the line isn't hidden in it
		const glm::vec3 world {point.x, land.GetHeightAt(point) + 0.5f, point.y};
		if (!camera.ProjectWorldToScreen(world, viewport, screen))
		{
			return std::nullopt;
		}
		return ImVec2(screen.x, screen.y);
	};
	// From where it stands along each leg of the route still to go
	auto from = project(glm::xz(transform->position));
	for (const auto& node : self->follower->GetRouteAhead())
	{
		const auto to = project({node.to.x, node.to.z});
		if (from && to)
		{
			drawList->AddLine(*from, *to, IM_COL32(255, 220, 60, 255), 2.5f);
		}
		from = to;
	}
	if (from)
	{
		drawList->AddCircle(*from, 6.0f, IM_COL32(255, 120, 40, 255), 16, 2.0f);
	}
	if (self->destination.has_value())
	{
		if (const auto destination = project(*self->destination))
		{
			drawList->AddCircleFilled(*destination, 3.0f, IM_COL32(255, 80, 40, 255));
		}
	}
}

void CreatureSpawner::DrawMind(entt::entity entity) noexcept
{
	namespace animations = creature_layers::animations;
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = Find<CreatureMindState>(registry, entity);
	const auto* animation = std::as_const(registry).TryGet<const CreatureAnimation>(entity);
	if (mind == nullptr || animation == nullptr || !Locator::creatureMindSystem::has_value())
	{
		return;
	}
	auto& minds = Locator::creatureMindSystem::value();

	ImGui::SeparatorText("Mind");
	ImGui::Checkbox("Pause mind", &mind->paused);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("The mind leaves the body alone; the buttons below still work");
	}
	if (Locator::infoConstants::has_value())
	{
		const auto& phases = Locator::infoConstants::value().creatureDevelopmentPhaseEntry;
		auto phase = static_cast<int>(std::min<size_t>(mind->developmentPhase, phases.size() - 1));
		if (ImGui::SliderInt("Grown up", &phase, 0, static_cast<int>(phases.size()) - 1,
		                     phases.at(static_cast<size_t>(phase)).name.data()))
		{
			ecs::player_creature::SetDevelopmentStage(registry, entity, phase);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Each stage of growing up brings desires and takes some away");
		}
	}
	const auto& idle = mind->idle;
	ImGui::Text("%s, step %zu of %zu", creature_mind::Name(idle.activity).data(), std::min(idle.step + 1, idle.agenda.size()),
	            idle.agenda.size());
	if (idle.outCold)
	{
		ImGui::TextUnformatted("Out cold until its agenda is over: chooses nothing, takes up no reaction, does not faint");
	}
	if (idle.step < idle.agenda.size())
	{
		const auto& step = idle.agenda[idle.step];
		switch (step.kind)
		{
		case creature_mind::Step::Kind::Wait:
			ImGui::Text("Waiting %.1f of %.1f s", static_cast<double>(idle.stepSeconds), static_cast<double>(step.seconds));
			break;
		case creature_mind::Step::Kind::Action:
			// Each turn its body refuses it, it draws for its mirror unless it holds something
			ImGui::Text("Action %s%s%s", AnimationLabel(step.animation).c_str(), step.sleepyEyes ? ", sleepy eyes" : "",
			            idle.stepStarted ? "" : ", waiting for its body to be free, drawing for its mirror each turn");
			break;
		case creature_mind::Step::Kind::Static:
			if (step.faint)
			{
				// It never waits the time drawn for it: it gets up as it lies and stays on the step
				ImGui::Text("Fainted, %s, %.1f s on the step (the %.1f s drawn are never waited)",
				            idle.sitEnding ? "up again" : "falling", static_cast<double>(idle.stepSeconds),
				            static_cast<double>(step.seconds));
				break;
			}
			// A static action draws for its mirror on every turn its body refuses it
			if (!idle.stepStarted && step.mirrorAtRandom)
			{
				ImGui::Text("%s waits for its body to be free, drawing for its mirror each turn",
				            AnimationLabel(step.sequence[1]).c_str());
				break;
			}
			ImGui::Text("%s %.1f of %.1f s", AnimationLabel(step.sequence[1]).c_str(), static_cast<double>(idle.stepSeconds),
			            static_cast<double>(step.seconds));
			// A script's static action holds its loop a count of whole turns
			if (step.heldInTurns && idle.stepStarted && !idle.sitEnding)
			{
				ImGui::Text("Held %u turns more", static_cast<uint32_t>(idle.heldTurns));
			}
			break;
		case creature_mind::Step::Kind::Move:
			ImGui::Text("Moving for %.1f s", static_cast<double>(idle.stepSeconds));
			break;
		case creature_mind::Step::Kind::Cast:
			// Its pose draws for its mirror on every turn its body refuses it
			if (!idle.stepStarted)
			{
				ImGui::Text("Casting pose waits for its body to be free, drawing each turn");
				break;
			}
			ImGui::Text("Casting for %.1f of %.1f s", static_cast<double>(idle.stepSeconds), static_cast<double>(step.seconds));
			break;
		case creature_mind::Step::Kind::Gesture:
			ImGui::Text("Drawing a gesture for %.1f s", static_cast<double>(idle.stepSeconds));
			break;
		case creature_mind::Step::Kind::WaitInMap:
			ImGui::Text("Waiting for a thing to land, %.1f s", static_cast<double>(idle.stepSeconds));
			break;
		case creature_mind::Step::Kind::TurnToFacePoint:
			ImGui::Text("Turning to face a script's focus, %.1f s", static_cast<double>(idle.stepSeconds));
			break;
		// The steps of a joint action with a friend that wait for the friend, or end in a desire about a town
		case creature_mind::Step::Kind::WaitForStill:
			ImGui::Text("Waiting for creature %u to stand still, %.1f s", step.object.value_or(0u),
			            static_cast<double>(idle.stepSeconds));
			break;
		case creature_mind::Step::Kind::ApplyDesire:
			ImGui::Text("Taking up %s about town %u", creature_desires::Name(step.desire).data(), step.object.value_or(0u));
			break;
		case creature_mind::Step::Kind::SwapMind:
			ImGui::Text("Swapping minds for %.1f s", static_cast<double>(idle.stepSeconds));
			break;
		case creature_mind::Step::Kind::Shatter:
			ImGui::Text("Breaking a rock in two");
			break;
		case creature_mind::Step::Kind::Repair:
			ImGui::Text("Mending a building");
			break;
		case creature_mind::Step::Kind::UseUp:
			ImGui::Text("Using a thing up");
			break;
		case creature_mind::Step::Kind::StopMoving:
			ImGui::TextUnformatted("Stopping where it is");
			break;
		case creature_mind::Step::Kind::Dance:
			ImGui::Text("Dance with villagers: stage %d, %u turns to dance", static_cast<int>(step.dance.kind),
			            idle.danceTurns);
			break;
		case creature_mind::Step::Kind::PracticeDance:
			ImGui::Text("Dancing alone, %u turns left", idle.danceTurns);
			break;
		case creature_mind::Step::Kind::Overlay:
			ImGui::Text("%s on top of its body", AnimationLabel(step.animation).c_str());
			break;
		case creature_mind::Step::Kind::RaiseTotem:
		case creature_mind::Step::Kind::LowerTotem:
			ImGui::Text("%s a town's totem, %.1f s",
			            step.kind == creature_mind::Step::Kind::RaiseTotem ? "Raising" : "Lowering",
			            static_cast<double>(idle.stepSeconds));
			break;
		case creature_mind::Step::Kind::Kiss:
			ImGui::Text("Kissing, %s", AnimationLabel(step.animation).c_str());
			break;
		case creature_mind::Step::Kind::CoolDown:
			ImGui::Text("Putting out its own fire");
			break;
		default:
			break;
		}
	}
	ImGui::Text("Shows a desire again in %.0f s", static_cast<double>(idle.showDesireSeconds));
	// A script's move: its last call, the leg the creature is on and whether the walk under way is carried on
	if (ImGui::TreeNode("A script's move"))
	{
		if (const auto last = minds.LastNative(ecs::systems::CreatureNative::MoveGameThing); last.has_value())
		{
			ImGui::Text("Last MOVE_GAME_THING x%u: thing %u%s to (%.1f, %.1f, %.1f) on turn %u, %.*s", last->calls,
			            last->object, last->object == entt::to_integral(entity) ? " (this creature)" : "",
			            static_cast<double>(last->point.x), static_cast<double>(last->point.y),
			            static_cast<double>(last->point.z), last->turn, static_cast<int>(last->note.size()), last->note.data());
		}
		else
		{
			ImGui::TextUnformatted("MOVE_GAME_THING: not called");
		}
		const auto isLeg = [](const creature_mind::Step& step) {
			return step.kind == creature_mind::Step::Kind::Move && step.startsWhileWalking;
		};
		if (idle.step < idle.agenda.size() && isLeg(idle.agenda[idle.step]))
		{
			const auto& leg = idle.agenda[idle.step];
			const auto legs = static_cast<size_t>(std::ranges::count_if(idle.agenda, isLeg));
			ImGui::Text("Leg %zu of %zu: to (%.1f, %.1f) within %.1f%s", idle.step + 1, legs,
			            static_cast<double>(leg.movement.point.x), static_cast<double>(leg.movement.point.y),
			            static_cast<double>(leg.movement.maxDistance), idle.stepStarted ? "" : ", not started");
			if (const auto* walk = std::as_const(registry).TryGet<const CreatureLocomotion>(entity); walk != nullptr)
			{
				constexpr float k_SamePoint = 0.1f;
				const bool carriedOn = walk->motion != CreatureLocomotion::Motion::Standing && walk->destination.has_value() &&
				                       glm::distance(*walk->destination, leg.movement.point) <= k_SamePoint;
				ImGui::Text("Walk's ring %.1f to %.1f: %s", static_cast<double>(walk->ring.min),
				            static_cast<double>(walk->ring.max),
				            carriedOn ? "walking on to the leg's point" : "not walking to the leg's point");
			}
		}
		else
		{
			ImGui::TextUnformatted("No script's move under way");
		}
		ImGui::TreePop();
	}
	// The player's creature swaps minds with this one, as a script's "swap creature" does
	if (const auto players = Locator::leashSystem::has_value()
	                             ? Locator::leashSystem::value().PlayersCreature(PlayerNames::PLAYER_ONE)
	                             : std::nullopt;
	    players.has_value() && *players != entity)
	{
		if (ImGui::Button("Swap minds with the player's creature"))
		{
			const bool swapped = minds.SwapMinds(*players, entity);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spawner: creature {} {} minds with the player's creature {}",
			                   entt::to_integral(entity), swapped ? "swapped" : "could not swap", entt::to_integral(*players));
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Each body keeps its name; the player's creature becomes this one");
		}
	}

	ImGui::SeparatorText("Body");
	// what its animations move on by each turn: the face and blink by the ms, the action and gesture by the step, the
	// breath and head by the seconds
	const auto clock = creature_turn_pose::ClockOfTurn(animation->playbackScale, static_cast<int32_t>(game_clock::MsPerTurn()),
	                                                   ecs::creature_pose::DrawnSize(std::as_const(registry), entity),
	                                                   animation->realSpeed, false);
	ImGui::Text("Each turn: %d ms, a step of %d ms, %.3f s", clock.ms, clock.step, static_cast<double>(clock.seconds));
	// a script's real time: the step is the turn's own milliseconds
	ImGui::Text("Real time (script): %s", animation->realSpeed ? "on" : "off");
	const auto& body = animation->body;
	ImGui::Text("Playing %s, %.0f ms%s", AnimationLabel(creature_layers::CurrentAnimation(body)).c_str(),
	            static_cast<double>(body.timeMs), body.mirrored ? ", mirrored" : "");
	const auto& face = animation->face;
	ImGui::Text("Face %s%s%s, %.0f ms in", face.current ? AnimationLabel(*face.current).c_str() : "none",
	            face.wanted != face.current ? " -> " : "",
	            face.wanted != face.current ? (face.wanted ? AnimationLabel(*face.wanted).c_str() : "relaxing") : "",
	            static_cast<double>(face.timeMs));
	if (face.wanted.has_value())
	{
		ImGui::Text("  pulled for %s, held %.1f s more", creature_face::Name(face.cue).data(),
		            static_cast<double>(face.remainingMs) / 1000.0);
	}
	ImGui::Text("  next face in %.1f s, variety %u, attitude to the player %+.2f",
	            static_cast<double>(std::max(idle.faceSeconds, 0.0f)), idle.faceVariety,
	            static_cast<double>(mind->attitudeToPlayer));
	ImGui::Text("Gesture %s", animation->gesture.animation ? AnimationLabel(*animation->gesture.animation).c_str() : "none");
	if (mind->look.id.has_value() && animation->lookAt.has_value())
	{
		ImGui::Text("Looks at a %s (entity %u) for %.1f s", LookKindName(mind->look.kind).data(), *mind->look.id,
		            static_cast<double>(mind->look.watchedTurns) / 10.0);
	}
	else
	{
		ImGui::Text("Looks %s", animation->lookAt.has_value() ? "ahead" : "nowhere in particular");
	}
	ImGui::Text("Head turned %+.0f degrees left, %+.0f up", static_cast<double>(glm::degrees(animation->yaw.angle)),
	            static_cast<double>(glm::degrees(animation->pitch.angle)));

	ImGui::SeparatorText("Tell it");
	const bool busy = creature_layers::IsPlaying(body);
	if (ImGui::BeginCombo("Action", AnimationLabel(animations::k_FirstAction + _action).c_str()))
	{
		for (size_t i = 0; i < animations::k_ActionCount; ++i)
		{
			if (ImGui::Selectable(AnimationLabel(animations::k_FirstAction + i).c_str(), i == _action))
			{
				_action = i;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(busy);
	if (ImGui::Button("Play"))
	{
		// Unmirrored: tossing the game's coin outside the turn would change its random numbers
		minds.PlayAction(entity, animations::k_FirstAction + _action, false);
	}
	ImGui::EndDisabled();
	if (ImGui::BeginCombo("Gesture", AnimationLabel(animations::k_FirstGesture + _gesture).c_str()))
	{
		for (size_t i = 0; i < animations::k_GestureCount; ++i)
		{
			if (ImGui::Selectable(AnimationLabel(animations::k_FirstGesture + i).c_str(), i == _gesture))
			{
				_gesture = i;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(animation->gesture.animation.has_value());
	if (ImGui::Button("Gesture"))
	{
		minds.PlayGesture(entity, animations::k_FirstGesture + _gesture);
	}
	ImGui::EndDisabled();
	ImGui::TextUnformatted("Face");
	for (size_t i = 0; i < animations::k_FaceCount; ++i)
	{
		if (i % 6 != 0)
		{
			ImGui::SameLine();
		}
		if (ImGui::SmallButton(AnimationLabel(animations::k_FirstFace + i).c_str()))
		{
			minds.PullFace(entity, animations::k_FirstFace + i);
		}
	}
	ImGui::TextUnformatted("Feel");
	ImGui::SetItemTooltip("Pulls the face the creature's mind would pull for a feeling or what it is doing");
	constexpr std::array k_Feelings {
	    creature_face::Cue::Idle,        creature_face::Cue::AttitudeToPlayer,
	    creature_face::Cue::Curiosity,   creature_face::Cue::Anger,
	    creature_face::Cue::Fear,        creature_face::Cue::Compassion,
	    creature_face::Cue::Playfulness, creature_face::Cue::Smile,
	    creature_face::Cue::Grimace,     creature_face::Cue::Amazed,
	    creature_face::Cue::Puzzled,     creature_face::Cue::Frightened,
	    creature_face::Cue::Sad,         creature_face::Cue::Exhausted,
	};
	for (size_t i = 0; i < k_Feelings.size(); ++i)
	{
		if (i % 5 != 0)
		{
			ImGui::SameLine();
		}
		if (ImGui::SmallButton(std::string(creature_face::Name(k_Feelings.at(i))).c_str()))
		{
			minds.ShowFeeling(entity, k_Feelings.at(i));
		}
	}
	ImGui::BeginDisabled(busy);
	if (ImGui::Button("Sit down"))
	{
		minds.SitDown(entity);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!creature_layers::IsLooping(body));
	if (ImGui::Button("Stand up"))
	{
		minds.StandUp(entity);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Stroke"))
	{
		minds.ReceiveFeedback(entity, 0.5f);
	}
	ImGui::SameLine();
	if (ImGui::Button("Slap"))
	{
		minds.ReceiveFeedback(entity, -0.5f);
	}

	ImGui::SeparatorText("Desires");
	{
		// the attention its god pays it, as the mind scroll tells it: the share of its turns it was looked at
		const auto* needs = std::as_const(registry).TryGet<const CreatureNeeds>(entity);
		const uint32_t lived = needs != nullptr ? needs->needs.turns : 0u;
		ImGui::Text("Looked at %u of %u turns, attention %d of 4", mind->turnsLookedAt, lived,
		            creature_attention::HowMuch(creature_attention::Share(mind->turnsLookedAt, lived)));
		ImGui::Text("Not looked at for %u turns, loneliness %.3f", mind->turnsNotLookedAt,
		            static_cast<double>(creature_attention::Loneliness(mind->turnsNotLookedAt, game_clock::MsPerTurn())));
		ImGui::Text(
		    "Sent to act %u turns ago, lack of interaction %.3f", mind->turnsSinceMadeToAct,
		    static_cast<double>(creature_attention::LackOfInteraction(mind->turnsSinceMadeToAct, game_clock::MsPerTurn())));
	}
	if (!mind->desires.has_value())
	{
		return;
	}
	if (ImGui::TreeNode("Timed dominant desire"))
	{
		const auto msPerTurn = game_clock::MsPerTurn();
		const uint32_t turnsPerSecond = msPerTurn > 0 ? 1000 / msPerTurn : 0;
		const auto rate = static_cast<float>(turnsPerSecond);
		auto& dominant = mind->dominantDesire;
		if (dominant.desire.has_value())
		{
			const auto left = creature_spell_mind::CheatTurnsLeft(dominant, rate);
			ImGui::Text("%s for %u s: %u turns kept, %.1f s left, body's needs %s",
			            creature_desires::Name(*dominant.desire).data(), dominant.seconds, dominant.turns,
			            turnsPerSecond > 0 ? static_cast<double>(left) / turnsPerSecond : 0.0,
			            dominant.needsToo ? "held too" : "free");
		}
		else
		{
			ImGui::TextUnformatted("None");
		}
		const auto chosen = static_cast<creature_desires::Desire>(_dominantDesire);
		if (ImGui::BeginCombo("Desire##dominant", creature_desires::Name(chosen).data()))
		{
			for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
			{
				const auto desire = static_cast<creature_desires::Desire>(i);
				if (ImGui::Selectable(creature_desires::Name(desire).data(), desire == chosen))
				{
					_dominantDesire = static_cast<int>(i);
				}
			}
			ImGui::EndCombo();
		}
		ImGui::InputFloat("Seconds##dominant", &_dominantSeconds, 1.0f, 60.0f, "%.1f");
		ImGui::Checkbox("Body's needs too##dominant", &_dominantNeedsToo);
		if (ImGui::Button("Set##dominant"))
		{
			const auto* creature = std::as_const(registry).TryGet<const Creature>(entity);
			const auto* info = creature != nullptr ? SpeciesInfo(creature->species) : nullptr;
			dominant = creature_spell_mind::SetCheatDominant(*mind->desires, chosen, _dominantNeedsToo, rate,
			                                                 info != nullptr ? info->desireFloor : 0.0f, _dominantSeconds);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear##dominant"))
		{
			creature_spell_mind::ClearCheatDominance(*mind->desires, dominant);
		}
		ImGui::TextWrapped("The mind counts it each turn; a leash, or a village it is tied to, sets it again");
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Training"))
	{
		ImGui::Text("Interaction magnitude %.2f", static_cast<double>(minds.GetInteractionMagnitude(entity)));
		ImGui::SetItemTooltip("How strongly the player last stroked (above 0) or slapped (below 0) it, as the scripts read it");
		ImGui::SameLine();
		if (ImGui::SmallButton("Clear##magnitude"))
		{
			minds.ClearInteractionMagnitude(entity);
		}
		if (mind->actionCounts.empty())
		{
			ImGui::TextUnformatted("No action carried out to its end yet");
		}
		const auto* tables = minds.GetTables();
		for (const auto& [action, count] : mind->actionCounts)
		{
			const auto name = tables != nullptr && action < tables->actions.size() ? tables->actions[action].name
			                                                                       : fmt::format("action {}", action);
			ImGui::Text("%s: %u", name.c_str(), count);
		}
		const auto chosen = static_cast<creature_desires::Desire>(_dominantDesire);
		if (ImGui::Button(fmt::format("Script only desire: {}##training", creature_desires::Name(chosen)).c_str()))
		{
			minds.SetOnlyDesire(entity, chosen, _dominantSeconds);
		}
		ImGui::SetItemTooltip("As SET_CREATURE_ONLY_DESIRE, with the desire and seconds chosen above");
		ImGui::SameLine();
		if (ImGui::Button("Off##training"))
		{
			minds.ClearOnlyDesire(entity);
		}
		if (ImGui::Button("Point out highlight##training"))
		{
			if (!minds.PointOutHighlight(entity))
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} has no lesson highlight near it to point out",
				                   entt::to_integral(entity));
			}
		}
		ImGui::TreePop();
	}
	const auto& desires = *mind->desires;
	ImGui::Text("All desires add up to %.2f", static_cast<double>(desires.sum));
	for (const auto desire : DesiresByStrength(desires))
	{
		const auto& state = desires[desire];
		const auto label = fmt::format("{:.2f} / {:.2f}{}", state.value, state.max, state.suppressedTurns > 0 ? " held" : "");
		ImGui::ProgressBar(state.max > 0.0f ? state.value / state.max : 0.0f, ImVec2(160.0f, 0.0f), label.c_str());
		ImGui::SameLine();
		ImGui::TextUnformatted(creature_desires::Name(desire).data());
		if (ImGui::IsItemHovered() && !state.sources.empty())
		{
			std::string sources;
			for (const auto& source : state.sources)
			{
				sources += fmt::format("source {}: {:.2f} past {:.2f}\n", source.type, source.value, source.threshold);
			}
			ImGui::SetTooltip("%s", sources.c_str());
		}
	}
}

void CreatureSpawner::DrawPlacing() noexcept
{
	const auto colour = _placing ? k_PlacingColour : k_StartColour;
	ImGui::PushStyleColor(ImGuiCol_Button, colour);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(colour.x * 1.15f, colour.y * 1.15f, colour.z * 1.15f, 1.0f));
	if (ImGui::Button(_placing ? "Stop Placing Creatures" : "Start Placing Creatures", ImVec2(-1.0f, 0.0f)))
	{
		_placing = !_placing;
		_clicked = false;
		if (_placing)
		{
			_commanding = false;
		}
	}
	ImGui::PopStyleColor(2);
	if (_placing)
	{
		ImGui::TextWrapped("Left click on the land to place a creature there");
	}
}

void CreatureSpawner::DrawCreatures() noexcept
{
	auto& registry = Locator::entitiesRegistry::value();

	std::vector<entt::entity> creatures;
	std::as_const(registry).Each<const Creature>(
	    [&creatures](entt::entity entity, const Creature&) { creatures.push_back(entity); });

	ImGui::SeparatorText("On the land");
	DrawSharedSettings();
	ImGui::Text("%zu creature%s", creatures.size(), creatures.size() == 1 ? "" : "s");
	if (_selected.has_value() && std::ranges::find(creatures, *_selected) == creatures.end())
	{
		_selected.reset();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(creatures.empty());
	if (ImGui::Button("Remove all"))
	{
		// Each goes through the game's removal, which lets go of its leash, fights, hand and lists
		for (const auto creature : creatures)
		{
			ecs::creature_removal::RemoveFromGame(creature);
		}
		creatures.clear();
		_selected.reset();
	}
	ImGui::EndDisabled();

	std::optional<entt::entity> remove;
	if (ImGui::BeginTable("Creatures", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
	                      ImVec2(0.0f, 0.0f)))
	{
		ImGui::TableSetupColumn("Species");
		ImGui::TableSetupColumn("Owner");
		ImGui::TableSetupColumn("Where");
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();
		for (const auto entity : creatures)
		{
			const auto& creature = std::as_const(registry).Get<const Creature>(entity);
			const auto* transform = std::as_const(registry).TryGet<const Transform>(entity);
			ImGui::PushID(static_cast<int>(entt::to_integral(entity)));
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(SpeciesName(creature.species).data());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(OwnerName(creature.owner).data());
			ImGui::TableNextColumn();
			if (transform != nullptr)
			{
				ImGui::Text("%.0f, %.0f", static_cast<double>(transform->position.x),
				            static_cast<double>(transform->position.z));
			}
			ImGui::TableNextColumn();
			const bool selected = _selected == entity;
			if (ImGui::SmallButton(selected ? "Selected" : "Select"))
			{
				_selected = entity;
			}
			ImGui::TableNextColumn();
			if (ImGui::SmallButton("Remove"))
			{
				remove = entity;
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if (remove.has_value())
	{
		if (_selected == *remove)
		{
			_selected.reset();
		}
		ecs::creature_removal::RemoveFromGame(*remove);
	}
}

void CreatureSpawner::DrawSharedSettings() noexcept
{
	if (Locator::creatureHairSystem::has_value())
	{
		auto& hair = Locator::creatureHairSystem::value();
		bool shown = hair.IsShown();
		if (ImGui::Checkbox("Show hair", &shown))
		{
			hair.SetShown(shown);
		}
	}
	DrawAudioSettings();
	DrawFootprintSettings();
}

void CreatureSpawner::Update() noexcept
{
	if (!_clicked)
	{
		return;
	}
	_clicked = false;
	if (_placing)
	{
		Spawn();
	}
	else if (_commanding)
	{
		Command();
	}
}

void CreatureSpawner::Command() noexcept
{
	if (!_selected.has_value() || !Locator::creatureLocomotionSystem::has_value() || !Locator::camera::has_value() ||
	    !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto hit = Locator::camera::value().RaycastMouseToLand();
	if (!hit.has_value())
	{
		_lastOrder = "No land under the click";
		return;
	}
	using Pace = ecs::systems::CreatureLocomotionSystemInterface::Pace;
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	const auto point = glm::xz(*hit);
	switch (_order)
	{
	case Order::Walk:
	case Order::Run:
	{
		const auto result = locomotion.MoveTo(*_selected, point, _order == Order::Run ? Pace::Run : Pace::Walk, 0.0f, 1.0f);
		_lastOrder = fmt::format("To {:.0f}, {:.0f}: {}", point.x, point.y, MoveResultName(result));
		break;
	}
	case Order::Flee:
		_lastOrder = fmt::format("Away from {:.0f}, {:.0f}: {}", point.x, point.y,
		                         MoveResultName(locomotion.FleeFrom(*_selected, point)));
		break;
	case Order::Face:
		_lastOrder = fmt::format("Facing {:.0f}, {:.0f}: {}", point.x, point.y,
		                         locomotion.TurnToFace(*_selected, point) ? "turning" : "can't");
		break;
	case Order::PickUp:
	case Order::Destroy:
	{
		if (!Locator::creatureObjectActionSystem::has_value())
		{
			break;
		}
		// What is nearest where the land was clicked
		auto& hands = Locator::creatureObjectActionSystem::value();
		const bool pickUp = _order == Order::PickUp;
		std::vector<debug::miracles::ThingAt> things;
		std::as_const(Locator::entitiesRegistry::value()).Each<const Transform>([&](entt::entity entity, const Transform& at) {
			if (entity != *_selected && (pickUp ? hands.CanPickUp(*_selected, entity) : hands.CanDestroy(entity)))
			{
				things.push_back({.entity = entity, .position = at.position});
			}
		});
		const auto nearest = debug::miracles::NearestThing(things, *hit, k_PickRadius);
		if (!nearest.has_value())
		{
			_lastOrder = fmt::format("Nothing to {} there", pickUp ? "pick up" : "knock down");
			break;
		}
		const auto started = pickUp ? hands.PickUp(*_selected, *nearest) : hands.Destroy(*_selected, *nearest);
		_lastOrder = fmt::format("{} entity {}: {}", pickUp ? "Picking up" : "Knocking down", entt::to_integral(*nearest),
		                         started ? "started" : "can't");
		break;
	}
	case Order::Throw:
		if (Locator::creatureObjectActionSystem::has_value())
		{
			_lastOrder =
			    fmt::format("Throwing at {:.0f}, {:.0f}: {}", point.x, point.y,
			                Locator::creatureObjectActionSystem::value().Throw(*_selected, *hit) ? "started" : "can't");
		}
		break;
	case Order::Point:
		if (Locator::creatureObjectActionSystem::has_value())
		{
			_lastOrder =
			    fmt::format("Pointing at {:.0f}, {:.0f}: {}", point.x, point.y,
			                Locator::creatureObjectActionSystem::value().PointAt(*_selected, *hit) ? "started" : "can't");
		}
		break;
	}
}

void CreatureSpawner::Spawn() noexcept
{
	if (!Locator::terrainSystem::has_value() || !Locator::pickingSystem::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	const auto hit = Locator::camera::value().RaycastMouseToLand();
	if (!hit.has_value())
	{
		return;
	}
	SpawnAt(*hit);
}

entt::entity CreatureSpawner::SpawnAt(const glm::vec3& position) noexcept
{
	const auto degrees = _randomFacing ? RandomFacingDegrees(_random) : _facingDegrees;
	// A mind from the cache is taken up on the creature's first turn of thought
	const auto entity = CreatureArchetype::Create(
	    position, _owner, _species, _spawnMind, glm::radians(degrees), _scale,
	    CreatureArchetype::Body {.alignment = _alignment, .fatness = _fatness, .strength = _strength});
	if (Locator::leashSystem::has_value())
	{
		// Made for trying things out, it knows every leash, as creatures made by the original's debug tools do
		for (const auto type : creature_leash::k_Types)
		{
			Locator::leashSystem::value().SetKnown(entity, type, true);
		}
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* tattoos = Find<CreatureTattoos>(registry, entity); tattoos != nullptr && _spawnTattoos.has_value())
	{
		tattoos->slots = *_spawnTattoos;
		++tattoos->revision;
	}
	return entity;
}

bool CreatureSpawner::TakesEvent(const SDL_Event& event) const noexcept
{
	// While placing or commanding, a left click on the land is the window's rather than the hand's
	return IsLeftClick(event) &&
	       _leftCapture.Takes(event.type == SDL_MOUSEBUTTONDOWN, _placing || (_commanding && _selected.has_value()),
	                          ImGui::GetIO().WantCaptureMouse);
}

void CreatureSpawner::ProcessEventOpen(const SDL_Event& event) noexcept
{
	if (!TakesEvent(event))
	{
		return;
	}
	const bool press = event.type == SDL_MOUSEBUTTONDOWN;
	_leftCapture.Seen(press);
	if (press)
	{
		_clicked = true;
	}
}

void CreatureSpawner::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
