/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureRemoval.h"

#include <algorithm>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "Audio/AudioManagerInterface.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/HandOnCreature.h"
#include "ECS/Components/PlayerCreatures.h"
#include "ECS/MapCells.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "EngineConfig.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

/// The game's steps, through its systems; each is skipped when its system isn't there
class GameWorld final: public creature_removal::WorldInterface
{
public:
	void LetGoFromHand(entt::entity creature) override
	{
		if (Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().GetCreature() == creature)
		{
			// What the stroke or slap came to is not wanted: the creature is going, and the camera on it gives way
			static_cast<void>(Locator::creatureHandSystem::value().Release());
			Locator::creatureHandSystem::value().GiveCameraBack();
		}
		if (Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().GetHeld() == creature)
		{
			Locator::handGrabSystem::value().ForceDrop();
		}
	}

	void EndFight(entt::entity creature) override
	{
		if (Locator::creatureFightSystem::has_value())
		{
			Locator::creatureFightSystem::value().Withdraw(creature);
		}
	}

	void KeepForNextLand(entt::entity creature) override
	{
		// only the local player's creature, with a profile to keep it in; and never in a run of the checks, which read a
		// game folder nothing may be written to
		if (!Locator::config::has_value() || !Locator::entitiesRegistry::has_value())
		{
			return;
		}
		const auto& config = Locator::config::value();
		const auto* body = Locator::entitiesRegistry::value().TryGet<const Creature>(creature);
		if (config.checkRun || player_creature::ProfileCreatureFile().empty() || body == nullptr ||
		    body->owner != PlayerNames::PLAYER_ONE)
		{
			return;
		}
		player_creature::SaveCreature(creature);
	}

	void TakeOffLeash(entt::entity creature) override
	{
		if (Locator::leashSystem::has_value())
		{
			auto& leash = Locator::leashSystem::value();
			leash.SetLeashable(creature, false);
			// The game lets go of its owner's leash as the creature goes, whether or not one was on: with none, the gesture
			// path of a local owner's hand is wiped all the same
			leash.ReleaseUnleashed(creature);
		}
	}

	void ReleaseEffects(entt::entity creature) override
	{
		if (Locator::magicSystem::has_value())
		{
			Locator::magicSystem::value().ReleaseCreatureCast(creature);
		}
		// Its sounds lose their owner
		if (Locator::audio::has_value())
		{
			Locator::audio::value().OnThingDeleted(creature);
		}
	}

	void RemoveReactions(entt::entity creature) override
	{
		if (Locator::reactionSystem::has_value())
		{
			Locator::reactionSystem::value().RemoveFrom(creature);
		}
	}

	void StopActing(entt::entity creature) override
	{
		if (Locator::creatureObjectActionSystem::has_value())
		{
			auto& actions = Locator::creatureObjectActionSystem::value();
			actions.Cancel(creature);
			actions.Drop(creature);
		}
		if (Locator::creatureLocomotionSystem::has_value())
		{
			Locator::creatureLocomotionSystem::value().Stop(creature);
		}
	}

	void LeaveView(entt::entity creature) override
	{
		if (Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().GetCreature() == creature)
		{
			Locator::creatureModeSystem::value().Leave();
		}
	}

	void RemoveFromWorld(entt::entity creature) override
	{
		// Deleted at once: out of the physics and its place on the map, then its fire
		ecs::ToBeDeleted(creature, true);
	}

	[[nodiscard]] std::optional<entt::entity> PlayersCreature(PlayerNames owner) const override
	{
		return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetPrimaryCreature(owner) : std::nullopt;
	}

	void LosePlayersCreature(PlayerNames owner) override
	{
		// Another creature it held stops being the one its owner leads, and its leash comes off. That is ours: what the
		// game's leash does once the player has no creature is not known
		if (const auto other = PlayersCreature(owner); other.has_value() && Locator::leashSystem::has_value())
		{
			Locator::leashSystem::value().SetLeashable(*other, false);
		}
		if (Locator::playerSystem::has_value())
		{
			Locator::playerSystem::value().SetPrimaryCreature(owner, std::nullopt);
		}
	}
};

} // namespace

void creature_removal::ForgetInPlayerLists(Registry& registry, entt::entity creature)
{
	registry.Each<PlayerCreatures>([creature](entt::entity, PlayerCreatures& list) { std::erase(list.acquired, creature); });
}

