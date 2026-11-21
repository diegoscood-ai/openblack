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

#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <CreatureRecords.h>
#include <LHVMTypes.h>
#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"
#include "ECS/SavedWorld/WorldWriter.h"

namespace openblack::creature_fight
{
struct Fighter;
}

namespace openblack::ecs::components
{
struct CreatureFighting;
struct CreatureKnockedOut;
struct CreatureMindState;
} // namespace openblack::ecs::components

// The creatures in a saved game's world file: their own values after a living thing's parts, written from what openblack
// holds (docs/wiki/engine/saved-games.md#openblack, the creature).
namespace openblack::ecs::saved_world_write
{
/// The script the alignment of the local player's creature is written through while it runs, and the global it
/// writes instead
inline constexpr std::string_view k_CurseScript = "CreatureCurse";
inline constexpr std::string_view k_CurseAlignmentGlobal = "OriginalAlignmentOfMyCreature";

/// Whether the script machine has a task of the script of that name, as the script library answers it: a task in its
/// list whose script's name is the name, letter for letter, whatever the task is doing
[[nodiscard]] bool ScriptIsRunning(const std::map<uint32_t, lhvm::VMTask>& tasks, std::string_view script);
/// The value of the first of the script machine's globals of that name, read as a float, as the script library gives
/// it; 0 when there is none
[[nodiscard]] float GlobalAsFloat(std::span<const lhvm::VMVar> globals, std::string_view name);

/// The alignment a creature's record writes: its own, but for the local player's creature while the curse's script
/// runs, the alignment it had before the curse, from the script's global
[[nodiscard]] float SavedAlignment(float alignment, bool localPlayers, const std::map<uint32_t, lhvm::VMTask>* tasks,
                                   std::span<const lhvm::VMVar> globals);

/// A creature's own values, from what openblack holds (kept: the part its load kept, for the values openblack does not
/// hold); none, and the write fails naming the creature, for a value with no source yet
[[nodiscard]] std::optional<gamesave::CreatureFields> CreatureFieldsOf(WriteContext& context, entt::entity creature,
                                                                       const gamesave::CreatureFields* kept);

/// The plan a creature a script holds keeps in the save, as the game writes it: a script's play of an animation (its
/// plan of obeying its player, about itself, with its agenda from the step it is at: each play of the animation, then
/// the wait until its body is free), or, with no plan and no steps left, the plan a finished one leaves. The words a
/// step's argument does not set, and one word of the agenda the game never sets, hold what was in their memory before:
/// they are written as 0. An action a script forces on it (its plan of obeying its player with the action's row, what
/// it is done to and with) is written with its agenda from the step it is at, for the rows whose agenda openblack builds
/// as the game does (sitting down at a thing). `refOf` names a thing by its number. None, with the reason, for any other
/// plan
/// The steps from `from` of a creature's dance with villagers as the game keeps its agenda: stop, go near the villager,
/// face it, the summoning clip (the first main step), make the dance at its place (the land's height there, `groundAt`),
/// call the villagers, stop looking about, walk to the place and point at it (the last main step), send the dancers
/// to their places, wait for them to stand still, start the dance, tell them to dance and dance. None for an agenda of
/// other steps
[[nodiscard]] std::optional<std::vector<gamesave::AgendaStep>>
DanceStepsOf(std::span<const creature_mind::Step> agenda, size_t from, const std::function<float(glm::vec2)>& groundAt,
             const std::function<gamesave::Ref(uint32_t)>& refOf);

/// Whether a fighter is written by the plan its win forced: the winner finishing its last move, whose mind already
/// holds the forced plan of obeying its player that the game puts in place of the fight's at the knock-out
/// Whether a fighter's fight was called off or left with no response: the game's fight plan failed, so it is written
/// out of a fight, its own plan as any other creature's
[[nodiscard]] bool FightCalledOff(const components::CreatureFighting& fighting);
/// Whether the other creature of a fight is only asked to fight, before the first creature's first order: in the game it
/// still carries out its own plan, so it is written out of a fight
[[nodiscard]] bool AskedAndNotOrdered(const components::CreatureFighting& self, const components::CreatureFighting& starter);
[[nodiscard]] bool WrittenByItsWin(const components::CreatureFighting& fighting, const components::CreatureMindState* mind);

[[nodiscard]] std::optional<gamesave::KeptCreaturePlan> ScriptHeldPlanOf(const components::CreatureMindState& mind,
                                                                         gamesave::Ref self,
                                                                         const std::function<gamesave::Ref(uint32_t)>& refOf,
                                                                         std::string& why);

/// The plan a knocked-out creature a script holds keeps in the save: the Die action every faint is, its plan of obeying
/// its player about the thing it is done to (the creature itself for a faint), with its agenda from the step it is at:
/// the main faint, the lie's wait, the carry home for a player's creature, a wait of three seconds, the wait for the
/// spells and, when it was decided, the rest. Written while it falls and lies; none, with the reason, once it is
/// carried home or later
[[nodiscard]] std::optional<gamesave::KeptCreaturePlan> KnockedOutPlanOf(const components::CreatureKnockedOut& knockedOut,
                                                                         gamesave::Ref self, gamesave::Ref actedOn,
                                                                         bool hasPlayer, std::string& why);

/// Who fights for a fighter, as the game keeps it: 0 nobody, 1 the player with the computer waiting to take over, 2
/// the computer
[[nodiscard]] uint32_t AutoFightOf(const creature_fight::Fighter& fighter);

/// The plan a creature in its duel keeps in the save: the creature that started the fight (it made the arena) at the
/// main step of its fight's agenda, with the orders it gives its opponent; the opponent obeying it, its fight and then
/// a wait for its next order. `opponent` names the other one. None, with the reason, when the starter's mind holds no
/// plan of a fight
[[nodiscard]] std::optional<gamesave::KeptCreaturePlan> DuelPlanOf(const components::CreatureMindState* mind, bool madeTheArena,
                                                                   gamesave::Ref self, gamesave::Ref opponent,
                                                                   std::string& why);

/// Where a fighter is in its fight's agenda, as the save keeps it. The creature that started the fight: the step of
/// its eighteen it is at and its state (0 not started, 1 under way). The other creature: the step its last order gave
/// it, and its state; none while it waits for its next order
struct FightPlace
{
	uint32_t current {0};
	uint32_t state {0};
	std::optional<uint32_t> ordered;

