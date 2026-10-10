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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "Magic/Core/SpellCastData.h"
#include "Magic/HandMotion.h"
#include "Particles/SpellLink.h"

namespace openblack::magic
{
class FlockMiracleInterface;
}

namespace openblack::ecs::effects
{
struct EffectValues;
}

namespace openblack::ecs::systems
{
class FallingSpellSystemInterface;
class HandMagicStateInterface;
class MagicObjectsSystemInterface;
class SpellSystemInterface;

/// The running miracles and the seeds the player is given, as the rest of the game reaches them.
///
/// A miracle is an entity with a components::Spell: cast at a point or on an object by a caster who pays for it, it
/// runs its particle effect, acts on the events the effect sends and pays its upkeep each turn until its time or its
/// prayer power runs out. A one-shot bubble floats a seed until the hand takes it; a seed may also be put straight into
/// the hand. A miracle dispenser is a building that floats such a bubble above itself, and another once its period has
/// passed since the last was taken. The game loop drives the miracles through the turn and frame steps below, each at
/// its place in the turn.
///
/// The miracles themselves (the spells, the seeds, the hand's casting, the magic objects) live under src/Magic; this
/// service is the one way in from the game loop and the tools.
class MagicSystemInterface
{
public:
	/// What a frame tells the miracles about the player's hand
	struct HandFrame
	{
		/// Where the hand is
		glm::vec3 handPosition {0.0f};
		/// The land or object under the cursor, none over the sky
		std::optional<glm::vec3> point;
		/// The line of sight through the cursor
		glm::vec3 rayOrigin {0.0f};
		glm::vec3 rayDirection {0.0f, 0.0f, 1.0f};
		/// Which way the camera looks
		glm::vec3 cameraForward {0.0f, 0.0f, 1.0f};
		/// Whether the hand is over the world, not over a window or in the temple
		bool overWorld {true};
	};

	/// What the debug window shows of a running miracle
	struct SpellInfo
	{
		entt::entity entity;
		MagicType magicType;
		PlayerNames player;
		float age;
		float duration;
		float chants;
		float initialChants;
		float strength;
		float upkeep;
		bool closing;
		bool fromHand;
		uint32_t effect;
		glm::vec3 position;
	};

	/// What the debug window shows of a dispenser
	struct DispenserInfo
	{
		entt::entity entity;
		MagicType magicType;
		glm::vec3 position;
		bool hasOrb;
		uint32_t tick;
		uint32_t period;
		bool active;
	};

	virtual ~MagicSystemInterface() = default;

	// Casting

	/// A miracle cast by a player at a point of the world, as a debug tool casts it: the entity, or none when it could
	/// not start. The process info is what its effect starts with: the hand's place, the camera, the throw.
	virtual entt::entity CastAtPoint(MagicType type, PlayerNames player, glm::vec3 point, const magic::SpellCastData& cast,
	                                 const psys::ProcessInfo& info) = 0;
	/// A miracle cast on an object: at its feet, or with its effect given the object to act on, as its seed says
	virtual entt::entity CastOnObject(MagicType type, PlayerNames player, entt::entity target, const magic::SpellCastData& cast,
	                                  const psys::ProcessInfo& info) = 0;
	/// The miracle stops: its effect dies away and it goes once that has gone
	virtual void CloseDown(entt::entity spell) = 0;
	/// A creature casts a miracle at an object, as its casting pose's loop begins: whether it could. No creature casts
	/// through the service yet.
	virtual bool CastByCreature(entt::entity /*creature*/, MagicType /*type*/, entt::entity /*target*/) { return false; }
	/// The creature lets go of the miracle it cast: one that lasts only while it is held stops
	virtual void ReleaseCreatureCast(entt::entity /*creature*/) {}
	/// An effect at a point that no miracle is behind, such as a lightning strike a script calls down: everything it
	/// reaches takes it, as from the player
	virtual void ApplyEffectAt(glm::vec3 /*point*/, const ecs::effects::EffectValues& /*values*/, PlayerNames /*player*/) {}
	/// Whether a magic type may be cast at a point of the world. The player is not asked about yet: the cast rules
	/// look only at the land under the point.
	[[nodiscard]] virtual bool CanCastAt(MagicType type, PlayerNames player, glm::vec3 point) = 0;
	/// Something the miracle made acts for it, as its particles would: whether the miracle acted
	virtual bool SpellEvent(entt::entity spell, const psys::SpellEventInfo& event) = 0;
	/// The miracle pays prayer power, or is given it back for a negative cost
	virtual void PayForSpell(entt::entity spell, float cost) = 0;

