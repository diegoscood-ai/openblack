/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFight.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <vector>

#include <glm/geometric.hpp>

#include "3D/MapCoords.h"
#include "Common/X87Precision.h"
#include "Help/HelpProfile.h"

using namespace openblack;
using namespace openblack::creature_fight;

namespace
{
/// The computer attacks a waiting opponent one turn in this many, by its tier
constexpr std::array<uint32_t, 3> k_AttackChances {16, 8, 1};
/// A computer of the top tier counter-attacks all but one time in this many
constexpr uint32_t k_CounterLots = 5;
/// The computer stops blocking one turn in this many
constexpr uint32_t k_EndBlockLots = 8;
/// A blow at a band asked for scores its steps; any other this
constexpr int k_OutOfBandScore = 999;
/// The tendency clamps here, and each move nudges it by this share
constexpr float k_TendencyKeep = 0.98f;
constexpr float k_TendencyNudge = 0.02f;
/// A winner more evil than this has a poo on the loser
constexpr float k_AlignmentPooBelow = -0.5f;
/// Knocked out, it lies this long and this much longer for each of its size
constexpr float k_FaintBaseSeconds = 8.0f;
constexpr float k_FaintSecondsPerSize = 4.0f;
/// The special move plays at this speed
constexpr float k_SpecialSpeed = 1.5f;

constexpr size_t RecoilOffset(RecoilDirection direction)
{
	return static_cast<size_t>(direction);
}
} // namespace

bool creature_fight::IsAttack(size_t animation)
{
	return animation >= animations::k_FirstAttack && animation < animations::k_FirstAttack + animations::k_AttackCount;
}

bool creature_fight::IsSpecialAttack(size_t animation)
{
	return animation == animations::k_AttackSpecial || animation == animations::k_AttackSpecial2;
}

bool creature_fight::IsStep(size_t animation)
{
	return animation >= animations::k_StepForward && animation <= animations::k_StepLeft;
}

float creature_fight::ArenaRadius(float sizeA, float sizeB)
{
	const float unitsA = sizeA * k_ArenaUnitsPerSize;
	const float unitsB = sizeB * k_ArenaUnitsPerSize;
	// the first's, unless the second's are as many or more (the first's when either is not a number)
	const float radius = (!(unitsB >= unitsA) ? unitsA : unitsB) * k_ArenaRadiusFactor;
	if (!(radius >= 0.0f))
	{
		return 0.0f;
	}
	return radius > k_MaxArenaRadius ? k_MaxArenaRadius : radius;
}

Arena creature_fight::MakeArena(glm::vec2 a, glm::vec2 b, float sizeA, float sizeB)
{
	return {.centre = (a + b) * 0.5f, .radius = ArenaRadius(sizeA, sizeB)};
}

glm::vec2 creature_fight::ArenaSoundPoint(const Arena& arena)
{
	return {map_coords::Quantise(arena.centre.x), map_coords::Quantise(arena.centre.y)};
}

glm::vec2 creature_fight::ArenaSpot(const Arena& arena, float size, bool madeIt)
{
	const auto offset = k_SpotPerSize * size;
	return arena.centre + glm::vec2(madeIt ? offset : -offset, 0.0f);
}

float creature_fight::ArrivalDistance(float size)
{
	return k_ArrivalPerSize * size;
}

bool creature_fight::WithinRange(const Arena& arena, glm::vec2 point)
{
	return glm::distance(point, arena.centre) <= k_RangeShare * arena.radius;
}

bool creature_fight::StrictlyWithinRange(const Arena& arena, glm::vec2 point)
{
	return glm::distance(point, arena.centre) < k_RangeShare * arena.radius;
}

glm::vec2 creature_fight::ActionMoveTo(const Arena& arena, State state, glm::vec2 at, glm::vec2 by)
{
	const auto to = at + by;
	return state == State::BlockRecoil && !StrictlyWithinRange(arena, to) ? at : to;
}

Band creature_fight::BandOf(float shareOfHeight)
{
	if (shareOfHeight > k_HighAbove)
	{
		return Band::High;
	}
	return shareOfHeight < k_LowBelow ? Band::Low : Band::Mid;
}

std::optional<glm::vec3> creature_fight::LookAt(State state, glm::vec3 opponentHead)
{
	if (state == State::Stance)
	{
		return opponentHead;
	}
	return std::nullopt;
}