	bool operator==(const FightPlace&) const = default;
};
/// Where a fighter is in its fight's agenda, from its fight and its opponent's: the steps before the duel by the stage
/// and its turns, the duel, and the response after it. None, with the reason, where openblack does not write it: the
/// other creature before its first order (in the game it still carries out its own plan then), a fight won, and one
/// called off or left with no response
[[nodiscard]] std::optional<FightPlace> FightPlaceOf(const components::CreatureFighting& self,
                                                     const components::CreatureFighting& other, std::string& why);
/// The plan a fighter keeps in the save, at any step of its fight that FightPlaceOf places: the creature that started
/// the fight (it made the arena) at its step of the fight's eighteen, the steps from there on; the other obeying it, the
/// step its last order gave it and then a wait for its next order. `opponent` names the other one. None, with the
/// reason, for a place not written or a starter whose mind holds no plan of a fight
[[nodiscard]] std::optional<gamesave::KeptCreaturePlan>
FightPlanOf(const components::CreatureMindState* mind, const components::CreatureFighting& self,
            const components::CreatureFighting& other, gamesave::Ref selfRef, gamesave::Ref opponent, std::string& why);

/// As the game's save does to a creature it writes: the creature whose invisible spell is off has its fizz, its
/// fizz's target and its fizz's rate set to 0
void ZeroFizzForSave(Registry& registry, entt::entity creature);
} // namespace openblack::ecs::saved_world_write