	// Dispensers and one-shot miracles

	/// A dispenser of a magic type standing on the land at a point, turned about the vertical, with its building's own
	/// period: it floats its first bubble at once, as a script's dispenser does. None when the tables have no dispenser
	/// building or it could not be made.
	virtual entt::entity CreateDispenser(glm::vec3 position, MagicType type, float yAngleRadians) = 0;
	/// For the testbed: a dispenser that has counted its period, floating its bubble now if it has none
	virtual void ChargeDispenser(entt::entity /*dispenser*/) {}
	/// The time from a dispenser's bubble being taken to the next, in seconds. A time shorter than a game turn leaves
	/// the period as it was, as a script's timer time does; whether the dispenser is active is left alone.
	virtual void SetDispenserPeriod(entt::entity dispenser, float seconds) = 0;
	/// A one-shot bubble of a seed at a point, floating there
	virtual entt::entity CreateOneOffSeed(glm::vec3 position, SpellSeedType seed, int powerUp, float multiplier) = 0;
	/// A one-shot bubble of whatever seed casts a magic type, at the power-up level that casts it
	virtual entt::entity CreateOneOffSeedFor(glm::vec3 position, MagicType type) = 0;
	/// Takes a dispenser away with its bubble and its swirl, or a bubble with the seed spinning in it; false for anything
	/// else, which is left alone
	virtual bool Remove(entt::entity entity) = 0;
	/// A seed straight into the player's hand, as from a bubble: the seed, or none
	virtual entt::entity GiveSeedToHand(PlayerNames player, SpellSeedType seed, int powerUp, float multiplier) = 0;
	/// A seed summoned from the player's worship into their hand, if it is free, its cost to create charged from the
	/// player's prayer power: the seed, or none. None yet: the player's seeds are charged only at the worship sites.
	virtual entt::entity SummonSeed(PlayerNames player, SpellSeedType seed, int powerUp) = 0;
	/// The hand drops the seed it holds, as a shake does: the seed goes back to its worship site on the next turn.
	/// Nothing happens when the hand holds no seed.
	virtual void DiscardHeldSeed() = 0;

	// The hand

	/// Whether the hand holds a miracle, so it doesn't take hold of creatures or things
	[[nodiscard]] virtual bool IsHandBusy() const = 0;
	/// How a pour of food or wood lifts and tips the hand now, a fraction of the way from the last turn to the next
	[[nodiscard]] virtual magic::PourPose GetHandPour(float fraction) const = 0;
	/// The seed in the player's hand, if it holds one
	[[nodiscard]] virtual std::optional<entt::entity> GetHeldSeed() const = 0;

	// What a miracle's objects in the world do for it

	/// An event a miracle's object sends it, as its particles do, such as a blow on the physical shield's dome; whether it
	/// acted
	virtual bool SendSpellEvent(entt::entity spell, const psys::SpellEventInfo& event) = 0;
	/// A miracle is made to pay prayer power, its caster asked for the whole shortfall, as a blow on a shield is; its
	/// strength after, 0 once it has gone
	virtual float ForcePayForSpell(entt::entity spell, float cost) = 0;
	/// A miracle's strength now, 0 once it has gone
	[[nodiscard]] virtual float SpellStrength(entt::entity spell) = 0;

	// Turns and frames