size_t creature_fight::StepAnimation(Step step)
{
	switch (step)
	{
	case Step::Forward:
		return animations::k_StepForward;
	case Step::Back:
		return animations::k_StepBack;
	case Step::Right:
		return animations::k_StepRight;
	case Step::Left:
		return animations::k_StepLeft;
	}
	return animations::k_StepForward;
}

Step creature_fight::GroundStep(glm::vec2 point, glm::vec2 at, float heading)
{
	namespace x87 = game_math::x87;
	// The way to the point across, kept on the floating point unit, and along, stored as a float
	const double across = x87::Sub(point.x, at.x);
	const double along = x87::Store(x87::Sub(point.y, at.y));
	// The turn into the creature's frame by its heading: the cosine stored as a float, the sine both stored and, for
	// the first axis, kept unrounded
	const auto sine = x87::Sin(heading);
	const double cosine = x87::Store(x87::Cos(heading));
	const double storedSine = x87::Store(sine);
	const float side = x87::Store(x87::Add(x87::Add(x87::Mul(cosine, across), x87::Mul(sine, -along)), 0.0));
	const float ahead = x87::Store(x87::Add(x87::Add(x87::Mul(storedSine, x87::Store(across)), x87::Mul(cosine, along)), 0.0));
	if (-ahead > std::abs(side))
	{
		return Step::Forward;
	}
	if (ahead > std::abs(side))
	{
		return Step::Back;
	}
	if (-side > std::abs(ahead))
	{
		return Step::Right;
	}
	return Step::Left;
}

bool creature_fight::CanStep(const Arena& arena, glm::vec2 position, Step step)
{
	return step == Step::Forward || WithinRange(arena, position);
}

Move creature_fight::AttackMove(Band band)
{
	switch (band)
	{
	case Band::High:
		return {.kind = Move::Kind::High};
	case Band::Mid:
		return {.kind = Move::Kind::Mid};
	case Band::Low:
		return {.kind = Move::Kind::Low};
	}
	return {.kind = Move::Kind::Mid};
}

Move creature_fight::StepMove(Step step)
{
	return {.kind = Move::Kind::Animation, .value = static_cast<uint32_t>(StepAnimation(step))};
}

Move creature_fight::BlockMove()
{
	return {.kind = Move::Kind::Block};
}

bool creature_fight::IsBlow(Move::Kind kind)
{
	return kind == Move::Kind::High || kind == Move::Kind::Mid || kind == Move::Kind::Low;
}

std::optional<Move> creature_fight::ScriptMove(uint32_t value)
{
	constexpr uint32_t k_NumberMask = 0x3FFF;
	if ((value & k_ScriptAnimationBit) != 0)
	{
		return Move {.kind = Move::Kind::Animation, .value = value & k_NumberMask};
	}
	if ((value & k_ScriptSpellBit) != 0)
	{
		return Move {.kind = Move::Kind::Spell, .value = value & k_NumberMask};
	}
	switch (value)
	{
	case 0:
		return Move {.kind = Move::Kind::High};
	case 1:
		return Move {.kind = Move::Kind::Mid};
	case 2:
		return Move {.kind = Move::Kind::Low};
	case 3:
		return Move {.kind = Move::Kind::Block};
	case 4:
		return Move {.kind = Move::Kind::Special};
	default:
		return std::nullopt;
	}
}

FightAction creature_fight::FightActionOf(size_t animation)
{
	const auto within = [animation](size_t first, size_t last) { return animation >= first && animation <= last; };
	if (animation == animations::k_Faint)
	{
		return FightAction::Fainting;
	}
	if (within(animations::k_StartCast, animations::k_EndCast))
	{
		return FightAction::Casting;
	}
	if (within(animations::k_Start, animations::k_Finish))
	{
		return FightAction::Stance;
	}
	if (animation == animations::k_AttackSpecial || animation == animations::k_AttackSpecial2)
	{
		return FightAction::Special;
	}
	if (within(animations::k_FirstAttack, animations::k_FirstAttack + animations::k_AttackCount - 1))
	{
		return FightAction::Blow;
	}
	if (within(animations::k_StepForward, animations::k_StepLeft))
	{
		return FightAction::Step;
	}
	if (within(animations::k_StartBlock, animations::k_EndBlock))
	{
		return FightAction::Block;
	}
	// Each reel is followed by its right, left, top and bottom versions
	constexpr size_t k_ReelVersions = 5;
	if (within(animations::k_RecoilHigh, animations::k_RecoilHigh + k_ReelVersions - 1))
	{
		return FightAction::ReelingHigh;
	}
	if (within(animations::k_RecoilMid, animations::k_RecoilMid + k_ReelVersions - 1))
	{
		return FightAction::ReelingMiddle;
	}
	if (within(animations::k_RecoilLow, animations::k_RecoilLow + k_ReelVersions - 1))
	{
		return FightAction::ReelingLow;
	}
	return FightAction::Other;
}

