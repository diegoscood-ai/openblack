/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdio>
#include <cstdlib>

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Transform.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::animal_ai
{
using components::Animal;
using components::AnimalBrain;
using components::Life;
using components::Transform;

namespace
{
std::optional<entt::entity> NthAnimal(int wanted)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::optional<entt::entity> found;
	int index = 0;
	// OPENBLACK_TEST_ANIMAL_SPECIES=<AnimalInfo>: count only that species
	static const char* species = std::getenv("OPENBLACK_TEST_ANIMAL_SPECIES");
	registry.Each<const Animal, const Transform>([&](entt::entity e, const Animal& animal, const Transform&) {
		if (species != nullptr && static_cast<int>(animal.type) != std::atoi(species))
		{
			return;
		}
		if (!found && index++ == wanted)
		{
			found = e;
		}
	});
	return found;
}
} // namespace

/// Test hooks, once per turn (docs/bw1-notes/animals.md):
/// - OPENBLACK_TEST_VIEW_ANIMAL="n[,distance[,angle[,every]]]": the camera flies to the n-th animal from that many
///   metres (default 6), from that side (degrees around it, 0 = +z), slightly above; again every that many turns.
/// - OPENBLACK_TEST_THROW_ANIMAL="n,turn[,vx,vy,vz]": at that turn the n-th animal is thrown with that velocity.
/// - OPENBLACK_TEST_KILL_ANIMAL="n,turn": at that turn the n-th animal loses its life (DestroyedByEffect).
/// - OPENBLACK_TEST_ANIMAL_SPECIES=<AnimalInfo>: n counts only the animals of that species (4 sheep, 8 cow...).
/// - OPENBLACK_TEST_HUNGRY=<AnimalInfo>: at turn 1 every animal of that species is hungry (0 lion, 1 tiger, 2 wolf).
/// - OPENBLACK_TEST_SPREAD_REACTIONS=<turn>: at that turn every predator spreads its flee reaction again (what the
///   original's disabled Reaction::ProcessReactions would do).
/// - OPENBLACK_ANIMAL_TRACE=1: every state change, and every 50 turns how many animals are in each state.
void RunDebugHooks(uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const char* view = std::getenv("OPENBLACK_TEST_VIEW_ANIMAL"); view != nullptr && Locator::camera::has_value())
	{
		int wanted = 0;
		float distance = 6.0f;
		float angle = 0.0f;
		unsigned every = 0;
		std::sscanf(view, "%d,%f,%f,%u", &wanted, &distance, &angle, &every);
		// OPENBLACK_TEST_VIEW_LOCK=1: the camera is put there every turn (no flight), for fast animals such as birds
		static const bool lock = std::getenv("OPENBLACK_TEST_VIEW_LOCK") != nullptr;
		const bool aim = lock || turn == 0 || (every != 0 && turn % every == 0);
		if (const auto e = NthAnimal(wanted); e && aim)
		{
			const auto& t = registry.Get<const Transform>(*e);
			const float radians = glm::radians(angle);
			const glm::vec3 focus = t.position + glm::vec3(0.0f, 0.8f, 0.0f);
			const glm::vec3 origin = focus + glm::vec3(std::sin(radians) * distance, distance * 0.35f, std::cos(radians) * distance);
			if (lock)
			{
				Locator::camera::value().SetOrigin(origin).SetFocus(focus);
			}
			else
			{
				Locator::camera::value().GetModel().SetFlight(origin, focus);
			}
			if (!lock || turn % 50 == 0)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal view: animal {} (entity {}) at ({:.1f}, {:.1f}, {:.1f}) state {}",
				                   wanted, static_cast<uint32_t>(*e), t.position.x, t.position.y, t.position.z,
				                   static_cast<int>(TopState(*e)));
			}
		}
	}
	if (const char* throwTest = std::getenv("OPENBLACK_TEST_THROW_ANIMAL"); throwTest != nullptr)
	{
		int wanted = 0;
		unsigned when = 0;
		glm::vec3 velocity(0.0f, 8.0f, 4.0f);
		std::sscanf(throwTest, "%d,%u,%f,%f,%f", &wanted, &when, &velocity.x, &velocity.y, &velocity.z);
		if (turn == when)
		{
			if (const auto e = NthAnimal(wanted); e)
			{
				auto& transform = registry.Get<Transform>(*e);
				transform.position.y += 1.0f;
				const bool thrown = physics::PhysicsObjects::AddObject(*e, velocity, glm::vec3(2.0f, 0.0f, 1.0f), entt::null, true) != nullptr;
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal test: animal {} thrown {}", wanted, thrown);
			}
		}
	}
	if (const char* kill = std::getenv("OPENBLACK_TEST_KILL_ANIMAL"); kill != nullptr)
	{
		int wanted = 0;
		unsigned when = 0;
		std::sscanf(kill, "%d,%u", &wanted, &when);
		if (turn == when)
		{
			if (const auto e = NthAnimal(wanted); e)
			{
				(registry.AllOf<Life>(*e) ? registry.Get<Life>(*e) : registry.Assign<Life>(*e)).value = 0.0f;
				DestroyedByEffect(*e);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal test: animal {} (entity {}) killed", wanted, static_cast<uint32_t>(*e));
			}
		}
	}
	if (const char* hungry = std::getenv("OPENBLACK_TEST_HUNGRY"); hungry != nullptr && turn == 1)
	{
		const int wanted = std::atoi(hungry);
		registry.Each<const Animal, AnimalBrain>([wanted](entt::entity, const Animal& animal, AnimalBrain& brain) {
			if (static_cast<int>(animal.type) == wanted)
			{
				brain.hunger = static_cast<int16_t>(Locator::infoConstants::value().animal.at(static_cast<size_t>(wanted)).hunger);
			}
		});
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal test: species {} hungry", wanted);
	}
	if (const char* spread = std::getenv("OPENBLACK_TEST_SPREAD_REACTIONS"); spread != nullptr && turn == static_cast<uint32_t>(std::atoi(spread)))
	{
		std::vector<entt::entity> animals;
		registry.Each<const Animal>([&animals](entt::entity e, const Animal&) { animals.push_back(e); });
		for (const auto e : animals)
		{
			SpreadPredatorReaction(e);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal test: predator reactions spread");
	}
	if (std::getenv("OPENBLACK_ANIMAL_TRACE") != nullptr && turn % 50 == 0)
	{
		std::map<int, int> states;
		registry.Each<const AnimalBrain>([&states](entt::entity, const AnimalBrain& brain) { ++states[brain.topState]; });
		std::string line;
		for (const auto& [state, count] : states)
		{
			line += fmt::format(" {}:{}", state, count);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animals turn {}: state:count{}", turn, line);
	}
}

} // namespace openblack::ecs::animal_ai
