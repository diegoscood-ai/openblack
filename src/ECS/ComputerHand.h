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
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/ComputerHand.h"
#include "ECS/Components/ComputerMind.h"
#include "ECS/ComputerPlayerMind.h"
#include "Enums.h"

/// The rules of a computer player's hand: its line of steps, the move that some steps need in front of them, and how the
/// first step goes through its stages, one stage a turn.
namespace openblack
{
namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;
} // namespace openblack

namespace openblack::computer_player
{
class ScoreWorld;
} // namespace openblack::computer_player

namespace openblack::computer_hand
{

/// What a stage of a step gives
enum class StageResult : uint8_t
{
	/// The stage is over: the next turn runs the next stage, and after the third stage the next step
	Done = 0,
	/// The step cannot be done: the hand gives up every step and lets go of what it holds
	Failed = 1,
	/// The stage goes on the next turn
	Going = 2,
};

/// The move that the step needs in front of it, or HandSubAction::None
[[nodiscard]] ecs::components::HandSubAction Prerequisite(ecs::components::HandSubAction type);

/// The hand has no step left, so a script waiting for it goes on
[[nodiscard]] bool IsReady(const ecs::components::ComputerHand& hand);

/// Where the player's interface reports its hand in the turn: where the hand was as the turn began when the turn gave it
/// steps, else where its steps left it; nowhere (the map's corner, on the ground) without a hand or before its first
/// turn of steps
[[nodiscard]] map_coords::MapCoords ReportedHandPlace(const ecs::components::ComputerHand* hand, uint32_t turn);

/// The step goes at the end of the line
void Append(ecs::components::ComputerHand& hand, ecs::components::HandStep step);

/// What the steps of the hand do in the game
/// Where a hand that follows a thing goes: the thing's place on the map, its height above the land there included,
/// and the thing's height
struct FollowedPlace
{
	map_coords::MapCoords place {};
	float height {0.0f};
};

/// A creature as the journey's speed reads it: where it is, through the map grid, and the fractions of its top speed
/// its kind walks and runs at
struct JourneyBody
{
	glm::vec3 point {};
	float walk {0.0f};
	float run {0.0f};
};

/// How far across the gesture a hand's gesture step lays on the land is, in metres
constexpr float k_GestureOnLandSize = 30.0f;

class StageRunner
{
public:
	virtual ~StageRunner() = default;

	/// Runs the stage (0 start, 1 going on, 2 end) of the step
	virtual StageResult Run(ecs::components::ComputerHand& hand, ecs::components::HandStep& step, uint8_t stage) = 0;
	/// The hand lets go of what it holds, where it is
	virtual void LetGo(ecs::components::ComputerHand& hand) = 0;
	/// The miracle is an angry one (its effect's first perceived desire is anger): a cast of it at a thing or a point
	/// is moved near to rather than to
	[[nodiscard]] virtual bool IsAngryMiracle(int32_t miracle) const = 0;
	/// The hand's player's influence at a place on the map grid, its allies' counted
	[[nodiscard]] virtual float InfluenceAt(const map_coords::MapCoords& place) const = 0;
	/// Where the hand acts on a thing: its place on the map grid, at its height above the land there
	[[nodiscard]] virtual glm::vec3 InteractPoint(entt::entity thing) const = 0;
	/// The hand throws what it holds, from where it is, at the speed, as the release of a player's hand throws it
	virtual void ThrowHeld(ecs::components::ComputerHand& hand, const glm::vec3& velocity) = 0;
	/// The thing that a step acts on is no longer in the game
	[[nodiscard]] virtual bool IsGone(entt::entity thing) const = 0;
	/// The thing may be picked up by a computer player's hand
	[[nodiscard]] virtual bool CanBePickedUp(entt::entity thing) const = 0;
	/// The thing goes into the hand; whether it went in
	virtual bool TakeIntoHand(ecs::components::ComputerHand& hand, entt::entity thing) = 0;
	/// The hand's player gives what the hand holds to the thing, as a player's hand gives a held object to the object
	/// under it: the thing takes it or not, and what it does not take stays in the hand
	virtual void GiveHeldTo(ecs::components::ComputerHand& hand, entt::entity thing) = 0;
	/// How much of the resource the thing holds (a store's pile answers with its store's)
	[[nodiscard]] virtual int32_t ResourceOf(entt::entity thing, ResourceType type) const = 0;
	/// The thing is a pot, or a pile, of the resource
	[[nodiscard]] virtual bool IsPotOf(entt::entity thing, ResourceType type) const = 0;
	/// The amount of the resource is taken out of the thing
	virtual void RemoveResource(entt::entity thing, ResourceType type, int32_t amount) = 0;
	/// The pot takes the amount of the resource
	virtual void AddToPot(entt::entity pot, ResourceType type, int32_t amount) = 0;
	/// A new pot of the kind a hand carries for the resource, holding the amount, made where the thing is; none when
	/// none was made
	virtual entt::entity MakeHandPot(entt::entity at, ResourceType type, int32_t amount) = 0;

	// ---- what the miracle steps ask

