/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PathFinding.h"

#include <algorithm>
#include <string>

#include <SDL.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/FootpathFinder.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDeaths.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerHiding.h"
#include "ECS/Components/VillagerInspection.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Footpaths.h"
#include "ECS/LivingFootpath.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerCover.h"
#include "ECS/Town/TownCreatureAttitude.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/WallHugBookkeeping.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::debug::gui;
namespace map_coords = openblack::map_coords;

namespace
{
/// A walker's move state, as the readout names it
const char* WalkStateName(uint8_t state)
{
	using openblack::ecs::wall_hug::WalkState;
	switch (static_cast<WalkState>(state))
	{
	case WalkState::Arrived:
		return "arrived";
	case WalkState::FinalStep:
		return "final step";
	case WalkState::Wander:
		return "wander";
	case WalkState::StepThrough:
		return "step through";
	case WalkState::Linear:
		return "linear";
	case WalkState::LinearClockwise:
		return "linear, clockwise";
	case WalkState::LinearAnticlockwise:
		return "linear, anticlockwise";
	case WalkState::OrbitClockwise:
		return "going round, clockwise";
	case WalkState::OrbitAnticlockwise:
		return "going round, anticlockwise";
	case WalkState::ExitAnticlockwise:
		return "leaving, anticlockwise";
	case WalkState::ExitClockwise:
		return "leaving, clockwise";
	}
	return "other";
}

/// An animal's walk round the circles, read only: its move state, its states, its needs' counters (breeding, hunger
/// and sleep) and the reaction it takes (its type and its thing: a creature for one it runs from), the thing whose
/// circle it goes round, and the distance to its goal its orbit began with, which a thing of many cells leaving the
/// map turns into "leave at once"
void DrawAnimalWalk(entt::entity animal, const openblack::ecs::components::AnimalBrain& brain)
{
	ImGui::Text("Animal %u: %s (%#x)", static_cast<uint32_t>(animal), WalkStateName(brain.moveState),
	            static_cast<uint32_t>(brain.moveState));
	ImGui::Text("Top state %u, final %u", static_cast<uint32_t>(brain.topState), static_cast<uint32_t>(brain.finalState));
	ImGui::Text("Needs: breed %d, hunger %d, sleep %d", brain.breed, brain.hunger, brain.sleep);
	const auto* reaction = brain.reaction != 0 ? openblack::ecs::effects::reactions::Find(brain.reaction) : nullptr;
	if (reaction != nullptr)
	{
		ImGui::Text("Reaction %u: type %u, to thing %u", brain.reaction, static_cast<uint32_t>(reaction->type),
		            static_cast<uint32_t>(brain.predator));
	}
	else
	{
		ImGui::Text("Reaction: none");
	}
	const auto& circle = brain.hugCircle;
	if (!circle.set)
	{
		ImGui::Text("Circle: none");
	}
	else if (circle.owner == entt::null)
	{
		ImGui::Text("Circle: water or the land's edge at (%.2f, %.2f) r %.2f", static_cast<double>(circle.centre.x),
		            static_cast<double>(circle.centre.y), static_cast<double>(circle.radius));
	}
	else
	{
		ImGui::Text("Circle: thing %u at (%.2f, %.2f) r %.2f", static_cast<uint32_t>(circle.owner),
		            static_cast<double>(circle.centre.x), static_cast<double>(circle.centre.y),
		            static_cast<double>(circle.radius));
	}
	if (brain.hugGoalDistance == openblack::ecs::wall_hug::k_LeaveAtOnce)
	{
		ImGui::Text("Entry distance: leave at once");
	}
	else
	{
		// kept as 128ths of a metre, less one
		ImGui::Text("Entry distance: %u (%.2f m)", brain.hugGoalDistance,
		            (static_cast<double>(brain.hugGoalDistance) + 1.0) / 128.0);
	}
}
/// A town's attitude to a creature, as the readout names it
const char* AttitudeName(uint32_t attitude)
{
	namespace town_attitude = openblack::ecs::town_creature_attitude;
	switch (attitude)
	{
	case town_attitude::k_New:
		return "not judged yet";
	case town_attitude::k_None:
		return "none";
	case town_attitude::k_Fear:
		return "fear";
	case town_attitude::k_Respect:
		return "respect";
	default:
		return "other";
	}
}

/// A villager's reaction to a creature, read only: its top and final states, the creature it reacts to, its look at
/// the creature, its town's attitude to each creature the town keeps a record of, and its town's deaths
void DrawCreatureReaction(entt::entity villager)
{
	using namespace openblack::ecs::components;
	const auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto stateName = [](uint8_t state) {
		return std::string(
		    openblack::k_VillagerStateStrings.at(std::min<size_t>(state, openblack::k_VillagerStateStrings.size() - 1)));
	};
	if (const auto* action = registry.TryGet<const LivingAction>(villager); action != nullptr)
	{
		const auto top = action->states.at(static_cast<size_t>(LivingAction::Index::Top));
		const auto final = action->states.at(static_cast<size_t>(LivingAction::Index::Final));
		ImGui::Text("Top %u %s, final %u %s", top, stateName(top).c_str(), final, stateName(final).c_str());
	}
	const auto* slot = registry.TryGet<const LivingReaction>(villager);
	if (slot != nullptr && slot->reaction != 0 && slot->type == openblack::Reaction::ReactToCreature)
	{
		ImGui::Text("Reacting to creature %u (reaction %u)", static_cast<uint32_t>(slot->object), slot->reaction);
	}
	else
	{
		ImGui::Text("Not reacting to a creature");
	}
	if (const auto* inspection = registry.TryGet<const VillagerInspection>(villager); inspection != nullptr)
	{
		ImGui::Text("Last look: from %.2f m, %u turns left", static_cast<double>(inspection->distance),
		            inspection->turnsUntilApproach);
	}
	const auto* component = registry.TryGet<const Villager>(villager);
	const auto town = component != nullptr ? component->town : entt::entity(entt::null);
	const auto* records = town != entt::null && registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	if (records == nullptr)
	{
		ImGui::Text("No town");
		return;
	}
	if (records->creatureRecords.empty())
	{
		ImGui::Text("Its town keeps no record of a creature");
	}
	for (const auto& record : records->creatureRecords)
	{
		ImGui::Text("Town's attitude to creature %u: %s (%u), since turn %u", static_cast<uint32_t>(record.creature),
		            AttitudeName(record.attitude), record.attitude, record.setTurn);
	}
	// its town's deaths: a person a creature eats is counted with no reason, put down to the creature's player
	if (const auto* deaths = registry.TryGet<const TownDeaths>(town); deaths != nullptr)
	{
		ImGui::Text("Town's deaths: %u, the last at turn %u; with no reason %u", deaths->count, deaths->lastDeathTurn,
		            deaths->byReason.at(static_cast<size_t>(openblack::DeathReason::None)));
	}
}
} // namespace

