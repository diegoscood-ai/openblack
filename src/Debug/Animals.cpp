/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Animals.h"

#include <cstddef>
#include <cstdint>

#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <glm/vec3.hpp>
#include <imgui.h>

#include "AnimalsModel.h"
#include "ECS/AnimalAI.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Villager/VillagerHome.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "MiraclesModel.h"

using namespace openblack;
using namespace openblack::debug::gui;

namespace
{
using ecs::components::Animal;
using ecs::components::AnimalBrain;
using ecs::components::Flock;
using ecs::components::Transform;

/// An entity's number, or none
std::string EntityText(entt::entity entity)
{
	return entity == entt::null ? std::string("none") : fmt::format("{}", entt::to_integral(entity));
}

void Line(const std::string& text)
{
	ImGui::TextUnformatted(text.c_str());
}

/// A state with the state table's name for it, when the table is loaded
std::string StateText(uint8_t state)
{
	if (!Locator::infoConstants::has_value())
	{
		return fmt::format("{}", state);
	}
	const auto& table = Locator::infoConstants::value().animalStateTable;
	if (state >= table.size())
	{
		return fmt::format("{}", state);
	}
	return debug::animals::StateLabel(state, table.at(state).name);
}

/// Where the player's hand is, the right one first
std::optional<glm::vec3> HandPosition()
{
	if (!Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	using Side = ecs::systems::HandSystemInterface::Side;
	const auto positions = Locator::handSystem::value().GetPlayerHandPositions();
	return debug::miracles::HandPoint(positions.at(static_cast<size_t>(Side::Left)),
	                                  positions.at(static_cast<size_t>(Side::Right)));
}

/// Whether the hunted thing can be got at now: a villager not at home, in the hand or hiding; an animal still there
bool Reachable(const ecs::Registry& registry, entt::entity prey)
{
	if (prey == entt::null || !registry.Valid(prey))
	{
		return false;
	}
	if (registry.AllOf<ecs::components::Villager>(prey))
	{
		return ecs::villager::IsReachable(prey);
	}
	return ecs::animal_ai::detail::Available(prey);
}
} // namespace

Animals::Animals() noexcept
    : Window("Animals", ImVec2(440.0f, 560.0f))
{
}

void Animals::Draw() noexcept
{
	if (!Locator::entitiesRegistry::has_value())
	{
		ImGui::TextUnformatted("No animals: no land loaded");
		return;
	}
	const ecs::Registry& registry = Locator::entitiesRegistry::value();

	std::vector<debug::animals::AnimalAt> animals;
	registry.Each<const Animal, const AnimalBrain, const Transform>(
	    [&animals](entt::entity entity, const Animal&, const AnimalBrain&, const Transform& transform) {
		    animals.push_back({.entity = entity, .position = transform.position});
	    });

	ImGui::Checkbox("Keep this animal", &_keep);
	const bool keptThere =
	    _kept != entt::null && registry.Valid(_kept) && registry.AllOf<Animal, AnimalBrain, Transform>(_kept);
	if (!_keep || !keptThere)
	{
		const auto hand = HandPosition();
		const auto nearest = hand ? debug::animals::Nearest(animals, *hand) : std::nullopt;
		_kept = nearest.value_or(entt::null);
	}
	if (_kept == entt::null)
	{
		Line(fmt::format("{} animals; none near the hand", animals.size()));
		return;
	}

	const auto& animal = registry.Get<const Animal>(_kept);
	const auto& transform = registry.Get<const Transform>(_kept);
	Line(fmt::format("Animal {}: {}, of {} on the land", EntityText(_kept), debug::animals::SpeciesName(animal.type),
	                 animals.size()));
	Line(fmt::format("At ({:.1f}, {:.1f}, {:.1f})", transform.position.x, transform.position.y, transform.position.z));

	DrawStates(_kept);
	DrawReaction(_kept);
	DrawTargets(_kept);
	DrawFlock(_kept);
}

void Animals::DrawStates(entt::entity entity) const noexcept
{
	const ecs::Registry& registry = Locator::entitiesRegistry::value();
	const auto& brain = registry.Get<const AnimalBrain>(entity);
	ImGui::SeparatorText("States");
	Line(fmt::format("Top state: {}", StateText(brain.topState)));
	Line(fmt::format("Final state: {}", StateText(static_cast<uint8_t>(ecs::animal_ai::FinalState(entity)))));
	Line(fmt::format("Previous state: {}", StateText(brain.previousState)));
	Line(fmt::format("Turns since the state changed: {}", brain.turnsSinceStateChange));

	ImGui::SeparatorText("Movement");
	const auto direction = ecs::animal_ai::MovementDirection(entity);
	Line(fmt::format("Heading: {} ({:.1f} degrees), speed {}", brain.angle, debug::animals::HeadingDegrees(brain.angle),
	                 brain.speed));
	Line(fmt::format("Moving along ({:.3f}, {:.3f}, {:.3f}), {:.3f} m last turn", direction.x, direction.y, direction.z,
	                 brain.movedLastTurn));
	Line(fmt::format("Goal ({:.1f}, {:.1f}), move state {}", brain.goal.x, brain.goal.y, brain.moveState));
}

void Animals::DrawReaction(entt::entity entity) const noexcept
{
	namespace reactions = ecs::effects::reactions;
	const ecs::Registry& registry = Locator::entitiesRegistry::value();
	const auto& brain = registry.Get<const AnimalBrain>(entity);
	ImGui::SeparatorText("Reaction");
	Line(fmt::format("In a reaction's state: {}", ecs::animal_ai::detail::IsReactionState(brain.topState) ? "yes" : "no"));
	if (brain.reaction == 0)
	{
		ImGui::TextUnformatted("Reaction: none");
		return;
	}
	const auto* reaction = reactions::Find(brain.reaction);
	if (reaction == nullptr)
	{
		Line(fmt::format("Reaction {}: gone", brain.reaction));
		return;
	}
	const auto type = static_cast<uint8_t>(reaction->type);
	Line(fmt::format("Reaction {}: {} ({}), from {}, {}", brain.reaction, debug::miracles::ReactionName(reaction->type),
	                 static_cast<int>(reaction->type), EntityText(reaction->initiator),
	                 reactions::IsAvailable(reaction) ? "available" : "not available"));
	Line(fmt::format("Reacting to: {}", EntityText(brain.predator)));
	// The switch to another reaction weighs how long this one has lasted, from the record of its type
	const auto since = reactions::RecordTurn(entity, type);
	const auto now = reactions::Turn();
	Line(fmt::format("Held since turn {} ({} turns); may switch to the same type: {}", since, now >= since ? now - since : 0,
	                 reactions::SameTypeSwitch(type) ? "yes" : "no"));
}

void Animals::DrawTargets(entt::entity entity) const noexcept
{
	const ecs::Registry& registry = Locator::entitiesRegistry::value();
	const auto& brain = registry.Get<const AnimalBrain>(entity);
	ImGui::SeparatorText("Targets");
	Line(
	    fmt::format("Prey: {}{}", EntityText(brain.target),
	                brain.target == entt::null ? "" : (Reachable(registry, brain.target) ? ", reachable" : ", not reachable")));
	Line(fmt::format("Prey seen at ({:.1f}, {:.1f}), chase started on turn {}", brain.preyCell.x, brain.preyCell.y,
	                 brain.chaseStart));
	Line(fmt::format("Food: {}", EntityText(brain.foodTarget)));

	ImGui::SeparatorText("Needs");
	Line(fmt::format("Hunger {}, sleep {}, breed {}, eat counter {}", brain.hunger, brain.sleep, brain.breed, brain.counter));
	Line(fmt::format("Sleep cell ({}, {}), status {:#x}", brain.sleepCell.x, brain.sleepCell.y, brain.status));
}

void Animals::DrawFlock(entt::entity entity) const noexcept
{
	const ecs::Registry& registry = Locator::entitiesRegistry::value();
	const auto& animal = registry.Get<const Animal>(entity);
	ImGui::SeparatorText("Flock");
	const auto* flock =
	    animal.flock != entt::null && registry.Valid(animal.flock) ? registry.TryGet<const Flock>(animal.flock) : nullptr;
	if (flock == nullptr)
	{
		ImGui::TextUnformatted("Flock: none");
		return;
	}
	const auto leader = flock->members.empty() ? entt::entity {entt::null} : flock->members.front();
	Line(fmt::format("Flock {} (script id {}): {} members, at most {}, leader {}{}", EntityText(animal.flock), flock->id,
	                 flock->members.size(), flock->maxMembers, EntityText(leader), leader == entity ? " (this one)" : ""));
	Line(fmt::format("Calm {}, leader's turns {}", flock->calm, flock->leaderTurns));
	// A predator's lair is its flock's domain centre
	Line(fmt::format("Domain (the lair of a predator): ({:.1f}, {:.1f}, {:.1f}), radius {}", flock->domainCentre.x,
	                 flock->domainCentre.y, flock->domainCentre.z, flock->domainRadius));
}