std::optional<float> creature_fight::InitialCharge(Move::Kind kind)
{
	switch (kind)
	{
	case Move::Kind::High:
	case Move::Kind::Mid:
	case Move::Kind::Low:
		return std::nullopt;
	case Move::Kind::Block:
	case Move::Kind::Animation:
		return 0.0f;
	case Move::Kind::Special:
	case Move::Kind::Spell:
		return k_MaxChargeMs;
	}
	return 0.0f;
}

float creature_fight::ReleasedCharge(float heldMs)
{
	return std::clamp(heldMs, 0.0f, k_MaxChargeMs);
}

float creature_fight::BlowSpeed(float chargeMs)
{
	return 0.5f + (ReleasedCharge(chargeMs) / k_MaxChargeMs);
}

bool MoveQueue::Push(const Move& move, bool replace)
{
	if (replace)
	{
		Clear();
	}
	if (_count >= k_Capacity)
	{
		_awaitingRelease = false;
		return false;
	}
	_moves.at(_count) = {.move = move, .chargeMs = InitialCharge(move.kind)};
	++_count;
	_awaitingRelease = !_moves.at(_count - 1).chargeMs.has_value();
	return true;
}

std::optional<QueuedMove> MoveQueue::Front() const
{
	return _count > 0 ? std::optional(_moves.front()) : std::nullopt;
}

void MoveQueue::Pop()
{
	if (_count == 0)
	{
		return;
	}
	std::shift_left(_moves.begin(), _moves.begin() + static_cast<std::ptrdiff_t>(_count), 1);
	--_count;
}

void MoveQueue::Clear()
{
	_count = 0;
	_awaitingRelease = false;
}

bool MoveQueue::Release(float heldMs)
{
	if (!_awaitingRelease || _count == 0)
	{
		return false;
	}
	_awaitingRelease = false;
	_moves.at(_count - 1).chargeMs = ReleasedCharge(heldMs);
	return true;
}

bool MoveQueue::TakeWaitingBlow()
{
	if (!_awaitingRelease || _count == 0)
	{
		return false;
	}
	--_count;
	_awaitingRelease = false;
	return true;
}

uint32_t creature_fight::QueuedBlows(std::span<const QueuedMove> moves)
{
	return static_cast<uint32_t>(
	    std::ranges::count_if(moves, [](const QueuedMove& queued) { return IsBlow(queued.move.kind); }));
}

float creature_fight::LearnTendency(float tendency, Move::Kind kind)
{
	if (kind == Move::Kind::Spell)
	{
		return tendency;
	}
	const bool defends = kind == Move::Kind::Block || kind == Move::Kind::Animation;
	const auto nudge = defends ? -k_TendencyNudge : k_TendencyNudge;
	return std::clamp((k_TendencyKeep * tendency) + nudge, -1.0f, 1.0f);
}

bool creature_fight::LearnsFromMove(Move::Kind kind, bool localPlayersCreature)
{
	if (kind == Move::Kind::Spell)
	{
		return false;
	}
	return kind == Move::Kind::Animation || localPlayersCreature;
}

float creature_fight::FirstTendency(float alignment)
{
	return std::clamp(-alignment, -1.0f, 1.0f);
}

float creature_fight::TendencyAtStart(bool foughtBefore, float learnt, float alignment)
{
	return foughtBefore ? learnt : FirstTendency(alignment);
}

float creature_fight::FightHealthAtStart(float life)
{
	return (life + 1.0f) * 0.5f;
}

float creature_fight::StaminaAtStart(float energy, float exhaustion)
{
	return std::clamp(energy - exhaustion, 0.0f, 1.0f);
}

float creature_fight::RegainStamina(float stamina)
{
	return std::min(stamina + k_StaminaPerTurn, 1.0f);
}

float creature_fight::LifeAfterFight(float life, float fightHealth)
{
	return life - ((1.0f - fightHealth) * k_LifeLostShare);
}

float creature_fight::HealthAfterBlow(float health, float damage)
{
	return health - damage;
}

