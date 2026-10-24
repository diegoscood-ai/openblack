/*******************************************************************************
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
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

/// The rules the physics follows over a game turn, around the bodies' own steps: how hard a turn's knocks were, which
/// knocks make a sound, which bodies stay awake and when a body has sunk out of sight.
namespace openblack::physics::turn
{

/// The impact of a turn's summed force: the mean force of the turn's twenty steps, 0 when the force is too small to count
[[nodiscard]] float Impact(glm::vec3 forceSum);
/// The impact of a turn's summed force when it counts (Impact), no value when it is too small: the body's impact is
/// then left as it was
[[nodiscard]] std::optional<float> CountedImpact(glm::vec3 forceSum);
/// How hard a knock was, in multiples of the body's own weight: 1 while lying on the ground
[[nodiscard]] float GLoad(float impact, float mass);

/// What the end of a turn does with a body's hitter, when its force counts
enum class HitterLookup : uint8_t
{
	None, ///< It touched no body this turn: it has no hitter
	Keep, ///< The list grew during the walk, so the body it touched is not found: the hitter stays, with no credit taken
	Slot, ///< The body it touched, by its place in the list, is its hitter (and gives it its credit)
};
/// The end of a turn's lookup of a body's hitter: whether it touched a body in the turn's steps, and whether the list
/// grew since the walk started
[[nodiscard]] HitterLookup LookUpHitter(bool lastHitSet, bool listGrewInThisLoop);

/// Whether a knock makes a sound: the body is moving and something hit it, or it was knocked harder than half its
/// weight
[[nodiscard]] bool WantsCollisionSound(bool resting, bool hitByBody, float impact, float mass);

/// The pairs of things that have just sounded against each other: the same pair stays quiet for the turn it was added
/// and the next, and no more than 128 pairs are kept
class SoundPairs
{
public:
	static constexpr size_t k_MostPairs = 128;
	static constexpr int k_QuietTurns = 2;

	/// Whether a pair may sound now: not listed either way round, and the list not full
	[[nodiscard]] bool MaySound(entt::entity a, entt::entity b) const;
	/// Lists a pair that has just sounded (MaySound said it may)
	void Add(entt::entity a, entt::entity b);
	/// A turn has gone by
	void EndTurn();
	[[nodiscard]] size_t Size() const { return _pairs.size(); }

private:
	struct Pair
	{
		entt::entity a;
		entt::entity b;
		int turns;
	};
	std::vector<Pair> _pairs;
};

/// Whether a body's step took it into the camera's ten metres fast enough to whoosh past it: its squared distance to
/// the camera now, at the last step, and its velocity
[[nodiscard]] bool PassesCamera(float distanceSquared, float lastDistanceSquared, glm::vec3 velocity);

/// Whether a body is awake as a turn starts, before what moves wakes what is near it: a moving body is, a resting one is
/// not. A body still asleep after the waking leaves the physics.
[[nodiscard]] constexpr bool AwakeAtTurnStart(bool resting)
{
	return !resting;
}

/// Whether a moving body has sunk out of sight: its centre at least its radius under the sea's level. Its pose is not
/// updated from then on.
[[nodiscard]] bool SunkOutOfSight(float centreHeight, float radius);

/// Takes an entry out of the physics list as the original does everywhere: the last entry moves into its place, so the
/// others keep their places but the last one's changes
template <typename T>
void RemoveByMovingLast(std::vector<T>& list, size_t index)
{
	if (index + 1 != list.size())
	{
		list[index] = std::move(list.back());
	}
	list.pop_back();
}

/// Whether a body being raised clear of what is under it is raised over another body in the physics. Every body counts,
/// whatever its class (a physical shield, a way out of a land), except that a villager is not raised over what a Living
/// pushed, nor that over a villager
[[nodiscard]] constexpr bool RaisedOver(bool raisedIsVillager, bool raisedPushedByLiving, bool otherIsVillager,
                                        bool otherPushedByLiving)
{
	return !((raisedIsVillager && otherPushedByLiving) || (otherIsVillager && raisedPushedByLiving));
}

/// Whether a knocked body teaches its player's creature the deed of damage by throwing: it came from the hand, a player
/// has the credit for it, and it is a living thing or a rock, and no kind of tree
[[nodiscard]] constexpr bool IsDamageByThrowing(bool fromHand, bool credited, bool living, bool rock, bool tree)
{
	return fromHand && credited && (living || rock) && !tree;
}

/// The work the end of a physics turn does on each body, which EndOfTurn runs in order. A body is named by its place in
/// the list as the walk reaches it. Nothing leaves the list during the walk but a resting body that AddObject replaces
/// (by the last one moving into its place), so from the reaction on the steps are about the body just reacted.
class EndOfTurnSteps
{
public:
	virtual ~EndOfTurnSteps() = default;

	/// How many bodies are listed now. It is read again at every body, so the bodies a reaction adds are walked too
	[[nodiscard]] virtual size_t Count() const = 0;
	/// The turn's steady pushes end
	virtual void ClearPushes(size_t i) = 0;
	/// A felled tree that has toppled sounds once if it is tall, and is marked as toppled
	virtual void ToppleFelledTree(size_t i) = 0;
	/// A resting body follows its object about
	virtual void FollowObject(size_t i) = 0;
	/// The impact from the turn's summed force (CountedImpact): false when the force is too small to count, and the
	/// impact is then left as it was
	virtual bool TakeImpact(size_t i) = 0;
	/// What hit it (LookUpHitter), only when its force counts: the body it last hit, when that body is found in the list.
	/// Only then does it take that body's credit for the throw, when it has none of its own
	virtual void FindHitter(size_t i) = 0;
	/// The collision sound, when the knock makes one (WantsCollisionSound)
	virtual void CollisionSound(size_t i) = 0;
	/// The object's reaction to its impact
	virtual void React(size_t i) = 0;
	/// The deed of damage by throwing, or of damage by throwing at something, when IsDamageByThrowing holds
	virtual void ConsiderDamageByThrowing(bool atWhatItHit) = 0;
	/// Something hit the body just reacted
	[[nodiscard]] virtual bool WasHit() const = 0;
	/// The scripts' record of the last thing hit and what hit it becomes this body and its hitter
	virtual void RecordHit() = 0;
	/// After the walk: the sound pairs age a turn (SoundPairs::EndTurn)
	virtual void AgeSoundPairs() = 0;
};

/// The end of a physics turn, in the original's order. Body by body, in list order: the pushes end, a felled tree's
/// sound, a resting body follows its object, the impact. Then, only when the turn's force counts: the hitter and the
/// credit, the collision sound, the reaction, the deed of damage by throwing, and when something hit it, the scripts'
/// hit record and then the deed of damage by throwing at something. A body whose force does not count keeps its impact
/// and its hitter as they were. After the walk, the sound pairs age.
void EndOfTurn(EndOfTurnSteps& steps);

/// A colour moved towards another by an amount in 256ths, channel by channel, rounding down; its alpha kept. The snow
/// lying where a dust is made moves its colour so towards the land's light, by the snow's depth (0 to 255)
[[nodiscard]] uint32_t BlendColour(uint32_t argb, uint32_t towards, int32_t amount);

/// The kinds of puff the shared pool of puffs holds. Each kind has its own life, growth and frames.
enum class PuffKind : uint8_t
{
	/// Dust that a landing or a falling piece throws up. It lives one second.
	Dust,
	/// Dark smoke that rises off an evil creature. It lives two seconds.
	Smoke,
	/// A light mote that shows on a good creature. It lives two seconds and is added to what is behind it.
	Mote,
	/// The same mote, but its frames play in the opposite order
	MoteReversed,
};
/// How long a puff of a kind lives, in seconds of game time: it goes once its age is past this, not at it
[[nodiscard]] constexpr float PuffLife(PuffKind kind)
{
	return kind == PuffKind::Dust ? 1.0f : 2.0f;
}
/// Whether a puff of a kind is added to what is behind it, rather than blended over it
[[nodiscard]] constexpr bool IsAdditive(PuffKind kind)
{
	return kind == PuffKind::Mote || kind == PuffKind::MoteReversed;
}
/// A puff's half size at an age, `size` the largest half size of its kind's curve. Dust and smoke grow over the first
/// eighth of their life and shrink to nothing at its end. A mote grows over its first quarter second, shrinks to nothing
/// at two seconds and pulses once a second, between a third of its size and its size. Each sum and product is rounded
/// to a float in the order written in the source.
[[nodiscard]] float DustPuffSize(PuffKind kind, float age, float size);
/// The frame of the puffs' sheet (8 by 8) a puff shows, from the number from 0 to 15 it was given. Dust and smoke show
/// one of the sixteen after the first sixteen, moving on twice a second. A mote shows one of the first sixteen, moving
/// on eight times a second, and a reversed mote counts down through them instead.
[[nodiscard]] int32_t DustPuffFrame(PuffKind kind, uint32_t variant, float age);

/// The scripts' record of the last hit in the physics: the thing hit and what hit it, entt::null for none
struct HitRecord
{
	entt::entity hit {entt::null};
	entt::entity hitter {entt::null};
};
/// Whether a thing the record holds is still available to it, as the original asks each thing: it is not marked for
/// deletion and, for a villager only, it is not dying (the dying state is its final state). The dying test is asked
/// only of a villager that passes the first.
template <std::predicate<> VillagerDying>
[[nodiscard]] bool IsAvailableToHitRecord(bool markedForDeletion, bool isVillager, VillagerDying&& villagerDying)
{
	return !markedForDeletion && (!isVillager || !villagerDying());
}
/// The record once each of its two has been asked whether it is still available: one that is not is forgotten, the
/// other is kept. None is not asked. The scripts' turn does this before the scripts run, and so does a land change.
template <std::predicate<entt::entity> Available>
[[nodiscard]] HitRecord ForgetUnavailable(HitRecord record, Available&& available)
{
	if (record.hit != entt::null && !available(record.hit))
	{
		record.hit = entt::null;
	}
	if (record.hitter != entt::null && !available(record.hitter))
	{
		record.hitter = entt::null;
	}
	return record;
}

/// A felled tree has fallen once its up axis has tipped this far from upright; taller than this it makes a sound
inline constexpr float k_FelledUpright = 0.98f;
inline constexpr float k_FelledSoundHeight = 10.0f;
/// Whether a felled tree has toppled: its up axis has tipped past k_FelledUpright
[[nodiscard]] constexpr bool FelledTreeToppled(float upAxisHeight)
{
	return upAxisHeight < k_FelledUpright;
}
/// Whether a felled tree that topples makes the sound of a tree that falls: only a tree taller than k_FelledSoundHeight
[[nodiscard]] constexpr bool FelledTreeFallSounds(float treeHeight)
{
	return treeHeight > k_FelledSoundHeight;
}

} // namespace openblack::physics::turn