	/// The gesture a miracle is made with: its seed's selection gesture when `which` is 0, else its seed's own gesture
	[[nodiscard]] virtual uint8_t GestureOf(int32_t miracle, int32_t which) const = 0;
	/// The gesture's shape: its points, each from 0 to 1 across (x) and up (z)
	[[nodiscard]] virtual const std::vector<glm::vec3>& GestureShape(uint8_t gesture) const = 0;
	/// How long the gesture's shape is, against the circle's
	[[nodiscard]] virtual float GestureLengthRatio(uint8_t gesture) const = 0;
	/// The gesture's sparkles are laid on the land from the corner, k_GestureOnLandSize across, for the hand's player
	virtual void LayGestureOnLand(uint8_t gesture, const glm::vec3& corner) = 0;
	/// The spell icon has a worship site: a site's icon its site, a town's icon its town's
	[[nodiscard]] virtual bool HasWorshipSite(entt::entity icon) const = 0;
	/// The thing is a worship site's icon
	[[nodiscard]] virtual bool IsWorshipIcon(entt::entity thing) const = 0;
	/// The icon is charging, for anyone
	[[nodiscard]] virtual bool IsIconCharging(entt::entity icon) const = 0;
	/// The icon may start charging for the hand's player at the power-up level, from its site's chants
	[[nodiscard]] virtual bool ValidForStartCharge(entt::entity icon, int32_t powerUp) const = 0;
	/// The icon starts charging for the hand's player at the power-up level, from its site's chants
	virtual void StartCharge(entt::entity icon, int32_t powerUp) = 0;
	/// The chants the icon still needs before its miracle is charged
	[[nodiscard]] virtual float ChantNeeded(entt::entity icon) const = 0;
	/// The power-up level a miracle is: -1 for a miracle at its base level
	[[nodiscard]] virtual int32_t PowerUpOf(int32_t miracle) const = 0;
	/// How long, in seconds, the hand stays with a thing after casting the miracle on it
	[[nodiscard]] virtual float SecondsWithTheThingAfterACast(int32_t miracle) const = 0;
	/// Where the hand follows a thing to: its place on the map and its height; none without a place
	[[nodiscard]] virtual std::optional<FollowedPlace> PlaceToFollow(entt::entity thing) const = 0;
	/// The icon's miracle cast on the thing, an object, from the hand's point: none when the icon still needs chants,
	/// the thing is no object or the miracle can't be cast on it
	virtual entt::entity CastOnObject(entt::entity thing, int32_t miracle, const glm::vec3& from, entt::entity icon) = 0;
	/// The hand's player's mind keeps the miracle it cast on the thing, the newest first
	virtual void KeepCast(entt::entity spell, entt::entity thing) = 0;
	/// The icon's miracle cast at a place on the map grid from another, each on the land there at its height above it,
	/// with a radius: none when the icon still needs chants, the miracle's own test refuses the place or nothing was
	/// made
	virtual entt::entity CastAtPlace(int32_t miracle, const map_coords::MapCoords& to, const map_coords::MapCoords& from,
	                                 entt::entity icon, float radius) = 0;
	/// The hand's player's mind keeps the miracle it cast at the place, the newest first
	virtual void KeepCastAt(entt::entity spell, const map_coords::MapCoords& place) = 0;
	/// Whether the thing is a creature that has a player
	[[nodiscard]] virtual bool IsCreatureWithPlayer(entt::entity thing) const = 0;
	/// Whether the thing is an object the hand acts on (not a creature's leash point or a mark)
	[[nodiscard]] virtual bool IsLeashableObject(entt::entity thing) const = 0;
	/// The creature's player's leash takes the type (a leash type's number), whether or not it is on
	virtual void SetLeashType(entt::entity creature, int32_t type) = 0;
	/// Whether the creature's leash is on and tied to something
	[[nodiscard]] virtual bool IsLeashTied(entt::entity creature) const = 0;
	/// The creature's leash comes off
	virtual void TakeLeashOff(entt::entity creature) = 0;
	/// The creature's leash is put on, tied to the thing
	virtual void TieLeash(entt::entity creature, entt::entity thing) = 0;
	/// The creature stops what it does as a failure, and the desire (by its number) is made its dominant one for the
	/// seconds, its body's needs left free
	virtual void ForceDominantDesire(entt::entity creature, int32_t desire, float seconds) = 0;
	/// The creature's god orders it to do an action (by its row of the game's table) to the thing, a plan made commanded
	virtual void OrderCreature(entt::entity creature, uint32_t action, entt::entity thing) = 0;
	/// Whether the creature still carries out a commanded plan
	[[nodiscard]] virtual bool IsCommanded(entt::entity creature) const = 0;
	/// The creature is taught an ordinary action or a miracle (by its kind and number) at once, its lesson shown
	virtual void TeachCreature(entt::entity creature, uint32_t kind, uint32_t action) = 0;
	/// The hand's player takes hold of a village totem and sets its share: the players' living near it react, and with
	/// a town the town's player's creature sees its player wants to impress it at the totem and the town's share of
	/// worship is set, else the totem's own
	virtual void RaiseTotem(entt::entity totem, float share) = 0;
	/// The creature a thing is, as the journey's speed reads it; none for a thing that is no creature
	[[nodiscard]] virtual std::optional<JourneyBody> JourneyBodyOf(entt::entity thing) const = 0;
	/// A draw of the synced generator up to the most, its value thrown away
	virtual void DrawThrownAway(float most) = 0;
	/// The creature keeps that fraction of its top speed, as for a journey
	virtual void SetJourneySpeed(entt::entity creature, float fraction) = 0;

	// ---- what the mind's actions ask

	/// The villager becomes a disciple of the kind (0 none), as a disciple a player's hand makes
	virtual void MakeDisciple(entt::entity villager, int32_t disciple) = 0;
};

/// The hand gives up every step and lets go of what it holds; its next step will be looked at for a move again
void Clear(ecs::components::ComputerHand& hand, StageRunner& runner);

/// Every turn: when a thing that one of the steps acts on is gone, the hand gives up every step
void SweepGone(ecs::components::ComputerHand& hand, StageRunner& runner);

/// The hand's place on the map grid from where it is, as each turn of its steps keeps it
void KeepMapPlace(ecs::components::ComputerHand& hand);

/// One turn of the hand's steps: the move that the first step needs is put in front of it, or the first step's stage
/// runs. Then, unless it put a move in front, the hand's place becomes its reported camera. Whether the hand kept its
/// steps (false when a stage failed and the hand gave them all up).
bool Step(ecs::components::ComputerHand& hand, StageRunner& runner);

// ---- the moves of the hand ------------------------------------------------------------------------------------------

/// The length of the game's turn in milliseconds: a step of the hand is its speed over the turns in a second
constexpr uint32_t k_TurnMilliseconds = 100;
/// How high a hand that moves over the land is held above the ground, in metres
constexpr float k_HoverHeight = 10.0f;
/// A distance above which a move to an object takes long strides, and the stride
constexpr float k_FarFromObject = 300.0f;
constexpr float k_FarStride = 100.0f;
/// A distance above which (up to the far one) a move to an object takes short strides, and the stride
constexpr float k_NearObject = 100.0f;
constexpr float k_NearStride = 10.0f;
/// How near the object the hand must be for the move to it to end
constexpr float k_AtObject = 1.0f;

/// The height of the land at a place on the map grid
class Ground
{
public:
	virtual ~Ground() = default;

