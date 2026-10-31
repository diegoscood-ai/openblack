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
#include <cstdint>

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureFight.h"
#include "Creature/CreatureFightHud.h"
#include "Creature/CreatureSkinRay.h"

namespace openblack::ecs::components
{
struct CreatureAnimationInputs;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

/// Creature fights, and creatures knocked out (see components::CreatureFighting, CreatureFightRecord and
/// CreatureKnockedOut). A fight starts when an angry creature picks one, when the player leashes their creature to
/// another, or when told to by a script or the debug tools. The two walk to their places in an arena, taunt each other
/// and duel until one is knocked out; the player directs their own creature with the hand, or the creature fights by
/// itself. The loser lies out cold, is taken home, rests and gets up again; only a script kills a creature for good.
class CreatureFightSystemInterface
{
public:
	enum class StartResult : uint8_t
	{
		Started,
		/// Not two different creatures
		NoOpponent,
		/// Either is already fighting or out cold
		Busy,
		/// The creature starting it isn't healthy enough
		TooWeak,
		/// The other creature, asked first, won't fight it: it is less than half or more than twice its height, it lies
		/// dead, or only one of the two is under a script's control
		Refused,
	};

	/// The arena of a fight now on: the creature that made it, its opponent, and its middle (as a map position stores it)
	/// and radius on the ground
	struct ArenaView
	{
		entt::entity maker {entt::null};
		entt::entity opponent {entt::null};
		glm::vec2 centre {0.0f};
		float radius {0.0f};
		/// Arenas are counted as they are made, from 1
		uint32_t serial {0};
	};

	/// Where a line of sight first meets a creature's skin, how far up the body that is as a share of the height of its
	/// head above the land, and the spot on the skin
	struct BodyPoint
	{
		glm::vec3 point {0.0f};
		float heightShare {0.0f};
		creature_skin_ray::Spot spot {};
	};

	virtual ~CreatureFightSystemInterface() = default;

	/// Once a game turn, after the creatures have moved: the fight animations play on by a turn, blows land and the
	/// fighters move as their animations carry them; fights start and end, moves are chosen and made, stamina comes
	/// back, and creatures knocked out come round
	virtual void ProcessTurn() = 0;
	/// Once a frame, by how far through the turn the frame is (0 to 1) and the frame's milliseconds: only a held blow's
	/// charge and the drawn animation time move on
	virtual void Update(float turnFraction, float frameMs) = 0;
	/// The fight animation Update sets a fighter's body playing at a share of the turn, written into `inputs` as Update
	/// writes it into its animation: nothing for a creature that isn't fighting. Nothing is changed, a held blow's charge
	/// included
	virtual void AnimationAt(entt::entity creature, float turnFraction, components::CreatureAnimationInputs& inputs) const = 0;

	/// A fight between two creatures, the first making the arena, as scripts and the leash start them
	virtual StartResult StartFight(entt::entity creature, entt::entity opponent) = 0;
	/// Ends a creature's fight with no winner: both finish and stand
	virtual void AbortFight(entt::entity creature) = 0;
	/// A creature taken out of the game leaves its fight at once; its opponent's fight ends as a fight given up does, and
	/// it forgets the creature gone
	virtual void Withdraw(entt::entity creature) { AbortFight(creature); }
	/// The creature is taken out of its fight, as the leash pulling it away from what it does: the fight ends with no
	/// winner. Both creatures finish the move they are making and stand, without celebrating or sulking, and each loses
	/// from its life a quarter of the fight health it lost. A fight still being set up is simply left.
	virtual void CallOff(entt::entity creature) { AbortFight(creature); }
	/// Only this creature's side of its duel ends, as its body is made to stop what it does: in the midst of its fight
	/// (starting, standing, a blow, a block or a cast) the player's camera is told the fight is over, it loses from its
	/// life a quarter of the fight health it lost, and it finishes its move and stands; at any other moment nothing
	/// changes. Its opponent's fight is left as it is
	virtual void EndOwnFight(entt::entity /*creature*/) {}
	[[nodiscard]] virtual bool IsFighting(entt::entity creature) const = 0;
	/// Whether a creature is in the duel itself, from its fight start to the end of its fight: what scripts call
	/// fighting. Not while it walks to its place, taunts or waits for its opponent, nor once its fight has ended.
	[[nodiscard]] virtual bool IsDuelling(entt::entity /*creature*/) const { return false; }
	[[nodiscard]] virtual std::optional<entt::entity> OpponentOf(entt::entity creature) const = 0;
	/// The arenas of the fights now on, newest first. An arena lasts while the creature that made it is in its fight.
	[[nodiscard]] virtual std::vector<ArenaView> GetArenas() const = 0;
	/// The arena a creature made or was called into, if it is still there
	[[nodiscard]] virtual std::optional<ArenaView> ArenaOf(entt::entity fighter) const = 0;

	/// A move added to a fighter's queue as the player makes it, in place of the rest or after them; a blow waits for its
	/// charge until ReleaseCharge
	virtual bool QueueMove(entt::entity creature, const creature_fight::Move& move, bool replace) = 0;
	/// The charge, in milliseconds held, of the blow waiting in the fighter's queue
	virtual void ReleaseCharge(entt::entity creature, float heldMs) = 0;
	/// A move a script adds to the back of a fighter's queue: as the player's are, but teaching the creature only what
	/// the game teaches it from a script's move. False when it isn't fighting or its queue is full.
	virtual bool QueueScriptMove(entt::entity /*creature*/, const creature_fight::Move& /*move*/) { return false; }
	/// How many blows at a band wait in a fighter's queue
	[[nodiscard]] virtual uint32_t QueuedBlows(entt::entity /*creature*/) const { return 0; }
	/// What a creature is doing in its fight, as scripts ask it: what its fighter counts as doing; out of a fight,
	/// fainting while it lies knocked out, and otherwise nothing a fight counts (Other)
	[[nodiscard]] virtual creature_fight::FightAction CurrentFightAction(entt::entity /*creature*/) const
	{
		return creature_fight::FightAction::Other;
	}
	/// Whether the creature fights by itself whatever the player does. Off, nobody fights for it: it makes only the
	/// moves queued for it, and the computer never takes over.
	virtual void SetAutoFighting(entt::entity creature, bool autoFight) = 0;
	[[nodiscard]] virtual bool IsAutoFighting(entt::entity creature) const = 0;