float creature_fight::BlowNicenessChange(float hitOnCreatureWeight)
{
	return k_BlowHit * hitOnCreatureWeight;
}

creature_fight::MiracleTaken creature_fight::TakeMiracle(float health, float heal, float harm)
{
	MiracleTaken taken {.health = health};
	if (heal > 0.0f)
	{
		taken.health = std::clamp(taken.health + heal, 0.0f, 1.0f);
	}
	if (harm > 0.0f)
	{
		taken.health -= harm;
		taken.knockedOut = taken.health <= 0.0f;
	}
	return taken;
}

float creature_fight::HealthAfterLifeLost(float health, float lost)
{
	return std::max(health - lost, 0.0f);
}

float creature_fight::HealthAfterLifeGained(float health, float gained)
{
	return std::min(health + gained, 1.0f);
}

bool creature_fight::HealthyEnoughToFight(float life)
{
	return life > k_MinLifeToFight;
}

bool creature_fight::AgreesToFight(float height, float askerHeight, bool dead, bool scripted, bool askerScripted)
{
	const float share = height / askerHeight;
	return share >= k_SmallestOpponentShare && share <= k_BiggestOpponentShare && !dead && scripted == askerScripted;
}

float creature_fight::FaintSeconds(float size)
{
	return (k_FaintSecondsPerSize * size) + k_FaintBaseSeconds;
}

bool creature_fight::NeedsRest(float life, float exhaustion)
{
	return !(exhaustion <= k_RestedExhaustion && life >= k_GetUpLife);
}

float creature_fight::DamageFactor(const Blow& blow)
{
	// The game multiplies in this order: the strengths, then the sizes, then the speed
	const auto strengths = (blow.attackerStrength + k_StrengthShift) / (blow.defenderStrength + k_StrengthShift);
	const auto factor =
	    std::clamp((strengths * (blow.attackerSize / blow.defenderSize)) * blow.speed, k_MinDamageFactor, k_MaxDamageFactor);
	return blow.special ? factor + factor : factor;
}

Hit creature_fight::ResolveBlow(const Blow& blow, RecoilDirection direction)
{
	const auto band = BandOf(blow.heightShare);
	const auto base = band == Band::High ? k_HighDamage : k_LowDamage;
	const auto plain = band == Band::High  ? animations::k_RecoilHigh
	                   : band == Band::Mid ? animations::k_RecoilMid
	                                       : animations::k_RecoilLow;
	auto damage = base * DamageFactor(blow);
	if (blow.blocked)
	{
		damage *= k_BlockedShare;
	}
	return {.band = band, .damage = damage, .recoil = plain + RecoilOffset(direction), .plainRecoil = plain};
}

RecoilDirection creature_fight::RecoilDirectionOf(glm::vec2 offset)
{
	const auto sideways = std::abs(offset.x);
	const auto upright = std::abs(offset.y);
	if (sideways < k_PlainRecoilWithin && upright < k_PlainRecoilWithin)
	{
		return RecoilDirection::Plain;
	}
	if (sideways >= upright)
	{
		return offset.x > 0.0f ? RecoilDirection::Right : RecoilDirection::Left;
	}
	return offset.y > 0.0f ? RecoilDirection::Top : RecoilDirection::Bottom;
}

std::optional<Step> creature_fight::StepOf(size_t animation)
{
	switch (animation)
	{
	case animations::k_StepForward:
		return Step::Forward;
	case animations::k_StepBack:
		return Step::Back;
	case animations::k_StepRight:
		return Step::Right;
	case animations::k_StepLeft:
		return Step::Left;
	default:
		return std::nullopt;
	}
}

size_t creature_fight::WobbleAnimation(Band band, bool sideways)
{
	if (band == Band::High)
	{
		return sideways ? animations::k_WobbleHighSide : animations::k_WobbleHighFrontBack;
	}
	return sideways ? animations::k_WobbleLowSide : animations::k_WobbleLowFrontBack;
}

uint8_t creature_fight::WoundKind(uint8_t blowWoundType, const Random& random)
{
	if (random(2) == 0)
	{
		return 0;
	}
	switch (blowWoundType)
	{
	case 0:
		return 1;
	case 1:
		return static_cast<uint8_t>(3 + random(3));
	default:
		return 2;
	}
}