	[[nodiscard]] virtual float AltitudeAt(int32_t mapX, int32_t mapZ) const = 0;
};

/// How far the hand goes in a turn at the speed
[[nodiscard]] float StepLength(float speed);

/// The hand is held the hover height above the land under it
void HoverOverGround(ecs::components::ComputerHand& hand, const Ground& ground);

/// A point from a place on the map grid and a height above the land there, as a thing's place is kept
[[nodiscard]] glm::vec3 PointFromMap(int32_t mapX, int32_t mapZ, float heightAboveGround, const Ground& ground);

/// The start of a move: the hand will go to the point
void StartTowards(ecs::components::ComputerHand& hand, const glm::vec3& target);

/// A turn of the move to a point over the land: across the land only, held above the ground, until the target is no
/// further than a step. Done when it got there
[[nodiscard]] StageResult MoveOverGround(ecs::components::ComputerHand& hand, const Ground& ground);

/// A turn of the move to a point over the land that stops at the edge of the hand's player's influence: as
/// MoveOverGround, and done too once the step left the hand where its player has no influence at all
[[nodiscard]] StageResult MoveUntilOutOfInfluence(ecs::components::ComputerHand& hand, const Ground& ground,
                                                  StageRunner& runner);

/// A turn of the move straight to a point, its height too. Done when it got there
[[nodiscard]] StageResult MoveStraight(ecs::components::ComputerHand& hand);

/// A turn of the move to a thing: across the land, in long strides when far and short ones nearer, held above the
/// ground. Done when it is within a metre
[[nodiscard]] StageResult MoveToThing(ecs::components::ComputerHand& hand, const Ground& ground);

/// The move near a point ends this many metres from it, or nearer
constexpr float k_NearThePoint = 80.0f;
/// Within this share of that distance from its point across the land the move first drifts away from it, and a drift's
/// longest step
constexpr float k_TooNearShare = 0.33f;
constexpr float k_TooNearThePoint = k_NearThePoint * k_TooNearShare;
constexpr float k_LongestDrift = 1.4142f;

/// A turn of the move near a point, across the land and held above the ground. Within 26.4 m of the point the hand
/// drifts away along x, a step of at most 1.4142 m, and goes on; further away it goes a step towards the point, no
/// further than the point, and is done once it is within 80 m of it. The point's height becomes the turn's leftover
/// milliseconds, 0 at the game's turn
[[nodiscard]] StageResult MoveNearTurn(ecs::components::ComputerHand& hand, const Ground& ground);

/// The kinds of miracle the game's tables hold, from none to the last creature miracle
constexpr int32_t k_MiracleKinds = 42;

/// The longest drift of the move near a thing, a hair longer than the move near a point's
constexpr float k_LongestDriftNearThing = 1.41421f;

/// A turn of the move near a thing, whose place the start took as the target: as MoveNearTurn, its drift at most
/// 1.41421 m, and the target's height left as it is
[[nodiscard]] StageResult MoveNearThingTurn(ecs::components::ComputerHand& hand, const Ground& ground);

/// The pull of the land on a thrown thing, in metres a second squared, and its inverse as the game keeps it
constexpr float k_Gravity = -9.81f;
constexpr float k_OneOverGravity = 0.101936795f;

/// The speed a computer hand throws a thing at, from its point to the point to hit: the time of the flight is the
/// square root of the distance over the land's pull, and the speed covers the way, the rise against the fall
/// included, in that time. Not a number when the two points are the same
[[nodiscard]] glm::vec3 ThrowVelocity(const glm::vec3& from, const glm::vec3& to);

/// A stage of the ThrowObjectInHandAtPos step (the point): only its turn does anything: the hand throws what it holds
/// towards the point, at ThrowVelocity from the hand's point
[[nodiscard]] StageResult ThrowAtPosStage(ecs::components::ComputerHand& hand, const ecs::components::HandStep& step,
                                          uint8_t stage, StageRunner& runner);

/// A stage of the GiveObjectInHandToCreature step (the creature): only its turn does anything: the hand throws what it
/// holds to where it acts on the creature, at ThrowVelocity from the hand's point
[[nodiscard]] StageResult GiveToCreatureStage(ecs::components::ComputerHand& hand, const ecs::components::HandStep& step,
                                              uint8_t stage, StageRunner& runner);

/// The hand is put at the point at once: held above the ground, or at the point's own height without a ground
void PutAt(ecs::components::ComputerHand& hand, const glm::vec3& point, const Ground* ground);

// ---- a script takes the hand ----------------------------------------------------------------------------------------

/// A step that takes the hand to a point
[[nodiscard]] ecs::components::HandStep StepToPoint(ecs::components::HandSubAction type, const glm::vec3& point);

/// A script takes the hand for the step: the hand gives up its steps and lets go of what it holds, takes the speed, and
/// the step is its only one; the hand is then held by the script and busy
void TakeForScript(ecs::components::ComputerHand& hand, ecs::components::HandStep step, float speed, StageRunner& runner);

/// A pause (or its end) holds the hand (or lets it go), and the steps take the player's speed again
void Pause(ecs::components::ComputerHand& hand, bool paused);

/// The player's speed and the speed of the steps under way
void SetSpeed(ecs::components::ComputerHand& hand, float speed);

/// A script is moving the hand: it gave it steps it has not finished, or queued actions for it
[[nodiscard]] bool MovedByScript(const ecs::components::ComputerHand& hand);

/// The hand goes through its steps this turn: it has some, or a script holds it
[[nodiscard]] bool TakesItsTurn(const ecs::components::ComputerHand& hand);

/// The land under the hand as of its last turn
[[nodiscard]] glm::vec3 OnGround(const ecs::components::ComputerHand& hand, const Ground& ground);

// ---- holding --------------------------------------------------------------------------------------------------------

/// A computer player's hand may not pick up a thing that a hand already holds: the player's own, or a computer player's
[[nodiscard]] bool MayBePickedUp(entt::entity thing, std::optional<entt::entity> heldByPlayersHand,
                                 std::span<const entt::entity> heldByComputerHands);

/// The player a computer player's hand's let-go is credited to: the hand lets go through its player's own interface, so
/// it is that player's throw; a hand of no player's (the neutral player has no mind) is no one's
[[nodiscard]] std::optional<PlayerNames> LetGoCreditedTo(PlayerNames handPlayer);

/// What a let-go leaves for the hand that let go (the rest of a scaffold put down gently, or nothing) goes straight
/// into that hand, as a pick-up's thing does; whether it went in
bool TakeLeftOver(ecs::components::ComputerHand& hand, entt::entity leftOver, StageRunner& runner);

/// A turn of the pick up of the thing: a hand that holds something lets go of it first and tries again the next turn; a
/// thing that may not be picked up fails the step; else the thing goes into the hand (or does not, and the step is over
/// either way)
[[nodiscard]] StageResult PickUpTurn(ecs::components::ComputerHand& hand, entt::entity thing, StageRunner& runner);

/// A turn of the drop: the hand lets go of what it holds where it is
[[nodiscard]] StageResult DropTurn(ecs::components::ComputerHand& hand, StageRunner& runner);

/// A turn of the giving of what the hand holds to the thing: the hand's player gives it (the thing takes it or not),
/// and the step's turn is over either way
[[nodiscard]] StageResult GiveHeldTurn(ecs::components::ComputerHand& hand, entt::entity thing, StageRunner& runner);

/// A turn of taking the resource out of a container into the hand: the amount asked for, less what a pot of the
/// resource in the hand holds already, and no more than the container has. A hand that holds something else waits; the
/// amount goes into the pot held, or into a new pot of the hand's kind that goes into the hand. Done when the hand has
/// a pot of it; the step fails when it has none
[[nodiscard]] StageResult TakeFromContainerTurn(ecs::components::ComputerHand& hand, entt::entity container, int32_t asked,
                                                ResourceType type, StageRunner& runner);

// ---- the miracle's gesture ------------------------------------------------------------------------------------------

/// How many metres across a computer hand draws a gesture's shape
constexpr float k_GestureSize = 30.0f;
/// How many turns a gesture as long as the circle takes
constexpr float k_GestureTurnsForTheCircle = 30.0f;

/// The gesture a miracle is made with: the selection gesture of the seed the game gives the miracle (its first seed
/// that has it) when `which` is 0, else that seed's own gesture; none for a miracle out of the tables or no seed has
[[nodiscard]] uint8_t GestureOfMiracle(const InfoConstants& info, int32_t miracle, int32_t which);

/// The hand's point at a point of a gesture's shape: the shape stands upright from where the gesture began, its
/// across along the land's x and its up as height; the hand keeps the z it began at
[[nodiscard]] glm::vec3 GesturePoint(const glm::vec3& origin, const glm::vec3& shapePoint);

/// The start of a gesture: it begins where the hand is, and the hand goes to the start of the shape
void StartGesture(ecs::components::ComputerHand& hand, const std::vector<glm::vec3>& shape);

/// A turn of a gesture: it goes on by one over the turns that a gesture of its length takes (the circle's 30 times its
/// length against the circle's), up to the whole of it. Done once it has the whole (the hand stays where it was), else
/// the hand goes to that point of the shape
[[nodiscard]] StageResult GestureTurn(ecs::components::ComputerHand& hand, const std::vector<glm::vec3>& shape,
                                      float lengthRatio);

/// The end of a gesture: the hand goes back where it began
void EndGesture(ecs::components::ComputerHand& hand);

/// A stage of the MakeGesture step (its miracle and which of its gestures): the stages above, with the gesture's shape
/// from the game
[[nodiscard]] StageResult GestureStage(ecs::components::ComputerHand& hand, const ecs::components::HandStep& step,
                                       uint8_t stage, StageRunner& runner);

// ---- the worshippers' chant for a miracle ---------------------------------------------------------------------------

/// How long, in seconds, the hand waits for its icon to start charging
constexpr uint32_t k_SecondsToStartCharging = 5;
/// How long, in seconds, the hand waits for its icon to be charged
constexpr uint32_t k_SecondsToCharge = 30;

/// The whole seconds the hand's count of turns makes (the turns over the turns in a second, cut toward zero)
[[nodiscard]] uint32_t CountedSeconds(int32_t turns);

/// A stage of the MakeWorshippersChantForSpell step (its miracle and its icon). Its start counts from 0 and needs an
/// icon with a worship site. Each turn of it counts one: it needs a worship site's icon; it is done once the icon is
/// charging for anyone, or once it starts charging for the hand's player at the miracle's power-up level; else it fails
/// when the icon has no site, or once more than 5 seconds are counted, and goes on otherwise. Its end needs a worship
/// site's icon, and is done once the icon needs no more chants; else it counts one, fails once more than 30 seconds are
/// counted, and goes on
/// A stage of the MakeWorshippersChantForSpellNoWait step: the chant's start and turn, and an end that does nothing,
/// so the hand does not wait for the miracle to be charged
[[nodiscard]] StageResult ChantNoWaitStage(ecs::components::ComputerHand& hand, const ecs::components::HandStep& step,
                                           uint8_t stage, StageRunner& runner);
[[nodiscard]] StageResult ChantStage(ecs::components::ComputerHand& hand, const ecs::components::HandStep& step, uint8_t stage,
                                     StageRunner& runner);

// ---- the cast of a miracle at an object ----------------------------------------------------------------------------

/// How high over the land a hand following a thing is, in the thing's heights
constexpr float k_HeightsOverAFollowedThing = 2.5f;

/// A turn of the hand following a thing: it goes to the thing's place again, across the land, in long strides when
/// far and short ones nearer, no further than the thing, and is held two and a half of the thing's heights over the
/// land
void FollowThing(ecs::components::ComputerHand& hand, const FollowedPlace& at, const Ground& ground);

/// A stage of the CastSpellAtObject step (the thing, the miracle and its icon). Its start does nothing more. Its turn
/// follows the thing once, unless the miracle is an angry one, and casts the icon's miracle on it from where the
/// hand is: none fails; else the mind keeps the cast and the hand stays with the thing for the miracle's time after a
/// cast. Its end follows the thing until that time is over, counting it down a turn at a time
/// A stage of the CastSpellAtPos step (the point, the miracle and its icon). Its start does nothing more. Its turn
/// casts the icon's miracle at the point's place on the map grid, on the land there, from the hand's point through
/// the map grid, with a radius of 1: none fails; else the mind keeps the cast and the hand stays for the miracle's time after a
/// cast. Its end counts that time down a turn at a time
[[nodiscard]] StageResult CastAtPosStage(ecs::components::ComputerHand& hand, const ecs::components::HandStep& step,
                                         uint8_t stage, StageRunner& runner);
/// A stage of the CastShieldAtPos step (the point, the miracle, the radius and the icon). Only its turn does anything:
/// it casts the icon's miracle at the point's place on the map grid, on the land there, with the radius, from the
/// hand's place on the map grid: none fails; else the mind keeps the cast
[[nodiscard]] StageResult CastShieldAtPosStage(ecs::components::ComputerHand& hand, const ecs::components::HandStep& step,
                                               uint8_t stage, StageRunner& runner);
/// The steps of a miracle's lesson after the creature was taught it: the hand goes to the icon, orders the creature to
/// look at it, goes to the creature and casts the miracle on it from the icon
void TeachMiracle(ecs::components::ComputerHand& hand, entt::entity creature, entt::entity icon, MagicType miracle);
/// How far a journey is: the root of the squares of the three differences, added as the game adds them
[[nodiscard]] float JourneyDistance(const glm::vec3& from, const glm::vec3& to);
/// The fraction of its top speed a creature keeps for a journey: half again its run beyond 100 metres, else the mean
/// of its walk and run (the hand's step clamps it from none to all)
[[nodiscard]] float JourneyFraction(float distance, float walk, float run);
/// A stage of the SetCreatureSpeedForJourney step (the creature, the journey's point): its going-on stage fails for a
/// thing that is no creature, else sets the creature's speed for the journey, with a draw thrown away for a short
/// one; the other stages are done at once
[[nodiscard]] StageResult JourneySpeedStage(const ecs::components::HandStep& step, uint8_t stage, StageRunner& runner);
/// The step that sets the creature's speed for a journey to the point
void SpeedForJourney(ecs::components::ComputerHand& hand, entt::entity creature, const glm::vec3& point);
/// A stage of the RaiseTotem step (the totem, the share): its first stage raises the totem to the share; every stage
/// is done at once
[[nodiscard]] StageResult RaiseTotemStage(const ecs::components::HandStep& step, uint8_t stage, StageRunner& runner);
/// The steps of an ordinary action's lesson after the creature was taught it, which only the action at a totem has:
/// the creature is ordered to the nearest totem within 250 metres and the hand raises it fully. Whether there is such
/// a lesson and a totem
bool TeachNormalAction(ecs::components::ComputerHand& hand, entt::entity creature, uint32_t action, entt::entity totem);
/// The steps that put the creature on a leash (the angry one unless told) and tie it to the thing
void LeashToThing(ecs::components::ComputerHand& hand, entt::entity creature, entt::entity thing,
                  LeashType type = LeashType::Evil);
/// A stage of the ForceCreatureDominantDesire step (the creature, the desire's number, the seconds): its first stage
/// makes the creature stop what it does as a failure and the desire its dominant one for the seconds, its body's needs
/// left free, and the hand waits no more; every stage is done at once
[[nodiscard]] StageResult ForceDominantDesireStage(ecs::components::ComputerHand& hand, const ecs::components::HandStep& step,
                                                   uint8_t stage, StageRunner& runner);
/// A stage of the ForceCreatureToPerformAction step (the creature, the action's row, the thing): its first stage orders
/// the creature to do the action to the thing; its going-on stage follows the creature and goes on while it carries
/// out the commanded plan; its end is done at once
[[nodiscard]] StageResult ForceActionStage(ecs::components::ComputerHand& hand, const ecs::components::HandStep& step,
                                           uint8_t stage, StageRunner& runner, const Ground& ground);
/// A stage of the SetLeash step (the creature, the leash type): its going-on stage gives the creature's player's leash
/// that type when the creature has a player; every stage is done at once
[[nodiscard]] StageResult SetLeashStage(const ecs::components::HandStep& step, uint8_t stage, StageRunner& runner);
/// How near, in metres, the hand must be to the thing a leash is tied to before the tie's end is done
constexpr float k_TiedLeashNear = 30.0f;
/// A stage of the AttachLeashToObject step (the creature, the thing). The start takes a tied leash off, failing for a
/// creature with no player; the going-on stage ties the leash to the thing, an object, and waits two seconds; the end
/// follows the thing, counting the wait down, and is done once the hand is within 30 m of it
[[nodiscard]] StageResult AttachLeashStage(ecs::components::ComputerHand& hand, const ecs::components::HandStep& step,
                                           uint8_t stage, StageRunner& runner, const Ground& ground);
[[nodiscard]] StageResult CastAtObjectStage(ecs::components::ComputerHand& hand, const ecs::components::HandStep& step,
                                            uint8_t stage, StageRunner& runner, const Ground& ground);

// ---- waiting --------------------------------------------------------------------------------------------------------

/// The turns a wait of so many seconds lasts: the turns in a second times the seconds, cut toward zero
[[nodiscard]] int32_t WaitTurns(float seconds);

/// The start of a wait: the hand will count down the turns of so many seconds
void StartWait(ecs::components::ComputerHand& hand, float seconds);

/// A turn of a wait: a turn less, and done when none is left. A wait of no turns goes below zero and does not end
[[nodiscard]] StageResult WaitTurn(ecs::components::ComputerHand& hand);

// ---- the actions of the mind that a script forces or queues ---------------------------------------------------------

/// The action of a node of the computer player's mind that a script names
enum class MindAction : uint8_t
{
	/// A node whose action openblack does not have yet
	NotPorted,
	/// The scaffold given is picked up and dropped at the marker
	FindPlaceToUseScaffoldForBuilding,
	/// The thing given is picked up and dropped at the marker, at once
	DirectPickupAndDropObject,
	/// The villager is taken from its town to the worship site's gate
	TakeVillagerFromTownToWorshipSite,
	/// Food is taken from another town's store and given to the object above
	GetFoodFromOtherTownForObject,
	/// Wood is taken from another town's store and given to the thing above, or a building site's building
	GetWoodFromOtherTown,
	/// Food is taken from another town's store and given to where the town above takes it
	GetFoodFromOtherTownForStoragePit,
	/// The miracle of the icon given is cast on the store of the town given
	CastIconsMiracleOnTownStore,
};

/// The first node of the mind whose name, up to its ':', the name begins with, in the order the game looks: none when
/// no node has such a name. The nodes are known by their rows' names as they are now
[[nodiscard]] std::optional<MindAction>
FindMindAction(std::string_view name, const computer_player::MindRowNames& names = computer_player::OwnRowNames());

/// A name that begins with "DIRECT", in any case: its node's action runs at once
[[nodiscard]] bool IsDirectAction(std::string_view name);

/// The two leaves' steps: the thing is picked up and dropped at the point, unless it may not be picked up
bool PickUpAndDrop(ecs::components::ComputerHand& hand, entt::entity thing, const glm::vec3& point, StageRunner& runner);

/// How long the hand holds a villager at a worship site's gate before it drops it, in seconds
constexpr float k_SecondsAtTheGate = 1.0f;
/// The number the move to the gate carries, which the move does not read
constexpr float k_GateMoveNumber = 900.0f;

/// How far from the hand the point a thing is thrown at to impress a town may be, in metres
constexpr float k_ImpressThrowReach = 700.0f;

/// The steps that give a thing to another: picked up at the player's speed, then given to the other, as a hand gives
/// what it holds to the object under it. The caller has asked that the thing may be picked up
void PickUpAndGive(ecs::components::ComputerHand& hand, entt::entity thing, entt::entity to);

/// The steps that throw a thing at a point: picked up at the player's speed, then thrown at the point. False, with no
/// steps, when the thing may not be picked up
bool PickUpAndThrow(ecs::components::ComputerHand& hand, entt::entity thing, const glm::vec3& point, StageRunner& runner);

/// The steps that take a villager to a worship site's gate: picked up at the player's speed, the move to the gate, a
/// second held there, and the drop at the gate. None when there is no gate, or the villager may not be picked up
void TakeToGate(ecs::components::ComputerHand& hand, entt::entity villager, std::optional<glm::vec3> gate, StageRunner& runner);

/// The steps that fetch food from a store's pile for an object: the move to the pile, the food asked for taken out of
/// it into the hand, and given to the object
void FetchFoodFor(ecs::components::ComputerHand& hand, entt::entity pile, const glm::vec3& pilePoint, int32_t asked,
                  entt::entity object);

/// The steps that fetch wood from a store's pile for a thing: the wood asked for taken out of the pile into the hand
/// (the move to the pile goes in front of it), and given to the thing
void FetchWoodFor(ecs::components::ComputerHand& hand, entt::entity pile, uint32_t asked, entt::entity thing);

/// From this many metres away from its icon the hand makes the miracle's gesture instead of going to the icon
constexpr float k_GestureFromIconMetres = 200.0f;

/// The steps that cast an icon's miracle on a thing. Nearer than 200 m to the icon the hand goes to it, else it makes
/// the miracle's gesture; the icon's worshippers chant for the miracle; then a miracle cast at an object is cast at the
/// thing, and any other kind of miracle no further. False, with no steps, for a miracle cast at a place (fireballs,
/// lightning, storms, tornadoes and shields), which openblack's hand does not cast yet
bool CastIconsMiracleAt(ecs::components::ComputerHand& hand, entt::entity icon, MagicType miracle, float metresToIcon,
                        entt::entity thing);

/// Where a miracle cast at a thing that is not cast on it goes: the thing's own place; for a fireball or lightning the
/// edge of the thing towards the hand, and the nearest place inside the god's influence to it (none when the god has no
/// influence)
struct TowardPlaces
{
	glm::vec3 place {0.0f};
	glm::vec3 edge {0.0f};
	std::optional<glm::vec3> inside;
};

/// As CastIconsMiracleAt, the miracles cast at a place too. A fireball or lightning: after the reach and the chant the
/// hand goes to the nearest place inside the influence, then towards the thing's edge until it would leave the
/// influence, and casts the miracle towards the edge; with no place inside the influence nothing follows the chant. A
/// storm, a tornado or a shield: the hand goes to the thing's place and casts it there as a shield with no radius
bool CastIconsMiracleAtOrToward(ecs::components::ComputerHand& hand, entt::entity icon, MagicType miracle, float metresToIcon,
                                entt::entity thing, const TowardPlaces& toward);

/// How long the hand stays after a miracle cast at a thing, in seconds
constexpr float k_SecondsAfterCastAtThing = 8.0f;

/// The steps that cast an icon's miracle on a thing and stay: as CastIconsMiracleAt, then eight seconds' wait and the
/// step that closes the miracle down, which does nothing. False, with no steps, where CastIconsMiracleAt is
bool CastIconsMiracleOnAndStay(ecs::components::ComputerHand& hand, entt::entity icon, MagicType miracle, float metresToIcon,
                               entt::entity thing);

/// The steps that cast an icon's miracle to impress a town: the hand goes to the icon, the icon's worshippers chant for
/// the miracle, and it is cast at the point, whatever kind of miracle it is
void ImpressWithIconsMiracle(ecs::components::ComputerHand& hand, entt::entity icon, MagicType miracle, const glm::vec3& point);

/// The steps of one round of an attack on a creature from an icon: the move near the creature, the chant for the
/// miracle and its cast on the creature
void AttackCreatureWithIcon(ecs::components::ComputerHand& hand, entt::entity icon, MagicType miracle, entt::entity creature);

/// Where a god drops a worker for a building site: round its building at 1.25 times its radius, from angle 0 in eighths
/// of a turn, the last place on the map, or the first whose map cell holds something with wood
[[nodiscard]] map_coords::MapCoords WorkerDropPlace(const map_coords::MapCoords& centre, float radius,
                                                    const std::function<bool(const map_coords::MapCoords&)>& woodThere);

/// Where a villager taken home is dropped: from the building's place towards where a walk to it arrives, at 1.25 times
/// the building's radius and the villager's own beyond it
[[nodiscard]] map_coords::MapCoords HomeDropPlace(const map_coords::MapCoords& building, const map_coords::MapCoords& arrive,
                                                  float buildingRadius, float villagerRadius);

/// Where the hand goes before throwing food at its creature: on the way from the hand to the creature, 20 m short of
/// it; when the creature is nearer than 20 m, 20 m back from the hand, away from it
[[nodiscard]] glm::vec3 FoodThrowFrom(const glm::vec3& hand, const glm::vec3& creature);

/// The food is picked up, the hand goes where FoodThrowFrom puts it and the food is thrown at the target; false, with
/// no steps, when the food may not be picked up
bool ThrowFoodAt(ecs::components::ComputerHand& hand, entt::entity food, const glm::vec3& creature,
                 const std::function<glm::vec3()>& target, StageRunner& runner);

/// The thing of the leaf at the end of the line of nodes the node chose, or of the nearest node above it that has one
[[nodiscard]] entt::entity ThingOfChosenLeaf(const ecs::components::ComputerMind& mind, uint16_t node);

/// The steps that cast a miracle on a creature from an icon: the hand goes to the icon, the icon's worshippers chant for
/// the miracle, and it is cast on the creature, whatever miracle the icon holds
void CastOnCreatureFromIcon(ecs::components::ComputerHand& hand, entt::entity icon, MagicType miracle, entt::entity creature);
/// The same with the heal miracle
void HealCreatureFromIcon(ecs::components::ComputerHand& hand, entt::entity icon, entt::entity creature);

/// The steps that charge an icon's miracle: nearer than 200 m to the icon the hand goes to it, else it makes the
/// miracle's gesture; then the icon's worshippers start chanting for the miracle, and the hand does not wait
void ChargeIconsMiracle(ecs::components::ComputerHand& hand, entt::entity icon, MagicType miracle, float metresToIcon);

class ThingFacts;

/// The steps that cast an icon's miracle at a place with a radius. The hand reaches the icon and the worshippers chant
/// as for CastIconsMiracleAt; then a miracle cast at an object (explosions, heal, teleport, forest, food, wood, water)
/// is cast at the point of the place's circle that faces the hand, and the hand goes to the place and casts it again
/// as a shield there with the radius; a storm, a tornado, a shield or a flock only that second way; any other kind of
/// miracle no further. False, with no steps, for a fireball or lightning, which openblack's hand does not cast yet
bool CastIconsMiracleAtPlace(ecs::components::ComputerHand& hand, entt::entity icon, MagicType miracle, float metresToIcon,
                             const map_coords::MapCoords& place, float radius, const ThingFacts& facts);

/// What the game knows of the things a mind's action acts on
class ThingFacts
{
public:
	virtual ~ThingFacts() = default;

