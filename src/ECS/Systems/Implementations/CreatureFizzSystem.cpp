/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureFizzSystem.h"

#include <utility>
#include <vector>

#include "Audio/Audio.h"
#include "Creature/CreatureFizzLook.h"
#include "ECS/Components/CreatureFizz.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/CreatureRemoval.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The teleport's energise, played where the creature stands as it starts fizzing right out of sight: once, in 3D,
/// from the in-game bank
constexpr int k_EnergiseSample = 39;
constexpr int k_EnergiseMode = 2;

/// The creature is drawn as fizzed as its fizz says
void Show(ecs::Registry& registry, entt::entity creature, float fizz)
{
	auto* spells = registry.TryGet<CreatureSpells>(creature);
	if (spells == nullptr)
	{
		spells = &registry.AssignState<CreatureSpells>(creature);
	}
	spells->fizz = fizz;
}
} // namespace

void CreatureFizzSystem::SetFizz(entt::entity creature, float target, float seconds, bool goesForGood)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	auto* current = registry.TryGet<CreatureFizz>(creature);
	const auto set =
	    creature_fizz::SetFizz(current != nullptr ? current->fizz : creature_fizz::Fizz {}, target, seconds, goesForGood);
	if (set.sound && Locator::soundTagSystem::has_value())
	{
		audio::tags::CreateAtMapCoords(ecs::object::MapCoordsOf(creature), k_EnergiseSample, false, k_EnergiseMode, 0, false,
		                               true, audio::SfxBank::InGame, 0);
	}
	// It is drawn so at the next turn, which shows the fizz before it moves on
	registry.AssignOrReplaceState<CreatureFizz>(creature, CreatureFizz {.fizz = set.fizz});
}

void CreatureFizzSystem::ProcessTurn()
{
	// Read through the const registry, so that a game with nothing fizzing makes no storage for it
	std::vector<entt::entity> fizzing;
	std::as_const(Locator::entitiesRegistry::value())
	    .Each<const CreatureFizz>([&fizzing](entt::entity entity, const CreatureFizz&) { fizzing.push_back(entity); });
	BeginCreatureTurns();
	for (const auto entity : fizzing)
	{
		ProcessCreatureTurn(entity);
	}
	EndCreatureTurns();
}

void CreatureFizzSystem::BeginCreatureTurns()
{
	_turnMs = game_clock::MsPerTurn();
	_gone.clear();
}

void CreatureFizzSystem::ProcessCreatureTurn(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(creature) || !registry.AllOf<CreatureFizz>(creature))
	{
		return;
	}
	// First it is drawn as fizzed as it is now, so the drawn fizz trails the moving one by a turn
	Show(registry, creature, lookup.Get<const CreatureFizz>(creature).fizz.now);
	auto& fizz = registry.Get<CreatureFizz>(creature).fizz;
	// Back in full sight with nothing left to do, it gives its fizz up
	if (creature_fizz::Settled(fizz))
	{
		registry.RemoveState<CreatureFizz>(creature);
		return;
	}
	// Then a fizz that is moving moves on; only one that has just fizzed right out for good goes, so one put right out
	// of sight at once stays
	if (fizz.perSecond != 0.0f)
	{
		fizz = creature_fizz::Step(fizz, _turnMs);
		if (creature_fizz::Gone(fizz))
		{
			_gone.push_back(creature);
		}
	}
}

bool CreatureFizzSystem::FizzedOut(entt::entity creature) const
{
	return std::ranges::find(_gone, creature) != _gone.end();
}

void CreatureFizzSystem::EndCreatureTurns()
{
	auto& registry = Locator::entitiesRegistry::value();
	// Fizzed right out for good, it leaves the game the way any deleted creature does
	for (const auto entity : _gone)
	{
		if (!ecs::creature_removal::RemoveFromGame(entity))
		{
			registry.Destroy(entity);
		}
	}
	if (!_gone.empty())
	{
		registry.SetDirty();
	}
	_gone.clear();
}

void CreatureFizzSystem::UpdateFrame(float seconds)
{
	// The eyes' static slides on every frame, whether any creature is fizzing or not
	_eyeStaticScroll = creature_fizz_look::Scroll(_eyeStaticScroll, seconds);
}

glm::vec2 CreatureFizzSystem::EyeStaticScroll() const
{
	return _eyeStaticScroll;
}

float CreatureFizzSystem::FizzOf(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return 0.0f;
	}
	if (const auto* component = registry.TryGet<const CreatureFizz>(creature))
	{
		return component->fizz.now;
	}
	return 0.0f;
}
