/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <vector>

#include <entt/entity/fwd.hpp>

#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "Enums.h"

// The creatures in the game loop: their turn, their updates between two turns, their leashes' ropes, a new land and
// the leash keys. Every pass walks the creatures found in the registry, so with no creature in the world nothing is
// changed and nothing is drawn from the random streams. Docs: docs/wiki/creature/overview.md#the-order-of-a-creatures-turn.

namespace openblack
{
class Profiler;
} // namespace openblack

namespace openblack::input
{
class GameActionInterface;
} // namespace openblack::input

namespace openblack::ecs::systems
{
class LeashSystemInterface;
class CreaturePhysiologySystemInterface;
class CreatureSkinSystemInterface;
class CreatureMindSystemInterface;
class CreatureObjectActionSystemInterface;
class CreatureLocomotionSystemInterface;
class CreatureFightSystemInterface;
class CreatureFizzSystemInterface;
class CreatureAudioSystemInterface;
class MagicSystemInterface;
} // namespace openblack::ecs::systems

namespace openblack::ecs::creature_loop
{
/// The creatures a turn takes, in the order it takes them: every creature not marked to go, newest first as the living
/// list holds them (a creature without a creation index last, in the registry's order). Nothing is changed
[[nodiscard]] std::vector<entt::entity> TurnCreatures();

/// Every creature's turn as one block (the tests'): the shared start, each creature's whole turn newest first, then the
/// shared end. The game takes each creature at its place in the living list instead (living_turn::ProcessLiving)
void ProcessTurn(Profiler& profiler);

/// The services a creature's turn goes through
struct Systems
{
	systems::LeashSystemInterface& leash;
	systems::CreaturePhysiologySystemInterface& physiology;
	systems::CreatureAnimationSystemInterface& animation;
	systems::CreatureSkinSystemInterface& skin;
	systems::CreatureMindSystemInterface& mind;
	systems::CreatureObjectActionSystemInterface& hands;
	systems::CreatureLocomotionSystemInterface& locomotion;
	systems::CreatureFightSystemInterface& fight;
	systems::CreatureFizzSystemInterface& fizz;
	systems::CreatureAudioSystemInterface& audio;
	systems::MagicSystemInterface& magic;
};

/// The creature services as the locator holds them
[[nodiscard]] Systems Services();
/// The same, or none when the locator lacks any of them (a test's world without creatures)
[[nodiscard]] std::optional<Systems> ServicesIfPresent();
/// The creatures that are no living (no creation index: the tattoo screen's), which take their turn after the living
/// list's walk, in the registry's order
[[nodiscard]] std::vector<entt::entity> CreaturesOffTheList();

/// The creatures' turn's shared start, before the first creature's turn: the search turns shared out, the sounds left
/// from the turn before played, every service's turn started; and its shared end, after the last creature's: every
/// service's turn ended, and the eyes of what is no creature
void BeginTurn(const Systems& services);
void EndTurn(const Systems& services);

/// One creature's whole turn, in the original's creature turn's order: its start size, knock marks, home, spells,
/// beams, pen size and autoscale; its leash and needs; its mind; its sub-action (hands, walk, the fight's part, the
/// hands' end); its shown fatness and skin; its fizz (fizzed out, nothing more); its body's update (breathing, the
/// fight's states, knock marks, animations and their sounds, pose, the creature it holds, knock); its alignment, its
/// web page and its camera dome
void ProcessCreature(const Systems& services, entt::entity creature);

/// What one creature's body is set to play a share of the way through the turn, as AnimationInputsAt sets each
[[nodiscard]] std::optional<components::CreatureAnimationInputs> AnimationInputsOf(entt::entity creature, float turnFraction);

/// What each creature's body is set to play a share of the way through the turn (0 to 1), as the frame sets it: from
/// what it plays now, the fight's animation, then the walk's, then what it acts on, each over what the one before set.
/// Nothing is changed; with no creature, no creature service is asked
[[nodiscard]] systems::CreatureAnimationSystemInterface::TurnInputs AnimationInputsAt(float turnFraction);

/// Whether the world's creatures stand still this frame: the world is paused while the player is inside the citadel,
/// and its creatures freeze with it (the citadel's creature room has updates of its own)
[[nodiscard]] constexpr bool FrozenInCitadel(bool paused, bool insideCitadel)
{
	return paused && insideCitadel;
}

/// The creatures between two turns, a share of the way through the turn and some game milliseconds on: fights, drawn
/// poses, the sick, what they act on, the sounds the drawing reaches, bodies, what they hold, hair, footprints and skins
void UpdateFrame(float turnFraction, uint32_t frameGameMs, Profiler& profiler);

/// The leashes' ropes, some game seconds on, once the hand is placed
void UpdateLeash(float frameGameSeconds, Profiler& profiler);

/// A new land: the footprints read today's date again (the last land's prints stay until they fade)
void OnLoadMap();

/// The leash shortcuts (L, V, B) that went down this frame, for the local player's creature when they have one they
/// can lead; with none, a press does nothing and logs nothing
void ProcessLeashKeys(const input::GameActionInterface& actions);

/// The leash the player's hand holds comes off, as a hand demo starts; one tied to something stays. Without a creature on
/// a leash in the hand nothing is done and nothing is logged
void ReleaseLeashHeldInHand(PlayerNames player);
} // namespace openblack::ecs::creature_loop