uint8_t creature_fight::BlowWoundType(size_t animation)
{
	if (!IsAttack(animation))
	{
		return 0;
	}
	// The second blows follow the first in the same order
	const auto blow = animations::k_FirstAttack + ((animation - animations::k_FirstAttack) % 6);
	switch (blow)
	{
	case animations::k_AttackHigh:
	case animations::k_AttackEye:
		return 1;
	case animations::k_AttackPelvis:
	case animations::k_AttackKnee:
		return 2;
	default:
		return 0;
	}
}

bool creature_fight::Bleeds(uint8_t woundKind)
{
	return woundKind == 2 || woundKind == 3 || woundKind == 5;
}

float creature_fight::PlaybackSpeed(size_t animation, float actionSpeed)
{
	return k_FightSpeed * (IsStep(animation) ? k_StepSpeedShare : k_OtherSpeedShare) * actionSpeed;
}

float creature_fight::AdvanceClock(float timeMs, float milliseconds, float actionSpeed, size_t animation)
{
	return timeMs + (milliseconds * PlaybackSpeed(animation, actionSpeed));
}

bool creature_fight::CrossesHit(float beforeMs, float afterMs, float hitMs)
{
	return beforeMs <= hitMs && afterMs > hitMs;
}

float creature_fight::DrawnTime(float fromMs, float toMs, float turnFraction)
{
	return fromMs + ((toMs - fromMs) * turnFraction);
}

std::string_view creature_fight::Name(State state)
{
	switch (state)
	{
	case State::Idle:
		return "idle";
	case State::Start:
		return "starting";
	case State::Stance:
		return "stance";
	case State::Action:
		return "acting";
	case State::Finish:
		return "finishing";
	case State::BlockStart:
		return "starting to block";
	case State::Block:
		return "blocking";
	case State::BlockRecoil:
		return "reeling in a block";
	case State::BlockEnd:
		return "ending a block";
	case State::Faint:
		return "fainting";
	case State::GetUp:
		return "getting up";
	case State::CastStart:
		return "starting to cast";
	case State::Cast:
		return "casting";
	case State::CastEnd:
		return "ending a cast";
	case State::Lying:
		return "lying";
	}
	return {};
}

size_t creature_fight::AnimationOf(State state)
{
	switch (state)
	{
	case State::Start:
		return animations::k_Start;
	case State::Finish:
		return animations::k_Finish;
	case State::BlockStart:
		return animations::k_StartBlock;
	case State::Block:
		return animations::k_Block;
	case State::BlockRecoil:
		return animations::k_RecoilBlock;
	case State::BlockEnd:
		return animations::k_EndBlock;
	case State::Faint:
	case State::Lying:
		return animations::k_Faint;
	case State::GetUp:
		return animations::k_GetUp;
	case State::CastStart:
		return animations::k_StartCast;
	case State::Cast:
		return animations::k_Cast;
	case State::CastEnd:
		return animations::k_EndCast;
	case State::Idle:
	case State::Stance:
	case State::Action:
		break;
	}
	return animations::k_Stance;
}

bool creature_fight::Loops(State state)
{
	return state == State::Stance || state == State::Block || state == State::Lying;
}

State creature_fight::AfterAnimation(State state, bool hasOpponent)
{
	switch (state)
	{
	case State::Idle:
		return State::Idle;
	case State::Start:
	case State::Stance:
		return hasOpponent ? State::Stance : State::Finish;
	case State::Action:
	case State::BlockEnd:
	case State::CastEnd:
		return State::Stance;
	case State::Finish:
	case State::GetUp:
		return State::Idle;
	case State::BlockStart:
	case State::BlockRecoil:
		return State::Block;
	case State::Block:
		return hasOpponent ? State::Block : State::BlockEnd;
	case State::Faint:
	case State::Lying:
		return State::Lying;
	case State::CastStart:
		return State::Cast;
	case State::Cast:
		return State::CastEnd;
	}
	return State::Idle;
}

creature_fight::FightSpellAt creature_fight::FightSpellTarget(bool aggressive, bool defensive, bool hasOpponent)
{
	if (!hasOpponent)
	{
		return FightSpellAt::Nothing;
	}
	return aggressive ? FightSpellAt::Opponent : defensive ? FightSpellAt::Itself : FightSpellAt::Nothing;
}

bool creature_fight::HoldFightCast(Fighter& fighter, float milliseconds)
{
	fighter.castMs += static_cast<uint32_t>(milliseconds);
	return static_cast<float>(fighter.castMs) > k_FightCastHoldMs;
}