	/// The thing is an object, as against a town, a marker or the like
	[[nodiscard]] virtual bool IsObject(entt::entity thing) const = 0;
	/// The thing has a place on the land
	[[nodiscard]] virtual bool HasPlace(entt::entity thing) const = 0;
	/// Where the thing is, when it is a script's marker
	[[nodiscard]] virtual std::optional<glm::vec3> MarkerPoint(entt::entity thing) const = 0;
	/// The thing is a villager
	[[nodiscard]] virtual bool IsVillager(entt::entity thing) const = 0;
	/// The thing is a creature
	[[nodiscard]] virtual bool IsCreature(entt::entity thing) const = 0;
	/// The thing is a scaffold
	[[nodiscard]] virtual bool IsScaffold(entt::entity thing) const = 0;
	/// A villager's town and abode, none for either it lacks
	struct VillagerHome
	{
		entt::entity town {entt::null};
		entt::entity abode {entt::null};
	};
	[[nodiscard]] virtual VillagerHome HomeOf(entt::entity villager) const = 0;
	/// Where a walk to the thing arrives, on the map grid: an abode's door, a town centre's place
	[[nodiscard]] virtual map_coords::MapCoords ArrivePlace(entt::entity thing) const = 0;
	/// The thing is a building site, one not on its way out of the game
	[[nodiscard]] virtual bool IsAvailableBuildingSite(entt::entity thing) const = 0;
	/// The thing is a worship site
	[[nodiscard]] virtual bool IsWorshipSite(entt::entity thing) const = 0;
	/// The point of a worship site's gate, through the map grid; none when the site has no gate
	[[nodiscard]] virtual std::optional<glm::vec3> GatePoint(entt::entity site) const = 0;
	/// Where the thing is, through the map grid
	[[nodiscard]] virtual glm::vec3 MapPoint(entt::entity thing) const = 0;
	/// The building of a building site; none for a thing that is not a building site, or a site without one
	[[nodiscard]] virtual entt::entity SiteBuilding(entt::entity thing) const = 0;
	/// How much of the resource the thing holds (a store's pile answers with its store's)
	[[nodiscard]] virtual uint32_t ResourceOf(entt::entity thing, ResourceType type) const = 0;
	/// The thing is a town
	[[nodiscard]] virtual bool IsTown(entt::entity thing) const = 0;
	/// The resource of a pot or a pile
	[[nodiscard]] virtual ResourceType ResourceTypeOf(entt::entity thing) const = 0;
	/// Where a town takes the resource: its store, else its temporary pot (made when it has none); none when it has
	/// neither
	[[nodiscard]] virtual entt::entity ResourceDropObject(entt::entity town, ResourceType type) const = 0;
	/// The miracle of a worship site's icon; none for a thing that is no such icon
	[[nodiscard]] virtual std::optional<MagicType> IconMagicType(entt::entity thing) const = 0;
	/// The thing's place on the map grid
	[[nodiscard]] virtual map_coords::MapCoords MapPlace(entt::entity thing) const = 0;
	/// The edge of a thing towards a place, on the map grid
	[[nodiscard]] virtual map_coords::MapCoords NearestEdgeTo(entt::entity thing, const map_coords::MapCoords& place) const = 0;
	/// The point of a place on the map grid: on the land there, at the place's height above it
	[[nodiscard]] virtual glm::vec3 LandPoint(const map_coords::MapCoords& place) const = 0;
	/// Where a worship site keeps its food, on the map grid; none for a thing that is no worship site
	[[nodiscard]] virtual std::optional<map_coords::MapCoords> WorshipFoodPlace(entt::entity site) const = 0;
	/// A worshipper of a site: whether it is hiding at the site, and its life
	struct Worshipper
	{
		entt::entity villager {entt::null};
		bool hiding {false};
		float life {0.0f};
	};
	/// A worship site's worshippers, the newest first; none for a thing that is no worship site
	[[nodiscard]] virtual std::vector<Worshipper> SiteWorshippers(entt::entity site) const = 0;
};

/// The actions of the mind that openblack has, done by the hand: the two leaves that pick a thing up and drop it at a
/// point, the villager taken to a worship site, the food and wood fetched from another town, and an icon's miracle cast
/// on a town's store or on an object. With what the score
/// rules read of the game it scores every way openblack has (computer_player::Score); without, it scores nothing. A
/// chosen action it lacks is told to `missing`, when one is given
class HandActions final: public computer_player::MindWorld
{
public:
	using Missing = std::function<void(computer_player::ActionKind, uint16_t)>;
	/// Told of each action run, with whether it was done (the mind's debug trace)
	using Ran = std::function<void(computer_player::ActionKind, uint16_t, bool)>;

