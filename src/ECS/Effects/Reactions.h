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

#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"

// Reaction (Reaction.cpp, 0x44 bytes, list g_game+0x205BDC): "something to react to" started by an object (a predator,
// a food pile, a thrown object, a fire, a spell, a teleport stone...). Reaction::CreateReaction 0x6E3D70 makes it and
// spreads it ONCE over a spiral of map cells (SpreadReaction 0x6E3E10, ApplyReactionToLivingObjectsAtSquare 0x6E3F90)
// to the Living there, each class with its own handler (the animals' in ECS/AnimalFlee.cpp, the villagers' in
// ECS/Systems/Implementations/VillagerReactions.cpp). The per-turn re-spreading (Reaction::ProcessReactions) is behind a
// debug flag the shipped game never sets. The parts every Living shares are here: its records (Living +0x98), the score
// fn_006E4620 and the rule to switch from the reaction it takes. Research: dev/tmp_dis/animals/flee.md, reactions.md;
// wiki docs/bw1-notes/animals.md and magic.md.

namespace openblack::ecs::effects::reactions
{
struct Reaction
{
	uint32_t id {0};
	entt::entity initiator {entt::null};                    ///< +0x14
	openblack::Reaction type {openblack::Reaction::None};  ///< +0x24
	PlayerNames player {PlayerNames::NEUTRAL};              ///< +0x38
	uint32_t turnCreated {0}; ///< +0x2C: 0 from the ctor; the turn when created with its last argument 1, or when a
	                          ///< Living first takes it (ApplyReactionToLivingObjectsAtSquare)
	float radius {0.0f};      ///< +0x3C: info.whetherReactionGrows ? 1 : info.maxReactionDistance (ctor 0x6E39D0), or
	                          ///< what its creator sets (SpellShield: the shield's radius + 30)
	bool available {true};    ///< +0x30
	bool stealth {false};     ///< +0x20 (0 from the ctor)
};

/// The Living classes a reaction reaches (the class of each object of the cell's list)
enum class LivingClass
{
	Villager,
	Animal,
	Creature,
};

/// ApplyReactionToLivingObjectsAtSquare 0x6E3F90 for one Living of the cell: `distance` is the one the original
/// computes there, (|dz| + |dx|) / 2 in metres from the initiator
using LivingReactionHandler = void (*)(entt::entity living, const Reaction& reaction, float distance);
void SetLivingReactionHandler(LivingClass living, LivingReactionHandler handler);

/// Reaction::CreateReaction 0x6E3D70 (initiator, type, player, stamp): 0 for REACTION -1; else the new reaction, spread
/// at once (SpreadReaction), and its id
uint32_t CreateReaction(entt::entity initiator, openblack::Reaction type, PlayerNames player, bool stamp);

/// SpreadReaction 0x6E3E10: (max(1, trunc(radius x 0.2)))^2 x GetReactionPower (1) map cells of GUtils::Spiral from
/// the initiator's cell, the ones within the radius; each cell's Living, in the cell's order, to its class's handler.
/// (inferido) GetReactionPower (vt+0x4F4) is 1 for every initiator: GameThingWithPos 0x4024D0 is 1.0, but the overrides
/// Spell 0x55CF10 (= Spell::GetSpellStrength 0x720750) and Tree 0x55D8D0 (vt+0x11C, GetLife: ECS/Life.h) are not
/// ported, so a spell or tree initiator (MagicTree) spreads over the unscaled count.
/// TODO(reactions): the stealth branch of 0x6E3E10 (+0x20 set and the cell's first object another Living:
/// GameRand(1000) > stealthRandomChance skips the cell, else stealth is cleared) is not ported; no creator sets +0x20.
/// (aproximado) The cells are openblack's map grid, rebuilt once per turn (and before a spread outside the turn), not
/// the original's lists that follow every move; a cell's order is the grid's (the registry's), not the original's
/// insertion order.
void SpreadReaction(uint32_t reaction);

/// Reaction::RemoveAllReactionsInitiatedByObject 0x6E4750
void RemoveAllReactionsInitiatedByObject(entt::entity initiator);
/// Reaction::RemoveAllReactionsOfTypeInitiatedByObject 0x6E4780 (ShutDown of each)
void RemoveAllReactionsOfTypeInitiatedBy(entt::entity initiator, openblack::Reaction type);
/// Object::ToBeDeleted: a deleted initiator takes its reactions with it
void Prune();

/// fn_006E4830 (FireEffect fn_00730960): the reaction moves to another initiator (a tree's fire to its DeadTree)
void SetInitiator(uint32_t reaction, entt::entity initiator);
/// reaction +0x2C = the game turn (SpellShield::UpdateStruckReaction 0x72B780 refreshes its reaction this way)
void Stamp(uint32_t reaction);
/// +0x2C = the turn if still 0 (a Living takes it for the first time)
void MarkStarted(uint32_t reaction, uint32_t turn);
/// reaction +0x3C (SpellShield::InitWithPos 0x72B5F0 writes the radius of its REACT_TO_MAGIC_SHIELD)
void SetRadius(uint32_t reaction, float radius);

/// Reaction::GetReactionInitiatedByObject 0x6E4870: the first one's id, 0 none
[[nodiscard]] uint32_t GetReactionInitiatedBy(entt::entity initiator);
/// The first reaction of that type the object started, 0 none
[[nodiscard]] uint32_t GetReactionOfTypeInitiatedBy(entt::entity initiator, openblack::Reaction type);

[[nodiscard]] const std::vector<Reaction>& All();
/// The reaction by its id, nullptr when gone
[[nodiscard]] const Reaction* Find(uint32_t id);
/// Reaction::IsAvailable [inferred: not removed and its initiator still there]
[[nodiscard]] bool IsAvailable(const Reaction* reaction);

// ---- the parts of Living every class shares

/// fn_006E4340 (Living +0x98, components::ReactionRecords): a type it knows only when more than `again` turns went
/// since (its turn renewed); a new one is added (the oldest dropped past 3); on the way the other types older than
/// 1800 turns are forgotten. True: it may react.
bool Records(entt::entity living, uint8_t type, uint32_t again, uint32_t now);
/// fn_005F0FB0: the record's turn (0 if none)
[[nodiscard]] uint32_t RecordTurn(entt::entity living, uint8_t type);
/// Living::StopReacting 0x5F1140: the record of the type it stops reacting to gets the turn
void RefreshRecord(entt::entity living, uint8_t type, uint32_t now);

/// fn_006E4620: the score of a reaction for a Living at that distance, 0..255: 0 when its info does not react to the
/// type (GLivingInfo.isReacting[type]) or beyond maxReactionDistance; else priority x (1 + 0.5 howImportantIsDistance
/// (max - d) / max), truncated
[[nodiscard]] uint32_t Score(uint8_t type, bool reactsToType, uint32_t priority, float distance);

/// ApplyReactionToLivingObjectsAtSquare's switch from the reaction a Living takes to a new one: the new one scores more
/// (the table's +0x04 restart flag taken as 1) and the current one lasted h = max(10, cur / new x 20 - 10) seconds (1
/// second if the current one is 16 REACT_TO_HAND_PICK_UP); `seconds` = (turn - its record's turn) / 10
[[nodiscard]] bool MaySwitch(float currentScore, float newScore, float seconds, uint8_t currentType);

/// The game turn (GGame +0x205A40, Game's turn count) as BeginTurn set it: the one clock of the reactions' stamps and
/// the villagers' records (the animals' records still run on their own animal_ai turn, the "animales" session's)
[[nodiscard]] uint32_t Turn();
/// The start of a game turn (Game::GameLogicLoop, before the Living): the turn for the stamps, and the reactions whose
/// initiator was deleted go (Object::ToBeDeleted -> RemoveAllReactionsInitiatedByObject; here once per turn, inf)
void BeginTurn(uint32_t turn);
/// The end of the game turn's logic: from here to the next BeginTurn a spread rebuilds the map cells first (the map
/// script and the debug hooks create reactions outside the turn, when the cells are not up to date)
void EndTurn();
/// A land is loaded: no reactions, turn 0, outside a turn (the handlers stay)
void Clear();
} // namespace openblack::ecs::effects::reactions