bool creature_fight::IsBlocking(State state, float timeMs, float startBlockDurationMs)
{
	const auto half = startBlockDurationMs * 0.5f;
	switch (state)
	{
	case State::Block:
		return true;
	case State::BlockStart:
		return timeMs > half;
	case State::BlockEnd:
		return timeMs < half;
	default:
		return false;
	}
}

bool creature_fight::TakesMoves(State state)
{
	return state == State::Stance || state == State::Block;
}

bool creature_fight::FacesOpponent(State state)
{
	return state == State::Start || state == State::Stance || state == State::Block || state == State::BlockStart ||
	       state == State::BlockEnd;
}

size_t creature_fight::ActionAnimationOn(size_t before, State state, size_t animation)
{
	switch (state)
	{
	case State::Idle:
	case State::Stance:
		return k_NoActionAnimation;
	case State::Action:
	case State::BlockRecoil:
	case State::Faint:
	case State::GetUp:
		return animation;
	case State::BlockStart:
		return animations::k_Block;
	case State::CastStart:
		return animations::k_Cast;
	case State::Start:
	case State::Finish:
	case State::Block:
	case State::BlockEnd:
	case State::Cast:
	case State::CastEnd:
	case State::Lying:
		break;
	}
	return before;
}

std::optional<State> creature_fight::StopTransition(State state, bool animationDone)
{
	switch (state)
	{
	case State::Stance:
		return State::Finish;
	case State::Block:
		return State::BlockEnd;
	case State::Cast:
		return State::CastEnd;
	case State::Start:
	case State::Action:
		return animationDone ? std::optional(State::Finish) : std::nullopt;
	case State::BlockStart:
	case State::BlockRecoil:
		return animationDone ? std::optional(State::BlockEnd) : std::nullopt;
	case State::CastStart:
		return animationDone ? std::optional(State::CastEnd) : std::nullopt;
	default:
		return std::nullopt;
	}
}

void creature_fight::Enter(Fighter& fighter, State state, std::optional<size_t> animation, float speed)
{
	fighter.state = state;
	fighter.animation = animation.value_or(AnimationOf(state));
	fighter.timeMs = 0.0f;
	fighter.speed = speed;
	fighter.landed = false;
	// Only the first special move doubles its blow
	fighter.special = state == State::Action && fighter.animation == animations::k_AttackSpecial;
	fighter.actionAnimation = ActionAnimationOn(fighter.actionAnimation, state, fighter.animation);
}

bool creature_fight::PlayerMove(Fighter& fighter, const Move& move, bool replace)
{
	if (fighter.control != Control::None)
	{
		fighter.control = Control::Player;
	}
	fighter.autoFight = false;
	fighter.computerWaitMs = k_ComputerWaitsMs;
	return fighter.queue.Push(move, replace);
}

bool creature_fight::ScriptQueueMove(Fighter& fighter, const Move& move)
{
	return PlayerMove(fighter, move, false);
}

bool creature_fight::CountComputerWait(Fighter& fighter, float turnMs)
{
	if (fighter.control != Control::Player)
	{
		return false;
	}
	const auto state = fighter.state;
	if (state != State::Stance && state != State::Action && state != State::Block && state != State::BlockRecoil)
	{
		return false;
	}
	fighter.computerWaitMs -= turnMs;
	if ((state == State::Stance || state == State::Block) && fighter.computerWaitMs < 0.0f)
	{
		fighter.control = Control::Computer;
		return true;
	}
	return false;
}

std::optional<Order> creature_fight::TakeOrder(Fighter& fighter, bool inRange)
{
	const auto front = fighter.queue.Front();
	if (!inRange || !front.has_value())
	{
		return std::nullopt;
	}
	// Anything at all ends a block, and is made from the stance after it
	if (fighter.state == State::Block)
	{
		fighter.computerWaitMs = k_ComputerWaitsMs;
		return Order {.kind = Order::Kind::EndBlock};
	}
	if (fighter.state != State::Stance)
	{
		return std::nullopt;
	}
	fighter.computerWaitMs = k_ComputerWaitsMs;
	const auto& move = front->move;
	std::optional<Order> order;
	switch (move.kind)
	{
	case Move::Kind::Animation:
		order = Order {.kind = Order::Kind::Animation, .value = move.value};
		break;
	case Move::Kind::Spell:
		order = Order {.kind = Order::Kind::Cast, .value = move.value};
		break;
	case Move::Kind::Block:
		order = Order {.kind = Order::Kind::Block};
		break;
	case Move::Kind::Special:
		order = Order {.kind = Order::Kind::Special, .speed = k_SpecialSpeed};
		break;
	case Move::Kind::High:
	case Move::Kind::Mid:
	case Move::Kind::Low:
		// A blow waits until it is let go
		if (!front->chargeMs.has_value())
		{
			return std::nullopt;
		}
		order = Order {.kind = Order::Kind::Blow,
		               .band = move.kind == Move::Kind::High  ? Band::High
		                       : move.kind == Move::Kind::Mid ? Band::Mid
		                                                      : Band::Low,
		               .speed = BlowSpeed(*front->chargeMs)};
		// A blow stays at the front until it is thrown: a step taken to close in, or no blow that lands, leaves it
		// there to be tried again from the next stance
		return order;
	}
	fighter.queue.Pop();
	return order;
}