	/// Once a game turn, after the living things: the fire, the magic objects, the reactions and every miracle's turn
	virtual void ProcessTurn() = 0;
	/// Once a frame, after the hand's update, with the seconds of game time that have passed (0 while paused): the
	/// bubbles' seeds, the effects drawn between turns, the hand's gestures and the worship sites
	virtual void Update(float seconds) = 0;
	/// A new land, once the particle effects are cleared and before its script runs: every miracle goes
	virtual void Reset() = 0;

	// For the debug window, the testbed, the scripts and the fires

	/// For the testbed and the debug window, until worship sets them: a player's power multiplier for a tribe
	virtual void SetTribalPower(PlayerNames /*player*/, Tribe /*tribe*/, float /*power*/) {}
	/// A cheat for the debug window: the miracles may be cast outside the player's influence. Kept; nothing reads it
	/// yet.
	virtual void SetIgnoreInfluence(bool ignore) = 0;
	[[nodiscard]] virtual bool IsIgnoringInfluence() const = 0;
	/// For the testbed's scenarios: the hand is where the scenario puts it, not where the mouse is, until none is given.
	/// Kept; nothing reads it yet.
	virtual void DriveHand(std::optional<HandFrame> frame) = 0;
	/// Where a scenario puts the hand, if it does
	[[nodiscard]] virtual std::optional<HandFrame> GetDrivenHand() const = 0;
	/// Every running miracle, in the order they are processed, with where it acts on the world
	[[nodiscard]] virtual std::vector<SpellInfo> GetSpells() const = 0;
	/// The newest miracle of a kind, closing down or not, whose last event (its cast, before any) was strictly within a
	/// radius of a point, measured across the land; never a shield
	[[nodiscard]] virtual std::optional<entt::entity> SpellAt(MagicType /*type*/, glm::vec3 /*point*/, float /*radius*/) const
	{
		return std::nullopt;
	}
	/// The rain puts out a fire at a point: the first storm miracle whose size covers it, without such a reaction going,
	/// has the people come to watch
	virtual void RainOnFire(const glm::vec3& point) = 0;
	/// Every dispenser: its miracle, where it stands, whether its bubble is there, and its count, period and switch
	[[nodiscard]] virtual std::vector<DispenserInfo> GetDispensers() const = 0;

	// Ours: the other steps of the turn the miracles take part in, each at its place in the game loop

	/// The game inputs, before the turn number goes up: the hand's casting
	virtual void ProcessGameInputs() = 0;
	/// The start of the game turn, before the global game lists and the living things: the atmosphere, the influence
	/// rings, the players and the dances
	virtual void ProcessTurnStart(uint32_t turn) = 0;
	/// The forests, after the global game lists and before the living things
	virtual void ProcessForests(uint32_t turn) = 0;
	/// The test hooks read from the environment, once a turn, after the miracles' turn
	virtual void RunDebugHooks() = 0;
	/// The end of the particle effects' turn, after the physics: the queue of exploded meshes and the sounds of this
	/// turn's particles
	virtual void ProcessSpellParticlesEndOfLoop() = 0;
	/// The hand's turn, after the belief's: the grain rising from the hand, then what the hand holds
	virtual void ProcessHandTurn() = 0;

	// The miracles' own parts, which the spells reach through the service (his tree has them on its spell services)

	/// The flock miracles; none when there are none
	[[nodiscard]] virtual magic::FlockMiracleInterface* Flocks() { return nullptr; }

	// Ours: the state the miracles' free functions keep, owned by the service for the whole game (his tree keeps it as
	// the service's own members). Each is the same object on every call until the service goes.

	/// The running spells, their sinks, their class table and the grid of where they act
	[[nodiscard]] virtual SpellSystemInterface& SpellStore() = 0;
	/// The map shields and the fireballs
	[[nodiscard]] virtual MagicObjectsSystemInterface& MagicObjects() = 0;
	/// The falling spell and whether its film's sparks were seen
	[[nodiscard]] virtual FallingSpellSystemInterface& FallingSpellStore() = 0;
	/// The state of the hand's magic modules
	[[nodiscard]] virtual HandMagicStateInterface& HandMagic() = 0;
};

} // namespace openblack::ecs::systems
