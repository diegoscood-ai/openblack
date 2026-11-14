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
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

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
/// The wobbles a blow sends through the body, high and low, side to side and front to back, played on top of it
constexpr size_t k_WobbleHighSide = 194;
constexpr size_t k_WobbleHighFrontBack = 195;
constexpr size_t k_WobbleLowSide = 196;
constexpr size_t k_WobbleLowFrontBack = 197;
/// Reeling back while blocking
constexpr size_t k_RecoilBlock = 198;
/// The actions played around a fight: the taunt before it, and being happy, sad or showing off after it
constexpr size_t k_Taunt = 70;
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
/// How fast a blow plays by its charge, 0.5 to 1.5
[[nodiscard]] float BlowSpeed(float chargeMs);
/// The fight's computer opponent hits at this speed
constexpr float k_AiBlowSpeed = 0.5f;

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
/// The lean a creature has learnt, as it starts a fight itself: against its alignment again until it has fought, else
/// what it learnt
[[nodiscard]] float TendencyAtStart(bool foughtBefore, float learnt, float alignment);

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
/// A creature asked to fight agrees only to one between half and twice its own height, not while it lies dead, and only
/// when both or neither of them are under a script's control
constexpr float k_SmallestOpponentShare = 0.5f;
constexpr float k_BiggestOpponentShare = 2.0f;
[[nodiscard]] bool AgreesToFight(float height, float askerHeight, bool dead, bool scripted, bool askerScripted);
/// Knocked out, a creature lies 8 seconds and 4 more for each of its size
[[nodiscard]] float FaintSeconds(float size);
/// Taken home, it fades out and in (creature_fizz::k_CarryHomeSeconds), then waits this long before resting
constexpr float k_WaitAfterHomeSeconds = 3.0f;
/// It rests until at least this healthy and no more exhausted than this, then gets up
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
	/// How fast the blow played, by its charge
	float speed {1.0f};
	/// A blow by the first special move (animations::k_AttackSpecial)
	bool special {false};
	/// Where it landed, as a share of the defender's height
	float heightShare {0.5f};
	bool blocked {false};
};
/// How hard a blow lands: the attacker's strength and size against the defender's, times its speed, kept from 0.04
/// to 2, and two times that for the first special move. Each strength adds 3 before the division.
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
[[nodiscard]] Hit ResolveBlow(const Blow& blow, RecoilDirection direction);
/// A blow this near the middle of the body, in shares of its half width and half height, throws it straight back
constexpr float k_PlainRecoilWithin = 0.25f;
/// Which way a blow throws the body, by where it lands from the middle of the victim's body: x to its right as a share
/// of its half width, y up as a share of its half height. Off to a side it reels right or left, high or low it reels
/// by its top or bottom version; which of those the game means isn't known, so high is taken as top.
[[nodiscard]] RecoilDirection RecoilDirectionOf(glm::vec2 offset);
/// The wobble played on top of the body after a blow: high or low, side to side or front to back
[[nodiscard]] size_t WobbleAnimation(Band band, bool sideways);
/// Which step an animation is, if it is one
[[nodiscard]] std::optional<Step> StepOf(size_t animation);
/// The kind of wound a blow leaves: half the time the first kind, else by the blow's own wound type (0, 1 or 2)
[[nodiscard]] uint8_t WoundKind(uint8_t blowWoundType, const Random& random);
/// The wound type each blow leaves; the game keeps one for each, which isn't found, so this guesses it by where the
/// blow lands: blows at the head cut, those to the body bruise, low blows graze
[[nodiscard]] uint8_t BlowWoundType(size_t animation);
/// Wounds of these kinds also bleed
[[nodiscard]] bool Bleeds(uint8_t woundKind);

// Playing the fight