Opponent creature_fight::OpponentOf(State state, size_t animation)
{
	switch (state)
	{
	case State::Stance:
		return Opponent::Stance;
	case State::CastEnd:
		return Opponent::EndingCast;
	case State::BlockRecoil:
		return Opponent::BlockRecoil;
	// The computer strikes at once at an opponent that starts a cast or casts
	case State::CastStart:
	case State::Cast:
		return Opponent::OtherAction;
	case State::Action:
		if (IsAttack(animation))
		{
			return Opponent::Attacking;
		}
		return animation == animations::k_StepForward ? Opponent::SteppingForward : Opponent::OtherAction;
	case State::BlockStart:
	case State::Block:
	case State::BlockEnd:
		return Opponent::Blocking;
	default:
		return Opponent::Other;
	}
}

uint32_t creature_fight::TierOf(float tendency)
{
	// The middle tier was meant for tendencies from -0.33, but the game never gives it
	return tendency >= 0.33f ? 2 : 0;
}

Choice creature_fight::ChooseMove(uint32_t tier, Opponent opponent, const Random& random)
{
	const auto blow = [&random] { return Choice {.kind = Choice::Kind::Blow, .band = static_cast<Band>(random(3))}; };
	const auto step = [&random] {
		constexpr std::array k_Steps {Step::Left, Step::Right, Step::Back};
		return Choice {.kind = Choice::Kind::Step, .step = k_Steps.at(random(3))};
	};
	const auto block = Choice {.kind = Choice::Kind::Block};
	// Only the most aggressive tier draws this lot, and only against the moves that it can strike back at
	const auto counters = [tier, &random] { return tier >= 2 && random(k_CounterLots) != 0; };
	switch (opponent)
	{
	case Opponent::Stance:
	case Opponent::EndingCast:
		return random(k_AttackChances.at(std::min<size_t>(tier, 2))) == 0 ? blow() : Choice {};
	case Opponent::BlockRecoil:
	case Opponent::OtherAction:
		return blow();
	case Opponent::Attacking:
		if (counters())
		{
			return blow();
		}
		return random(4) == 0 ? block : step();
	case Opponent::SteppingForward:
	{
		if (counters())
		{
			return blow();
		}
		const auto pick = random(4);
		if (pick == 0)
		{
			return block;
		}
		return pick == 3 ? blow() : step();
	}
	case Opponent::Blocking:
		if (counters())
		{
			return blow();
		}
		if (random(10) == 1)
		{
			return random(2) == 0 ? step() : blow();
		}
		return {};
	case Opponent::Other:
		break;
	}
	return {};
}

bool creature_fight::ComputerEndsBlock(const Random& random)
{
	return random(k_EndBlockLots) == 1;
}

AttackChoice creature_fight::ChooseAttack(std::span<const Reach> reaches, Band band, float gap, float stepLength,
                                          float attackerSize, float opponentHeight)
{
	const std::vector<std::optional<float>> gaps(reaches.size(), gap);
	return ChooseAttack(reaches, band, gaps, stepLength, attackerSize, opponentHeight);
}