size_t creature_removal::ForgetReferences(Registry& registry, entt::entity creature)
{
	size_t cleared = 0;
	// The markers of where its leash sent it
	std::vector<entt::entity> markers;
	registry.Each<const LeashMarker>([&markers, creature](entt::entity entity, const LeashMarker& marker) {
		if (marker.creature == creature)
		{
			markers.push_back(entity);
		}
	});
	cleared += markers.size();
	registry.Destroy(markers.begin(), markers.end());

	// What it holds, should it still hold anything
	std::vector<entt::entity> held;
	registry.Each<const HeldByCreature>([&held, creature](entt::entity entity, const HeldByCreature& holder) {
		if (holder.creature == creature)
		{
			held.push_back(entity);
		}
	});
	for (const auto entity : held)
	{
		registry.RemoveState<HeldByCreature>(entity);
		// Out of the creature's hand it goes back into the map's cells where it is
		if (registry.Valid(entity) && !ecs::map_cells::IsObjectInMap(entity))
		{
			ecs::map_cells::InsertMapObject(entity);
		}
	}
	cleared += held.size();

	// Creatures that follow it stop following
	registry.Each<CreatureLocomotion>([&cleared, creature](entt::entity, CreatureLocomotion& locomotion) {
		if (locomotion.following == creature)
		{
			locomotion.following.reset();
			++cleared;
		}
	});

	// A hand resting on it
	std::vector<entt::entity> hands;
	registry.Each<const HandOnCreature>([&hands, creature](entt::entity entity, const HandOnCreature& on) {
		if (on.creature == creature)
		{
			hands.push_back(entity);
		}
	});
	for (const auto entity : hands)
	{
		registry.RemoveState<HandOnCreature>(entity);
	}
	cleared += hands.size();
	return cleared;
}

std::optional<creature_removal::Removed> creature_removal::Remove(Registry& registry, entt::entity creature,
                                                                  WorldInterface& world)
{
	if (!registry.Valid(creature) || !registry.AllOf<Creature>(creature))
	{
		return std::nullopt;
	}
	Removed removed {.owner = registry.Get<const Creature>(creature).owner};
	// The creature the game keeps for its owner, which may be this one or another of theirs. A creature of no player has
	// none
	const bool ofAPlayer = static_cast<size_t>(removed.owner) < static_cast<size_t>(PlayerNames::_COUNT);
	const auto held = ofAPlayer ? world.PlayersCreature(removed.owner) : std::nullopt;
	removed.wasPrimary = held == creature;

	// The hand and a fight let go of it before the game's own deletion starts, which first keeps the player's own
	// creature for the next land
	world.LetGoFromHand(creature);
	world.EndFight(creature);
	world.KeepForNextLand(creature);
	// Then as the game deletes a creature: its leash comes off, its miracles and sounds stop, and its player forgets it
	world.TakeOffLeash(creature);
	world.ReleaseEffects(creature);
	ForgetInPlayerLists(registry, creature);
	// The reactions it set off, then what it does as a living thing: its actions, its walk and what it carries
	world.RemoveReactions(creature);
	world.StopActing(creature);
	world.LeaveView(creature);
	removed.referencesCleared = ForgetReferences(registry, creature);
	// Last its place in the world
	world.RemoveFromWorld(creature);

	// As the creature itself goes, once all of that is done, its owner is left with no creature, whichever one the game
	// kept for them, and none of their others takes its place
	if (held.has_value())
	{
		world.LosePlayersCreature(removed.owner);
		if (*held != creature)
		{
			removed.alsoLost = held;
		}
	}
	if (const auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Creature {} removed from the game{}", entt::to_integral(creature),
		                   removed.alsoLost.has_value()
		                       ? fmt::format("; creature {} is no longer its player's", entt::to_integral(*removed.alsoLost))
		                       : std::string {});
	}
	return removed;
}

std::unique_ptr<creature_removal::WorldInterface> creature_removal::MakeGameWorld()
{
	return std::make_unique<GameWorld>();
}

std::optional<creature_removal::Removed> creature_removal::RemoveFromGame(entt::entity creature)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	auto world = MakeGameWorld();
	return Remove(Locator::entitiesRegistry::value(), creature, *world);
}