PathFinding::PathFinding() noexcept
    : Window("Path Finding", ImVec2(400, 400))
{
}

void PathFinding::Draw() noexcept
{
	ImGui::TextWrapped(
	    "Select Villager by left clicking on it, then select a pathfinding action and right click the destination. "
	    "Left click on an animal to read its walk round the circles");

	const uint8_t numChildren = 2;
	const float rounding = 5.0f;
	const auto size = ImGui::GetWindowSize();
	const auto pos = ImGui::GetCursorPos();
	const float halfHeight = (size.y - pos.y) / numChildren - 2 * rounding;

	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, rounding);
	ImGui::BeginChild("Selected Villager", ImVec2(0, halfHeight), ImGuiChildFlags_Borders);
	if (_selectedVillager.has_value())
	{
		using namespace ecs::components;
		auto& registry = Locator::entitiesRegistry::value();

		ImGui::Columns(2);

		ImGui::Text("Villager %u", static_cast<uint32_t>(*_selectedVillager));
		ImGui::NextColumn();

		if (ImGui::Button("Clear Selection"))
		{
			_selectedVillager.reset();
		}
		ImGui::NextColumn();

		ImGui::Columns(1);

		if (_selectedVillager.has_value())
		{
			auto& transform = registry.Get<Transform>(_selectedVillager.value());
			auto& wallHug = registry.Get<WallHug>(_selectedVillager.value());
			ImGui::DragFloat3("Position", glm::value_ptr(transform.position));
			ImGui::DragFloat2("Goal", glm::value_ptr(wallHug.goal));
			ImGui::DragFloat("Speed", &wallHug.speed);
			// the circle it heads for or goes round: whose, which of its circles, where, and the turns to it
			if (const auto* hugged = registry.TryGet<const WallHugObjectReference>(_selectedVillager.value());
			    hugged != nullptr && hugged->landscape)
			{
				ImGui::Text("Circle: water or the land's edge, cell (%d, %d) at (%.2f, %.2f), %u turns", hugged->cell.x,
				            hugged->cell.y, hugged->centre.x, hugged->centre.y, static_cast<uint32_t>(hugged->stepsAway));
			}
			else if (hugged != nullptr)
			{
				ImGui::Text("Circle: thing %u, circle %u at (%.2f, %.2f) r %.2f, %u turns",
				            static_cast<uint32_t>(hugged->entity), static_cast<uint32_t>(hugged->circle), hugged->centre.x,
				            hugged->centre.y, hugged->radius, static_cast<uint32_t>(hugged->stepsAway));
			}
			else
			{
				ImGui::Text("Circle: none");
			}
			ImGui::Text("Handed over to water, the land's edge or a fence this turn: %s",
			            wallHug.needsLookahead ? "yes" : "no");
			// hiding from a fright: the building, its turns, what it hid from and the reaction whose row it reads
			if (const auto* hiding = registry.TryGet<const ecs::components::VillagerHiding>(_selectedVillager.value()))
			{
				ImGui::Text("Hiding in %u, %u turns, from %u, by the row of reaction %d",
				            static_cast<uint32_t>(hiding->building), hiding->turns, static_cast<uint32_t>(hiding->threat),
				            static_cast<int>(ecs::villager_cover::HidingReactionKind(_selectedVillager.value())));
			}
			// closed by default
			if (ImGui::CollapsingHeader("Reaction to a creature"))
			{
				DrawCreatureReaction(_selectedVillager.value());
			}
		}
	}
	else
	{
		ImGui::Text("No villager selected");
	}
	if (_selectedAnimal.has_value())
	{
		const auto& registry = Locator::entitiesRegistry::value();
		const auto* brain =
		    registry.Valid(*_selectedAnimal) ? registry.TryGet<const ecs::components::AnimalBrain>(*_selectedAnimal) : nullptr;
		ImGui::Separator();
		if (brain != nullptr)
		{
			DrawAnimalWalk(*_selectedAnimal, *brain);
		}
		else
		{
			ImGui::Text("Animal %u: gone, or not yet taken a turn", static_cast<uint32_t>(*_selectedAnimal));
		}
	}
	ImGui::EndChild();

	ImGui::BeginChild("Actions", ImVec2(0, halfHeight), ImGuiChildFlags_Borders);
	if (_selectedVillager.has_value())
	{
		if (ImGui::BeginTabBar("Actions"))
		{
			using namespace ecs::components;
			auto& registry = Locator::entitiesRegistry::value();
			if (ImGui::BeginTabItem("Teleport"))
			{
				ImGui::DragFloat3("Destination", glm::value_ptr(_destination));

				ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
				                    ImGui::GetStyle().Alpha * (_selectedVillager.has_value() ? 1.0f : 0.5f));
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !_selectedVillager.has_value());
				if (ImGui::Button("Execute"))
				{
					registry.Get<Transform>(*_selectedVillager).position = _destination;
				}
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Move To Point"))
			{
				ImGui::DragFloat3("Destination", glm::value_ptr(_destination));

				ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
				                    ImGui::GetStyle().Alpha * (_selectedVillager.has_value() ? 1.0f : 0.5f));
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !_selectedVillager.has_value());
				if (ImGui::Button("Execute"))
				{
					// the walk round obstacles, aimed and looking ahead at once
					ecs::villager::SetupMobileMoveToPos(*_selectedVillager, glm::xz(_destination), MoveState::Linear);
				}
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Move On Footpath"))
			{
				ImGui::DragFloat3("Destination", glm::value_ptr(_destination));
				ImGui::Columns(2);
				if (_selectedFootpath.has_value())
				{
					ImGui::Text("Footpath %u", static_cast<uint32_t>(*_selectedFootpath));
				}
				else
				{
					ImGui::TextWrapped("No footpath selected, left click near one");
				}
				// The links and the things that hold one: a multi-map fixed object deleted takes its link with it, unless
				// the rebuild plan made from it took the link first
				size_t links = 0;
				size_t held = 0;
				const auto& lookup = Locator::entitiesRegistry::value();
				lookup.Each<const FootpathLink>([&links](entt::entity, const FootpathLink&) { ++links; });
				lookup.Each<const FootpathLinkOf>([&held](entt::entity, const FootpathLinkOf&) { ++held; });
				ImGui::Text("%zu footpath links, %zu held by things", links, held);
				// the footpaths a worship site is still planning from its towns' storage pits, a few planner steps a turn
				size_t planning = 0;
				lookup.Each<const FootpathFinder>([&planning](entt::entity, const FootpathFinder&) { ++planning; });
				ImGui::Text("%zu footpaths being planned", planning);
				ImGui::NextColumn();

				if (ImGui::Button("Clear Selection"))
				{
					_selectedFootpath.reset();
				}
				ImGui::NextColumn();

				ImGui::Columns(1);

				ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
				                    ImGui::GetStyle().Alpha * (_selectedFootpath.has_value() ? 1.0f : 0.5f));
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !_selectedFootpath.has_value());
				if (ImGui::Button("Execute") && _selectedFootpath.has_value())
				{
					// as the game sends a villager along a link's footpath: the way whose end is nearer the destination,
					// the node nearest the villager, and the state it is in now to take up again at the far end
					const auto footpath = *_selectedFootpath;
					const auto from = ecs::living_footpath::PosOf(*_selectedVillager);
					const auto direction = ecs::footpaths::DirectionToward(footpath, map_coords::FromWorld(_destination));
					const auto node = ecs::footpaths::GetNearestPos(footpath, from, direction);
					const auto* action = registry.TryGet<const LivingAction>(*_selectedVillager);
					const auto final = action != nullptr ? action->states.at(static_cast<size_t>(LivingAction::Index::Top))
					                                     : static_cast<uint8_t>(VillagerStates::InvalidState);
					if (ecs::living_footpath::SetupMoveOnFootpath(*_selectedVillager, footpath, direction, final, node) == 0)
					{
						SPDLOG_LOGGER_WARN(spdlog::get("game"), "Villager {} found no node of footpath {} to walk on",
						                   static_cast<uint32_t>(*_selectedVillager), static_cast<uint32_t>(footpath));
					}
				}
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
	}
	else
	{
		ImGui::Text("No action available");
	}
	ImGui::EndChild();

	ImGui::PopStyleVar();
}