	HandActions(ecs::components::ComputerHand& hand, StageRunner& runner, const ThingFacts& facts,
	            computer_player::ScoreWorld* scores = nullptr, Missing missing = {}, Ran ran = {});

	float Score(computer_player::ScoreKind kind, ecs::components::ComputerMind& mind, uint16_t node) override;
	[[nodiscard]] bool HasScore(computer_player::ScoreKind kind) const override;
	[[nodiscard]] bool HasAction(computer_player::ActionKind kind) const override;
	void GiveUpSteps() override;
	/// FindPlaceToUseScaffoldForBuilding: the thing nearest above, one with a place, is picked up and dropped at the
	/// point of the node the node chose. DIRECTPickupAndDropObject: the thing nearest above, an object, is picked up and
	/// dropped at the node's second thing, a marker. TakeVillagerFromTownToWorshipSite: the node's villager is picked up,
	/// taken to the gate of the worship site above, held there a second and dropped; done with no steps when the site
	/// has no gate or the villager may not be picked up. GetFoodFromOtherTownForObject: the node's pile and the object above
	/// its parent, both objects, and the food its parent asks for: the food is fetched from the pile and given to it.
	/// GetWoodFromOtherTown: the node's pile, with a place and some wood, and the thing above its parent with a place
	/// (or a building site's building): all the pile's wood is fetched and given to it. GetFoodFromOtherTownForStoragePit:
	/// the node's pile, an object, and the town above its parent with somewhere to take it: the food the node asks for,
	/// cut to a whole number, is fetched and given to where the town takes it. ShallCastFoodSpell (the casting leaves
	/// under the towns' food and wood wants): the icon nearest above, a worship site's, and the town above its parent with
	/// somewhere to take the miracle's resource (wood for the wood miracle, else food): the icon's miracle is cast on it
	/// (CastIconsMiracleAt), from where the hand is. CastFoodSpellOn (the leaves that feed its creature, fill its
	/// workshop with wood and put out a burning building): the icon nearest above, a worship site's, and the object
	/// above its parent: the icon's miracle is cast on the object, and the hand stays (CastIconsMiracleOnAndStay).
	/// ShallCastWoodSpell (the leaf that gives a building site wood): the icon nearest above, a worship site's, and the
	/// building site above its parent with a building: the icon's miracle is cast at the building's place, with no radius
	/// (CastIconsMiracleAtPlace). CastFoodSpellOnWorshipSite (the leaf that feeds the worshippers): the icon nearest
	/// above, a worship site's, and the worship site above its parent: the icon's miracle is cast where the site keeps
	/// its food, with no radius (CastIconsMiracleAtPlace). RestoreMyCreatureHealth (the leaves that heal its creature):
	/// the icon nearest above, a worship site's, and the creature above its parent: the hand goes to the icon, the
	/// worshippers chant for the heal miracle and it is cast on the creature (HealCreatureFromIcon). ChargeUpSpell
	/// (the leaf that charges a miracle): the icon nearest above, a worship site's: its miracle is charged
	/// (ChargeIconsMiracle). CreateDiscipleToFulfillTownDesire (the leaf that makes a disciple for a town): the
	/// node's villager, and the town above its parent: the villager becomes a disciple of the kind the node chose,
	/// then it is picked up and dropped where it stood; the action fails, the villager still a disciple, when it may
	/// not be picked up. CastHealSpellAtWorshipSite (the leaf that heals tired worshippers): the icon nearest above, a
	/// worship site's, and the worship site above its parent with a worshipper to heal (MostHurtWorshipper): the
	/// icon's miracle is cast at that worshipper's place, with no radius (CastIconsMiracleAtPlace). The shields, with what the
	/// score rules read of the game: DefendtownWithSpells (a shield over a town attacked lately) and
	/// CastSpellToDefendAgainstBelief (a shield over a town under a belief attack, while the town has something to
	/// defend): the icon nearest above, a worship site's, and the town above its parent: the icon's miracle is cast at
	/// the centre of the town's area with the town's radius; the first not when the mind cast that miracle there
	/// already. CastPhysicalShieldToProtect: the icon and the object above its parent: the miracle is cast at the
	/// object's place with 1.2 times its radius, not when the mind cast it there already. CastSpellToImpress (the
	/// leaf that impresses a town with a miracle): the icon nearest above, a worship site's, and the town above its
	/// parent: the icon's miracle is cast at the node's point, where its weighing found the town's villagers
	/// (ImpressWithIconsMiracle). ThrowThingsToImpress: the thing nearest above, an object, the
	/// town above its parent, and the node's point within 700 m of the hand: the thing is thrown at the node's point,
	/// where its weighing found the town's villagers. ThrowrockatCreature: the thing nearest above, with a place, and the
	/// creature above its parent: the thing is thrown at the creature's place. UseCreatureAsDiverseionAgainstPlayer:
	/// nothing, and the action fails. AttackCreatureWithCreature: the creature nearest above and the creature above its
	/// parent: the first is put on the angry leash, tied to the second (LeashToThing). DestroytownWithCreature, with
	/// what the score rules read of the game: the thing with a place above its parent gives the town (a town is its
	/// own), and the creature nearest above is put on the angry leash tied to the town's store, else its centre, else its
	/// newest building (CreatureDestroyTarget); when the creature knows none of the angry miracles it could be taught,
	/// the mind owes it one more lesson of them. UseCreatureToImpressTown, the same way: the creature is put on the
	/// learning leash tied to the town's store, else its newest building, and its desire to impress is made dominant
	/// for 120 seconds, its body's needs left free. TeachCreatureAggressiveMagic, TeachCreatureCompassionateMagic and
	/// TeachCreatureImpressiveMagic, with what the score rules read of the game: the creature above its parent and the
	/// icon nearest above; the first miracle of the kind the creature could be taught and does not know, none failing;
	/// the mind's lessons owed of that kind are cleared, the creature is taught the miracle at once, and the hand goes
	/// to the creature, then to the icon, orders the creature to look at it, goes back and casts the miracle on the
	/// creature from the icon (TeachMiracle). TeachCreatureNormalAction, with what the score rules read of the game:
	/// the creature nearest above; the first ordinary action it could be taught and does not know, none failing; the
	/// hand goes to the creature and orders it to its lesson, the creature is taught the action at once, and the
	/// action's own lesson follows (TeachNormalAction), failing when it has none or no totem is near.
	/// SendCreatureHome: the creature nearest above, with a home: its speed set for the journey home, the hand to it,
	/// and the creature ordered home. GetCreaturetoHelp, and DefendTownWithCreature, which runs it: the town of the
	/// thing above its parent and the creature nearest above; its speed set for the journey to the town's store, else
	/// its newest building, then on the kind leash tied to it. GetCreaturetoHelpBuild: the building site above its
	/// parent with its building, and the creature nearest above; its speed set for the journey there, the creature
	/// ordered to help build, a wait of half a second, then its compassion made dominant for two minutes. The three
	/// helping actions owe the creature a lesson of compassionate miracles when it knows none it could be taught.
	/// DestroytownWithSpell, with what the score rules
	/// read of the game: the icon nearest above, a worship site's, and the thing above its parent, a town's building most worth
	/// attacking from the hand when it is a town, else an object: the icon's miracle is cast at it
	/// (CastIconsMiracleAtOrToward). AttackCreatureWithSpells, with what the score rules read of the game: the icon nearest
	/// above, a worship site's, and the creature above its parent: two or three times, by the game's random draws, the hand
	/// moves near the creature, the worshippers chant and the icon's miracle is cast on it (AttackCreatureWithIcon).
	/// CombineScaffolds: the scaffold nearest above and the scaffold of the leaf the node chose: the first is picked up and
	/// given to the second. CastFoodForMyCreature: the icon nearest above, a worship site's, and the creature above its
	/// parent: the hand goes to the icon, the worshippers chant for the food miracle and it is cast on the creature
	/// (CastOnCreatureFromIcon). GiveBuildingSiteWorkers, with what the score rules read of the game: the villager
	/// of the leaf the node chose and the building site nearest above: the villager is picked up and dropped by the
	/// site's building (WorkerDropPlace). The parents whose leaves (a field, a fish farm, a forest) have no action
	/// of their own run when those leaves are chosen: TownWithHighestFoodDesire (the town's food and wood wants, and
	/// winning a town by its food): the object of the chosen leaf is picked up and given to where the town nearest
	/// above takes its resource; GiveBuildingSiteWood: the thing of the chosen leaf is picked up and dropped at the
	/// building site's building; FillWorkshop: the thing of the chosen leaf is picked up and given to the workshop
	/// nearest above, nothing more when the hand may not take it; GiveCreatureFood and GiveWorshippersFood: the thing of
	/// the chosen leaf is picked up and given to the thing nearest above (the creature, the worship site);
	/// ReacttoCreatureStarvingEmergency: the thing of the chosen leaf is picked up and given to the creature nearest
	/// above from the hand. TakeVillagerFromWorshipSiteToTown, with what the score rules read of the game: the
	/// villager nearest above, the worship site above its parent, and the villager's town: the villager is picked up and
	/// dropped outside its abode's door, else its town centre's (HomeDropPlace), else at its town's place
	bool RunAction(computer_player::ActionKind kind, ecs::components::ComputerMind& mind, uint16_t node) override;
	void TimeSteps() override;
	void ActionMissing(computer_player::ActionKind kind, uint16_t node) override;
	void ActionRan(computer_player::ActionKind kind, uint16_t node, bool done) override;

private:
	/// The three shields' actions
	bool RunShield(computer_player::ActionKind kind, ecs::components::ComputerMind& mind, uint16_t node, entt::entity icon);

