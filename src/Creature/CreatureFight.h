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

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureKnock.h"

namespace openblack::physics
{
struct Ellipsoid;
}

/// Creature fights: two creatures duel inside a circular arena. Each plays a small set of fight states (starting,
/// standing in its stance, acting, blocking, casting, reeling, fainting) fed by a queue of up to twelve moves. The
/// player fills the queue by clicking the opponent's body high, in the middle or low, their own creature to block, or
/// the ground to step, holding the click to charge a blow. Left alone, the creature fights by itself, more or less
/// aggressively by what it has learnt of fighting. Each blow takes from a fight health of its own, which starts at
/// (life + 1) / 2; at nothing the creature faints, and a quarter of what it lost comes off its real life afterwards.
/// Creatures never die of it: the loser lies out cold a while, is taken home, rests and gets up again.
namespace openblack::creature_fight
{
/// The places of the fight animations in the creature spec file
namespace animations
{
constexpr size_t k_Faint = 106;
constexpr size_t k_GetUp = 107;
/// Played while a charged blow waits to be let go
constexpr size_t k_PowerUp = 108;
constexpr size_t k_StartCast = 110;
constexpr size_t k_Cast = 111;
constexpr size_t k_EndCast = 112;
constexpr size_t k_Start = 122;
constexpr size_t k_Stance = 123;
constexpr size_t k_Finish = 124;
/// The blows: high, eye, belly, pelvis, knee and special, then a second of each
constexpr size_t k_FirstAttack = 125;
constexpr size_t k_AttackCount = 12;
constexpr size_t k_AttackHigh = 125;
constexpr size_t k_AttackEye = 126;
constexpr size_t k_AttackBelly = 127;
constexpr size_t k_AttackPelvis = 128;
constexpr size_t k_AttackKnee = 129;
constexpr size_t k_AttackSpecial = 130;
constexpr size_t k_AttackSpecial2 = 136;
constexpr size_t k_StepForward = 137;
constexpr size_t k_StepBack = 138;
constexpr size_t k_StepRight = 139;
constexpr size_t k_StepLeft = 140;
constexpr size_t k_StartBlock = 142;
constexpr size_t k_Block = 143;
constexpr size_t k_EndBlock = 144;
/// Reeling from a blow high, in the middle and low, each followed by its right, left, top and bottom versions
constexpr size_t k_RecoilHigh = 179;
constexpr size_t k_RecoilMid = 184;
constexpr size_t k_RecoilLow = 189;
/// The body's sways, high and low, side to side and front to back, leaned by the forces on it (creature_sway); no
/// blow plays them
constexpr size_t k_WobbleHighSide = 194;
constexpr size_t k_WobbleHighFrontBack = 195;
constexpr size_t k_WobbleLowSide = 196;
constexpr size_t k_WobbleLowFrontBack = 197;
/// Reeling back while blocking
constexpr size_t k_RecoilBlock = 198;
/// The actions played around a fight: the taunt or the rude gesture before it, and being happy, sad or showing off after
/// it
constexpr size_t k_Taunt = 70;
constexpr size_t k_RudeGesture = 222;
constexpr size_t k_Happy = 55;
constexpr size_t k_Sad = 56;
constexpr size_t k_Impress = 65;
} // namespace animations

[[nodiscard]] bool IsAttack(size_t animation);
[[nodiscard]] bool IsSpecialAttack(size_t animation);
[[nodiscard]] bool IsStep(size_t animation);

/// random(n) is a whole number from 0 to n - 1
using Random = std::function<uint32_t(uint32_t)>;

// The arena

/// The arena's radius is 15 units for each of the bigger creature's drawn size, times 2.6, at most 60
constexpr float k_ArenaUnitsPerSize = 15.0f;
constexpr float k_ArenaRadiusFactor = 2.6f;
constexpr float k_MaxArenaRadius = 60.0f;
/// The two creatures take their places this many units of their size either side of its middle
constexpr float k_SpotPerSize = 15.0f;
/// Within this many units of their size of their place counts as arrived
constexpr float k_ArrivalPerSize = 1.5f * 15.0f;
/// Moves are only made, and creatures only step back or sideways, within this share of the radius
constexpr float k_RangeShare = 0.8f;

struct Arena
{
	glm::vec2 centre {0.0f};
	float radius {0.0f};
	/// Arenas are counted as they are made, from 1: the newest has the largest count
	uint32_t serial {0};
};
/// The arena's radius for two creatures drawn at these sizes: the first's units unless the second's are as many or more,
/// times the factor, worked out in that order; below none (or not a number) none, above the most the most
[[nodiscard]] float ArenaRadius(float sizeA, float sizeB);
/// The arena between two creatures drawn at these sizes: centred between them
[[nodiscard]] Arena MakeArena(glm::vec2 a, glm::vec2 b, float sizeA, float sizeB);
/// The game's sound played as a fight takes its arena (in-game bank)
constexpr int k_ArenaTakenSample = 176;
/// Where the arena's sound plays across the land: its middle as a map position keeps it
[[nodiscard]] glm::vec2 ArenaSoundPoint(const Arena& arena);
/// Where a creature takes its place: the one that made the arena to the east of the middle, the other to the west
[[nodiscard]] glm::vec2 ArenaSpot(const Arena& arena, float size, bool madeIt);
[[nodiscard]] float ArrivalDistance(float size);
/// Whether a point is within the range moves are made in
[[nodiscard]] bool WithinRange(const Arena& arena, glm::vec2 point);
/// How far a fighter's body reaches ahead of where it stands, given how far ahead each of its bones is: the furthest
/// of them, and none when every bone is behind it
[[nodiscard]] float ForwardExtent(std::span<const float> bonesAhead);
/// How far a fighter's body reaches ahead of a place along a way, from where its bones are on the land: the furthest of
/// them ahead of the place, and none when every bone is behind it. Each bone's distance ahead is its offset along x
/// times the way's x, plus its offset along the land's other axis times the way's other part, each step a float.
[[nodiscard]] float ForwardExtentFrom(std::span<const glm::vec2> bones, glm::vec2 at, glm::vec2 ahead);
/// In each turn of a blow the attacker and its opponent are pushed apart along the way the attacker faces when they
/// stand nearer than the attacker's reach ahead and half the opponent's: by half of how much nearer, the attacker back
/// and the opponent on. The push the opponent takes, none when they stand far enough apart
[[nodiscard]] std::optional<glm::vec2> BlowPushOf(glm::vec2 attacker, glm::vec2 opponent, glm::vec2 ahead, float attackerExtent,
                                                  float opponentExtent);
/// In its stance, while its opponent swings no blow, a fighter and its opponent are pulled apart along the way the
/// fighter faces when they stand nearer than both their reaches ahead together: by half of how much nearer, the fighter
/// back and the opponent on. The push the opponent takes, none when they stand far enough apart
[[nodiscard]] std::optional<glm::vec2> PullApartOf(glm::vec2 fighter, glm::vec2 opponent, glm::vec2 ahead, float fighterExtent,
                                                   float opponentExtent);
/// Whether a point is strictly inside the range, short of its edge: a block's recoil moves a creature only to such a place
[[nodiscard]] bool StrictlyWithinRange(const Arena& arena, glm::vec2 point);

// Bands of the body

/// High, middle and low on the body, by shares of its height
enum class Band : uint8_t
{
	High,
	Mid,
	Low,
};
constexpr float k_LowBelow = 0.33f;
constexpr float k_HighAbove = 0.66f;
/// The band a height up the body falls in, as a share of the body's height
[[nodiscard]] Band BandOf(float shareOfHeight);

// Moves

enum class Step : uint8_t
{
	Forward,
	Back,
	Right,
	Left,
};
[[nodiscard]] size_t StepAnimation(Step step);
/// The step a press on the arena's ground makes, from where the creature stands and its heading: the way to the point
/// is turned into the creature's frame, and it steps forward when the point lies further ahead than to either side,
/// back when it lies further behind, right when further to the right, and otherwise left (a point as far ahead as to
/// the side, or right on the creature, among them). Worked as the game's floating point does, to its bits
[[nodiscard]] Step GroundStep(glm::vec2 point, glm::vec2 at, float heading);
/// Whether a creature at a point may take a step: forward always, back or sideways only within range
[[nodiscard]] bool CanStep(const Arena& arena, glm::vec2 position, Step step);
/// The arena's keep-in rule: whether a fighter and its opponent both stand strictly inside the range, the arena's edge
/// fraction of its radius from its middle on the map. When either does not, a fighter in its block ends the block and
/// one in its stance steps back towards the middle (KeepInStep), and neither takes a move or chooses one that turn.
/// Worked as the game's floating point does
[[nodiscard]] bool BothKeptIn(const Arena& arena, glm::vec2 fighter, glm::vec2 opponent);
/// Whether a point stands strictly inside the range, the arena's edge fraction of its radius from its middle on the map,
/// worked as the game's floating point does: the keep-in rule's test for one fighter, and the pull-apart's for each
/// fighter's place after its push
[[nodiscard]] bool KeptIn(const Arena& arena, glm::vec2 point);
/// The step that takes a fighter in its stance back towards the arena's middle (its height the land's there): the way
/// to the middle turned into the fighter's frame by its heading; sideways when it lies less far ahead or behind than
/// half again as far to the side, right when to its right, else left; otherwise forward when ahead, else back. None when
/// it stands within its body's radius of the middle. Worked as the game's floating point does, to its bits
[[nodiscard]] std::optional<Step> KeepInStep(glm::vec3 middle, glm::vec3 at, float heading, float bodyRadius);

struct Move
{
	enum class Kind : uint8_t
	{
		/// Blows at a band of the opponent's body
		High,
		Mid,
		Low,
		Block,
		Special,
		/// Playing an animation, as stepping is
		Animation,
		Spell,
	};
	Kind kind {Kind::Mid};
	/// The animation played, or the spell cast
	uint32_t value {0};