/// The fight animations play at this speed, steps at 0.9 of it and everything else at 0.6, and an action's own speed
/// on top
constexpr float k_FightSpeed = 1.1f;
constexpr float k_StepSpeedShare = 0.9f;
constexpr float k_OtherSpeedShare = 0.6f;
[[nodiscard]] float PlaybackSpeed(size_t animation, float actionSpeed);
/// A fighter's animation clock some milliseconds on, at the animation's speed and the action's own
[[nodiscard]] float AdvanceClock(float timeMs, float milliseconds, float actionSpeed, size_t animation);
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
[[nodiscard]] std::string_view Name(State state);
/// Whether a script's forced finish hurries a fighter in this state: its fight's start, stance, move or finish, not its
/// block, cast, faint or getting up. Hurried, it plays at twice the pace until it stands
[[nodiscard]] bool HurriedByForcedFinish(State state);
inline constexpr float k_HurriedPace = 2.0f;
/// The animation a state plays, other than an action's own
[[nodiscard]] size_t AnimationOf(State state);
/// Whether a state's animation plays round and round until something else happens, or holds its last frame
[[nodiscard]] bool Loops(State state);
/// What comes after a state's animation has played through
[[nodiscard]] State AfterAnimation(State state, bool hasOpponent);
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
/// it again
[[nodiscard]] bool IsBlocking(State state, float timeMs, float startBlockDurationMs);
/// Whether a creature in a state can be told to make a move
[[nodiscard]] bool TakesMoves(State state);
/// Whether a creature in a state turns at once to face its opponent on each game turn: in its start, its stance, its
/// block and its actions that are not blows (a step or a reel, which face once their carry is done). Starting or ending
/// a block, casting, finishing and every other state keep the heading.
[[nodiscard]] bool FacesOpponentEachTurn(State state, size_t animation);
/// The next state of a fighter whose fight was called off, if it moves on now: in its stance it finishes, blocking it
/// ends the block, casting it ends the cast; starting, starting a block, reeling in one or starting a cast (which is then
/// never cast) end the same way once their animation is done, and a blow or step finishes once done. None: it carries on.
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
	/// The animation playing, how far through it is and the speed of the action
	size_t animation {0};
	float timeMs {0.0f};
	float speed {1.0f};
	bool mirrored {false};
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
};
/// The animation a fighter counts as doing once it enters a state playing an animation, from what it counted before:
/// none in its stance or out of the fight; the animation of an action, of a reel in a block, of a faint or of getting
/// up; the block's own animation as a block starts and the cast's as a cast starts; else what it counted before
[[nodiscard]] size_t ActionAnimationOn(size_t before, State state, size_t animation);
/// Plays a state, with an action's animation and speed
void Enter(Fighter& fighter, State state, std::optional<size_t> animation = std::nullopt, float speed = 1.0f);
/// Where a fighter's move this turn takes it from where it stands: as far as the move carries it, wherever that is; in
/// a block's recoil only to a place strictly inside the range, else it stays where it is
[[nodiscard]] glm::vec2 ActionMoveTo(const Arena& arena, State state, glm::vec2 at, glm::vec2 by);
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

/// One of the creature's blows as it lands: how far ahead of the creature it reaches and how high, in the world
struct Reach
{
	size_t animation;
	float reach;
	float height;
};
/// The blow to strike, or the step to take first
struct AttackChoice
{
	std::optional<size_t> animation;
	std::optional<Step> step;
};
/// Steps are tried this many each way
constexpr int k_MaxSteps = 10;
/// A blow lands well when it reaches this far into the opponent, at most, for each of the attacker's size
constexpr float k_DepthPerSize = 7.5f;
/// The blow at a band that lands from the fewest steps forward or back, of those that reach high enough. A blow lands
/// when it reaches into the opponent, but not by more than 7.5 units of the attacker's size; blows at the band asked for
/// beat all others, then the fewest steps, then the deepest. If the best needs steps, the creature steps once instead;
/// if none lands from up to ten steps either way, nothing is done.
/// gaps, one for each reach, are from the attacker to the opponent's body at that blow's height; a blow whose gap is
/// none does not meet the body at all and is not used.
[[nodiscard]] AttackChoice ChooseAttack(std::span<const Reach> reaches, Band band, std::span<const std::optional<float>> gaps,
                                        float stepLength, float attackerSize, float opponentHeight);
/// As above, with one gap for every blow
[[nodiscard]] AttackChoice ChooseAttack(std::span<const Reach> reaches, Band band, float gap, float stepLength,
                                        float attackerSize, float opponentHeight);

// The fight's end

/// The winner of a knock out shows off, unless evil, when it has a poo on the loser
[[nodiscard]] bool WinnerPoos(float alignment);
/// After a fight that ended otherwise, the creature with more life is happy and the other sad
[[nodiscard]] size_t Response(float life, float otherLife);

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
[[nodiscard]] std::optional<glm::vec3> BlowLandsAt(glm::vec3 boneBefore, std::span<const BlowSample> samples,
                                                   std::span<const physics::Ellipsoid> body, float reach, bool open);
} // namespace openblack::creature_fight