	/// How many moves wait in a fighter's queue
	[[nodiscard]] virtual size_t QueuedMoves(entt::entity /*creature*/) const { return 0; }
	/// Where a line of sight first meets a creature's skin as it is drawn this frame, if it does and its head's height is
	/// known: what a press of the hand on it lands on
	[[nodiscard]] virtual std::optional<BodyPoint> BodyHit(entt::entity /*creature*/, const glm::vec3& /*rayOrigin*/,
	                                                       const glm::vec3& /*rayDirection*/) const
	{
		return std::nullopt;
	}
	/// A press on the arena's ground a player sent, as the turn takes it: the creature steps towards the point, from where
	/// it stands and the way it faces, in place of its queued moves or after them
	virtual void GroundPress(entt::entity /*creature*/, const glm::vec3& /*point*/, bool /*replace*/) {}

	/// The marks of the hand's presses for a fighter's queued moves (creature_fight_marks): how many show, a press on a
	/// fighter's skin (its own or its opponent's) or on the ground marked as it is made, and the press on a skin let go
	[[nodiscard]] virtual size_t MarkCount(entt::entity /*creature*/) const { return 0; }
	virtual void MarkOnSkin(entt::entity /*creature*/, bool /*own*/, const creature_skin_ray::Spot& /*spot*/) {}
	virtual void MarkOnGround(entt::entity /*creature*/, const glm::vec3& /*point*/) {}
	virtual void LetGoOfMark(entt::entity /*creature*/) {}
	/// One sparkle of a mark as this frame draws it: where, its half size, its colour (0xAARRGGBB) and its frame of the
	/// sheet of blobs, added to what is behind it
	struct Sparkle
	{
		glm::vec3 position {0.0f};
		float halfSize {0.0f};
		uint32_t argb {0};
		uint32_t frame {0};
	};
	/// The sparkles of the frame, as the last frame update left them
	[[nodiscard]] virtual std::vector<Sparkle> Sparkles() const { return {}; }

	/// Whether a creature in a fight is blocking now
	[[nodiscard]] virtual bool IsBlocking(entt::entity /*creature*/) const { return false; }
	/// A creature in a fight reels from what a miracle did to it, unless it reels already: blocking, it recoils in its
	/// block; standing, striking or casting, it staggers
	virtual void Recoil(entt::entity /*creature*/) {}
	/// A miracle's effect reaching a creature in a fight: its heal and its harm go to the fight health
	/// (creature_fight::TakeMiracle); harmed to none, it is knocked out, its opponent winning the fight
	virtual void TakeMiracle(entt::entity /*creature*/, float /*heal*/, float /*harm*/) {}

	/// Knocks a creature out, as a fight's loser is
	virtual void KnockOut(entt::entity creature) = 0;
	/// The creature faints where it stands whatever it is doing, as something overwhelming it does (caught by a
	/// tornado): it lies helpless, then comes round as from a knock-out. A fight it is in is left, not lost.
	virtual void ForceFaint(entt::entity /*creature*/) {}
	/// The Die action forced on a creature, done to a thing: it faints as from its needs, but its spells and its leash are
	/// left alone; it lies by the size it is drawn at from the turn it is seen lying, is carried home, and there its spells
	/// and the thing's are brought to their end before it rests, as it was decided as the action was forced, and gets
	/// up. Lying knocked out already, it starts again from the faint
	virtual void ForceDie(entt::entity /*creature*/, entt::entity /*actedOn*/) {}
	/// Kills a creature for good: it faints and never gets up. Only scripts do this.
	virtual void KillPermanently(entt::entity creature) = 0;
	/// Brings a creature knocked out round at once, at 0.4 of its life
	virtual void Resurrect(entt::entity creature) = 0;
	[[nodiscard]] virtual bool IsKnockedOut(entt::entity creature) const = 0;
	/// The animation a fighter's body is named by while it is in a fight (creature_fight::Fighter::actionAnimation, the
	/// stand pose in its stance), or knocked out (its faint until it gets up, then getting up), which how far it has
	/// played is measured against; none otherwise
	[[nodiscard]] virtual std::optional<size_t> ActionAnimation(entt::entity /*creature*/) const { return std::nullopt; }

	/// The panel of the fight the player's camera watches, lingering on it too, while its arena is there: the creature
	/// that made the arena first
	[[nodiscard]] virtual std::optional<creature_fight_hud::Values> GetPanel() const = 0;

	/// Whether angry creatures pick fights by themselves, and whether the camera may go to watch a fight as it starts
	virtual void SetAngerStartsFights(bool enabled) = 0;
	[[nodiscard]] virtual bool GetAngerStartsFights() const = 0;
	virtual void SetCameraWatches(bool enabled) = 0;
	[[nodiscard]] virtual bool GetCameraWatches() const = 0;
	/// Whether the player's camera is watching a fight now
	[[nodiscard]] virtual bool IsCameraOnFight() const = 0;
};

} // namespace openblack::ecs::systems