	bool operator==(const Move&) const = default;
};
[[nodiscard]] Move AttackMove(Band band);
[[nodiscard]] Move StepMove(Step step);
[[nodiscard]] Move BlockMove();
[[nodiscard]] bool IsBlow(Move::Kind kind);

/// A move as scripts number them: 0, 1 and 2 blows high, in the middle and low, 3 a block, 4 the special move; with the
/// animation bit an animation played (a step), else with the spell bit a miracle cast, each by the number in the low 14
/// bits. None for any other number, which the game would try as a blow at no band.
[[nodiscard]] std::optional<Move> ScriptMove(uint32_t value);
constexpr uint32_t k_ScriptAnimationBit = 0x8000;
constexpr uint32_t k_ScriptSpellBit = 0x4000;

/// What a fighter is doing, as scripts ask it, by the animation it counts as doing (see Fighter::actionAnimation)
enum class FightAction : uint8_t
{
	/// Its fight start, stance or finish animation
	Stance = 0,
	Block = 1,
	Step = 2,
	Blow = 3,
	Casting = 4,
	Special = 5,
	ReelingHigh = 6,
	ReelingMiddle = 7,
	ReelingLow = 8,
	Fainting = 9,
	/// Any other animation, no action at all and reeling in a block among them
	Other = 10,
};
[[nodiscard]] FightAction FightActionOf(size_t animation);

/// A blow is charged for as long as the click is held, up to 1.2 seconds
constexpr float k_MaxChargeMs = 1200.0f;
/// What a move starts charged at: a blow waits to be let go (none), blocks and animations need no charge, and special
/// moves and spells are fully charged
[[nodiscard]] std::optional<float> InitialCharge(Move::Kind kind);
[[nodiscard]] float ReleasedCharge(float heldMs);
/// The speed that a blow asks for each millisecond of its charge: 1/1200 as a float
constexpr float k_SpeedPerChargeMs = 0.000833333354f;
/// The speed that a blow asks for by its charge, 0.5 to 1.5
[[nodiscard]] float BlowSpeed(float chargeMs);
/// The fight's computer opponent asks for this speed for its blows
constexpr float k_AiBlowSpeed = 0.5f;
/// An action keeps the speed that it asks for plus this value. Only the damage of a blow uses the kept speed.
constexpr float k_ActionSpeedBase = 0.5f;
[[nodiscard]] float ActionSpeed(float askedSpeed);

struct QueuedMove
{
	Move move {};
	std::optional<float> chargeMs;
};

/// The moves a creature has been told to make, in turn, at most twelve
class MoveQueue
{
public:
	static constexpr size_t k_Capacity = 12;