	ecs::components::ComputerHand& _hand;
	StageRunner& _runner;
	const ThingFacts& _facts;
	computer_player::ScoreWorld* _scores;
	Missing _missing;
	Ran _ran;
};

/// Where a shield of the mind goes and how wide
struct ShieldCast
{
	map_coords::MapCoords place;
	float radius {0.0f};
};
/// The shield of one of the three shields' actions over the thing above its leaf's parent: over a town (a shield
/// over a town attacked lately, or against a belief attack while the town has something to defend), at the centre of
/// its area with its radius; over an object (the physical shield), at its place with 1.2 times its radius. None when
/// the town is no town, has nothing to defend from a belief attack, or the mind cast the miracle there already (not
/// asked against a belief attack)
[[nodiscard]] std::optional<ShieldCast> ShieldFor(computer_player::ActionKind kind, const computer_player::ScoreWorld& world,
                                                  const ecs::components::ComputerMind& mind, entt::entity above,
                                                  MagicType miracle, const map_coords::MapCoords& objectPlace);

/// The worshipper the heal at a worship site goes to: the first, the newest first, of those not hiding at the site
/// whose life is the least and below full; none when every one is hiding or at full life
[[nodiscard]] entt::entity MostHurtWorshipper(std::span<const ThingFacts::Worshipper> worshippers);

/// A script forces the action named on the two things through the player's mind, as the game does
/// (computer_player::Force): the hand gives up its steps when a node's action runs, and is busy when that gave it steps.
/// Whether it did; nothing for a name no node has, a node whose action openblack lacks or a weighing it cannot make. The
/// nodes are known by their rows' names as they are now
bool ForceAction(ecs::components::ComputerHand& hand, ecs::components::ComputerMind& mind, std::string_view name,
                 entt::entity first, entt::entity second, uint32_t turn, StageRunner& runner, const ThingFacts& facts,
                 const computer_player::MindRowNames& names = computer_player::OwnRowNames());
/// The same through a world of the mind's own (the game's, which scores and tells the actions it lacks)
bool ForceAction(ecs::components::ComputerHand& hand, ecs::components::ComputerMind& mind, std::string_view name,
                 entt::entity first, entt::entity second, uint32_t turn, const ThingFacts& facts,
                 computer_player::MindWorld& world,
                 const computer_player::MindRowNames& names = computer_player::OwnRowNames());

/// A script queues the action, to be forced when the hand is free
void QueueAction(ecs::components::ComputerHand& hand, std::string name, entt::entity first, entt::entity second);

/// Every turn: a queued action one of whose things is gone is dropped
void SweepQueue(ecs::components::ComputerHand& hand, const StageRunner& runner);

/// The next queued action, taken off the queue
[[nodiscard]] std::optional<ecs::components::QueuedHandAction> TakeNext(ecs::components::ComputerHand& hand);

// ---- drawing what the hand holds ------------------------------------------------------------------------------------

/// What the draw keeps for the thing a computer hand holds: the point it slides from, the point it slides to, and the
/// game milliseconds drawn since the last turn of the hand and in the turn before
struct HeldDraw
{
	glm::vec3 start {0.0f};
	glm::vec3 hand {0.0f};
	int32_t elapsedMs {0};
	int32_t durationMs {0};
	/// The hand's turns of steps the draw has seen
	uint32_t turnsSeen {0};
};

/// The hand had one or more turns of steps since the draw last looked, the last one beginning at the point: the thing
/// slides from there to there, over the time drawn in the turn before (none when more than one turn went by), so it is
/// drawn a turn at a time, as the original draws it. With `smooth` (the View menu's smooth drawing, not as the
/// original) it slides from where the hand was as the turn before began
void NewTurnsOfTheHand(HeldDraw& draw, const glm::vec3& turnStartPoint, uint32_t turns, bool smooth = false);

/// Where the thing is between the two points after `elapsedMs` of `durationMs`: at the end point once the time is up,
/// or when there is no time
[[nodiscard]] glm::vec3 SlideOfHeld(const glm::vec3& start, const glm::vec3& hand, int32_t elapsedMs, int32_t durationMs);

/// A frame of the draw of the held thing: the frame's game milliseconds go by, and the point it is drawn at
[[nodiscard]] glm::vec3 HeldDrawPoint(HeldDraw& draw, uint32_t frameGameMs);

/// The point through the map grid, as a thing's place is kept: x and z on the grid, the height above the land kept
[[nodiscard]] glm::vec3 OnTheMapGrid(const glm::vec3& point, const Ground& ground);

} // namespace openblack::computer_hand