void PathFinding::Update() noexcept
{
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	auto const& meshes = Locator::resources::value().GetMeshes();

	std::optional<glm::vec3> handPosition;
	const auto positions = Locator::handSystem::value().GetPlayerHandPositions();
	if (positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)] ||
	    positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Right)])
	{
		handPosition = positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)].value_or(
		    positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Right)].value_or(glm::zero<glm::vec3>()));
	}

	if (handPosition.has_value())
	{
		glm::bvec3 invalidValue {};
		for (glm::length_t l = 0; l < std::remove_reference_t<decltype(*handPosition)>::length(); ++l)
		{
			invalidValue[l] = glm::isnan((*handPosition)[l]) || glm::isinf((*handPosition)[l]);
		}
		if (glm::any(invalidValue))
		{
			return;
		}
		_handPosition = *handPosition;
		_handPosition.y = Locator::terrainSystem::value().GetHeightAt(glm::xz(_handPosition));
	}

	if (_handTo == HandTo::Entity)
	{
		bool found = false;

		if (!found)
		{
			// the mobile list of the hand's map cell, from its head (ecs::map_cells)
			for (const auto entity : ecs::map_cells::MobileInCell(map_coords::CellOf(_handPosition)))
			{
				const bool animal = registry.AllOf<ecs::components::Animal>(entity);
				if (registry.AllOf<Villager>(entity) || animal)
				{
					const auto& mesh = meshes.Handle(registry.Get<Mesh>(entity).id);
					const auto& transform = registry.Get<Transform>(entity);
					auto vectorTo = transform.position - _handPosition;
					const auto& bb = mesh->GetBoundingBox();
					auto point = glm::transpose(transform.rotation) * vectorTo;
					point.y = bb.Center().y; // Fix to ignore altitude
					if (bb.Contains(point))
					{
						(animal ? _selectedAnimal : _selectedVillager) = entity;
						found = true;
					}
				}
			}
		}

		if (!found)
		{
			registry.Each<Footpath>([this, &found](entt::entity entity, const Footpath& footpath) {
				if (found)
				{
					return;
				}
				for (int32_t i = 0; i < static_cast<int32_t>(footpath.nodes.size()) - 1; ++i)
				{
					const auto distance2Threshold = 4.0f;

					const auto& p1 = footpath.nodes[i].position;
					const auto& p2 = footpath.nodes[i + 1].position;
					const auto p1p2 = p2 - p1;

					const float t2 = glm::dot(_handPosition - p1, p1p2) / glm::length2(p1p2);
					const auto t = glm::clamp(t2, 0.0f, 1.0f);
					const auto proj = p1 + t * p1p2;

					if (glm::distance2(_handPosition, proj) < distance2Threshold)
					{
						_selectedFootpath = entity;
						found = true;
						return;
					}
				}
			});
		}
	}
	else if (_handTo == HandTo::Destination)
	{
		if (handPosition.has_value())
		{
			_destination = *handPosition;
		}
	}
	_handTo = HandTo::None;
}

void PathFinding::ProcessEventOpen(const SDL_Event& event) noexcept
{
	const ImGuiIO& io = ImGui::GetIO();
	switch (event.type)
	{
	default:
		break;
	case SDL_MOUSEBUTTONDOWN:
	{
		if (!io.WantCaptureMouse)
		{
			if (event.button.button == SDL_BUTTON_LEFT)
			{
				_handTo = HandTo::Entity;
			}
			else if (event.button.button == SDL_BUTTON_RIGHT)
			{
				_handTo = HandTo::Destination;
			}
		}
	}
	break;
	}
}

void PathFinding::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