	/// A move added at the back, charged as its kind starts, or in place of every other at the front. False when full,
	/// and then no blow waits to be let go any more.
	bool Push(const Move& move, bool replace);
	[[nodiscard]] std::optional<QueuedMove> Front() const;
	/// The front move taken; whether the move added last waits to be let go is left as it was
	void Pop();
	void Clear();
	/// The move added last gets its charge, if it is a blow still waiting to be let go. False otherwise. A blow left
	/// waiting behind a later move never gets one.
	bool Release(float heldMs);
	/// Getting hit takes the move added last away, if it is a blow still waiting to be let go, and says so
	bool TakeWaitingBlow();
	/// Whether the move added last is a blow waiting to be let go: the fighter powers up in its stance meanwhile
	[[nodiscard]] bool AwaitsRelease() const { return _awaitingRelease; }
	[[nodiscard]] size_t Size() const { return _count; }
	[[nodiscard]] bool Empty() const { return _count == 0; }
	[[nodiscard]] std::span<const QueuedMove> Moves() const { return {_moves.data(), _count}; }

private:
	std::array<QueuedMove, k_Capacity> _moves {};
	size_t _count {0};
	/// The move added last is a blow waiting for the button to be let go
	bool _awaitingRelease {false};
};

/// How many of the queued moves are blows at a band (not blocks, steps, miracles or the special move)
[[nodiscard]] uint32_t QueuedBlows(std::span<const QueuedMove> moves);

// Learning to fight

/// How the creature leans in fights, from -1 (defensive) to 1 (aggressive). A move nudges it each time the creature
/// looks at it at the front of its queue, from its stance with its opponent in range, whoever queued it: a block or a
/// step towards defence, a blow or the special move towards attack; a miracle leaves it be. See LearnsFromMove.
[[nodiscard]] float LearnTendency(float tendency, Move::Kind kind);
/// Whether a move at the front of the queue teaches a creature to lean: a step teaches every creature, a miracle none,
/// and the other moves only the local player's creature
[[nodiscard]] bool LearnsFromMove(Move::Kind kind, bool localPlayersCreature);
/// A creature's first fight starts it leaning against its alignment: evil creatures attack, good ones defend
[[nodiscard]] float FirstTendency(float alignment);
/// The lean a creature has learnt, as it starts a fight itself: against its alignment again until it has performed a
/// fight (a fight it set out on as its plan and was left the stronger after, see FightPerformed), else what it learnt
[[nodiscard]] float TendencyAtStart(uint32_t fightsPerformed, float learnt, float alignment);

// Health, stamina and fainting

[[nodiscard]] float FightHealthAtStart(float life);
[[nodiscard]] float StaminaAtStart(float energy, float exhaustion);
/// Stamina comes back a little every game turn, up to 1. Nothing is known to use it up. The game's own amount, a little
/// less than a six-hundredth
constexpr float k_StaminaPerTurn = 0.0016666f;
[[nodiscard]] float RegainStamina(float stamina);
/// A quarter of the fight health lost comes off the real life at the end
constexpr float k_LifeLostShare = 0.25f;
/// The life after the fight: a quarter of the fight health it does not have comes off. Nothing keeps either within 0
/// and 1, so a fight health a blow took below none costs more than a quarter, and the life can go below none
[[nodiscard]] float LifeAfterFight(float life, float fightHealth);
/// One call of a stop chain, on one of the two creatures of a fight
enum class StopCall : uint8_t
{
	/// The creature gives up its action as a failure, and its body ends its fight
	Fail,
	/// The creature gives up its action as a failure, but its own body keeps what it does at the start. Each call that
	/// this call starts again ends the body of its creature.
	FailKeepingBody,
	/// Only the body of the creature ends its fight. Its partner does not change.
	EndBody,
};
/// A call of a stop chain and the creature that it is on: 0 is the creature that stops, 1 is its partner
struct StopStep
{
	size_t creature {0};
	StopCall call {StopCall::Fail};
};
/// The two creatures of a fight at a stop: 0 is the creature that stops, 1 is its partner
struct StopSides
{
	/// Whether each creature is in its duel against the other
	std::array<bool, 2> duels {};
	/// Whether the mind of each creature has the other creature as its partner
	std::array<bool, 2> partners {};
};
/// What the calls of a stop chain do to the two creatures
struct StopChainResult
{
	/// How many times each creature gets the strength gain of the fight action
	std::array<uint32_t, 2> gains {};
	/// How many times the body of each creature ends its fight. Each time, the life of the creature pays for the fight
	/// again if its body is in the midst of the fight (CameraLingersOnEnd). The body does not change its state during
	/// the chain.
	std::array<uint32_t, 2> bodyEnds {};
	/// The two creatures after the chain
	StopSides after {};
};
/// The calls of a stop chain, one after the other. A creature that gives up its action gets the fight's strength gain
/// while in its duel, then stops; a creature that stops while in its duel gives the gain to the other, takes the other
/// out of its duel and makes it give up its action, then its own body ends its fight; then, with a partner, its mind
/// forgets the partner and makes the partner give up its action
[[nodiscard]] StopChainResult RunStopChain(StopSides sides, std::span<const StopStep> steps);
/// The calls of a knock out: the winner gives up its action for the plan that the win forces on it, then the loser
/// faints and gives up its action for its faint
[[nodiscard]] std::span<const StopStep> KnockOutSteps();
/// The calls of the leash's walk back: the creature's body ends its actions, its fight among them, then the walk it is
/// sent on gives up its action keeping its body (the force's stop with no end of its own)
[[nodiscard]] std::span<const StopStep> WalkBackSteps();
/// A landed blow's damage comes off the victim's fight health with no lower limit
[[nodiscard]] float HealthAfterBlow(float health, float damage);
/// A landed blow, blocked or not, is also a hit from the attacker on the victim: the impact's whole strength, halved,
/// times this. The hit takes nothing from a fighter's life
constexpr float k_BlowHit = 0.5f * 0.05f;
/// How much nicer the victim of a landed blow comes to find its attacker (nastier below 0): the blow's hit times the
/// alignment table's weight for a hit on a creature
[[nodiscard]] float BlowNicenessChange(float hitOnCreatureWeight);
/// A miracle's effect on a fighter's fight health: its heal goes on, the health kept within 0 and 1, then its harm
/// comes off, unclamped; harmed to none or less, it is knocked out
struct MiracleTaken
{
	float health {1.0f};
	bool knockedOut {false};
};
[[nodiscard]] MiracleTaken TakeMiracle(float health, float heal, float harm);
/// A fighter's fight health as life is taken from it in its duel, by anything but a miracle's fight side: as much less,
/// never below none
[[nodiscard]] float HealthAfterLifeLost(float health, float lost);
/// A fighter's fight health as life is given to it in its duel: as much more, never above 1
[[nodiscard]] float HealthAfterLifeGained(float health, float gained);
/// Blocking, a fighter takes a tenth of a miracle's effect, but for the harm to its fight health, worked out before
constexpr float k_BlockedMiracleShare = 0.1f;
/// A creature this healthy or less won't start a fight
constexpr float k_MinLifeToFight = 0.1f;
[[nodiscard]] bool HealthyEnoughToFight(float life);
/// A creature asked to fight agrees only to one between half and twice its own height, and then only when it is free for
/// a request (`creature_friendship::AvailabilityOf`: not lying dead, under a script's control as the asker is or is not,
/// and with no partner but the asker)
constexpr float k_SmallestOpponentShare = 0.5f;
constexpr float k_BiggestOpponentShare = 2.0f;
[[nodiscard]] bool AgreesToFight(float height, float askerHeight, creature_friendship::Availability availability);
/// Knocked out, a creature lies 8 seconds and 4 more for each of its size
[[nodiscard]] float FaintSeconds(float size);
/// Taken home, it fades out and in (creature_fizz::k_CarryHomeSeconds), then waits this long before resting
constexpr float k_WaitAfterHomeSeconds = 3.0f;
/// One turn of that wait, in its game turns left, as the game's wait counts: a turn counts only while the body does
/// nothing (no action, move or turn) or lies dead, and the wait is over on the turn its count reaches none. Whether it
/// is over now; the spells are waited for from the next turn
[[nodiscard]] bool CountHomeWait(int32_t& turnsLeft, bool bodyBusy, bool liesDead);
/// Fainting less healthy than this or more exhausted than this, it rests once home
constexpr float k_GetUpLife = 0.4f;
constexpr float k_RestedExhaustion = 0.3f;
[[nodiscard]] bool NeedsRest(float life, float exhaustion);

// Blows

constexpr float k_HighDamage = 0.05f;
constexpr float k_LowDamage = 0.03f;
/// A blocked blow does a tenth of the damage
constexpr float k_BlockedShare = 0.1f;
/// The damage compares the strengths of the two bodies after it adds this to each
constexpr float k_StrengthShift = 3.0f;
constexpr float k_MinDamageFactor = 0.04f;
constexpr float k_MaxDamageFactor = 2.0f;

/// One blow landing
struct Blow
{
	float attackerSize {1.0f};
	float defenderSize {1.0f};
	/// The strength that the body shows, from -1 (weak) to 1 (strong): see creature_morph::Morph::weakStrong
	float attackerStrength {0.0f};
	float defenderStrength {0.0f};
	/// The speed that the blow's action keeps (ActionSpeed)
	float speed {1.0f};
	/// A blow by the first special move (animations::k_AttackSpecial)
	bool special {false};
	/// How high the striking bone stands in the attacker's own body at the blow's moment, as a share of the height of
	/// the defender's head over the land; not kept within 0 and 1
	float heightShare {0.5f};
	bool blocked {false};
};
/// How hard a blow lands: the attacker's strength and size against the defender's, times its speed, kept from 0.04
/// to 2 (a value that is not a number counts as 0.04), and two times that for the first special move. Each strength
/// adds 3 before the division.
[[nodiscard]] float DamageFactor(const Blow& blow);

enum class RecoilDirection : uint8_t
{
	Plain,
	Right,
	Left,
	Top,
	Bottom,
};
struct Hit
{
	Band band;
	float damage;
	/// The reeling animation played by a body that isn't blocking, and its plain version for a species without the
	/// directed one
	size_t recoil;
	size_t plainRecoil;
};
/// The reel's band and the health a blow takes follow its height share: above 0.66 high, below 0.33 low, else middle
[[nodiscard]] Hit ResolveBlow(const Blow& blow, RecoilDirection direction);
/// Which way a blow throws the victim, by the way the attacker's striking bone moves into it: its move over the last
/// moments before the blow's moment, in the attacker's own body, unmirrored (x across, y up, z back to front as -z).
/// A bone moving ahead faster than across and up together throws the body straight back. Otherwise a bone moving
/// along x at least as fast as along y plays the left version, against x the right one, up the bottom one and down
/// the top one.
[[nodiscard]] RecoilDirection RecoilDirectionOf(glm::vec3 boneMove);
/// Which step an animation is, if it is one
[[nodiscard]] std::optional<Step> StepOf(size_t animation);
/// The kind of wound a blow leaves: half the time the first kind, else by the cut type the species' file gives the
/// blow: 0 the second kind, 1 one of the fourth to sixth, 2 the third, any other the first
[[nodiscard]] uint8_t WoundKind(int32_t cutType, const Random& random);
/// Wounds of these kinds also bleed
[[nodiscard]] bool Bleeds(uint8_t woundKind);

// Playing the fight

/// The fight animations play at 0.9 of this speed for steps and at 0.6 of it for all other animations. The speed of
/// an action does not change how fast its animation plays.
constexpr float k_FightSpeed = 1.1f;
constexpr float k_StepSpeedShare = 0.9f;
constexpr float k_OtherSpeedShare = 0.6f;
[[nodiscard]] float PlaybackSpeed(size_t animation);
/// A fighter's animation clock some milliseconds on, at the animation's speed
[[nodiscard]] float AdvanceClock(float timeMs, float milliseconds, size_t animation);
/// Whether the clock passed a blow's moment going from one time to the next: from at or before it to after it
[[nodiscard]] bool CrossesHit(float beforeMs, float afterMs, float hitMs);
/// The animation time a frame draws, a share of the way through the turn from where the turn started to where it ends
[[nodiscard]] float DrawnTime(float fromMs, float toMs, float turnFraction);

enum class State : uint8_t
{
	/// Not fighting
	Idle,
	Start,
	Stance,
	/// A blow, a step or reeling from a blow
	Action,
	Finish,
	BlockStart,
	Block,
	BlockRecoil,
	BlockEnd,
	Faint,
	GetUp,
	CastStart,
	Cast,
	CastEnd,
	/// Lying where it fainted
	Lying,
};
/// Where a fighter looks: in its stance at its opponent's head, in any other move of the fight ahead (none)
[[nodiscard]] std::optional<glm::vec3> LookAt(State state, glm::vec3 opponentHead);
/// Where a creature looks on its walk to its place in the arena: the creature that asked for the fight (the one that
/// made the arena) at the top of its opponent, the height of the land where the opponent stands plus the opponent's
/// height. The other walks there as it was told, with no look of its own (none)
[[nodiscard]] std::optional<glm::vec3> ApproachLook(bool madeArena, glm::vec2 opponent, float groundThere,
                                                    float opponentHeight);
[[nodiscard]] std::string_view Name(State state);
/// Whether a script's forced finish hurries a fighter in this state: its fight's start, stance, move or finish, not its
/// block, cast, faint or getting up. Hurried until it stands, the span of its turn (TestSpanMs) is doubled: its start
/// moves on by twice that span, its finish tests its end by it and its blows test their hit through it. Its moves at
/// the fight speed keep their pace.
[[nodiscard]] bool HurriedByForcedFinish(State state);
inline constexpr float k_HurriedPace = 2.0f;
/// The animation a state plays, other than an action's own
[[nodiscard]] size_t AnimationOf(State state);
/// Whether a state's animation plays round and round until something else happens, or holds its last frame
[[nodiscard]] bool Loops(State state);
/// What comes after a state's animation has played through
[[nodiscard]] State AfterAnimation(State state, bool hasOpponent);
//// Whether a state tests its own end as a turn begins, before its animation moves on: the start, the finish, the start
/// and the end of a block, and the start, the hold and the end of a cast. When such a state ends, or a fighter called
/// off leaves its stance, its block or its cast, the next state moves on in the same turn.
[[nodiscard]] bool EndsByOwnTest(State state);
/// Whether a state with an animation ends as a turn begins, before it moves on, once its time and its advance in the
/// turn reach its end, with no test of a fighter called off: a blow, a step or a reel (an action) and a block's reel.
/// The stance or the block it goes back to moves on in the same turn.
[[nodiscard]] bool EndsAsItsTurnBegins(State state);
/// Whether a state's animation moves on in whole milliseconds, going round once at most: every state of the duel but
/// the faint, the getting up and the lying
[[nodiscard]] bool MovesOnInWholeMs(State state);
/// The time a fighter's stance moves on from: while it plays the power-up, a time past the power-up's end (left by the
/// stance's own loop) starts the power-up again from 0; else the time it has
[[nodiscard]] int32_t StanceLoopFrom(bool poweringUp, int32_t timeMs, int32_t durationMs);
/// Whether a fighter looks at its queue as soon as a state ends into its stance, before its turn moves anything on, so
/// that a queued move it takes plays from that same turn: at the end of a blow, a step or a reel, of a block or of a
/// cast, unless the computer fights for it
[[nodiscard]] bool ChecksQueueAsItEnds(State ending, State next, bool computer);
/// How far a fight animation moves on in a turn at the fight speed (PlaybackSpeed), in whole milliseconds
[[nodiscard]] int32_t TurnAdvanceMs(float milliseconds, size_t animation);
/// How far the animation of a state that tests its own end moves on in a turn: the start by the span of the turn (see
/// TestSpanMs), every other state by its advance at its speed
[[nodiscard]] int32_t StateAdvanceMs(State state, int32_t advanceMs, int32_t spanMs);
/// The time a state adds to its animation's time when it tests its end: the start and the finish add the span of the
/// turn, the others their advance in the turn
[[nodiscard]] int32_t EndTestMs(State state, int32_t advanceMs, int32_t spanMs);
/// Whether an animation that tests its own end ends in this turn: its time and the test time reach its duration, in
/// whole milliseconds. One with no animation ends at once
[[nodiscard]] bool OwnTestEnds(float animationMs, int32_t testMs, float durationMs);
/// A spell cast in a fight is held until the first update after more than this many milliseconds
inline constexpr float k_FightCastHoldMs = 2.0f;
/// What a spell cast in a fight is cast at: an aggressive one at the opponent, else a defensive one at the caster, and
/// nothing without an opponent
enum class FightSpellAt : uint8_t
{
	Nothing,
	Opponent,
	Itself,
};
[[nodiscard]] FightSpellAt FightSpellTarget(bool aggressive, bool defensive, bool hasOpponent);
/// Whether the creature is blocking: while blocking, once half way into starting to block, and until half way out of
/// it again. Both halves are half of the duration of the start of the block, cut down to a whole millisecond.
[[nodiscard]] bool IsBlocking(State state, float timeMs, float startBlockDurationMs);
/// Whether a creature in a state can be told to make a move
[[nodiscard]] bool TakesMoves(State state);
/// Whether a creature in a state turns at once to face its opponent on each game turn: in its start, its stance, its
/// block and its actions that are not blows (a step or a reel, which face once their carry is done). Starting or ending
/// a block, casting, finishing and every other state keep the heading.
[[nodiscard]] bool FacesOpponentEachTurn(State state, size_t animation);
/// The next state of a fighter whose fight was called off, if it moves on now: in its stance it finishes, blocking it
/// ends the block, casting it ends the cast; starting, starting a block or starting a cast (which is then never cast)
/// end the same way once their animation is done. A blow, a step, a reel and a block's reel have no such test: they play
/// out to the stance or the block, which then end. None: it carries on.
[[nodiscard]] std::optional<State> StopTransition(State state, bool animationDone);

/// Who chooses the creature's moves
enum class Control : uint8_t
{
	Player,
	Computer,
	/// Nobody, as a script leaves it: it makes only the moves queued for it, and the computer never takes over
	None,
};
/// The computer leaves a player's creature alone for 3 seconds as a fight starts. After each move that the player gives,
/// and each time the creature looks at a queued move in range in its stance or its block, the wait is 15 seconds again.
constexpr float k_ComputerWaitsAtStartMs = 3000.0f;
constexpr float k_ComputerWaitsMs = 15000.0f;

/// No action, as the game counts a fighter back in its stance or out of a fight
constexpr size_t k_NoActionAnimation = 0;

/// A creature's side of a fight
struct Fighter
{
	State state {State::Idle};
	/// The animation playing, how far through it is and the speed that the last action keeps (ActionSpeed)
	size_t animation {0};
	float timeMs {0.0f};
	float speed {1.0f};
	bool mirrored {false};
	/// In its start, it first turns on the spot to face its opponent; the start animation waits for the turn to end
	bool turning {false};
	/// A blow by the first special move, which reaches twice as far and lands twice as hard. The second special move does
	/// neither.
	bool special {false};
	/// The spell being cast, and the whole milliseconds the cast has been held
	std::optional<uint32_t> spell;
	uint32_t castMs {0};
	/// Its fight was called off: it finishes the move under way, then finishes and stands
	bool stopping {false};
	/// A script's forced finish came in its fight: its moves play at twice the pace until it stands again
	bool hurried {false};
	MoveQueue queue;
	float health {1.0f};
	float stamina {1.0f};
	Control control {Control::Computer};
	/// The computer always chooses, whatever the player does
	bool autoFight {false};
	float computerWaitMs {k_ComputerWaitsAtStartMs};
	/// The lean the computer chooses its moves by, all through the fight: the one the creature had learnt as it last
	/// started a fight itself (none before). What it learns goes to its fight record
	float tendency {0.0f};
	/// The animation the fighter counts as doing, as scripts ask it: none in its stance, the action it started last (a
	/// blow, a step or a reel) until it is back in it, and its block or its cast as a whole, or the reel in a block
	/// once it reeled there
	size_t actionAnimation {k_NoActionAnimation};
	/// What the states it entered since the knock last looked do to its body's knock: the last that starts or stops it
	/// (creature_knock::FightMark)
	std::optional<creature_knock::Mark> knockMark;
};
/// The animation a fighter counts as doing once it enters a state playing an animation, from what it counted before:
/// none in its stance or out of the fight; the animation of an action, of a reel in a block, of a faint or of getting
/// up; the block's own animation as a block starts and the cast's as a cast starts; else what it counted before
[[nodiscard]] size_t ActionAnimationOn(size_t before, State state, size_t animation);
/// Plays a state with its animation. An action keeps the speed that it asks for plus the base (ActionSpeed). The other
/// states keep the speed of the last action.
void Enter(Fighter& fighter, State state, std::optional<size_t> animation = std::nullopt, float askedSpeed = 0.0f);
/// Whether a fighter in its start still waits for its turn on the spot to its opponent: it does while the turn goes on.
/// The turn over, the start animation plays from its beginning on that same game turn, and it waits no more.
[[nodiscard]] bool WaitsForStartTurn(Fighter& fighter, bool stillTurning);
/// Where a fighter's move this turn takes it from where it stands: as far as the move carries it, wherever that is; in
/// a block's recoil only to a place strictly inside the range, else it stays where it is
[[nodiscard]] glm::vec2 ActionMoveTo(const Arena& arena, State state, glm::vec2 at, glm::vec2 by);
/// How far a move (a blow, a step or a reel) or a block's reel carries the creature in one turn, on the land: the root's
/// travel in its animation (the mesh's x and z) times a share, turned to the heading. A move's share is the turn at the
/// fight's pace, ((speed x turn) x scale) / duration; a block's reel's the turn's clip span, (scale x span) / duration.
/// Every step rounds to a float, the heading's sine and cosine too, and the carry is the same each turn of the move,
/// whatever share of the animation is left. None with no animation.
[[nodiscard]] glm::vec2 TurnCarry(State state, glm::vec2 travel, float speed, int32_t turnMs, int32_t spanMs, float scale,
                                  int32_t durationMs, float heading);
/// A cast in a fight held for one more update of so many milliseconds (counted whole): whether it is held long enough
/// to be let go
[[nodiscard]] bool HoldFightCast(Fighter& fighter, float milliseconds);
/// The player adds a move: the computer gives a creature it fought for back to the player (one nobody fights for stays
/// so) and waits 15 seconds. The creature learns from it only as it looks at it in its queue (LearnTendency)
bool PlayerMove(Fighter& fighter, const Move& move, bool replace);
/// A script adds a move at the back of the queue, as the player adds one
bool ScriptQueueMove(Fighter& fighter, const Move& move);
/// One turn of the wait before the computer takes over a player's creature. The wait goes down by the turn's
/// milliseconds in its stance, its moves (its blows, steps and reels), its block and its reels in the block. It ends
/// only in the stance or the block, when it is below 0. Returns true when the computer takes over in this turn.
bool CountComputerWait(Fighter& fighter, float turnMs);

/// What to do with the move at the front of the queue
struct Order
{
	enum class Kind : uint8_t
	{
		EndBlock,
		Animation,
		Cast,
		Block,
		/// A blow at a band, or the special move
		Blow,
		Special,
	};
	Kind kind {Kind::Blow};
	Band band {Band::Mid};
	uint32_t value {0};
	float speed {1.0f};
};
/// The move at the front of the queue taken off it, if one can be made now: only in range of the opponent, in the
/// stance or blocking (when anything ends the block), and a blow only once its charge is known. In range, in the stance
/// or the block, a queued move makes the wait for the computer 15 seconds again, also when the move stays in the queue.
[[nodiscard]] std::optional<Order> TakeOrder(Fighter& fighter, bool inRange);

// The computer's choices

/// What the opponent is doing, as the computer sees it
enum class Opponent : uint8_t
{
	Stance,
	EndingCast,
	BlockRecoil,
	Attacking,
	SteppingForward,
	OtherAction,
	Blocking,
	Other,
};
[[nodiscard]] Opponent OpponentOf(State state, size_t animation);
/// How aggressive the computer is, 0 to 2, by the tendency. The game meant 1 for the middle, but only ever gives 0 below
/// 0.33 and 2 from it, which is kept.
[[nodiscard]] uint32_t TierOf(float tendency);
struct Choice
{
	enum class Kind : uint8_t
	{
		None,
		Blow,
		Block,
		Step,
	};
	Kind kind {Kind::None};
	Band band {Band::Mid};
	Step step {Step::Back};
};
/// The computer's move this game turn, given its tier and what the opponent is doing
[[nodiscard]] Choice ChooseMove(uint32_t tier, Opponent opponent, const Random& random);
/// While blocking, the computer stops one turn in eight
[[nodiscard]] bool ComputerEndsBlock(const Random& random);

// Choosing the blow

/// One of the creature's blows as it lands: how far ahead of the creature it reaches and how high, in the world, and
/// the point it lands at in the body's own units, unscaled, ahead being -z (its root carried along to the blow's moment)
struct Reach
{
	size_t animation;
	float reach;
	float height;
	glm::vec3 point {0.0f};
};
/// The blow to strike, or the step to take first
struct AttackChoice
{
	std::optional<size_t> animation;
	std::optional<Step> step;
};
/// Steps are tried this many each way
constexpr int k_MaxSteps = 10;
/// A blow lands well when it reaches into the opponent by less than 7.5 units of the attacker's size
constexpr float k_DepthPerSize = 7.5f;
/// A blow as the creature thinks of it before it chooses: how high it lands in the world, and how far the opponent's
/// body is ahead of where it lands, along the line to the opponent. None when no line from the blow meets the body.
struct BlowDepth
{
	size_t animation;
	float height;
	std::optional<float> depth;
};
/// Before a creature chooses a blow, it finds how far the blow reaches into the opponent. Lines go from the point where
/// the blow lands, towards the opponent, in a row across, flat on the land. The row has eleven lines, from 2.25
/// opponent sizes to one side (15 x 0.15) to as far to the other side, each 0.1 of that width from the last.
constexpr int k_DepthLines = 11;
constexpr float k_DepthWidthPerSize = 15.0f;
constexpr double k_DepthWidthShare = 0.15;
constexpr float k_DepthSpacingShare = 0.1f;
/// The depth before a line meets the body: the most a blow can be from the body
constexpr float k_NoDepth = 1000.0f;
/// Half the width of the row of lines, for an opponent of this size
[[nodiscard]] float DepthRowHalfWidth(float opponentSize);
/// How far ahead of the point the body starts, along the direction to the opponent (a unit vector), as the nearest of
/// the row's lines that meet the bones of the body. A negative value is a point inside the body. None when no line
/// meets it.
[[nodiscard]] std::optional<float> DepthToBody(std::span<const physics::Ellipsoid> bones, const glm::vec3& point,
                                               const glm::vec3& towards, float opponentSize);
/// How far one step forward and one step back carry the creature
struct StepLengths
{
	float forward;
	float back;
};
/// The lowest and the highest that a blow can land to be in a band, as heights over the land
[[nodiscard]] std::pair<float, float> BandLimits(Band band, float headHeight);
/// The blow to strike, or the step to take first. The first special blow and blows higher than the opponent's head
/// are not used (the second special blow is chosen as any other). A blow lands after a number of steps when the opponent's body
/// starts behind where it lands, but by less than 7.5 units of the attacker's size. The creature tries the steps from ten back
/// to ten forward. For each, it tries the blows in turn:
/// - a blow in the band asked for scores its number of steps, any other blow 999,
/// - a lower score wins and takes its number of steps,
/// - on the same score, the blow that goes in less wins but keeps the number of steps that it found first.
/// If the best needs steps, the creature steps once instead. When no blow lands, the creature does nothing.
[[nodiscard]] AttackChoice ChooseAttack(std::span<const BlowDepth> blows, Band band, float headHeight, StepLengths steps,
                                        float attackerSize);

// The fight's end

/// The winner of a knock out shows off, unless evil, when it has a poo on the loser
[[nodiscard]] bool WinnerPoos(float alignment);
/// The priority of the plan a knock out gives the winner, as every priority of a plan it chooses itself is counted
constexpr float k_WinPlanPriority = 1.0f;
/// Whether the winner carries out its plan for the loser as an order, choosing nothing for itself until it is done: when
/// a miracle or a loss of life knocked the loser out. After a knock out by a blow it is not, so a plan of its own more
/// than twice as pressing can take over.
[[nodiscard]] bool WinPlanCommanded(bool byBlow);
/// The action a creature plays to its opponent before a fight: the rude gesture, or the taunt. Each creature's is drawn
/// from two, the other creature's first: any draw but 0 is the rude gesture
[[nodiscard]] size_t FaceOffAnimation(bool rude);
/// Whether each of the two creatures of a fight makes the rude gesture before it, rather than the taunt
struct Taunts
{
	/// The creature asked to fight
	bool otherRude {false};
	/// The creature that starts the fight
	bool ownRude {false};
};
/// The two draws of the taunts, the other creature's first, then the one of the creature that starts the fight. A fight
/// its creature's mind plans draws them as its agenda is made; any other fight as it starts
[[nodiscard]] Taunts DrawTaunts(const Random& random);
/// The steps of the face-off, in the game turns of the creature that starts the fight:
/// 1. On the first turn, it asks the other creature.
/// 2. On the second turn, it tells the mind of the other creature to stand still.
/// 3. On the third turn, it tells the other creature to turn to face it.
/// 4. From the fourth turn, it turns to face the other creature.
constexpr uint32_t k_FaceOffRequestTurn = 1;
constexpr uint32_t k_FaceOffStillTurn = 2;
constexpr uint32_t k_FaceOffOrderTurn = 3;
constexpr uint32_t k_FaceOffOwnTurn = 4;
/// The steps of the turn to face in the arena, in the game turns of the creature that started the fight after the turn
/// on which the other creature is ready. On the first turn, it tells the other creature to turn to face it. From the
/// second turn, it turns to face the other creature.
constexpr uint32_t k_SquareOrderTurn = 1;
constexpr uint32_t k_SquareOwnTurn = 2;
/// Each creature holds its turn to face the other for this long
constexpr float k_FaceOffHoldSeconds = 0.1f;
/// The creature that starts the fight points at the arena for this many game turns
constexpr uint32_t k_PointAtArenaTurns = 20;
/// After a fight that ended otherwise, the creature with more life than the other is happy, by the same test as
/// FightPerformed; at an equal life it is sad
[[nodiscard]] size_t Response(float life, float otherLife);
/// Whether the creature that set out to fight, as its plan to fight, counts the fight as one it performed: only once it
/// is found with more life than its opponent after it, or either life is no number; otherwise it goes off in fear, and
/// its plan to fight fails
[[nodiscard]] bool FightPerformed(float life, float opponentLife);

// Starting fights

/// An angry creature this angry picks a fight. The game weighs fighting against everything else through its planner;
/// this stands in for that.
constexpr float k_AngerToFight = 0.6f;
/// It picks on another creature within this many units of its size
constexpr float k_PickFightPerSize = 60.0f;
/// It waits this long after a fight before picking another
constexpr float k_SecondsBetweenFights = 120.0f;
[[nodiscard]] bool WantsToFight(float anger, float life, float distance, float size, float secondsSinceFight);

/// What the player's camera does as a fight starts
enum class FightView : uint8_t
{
	/// It goes to watch the fight
	Watch,
	/// It stays where it is, and the spirits remark on the player's creature fighting
	Remark,
	/// Nothing: the fight is neither the player's nor in view
	None,
};
/// The camera watches a fight as it starts if one of the fighters is the local player's and either is controlled by a
/// script, or if it is already looking at the arena. Otherwise a fight of the local player's creature only has the
/// spirits remark on it.
[[nodiscard]] FightView ViewAtFightStart(bool localFighter, bool scriptControlled, bool lookingAtArena);
/// Whether the player's camera is told a fight is over as it ends for a fighter in this state: while it starts, stands,
/// strikes, steps or reels, blocks (but not while recoiling in its block) or casts
[[nodiscard]] bool CameraLingersOnEnd(State state);

/// The event of the help record that the move at the front of the queue gives, each time the player's creature looks at
/// it from its stance with its opponent in range: a block, an attack (a blow, also while it waits to be let go, or the
/// special move), a miracle, or a step or another animation
[[nodiscard]] uint32_t HelpEventOf(Move::Kind kind);
/// The player's creature counts its moves in the help record only from its stance, with a move in its queue and its
/// opponent in range. A creature that blocks ends its block with the next move, and that counts nothing.
[[nodiscard]] bool CountsHelpEvent(State state, bool inRange, bool queued);
/// Each game turn a blow is tested at thirty times through a span of the animation from where it was at the turn
/// before. The span is the turn's time scaled down for a bigger creature, (1.6 - 0.425 size) of it, in whole
/// milliseconds. When a script set real time, the span is the turn's time.
constexpr int32_t k_BlowTests = 30;
[[nodiscard]] int32_t TestSpanMs(float turnMs, float size, bool realTime);
/// Only the tests within 0.6 of that span of the blow's land time, either way, count, in whole milliseconds
[[nodiscard]] int32_t TestWindowMs(int32_t spanMs);
/// The animation times a blow is tested at in a turn, from the time at the turn before, that fall within the window
[[nodiscard]] std::vector<int32_t> BlowTestTimes(int32_t fromMs, int32_t spanMs, int32_t landMs);
/// At each test, five lines go from the attacker's groin. The first goes to the striking bone moved along its swing
/// since the last test by the blow's reach, the others to points that far to either side of the bone and above and
/// below it, across the swing. A swing of no length moves none of them.
[[nodiscard]] std::array<glm::vec3, 5> BlowTestPoints(const glm::vec3& bone, const glm::vec3& boneBefore, float reach);
/// The blow's reach about the bone: a fifth of fifteen times the attacker's size, twice that for the special move
[[nodiscard]] float BlowTestReach(float attackerSize, bool special);
/// At each test a faint white puff marks the striking bone, still, twice the attacker's size across, four times for the
/// special move. It lives as dust does.
constexpr uint32_t k_SwingPuffColour = 0x30FFFFFFu;
[[nodiscard]] float SwingPuffSize(float attackerSize, bool special);
/// What flies from where a blow lands: blood for a victim not blocking, then sparks. Blood is a drop (the falling kind
/// of puff), sparks a dust puff
struct BlowSpray
{
	glm::vec3 velocity {0.0f};
	float size {0.0f};
	uint32_t argb {0};
	bool drop {false};
};
/// The blood's three colours, one drawn for each drop
inline constexpr std::array<uint32_t, 3> k_BloodColours {0xFF960014u, 0xFF781414u, 0xFF502800u};
inline constexpr uint32_t k_BlowSparkColour = 0x30F0C070u;
inline constexpr int k_BloodDrops = 40;
inline constexpr int k_BlowSparks = 8;
/// Drawn on the local stream: the blow's line (the striking bone's move times 150) gives each its way, scattered by a
/// hundredth of its length (a twentieth of that again, and a quarter of the way, for the sparks), each scatter a draw of
/// 51 less 25, z then y then x; each drop of blood then draws its colour. The blood is a tenth of the victim's size,
/// the sparks seven times the attacker's
using LocalRandom = std::function<int32_t(int32_t)>;
[[nodiscard]] std::vector<BlowSpray> BlowSprays(glm::vec3 boneMove, float attackerSize, float victimSize, bool victimBlocking,
                                                const LocalRandom& random);
/// Whether a point where a blow line meets a bone lies ahead of the start of the line and nearer than its end
[[nodiscard]] bool WithinBlowLine(const glm::vec3& point, const glm::vec3& origin, const glm::vec3& direction);
/// One moment a blow is tested at: where the striking bone and the groin are in the world then, none where the
/// animation gives no bone
struct BlowSample
{
	std::optional<glm::vec3> strikes;
	std::optional<glm::vec3> groin;
};
/// Where a blow lands over a turn's moments, from where the striking bone was at the turn before: at each moment the
/// five lines from the groin (BlowTestPoints) are tested against the opponent's bones, while it is open to blows and
/// until one moment lands, and the last line that meets a bone between its ends gives the point. A moment without the
/// striking bone is skipped; one without the groin tests nothing but moves the swing on.
struct BlowLanding
{
	/// Where the line met the bone
	glm::vec3 point {0.0f};
	/// How far the striking bone moved to the moment it landed, from the moment before: the way the blow cuts
	glm::vec3 move {0.0f};

	bool operator==(const BlowLanding&) const = default;
};
[[nodiscard]] std::optional<BlowLanding> BlowLandsAt(glm::vec3 boneBefore, std::span<const BlowSample> samples,
                                                     std::span<const physics::Ellipsoid> body, float reach, bool open);
} // namespace openblack::creature_fight
