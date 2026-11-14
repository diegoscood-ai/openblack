/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <functional>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

#include <EffectRecords.h>
#include <SavedRecords.h>
#include <WorldListRecords.h>
#include <entt/entity/entity.hpp>

#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Spell.h"
#include "ECS/SavedWorld/LoadParticles.h"
#include "Enums.h"

namespace openblack::gamesave
{
struct WorldRead;
}

// The spells of a saved game's world file made again, as the original's load keeps them: each spell is its saved
// values and its place in the game's list of spells, with its effect made again from its records, and nothing is
// started again (docs/wiki/engine/saved-games.md#openblack, the spells). The plain (a fireball's, a lightning bolt's, a
// beam's explosion's), the healing, the storm and tornado, the forest, the water, the shield, the flock and the creature
// spells are made, with the physical and spiritual shields their shields keep, the animals the flocks hold, the forests
// the forest spells made and the creatures the creature spells were cast on, and the teleport spells with their stones;
// the other classes are kept as the load read them.
namespace openblack::ecs::saved_spells
{

/// A plain, a healing, a storm or tornado, a forest, a teleport, a water, a shield, a flock or a creature spell a save
/// holds, by its number, with its record, its effect and what its objects are
struct PlannedSpell
{
	gamesave::ObjectNumber number {0};
	std::variant<gamesave::SpellWaterRecord, gamesave::SpellShieldRecord, gamesave::SpellFlockFlyingRecord,
	             gamesave::SpellFlockGroundRecord, gamesave::SpellCreatureRecord, gamesave::SpellRecord,
	             gamesave::SpellHealRecord, gamesave::SpellStormAndTornadoRecord, gamesave::SpellForestRecord,
	             gamesave::SpellTeleportRecord>
	    record;
	/// Its effect, when its particle interface was read
	std::optional<saved_particles::PlannedEffect> effect;
	/// What its creator is: a player, a worship site's icon, a creature or another thing (none for no creator)
	components::SpellCreator::Kind creatorKind {components::SpellCreator::Kind::None};
	/// The player its player object is, none for no player
	std::optional<PlayerNames> player;
};

/// The plain, the healing, the storm and tornado, the forest, the teleport, the water, the shield, the flock and the
/// creature spells of the game's list of spells (the newest first), each with its effect; the spells of the other
/// classes are left out (their records stay as the load kept them)
[[nodiscard]] std::vector<PlannedSpell>
PlanSpells(const gamesave::WorldRead& world, const std::vector<gamesave::ObjectNumber>& list,
           const std::function<std::optional<PlayerNames>(gamesave::ObjectNumber)>& playerOf);

/// A physical shield a save holds, by its number, with its record, its effect and its place in the game's list of map
/// shields (0 the newest)
struct PlannedPhysicalShield
{
	gamesave::ObjectNumber number {0};
	gamesave::PhysicalShieldRecord record;
	std::optional<saved_particles::PlannedEffect> effect;
	size_t place {0};
};

/// The physical shields of the game's list of map shields (the newest first), each with its effect
[[nodiscard]] std::vector<PlannedPhysicalShield> PlanPhysicalShields(const gamesave::WorldRead& world,
                                                                     const std::vector<gamesave::ObjectNumber>& list);

/// A spiritual shield a save holds, by its number, with its record and its place in the game's list of map shields
struct PlannedSpiritualShield
{
	gamesave::ObjectNumber number {0};
	gamesave::SpiritualShieldRecord record;
	size_t place {0};
};

/// The spiritual shields of the game's list of map shields (the newest first)
[[nodiscard]] std::vector<PlannedSpiritualShield> PlanSpiritualShields(const gamesave::WorldRead& world,
                                                                       const std::vector<gamesave::ObjectNumber>& list);

/// A spiritual shield's values from its record, as the original's load reads them: its place and its object's scale.
/// Its spell is left for the load to give it
[[nodiscard]] components::MagicShield SpiritualShieldOf(const gamesave::SpiritualShieldRecord& record);

/// A physical shield's values from its record, as the original's load reads them: its place (its height above the land
/// the place's altitude), its object's scale, its creation turn, its matrix and the last turn's, its start and final
/// scale, its start and end spin, its angle, its bob, whether it is dying, its alpha and its die time. Its spell and its
/// effect are left for the load to give it
[[nodiscard]] components::MagicShield PhysicalShieldOf(const gamesave::PhysicalShieldRecord& record);

/// What every spell keeps of its saved values, as the original's spell load reads them: its place, its movement's
/// direction, the place it is cast at, its direction, its chants and those it started with, whether it is closed down,
/// cast by this machine's interface, by a creature and by a human player, its duration, its process info, whether it is
/// free, its creator (of that kind, the entity the load made of it) and its player, whether it was cast from a hand
/// (an interface status named), its seed, its magic type, its age, magnitude and strength multiplier, the place it was
/// first cast at and the two counts of its mana path. Its effect and its reaction are left for the load to give it
[[nodiscard]] components::Spell SpellOf(const gamesave::SpellFields& fields, components::SpellCreator::Kind creatorKind,
                                        std::optional<PlayerNames> player, entt::entity creator, entt::entity seed);

/// What the spells need of the rest of the load: the entity the load made of a number (entt::null for none), and the key
/// of the forest it made of a number (0 for none)
struct SpellLinks
{
	std::function<entt::entity(gamesave::ObjectNumber)> entityOf;
	std::function<uint32_t(gamesave::ObjectNumber)> forestOf;
};

/// The spells made, by their numbers
using MadeSpells = std::unordered_map<gamesave::ObjectNumber, entt::entity>;

/// The physical and the spiritual shields made again, from the list's last to its first so that the newest has the
/// newest serial, each with its values and the record it was read from, a spiritual one with its creation number, a
/// physical one with its effect made again (of the physical shield's file, its target the shield); `keep` is told each
/// shield's entity before its effect is made, `keepSource` its effect's numbers. A physical shield whose effect cannot be
/// made is still made, with no effect and one log line
[[nodiscard]] MadeSpells MakePhysicalShields(const std::vector<PlannedPhysicalShield>& shields, const SpellLinks& links,
                                             const std::function<void(gamesave::ObjectNumber, uint32_t, uint64_t)>& keepSource,
                                             const std::function<void(gamesave::ObjectNumber, entt::entity, uint32_t)>& keep,
                                             const std::vector<PlannedSpiritualShield>& spiritual = {});

/// The spells made again, from the list's last to its first so that the list keeps its order, each with its values,
/// its class's own (the water's last ring, a forest's forest; a storm joins the storm spells), its effect made again (of
/// its particle type's file) and the record it was read from; its seed names it again as the spell cast from it.
/// `keepSource` is told the numbers of each effect's interface and manager with the effect's id, `keep` each spell's
/// entity. A creature spell on a creature, or a forest spell naming a forest, the load did not make is not made
[[nodiscard]] MadeSpells MakeSpells(const std::vector<PlannedSpell>& spells, const SpellLinks& links,
                                    const std::function<void(gamesave::ObjectNumber, uint32_t, uint64_t)>& keepSource,
                                    const std::function<void(gamesave::ObjectNumber, entt::entity, uint32_t)>& keep);

/// A teleport's stone a save holds, by its number, with its record, the player its record names and its place in that
/// player's list of stones (0 the newest; none when no player's list names it)
struct PlannedTeleportStone
{
	gamesave::ObjectNumber number {0};
	gamesave::MagicTeleportRecord record;
	std::optional<PlayerNames> player;
	std::optional<size_t> place;
};

/// The teleports' stones of the game's list of objects of several cells, in its order
[[nodiscard]] std::vector<PlannedTeleportStone>
PlanTeleportStones(const gamesave::WorldRead& world, const std::vector<gamesave::ObjectNumber>& list,
                   const std::function<std::optional<PlayerNames>(gamesave::ObjectNumber)>& playerOf);

/// The stones made again, as the game's load makes them: those no player's list names first, then each player's from
/// its list's last to its first, so that its list keeps its order (the newest first); each at its place, of its player,
/// at its X, Y and Z angles, with its creation number, its pool started again and the record it was read from; `keep`
/// is told each stone's entity. Its spell and its reaction are given by LinkTeleportStones
[[nodiscard]] MadeSpells MakeTeleportStones(const std::vector<PlannedTeleportStone>& stones,
                                            const std::function<void(gamesave::ObjectNumber, entt::entity, uint32_t)>& keep);

/// Each stone made is given the spell its record names (`spells`, the spells made, by their numbers) and, once the
/// reactions are made again, the reaction it spreads, by the id it was made with (`reactionIds`, by the reaction's number)
void LinkTeleportStones(const std::vector<PlannedTeleportStone>& stones, const MadeSpells& made, const MadeSpells& spells,
                        const std::unordered_map<gamesave::ObjectNumber, uint32_t>& reactionIds);

/// Each spell made is given the reaction it started, and a shield spell its struck and shield reactions, once the
/// reactions are made again: by the id each was made with (`reactionIds`, by the reaction's number)
void LinkReactions(const std::vector<PlannedSpell>& spells, const MadeSpells& made,
                   const std::unordered_map<gamesave::ObjectNumber, uint32_t>& reactionIds);

} // namespace openblack::ecs::saved_spells