AttackChoice creature_fight::ChooseAttack(std::span<const Reach> reaches, Band band, std::span<const std::optional<float>> gaps,
                                          float stepLength, float attackerSize, float opponentHeight)
{
	struct Best
	{
		size_t animation;
		int steps;
		int score;
		float depth;
	};
	std::optional<Best> best;
	const auto maxDepth = k_DepthPerSize * attackerSize;
	const int maxSteps = stepLength > 0.0f ? k_MaxSteps : 0;
	for (size_t index = 0; index < reaches.size(); ++index)
	{
		const auto& reach = reaches[index];
		if (IsSpecialAttack(reach.animation) || reach.height > opponentHeight || opponentHeight <= 0.0f ||
		    index >= gaps.size() || !gaps[index].has_value())
		{
			continue;
		}
		const float gap = *gaps[index];
		const bool inBand = BandOf(reach.height / opponentHeight) == band;
		// The fewest steps forward (more than 0) or back from which it lands
		std::optional<std::pair<int, float>> landing;
		for (int count = 0; count <= maxSteps && !landing.has_value(); ++count)
		{
			for (const int steps : {count, -count})
			{
				const auto depth = reach.reach - (gap - (static_cast<float>(steps) * stepLength));
				if (depth > 0.0f && depth < maxDepth)
				{
					landing = std::pair(steps, depth);
					break;
				}
			}
		}
		if (!landing.has_value())
		{
			continue;
		}
		const auto score = inBand ? std::abs(landing->first) : k_OutOfBandScore;
		if (!best.has_value() || score < best->score || (score == best->score && landing->second > best->depth))
		{
			best = Best {.animation = reach.animation, .steps = landing->first, .score = score, .depth = landing->second};
		}
	}
	if (!best.has_value())
	{
		// Nothing lands from up to ten steps either way: nothing is done
		return {};
	}
	if (best->steps != 0)
	{
		return {.step = best->steps > 0 ? Step::Forward : Step::Back};
	}
	return {.animation = best->animation};
}

bool creature_fight::WinnerPoos(float alignment)
{
	return alignment < k_AlignmentPooBelow;
}

size_t creature_fight::Response(float life, float otherLife)
{
	return life >= otherLife ? animations::k_Happy : animations::k_Sad;
}

bool creature_fight::WantsToFight(float anger, float life, float distance, float size, float secondsSinceFight)
{
	return anger >= k_AngerToFight && HealthyEnoughToFight(life) && distance <= k_PickFightPerSize * size &&
	       secondsSinceFight >= k_SecondsBetweenFights;
}

creature_fight::FightView creature_fight::ViewAtFightStart(bool localFighter, bool scriptControlled, bool lookingAtArena)
{
	if ((localFighter && scriptControlled) || lookingAtArena)
	{
		return FightView::Watch;
	}
	return localFighter ? FightView::Remark : FightView::None;
}

bool creature_fight::CameraLingersOnEnd(State state)
{
	switch (state)
	{
	case State::Start:
	case State::Stance:
	case State::Action:
	case State::BlockStart:
	case State::Block:
	case State::BlockEnd:
	case State::CastStart:
	case State::Cast:
	case State::CastEnd:
		return true;
	default:
		return false;
	}
}

uint32_t creature_fight::HelpEventOf(Move::Kind kind)
{
	switch (kind)
	{
	case Move::Kind::Block:
		return help::profile::k_FightBlock;
	case Move::Kind::Spell:
		return help::profile::k_FightCast;
	case Move::Kind::Animation:
		return help::profile::k_FightStep;
	case Move::Kind::High:
	case Move::Kind::Mid:
	case Move::Kind::Low:
	case Move::Kind::Special:
		break;
	}
	return help::profile::k_FightAttack;
}

bool creature_fight::CountsHelpEvent(State state, bool inRange, bool queued)
{
	return state == State::Stance && inRange && queued;
}

float creature_fight::ForwardExtent(std::span<const float> bonesAhead)
{
	float extent = 0.0f;
	for (const float ahead : bonesAhead)
	{
		extent = std::max(extent, ahead);
	}
	return extent;
}

std::optional<glm::vec2> creature_fight::PullApartOf(glm::vec2 fighter, glm::vec2 opponent, glm::vec2 ahead,
                                                     float fighterExtent, float opponentExtent)
{
	const float distance = glm::distance(fighter, opponent);
	const float extent = fighterExtent + opponentExtent;
	if (!(distance < extent))
	{
		return std::nullopt;
	}
	return ahead * ((extent - distance) * 0.5f);
}

std::optional<glm::vec2> creature_fight::BlowPushOf(glm::vec2 attacker, glm::vec2 opponent, glm::vec2 ahead,
                                                    float attackerExtent, float opponentExtent)
{
	const float distance = glm::distance(attacker, opponent);
	const float extent = attackerExtent + (opponentExtent * 0.5f);
	if (!(distance < extent))
	{
		return std::nullopt;
	}
	return ahead * ((extent - distance) * 0.5f);
}
