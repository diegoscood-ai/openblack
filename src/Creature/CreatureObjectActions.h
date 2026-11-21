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
#include <optional>
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"

namespace openblack
{
enum class PlayerNames : uint8_t;
}

/// What a creature does with the things about it: picking them up, holding, examining, eating, putting down, tossing
/// away and throwing them, knocking them down, and pointing at them. Each is played on the body by animations, and
/// whatever it is done to is taken hold of or let go of at a moment of the animation particular to the species.
namespace openblack::creature_object_actions
{
enum class Kind : uint8_t
{
	/// Walking up to something and reaching for it with the four reaching animations blended
	PickUp,
	/// Letting go of what it holds: setting it down where the hand is, tossing it away, or lobbing it gently
	PutDown,
	Discard,
	Lob,
	/// Eating what it holds
	Eat,
	/// Stroking, shaking, smelling or examining what it holds, which it keeps hold of
	Keep,
	/// Throwing what it holds at a point
	Throw,
	/// Walking up to something and striking it with the four striking animations blended
	Destroy,
	/// Pointing at something for a while
	Point,
	/// Reaching out to take what the player's hand gives it, which it takes from the hand at a moment of the animation
	TakeFromHand,
	/// Kicking a thing in front of it, which flies off as the foot meets it
	Kick,
	/// Kissing another creature, or kissing the air: the high and the low kiss played once together, blended by how far
	/// up its kiss it kisses
	Kiss,
};
[[nodiscard]] std::string_view Name(Kind kind);
/// Whether a script's forced finish plays the animations of the kind at twice the pace: holding, letting go of (put
/// down, tossed, lobbed, eaten), picking up and knocking down a thing; not throwing, kicking or taking from the hand
[[nodiscard]] bool HurriedByForcedFinish(Kind kind);
/// Whether the kind's animations play at the game's own pace whatever the creature's size: the kiss
[[nodiscard]] bool PlaysAtRealSpeed(Kind kind);
constexpr float k_HurriedPace = 2.0f;

/// Whether the arm that holds a thing shows the hold pose over the body, while the body plays the animation of an
/// action, or none. The animations that move the held thing themselves replace it: picking up, letting go of it (put
/// down, tossed away, lobbed or eaten), stroking, shaking, smelling or examining it, the throw once the body has turned
/// to it, and taking from the god's hand. Walking or turning to a thing, knocking it down, kicking and pointing keep it
[[nodiscard]] bool ShowsHoldPose(std::optional<Kind> playing);
/// Whether letting go of what it holds by the given animation lays it down where the creature stands (the put-down's
/// animation, 97) rather than tossing it away (the toss's, 95, or any other), as the game's discard chooses by its
/// animation
[[nodiscard]] bool DiscardPutsDown(size_t animation);

/// The animations of what it does to what it holds while keeping it: stroke, shake, smell, examine
constexpr size_t k_FirstKeepAnimation = 100;
constexpr size_t k_KeepAnimationCount = 4;
constexpr size_t k_StrokeObject = 100;
constexpr size_t k_ExamineObject = 103;
/// Pointing, low and high to the left and right
constexpr std::array<size_t, 4> k_PointAnimations {208, 209, 210, 211};
/// It points for this long, and turns to face what it points at first unless the point is less than this far round
/// from its head (PointTurnAngle)
constexpr float k_PointSeconds = 5.0f;
constexpr float k_PointTurnRadians = 0.471238941f;
/// It turns to face what it throws at first when it is more than this far round
constexpr float k_ThrowTurnRadians = 0.01f;

/// Pointing, it aims from its eye, this many times its size above where it stands (its standing height)
constexpr float k_PointEyeHeightOfSizeOne = 15.0f;
/// Pointing, how far round to either side (a twelfth of a turn) and how far up or down (the rise over the distance)
/// the four animations reach; a point nearer the eye than k_PointNearest is straight ahead
constexpr float k_PointAcrossLimit = 0.52359879f;
constexpr float k_PointRiseLimit = 0.5f;
constexpr float k_PointNearest = 0.1f;
/// One over the whole width of the sweep across, the float the game multiplies by
constexpr float k_PointAcrossScale = 0.95492965f;

/// Where a point is for the pointing animations, from the creature's eye
struct PointAim
{
	/// The way the creature faces less the way to the point, wrapped once, kept within k_PointAcrossLimit
	float across {0.0f};
	/// The rise to the point over its distance, kept within k_PointRiseLimit
	float rise {0.0f};
};
/// Where a point `offset` from the eye is, for a creature facing `heading` (a body yaw: it faces (sin, -cos) of it).
/// Each operation is a float in the game's order: the distance the root of z z + x x + y y
[[nodiscard]] PointAim AimOfPoint(glm::vec3 offset, float heading);
/// The weights of the four pointing animations (k_PointAnimations: low and high, left and right) for an aim, played
/// mirrored or not: mirrored, the sweep across goes the other way. The share across is (across + the limit) times
/// k_PointAcrossScale, the share up the rise + 0.5, and the four weights the products of the shares and their
/// complements
[[nodiscard]] std::array<float, 4> PointWeights(PointAim aim, bool mirrored);
/// Whether a creature that points without turning first plays the pointing mirrored: when the way from its head to the
/// point (`fromHead`) less the way it faces (`heading`), wrapped once, is above 0
[[nodiscard]] bool PointsMirroredFacing(glm::vec3 fromHead, float heading);
/// How far round a point is from the way the creature faces (`heading`), seen from its head: the way from its head to
/// the point (`fromHead`) less the heading, wrapped once
[[nodiscard]] float PointTurnAngle(glm::vec3 fromHead, float heading);
/// Whether the creature turns to face a point before it points at it: unless the turn angle is less than
/// k_PointTurnRadians either way
[[nodiscard]] bool TurnsToPoint(float turnAngle);
/// The pointing animations' time a turn on while the creature points: on by the turn's step only while it is before the
/// species' point moment, round once past the end of the low left animation (`durationMs`) and never at or past it;
/// held where it is from the point moment on
[[nodiscard]] float PointClipTime(float timeMs, float stepMs, float pointMs, float durationMs);

/// The kicking animation
constexpr size_t k_KickAnimation = 117;
/// A thing kicked flies off at this many times the creature's size
constexpr float k_KickSpeedPerSize = 45.0f;
/// How the creature leans along the land's two axes, from the land's heights a radius to either side of it along each:
/// the lean its body's placement takes (creature_lean::FromHeights)
[[nodiscard]] glm::vec2 LeanOf(float plusX, float minusX, float plusZ, float minusZ, float radius);
/// The way a kick sends what it meets: half up and half ahead of the creature, the up part tipped up or down with the
/// creature's lean the way it faces, as a unit
[[nodiscard]] glm::vec3 KickDirection(glm::vec2 forward, glm::vec2 lean);
/// How a thing it kicks starts to fly: the kick's way at the kick's speed for its size
[[nodiscard]] glm::vec3 KickVelocity(glm::vec2 forward, glm::vec2 lean, float size);
/// The player a thing a creature sets flying is credited to: the game hands the physics the interface of the creature's
/// player, so it is that player's throw; a creature of no player's is no one's
[[nodiscard]] std::optional<PlayerNames> ThrowerPlayerOf(PlayerNames creaturePlayer);

/// What a creature's test of whether it may kick a thing reads of the thing (a thing that is no home)
struct KickFacts
{
	/// The creature's leash is tied to the thing
	bool leashTiedTo {false};
	/// The thing's row has the toy bowling ball's model
	bool bowlingBall {false};
	/// The thing is a creature
	bool creature {false};
	/// The thing can be thrown or knocked into the physics, and whether it is marked immovable
	bool canBecomePhysicsObject {false};
	bool immovable {false};
	/// The thing's weight as an object, and the creature's own
	float thingWeight {0.0f};
	float creatureWeight {0.0f};
	/// The thing is marked indestructible
	bool indestructible {false};
	/// The thing is what this creature carries, or is in the player's hand
	bool carriedByIt {false};
	bool inHand {false};
};
/// Whether a creature may kick a thing that is no home: never what its leash is tied to nor the toy bowling ball;
/// always another creature; else a thing that can fly, is not immovable, weighs less than twice the creature, is not
/// marked indestructible, and is held neither by this creature nor by the player's hand
[[nodiscard]] bool CanBeKicked(const KickFacts& facts);
/// Whether a creature may kick a home: one built and not on fire
[[nodiscard]] bool CanHomeBeKicked(bool onFire, bool built);

/// A kick breaks pieces off a home with two blows of the foot, low on the home, at most this many times the creature's
/// size above the land, and each as wide as that or as six tenths of the home's radius, the smaller
constexpr float k_HomeKickReachPerSize = 3.75f;
constexpr float k_HomeKickRadiusShare = 0.6f;
/// Each blow strikes at twice the kick's way
constexpr float k_HomeKickBlowSpeed = 2.0f;
/// How many ways the second blow may be turned, centred on ahead, a tenth of a quarter turn apart; and how many heights
/// each blow may take, a hundredth apart
constexpr uint32_t k_HomeKickTurns = 11;
constexpr uint32_t k_HomeKickHeights = 100;
/// The two blows of a kick on a home: each strikes along its velocity through its point, as wide as the radius
struct HomeKick
{
	glm::vec3 firstPoint;
	glm::vec3 firstVelocity;
	glm::vec3 secondPoint;
	glm::vec3 secondVelocity;
	float radius;
};
/// Where a kick on a home strikes: half way from where the creature stands to where its foot meets the home
[[nodiscard]] glm::vec3 HomeKickCentre(glm::vec3 position, glm::vec3 kickPoint);
/// The two blows of a kick on a home, from the point half way to the foot and the land's altitude under it, the kick's
/// way, the creature's heading as the game keeps it (it faces sin, -cos of it), its size and the home's radius, and
/// three of the game's random numbers: one below k_HomeKickTurns for the second blow's turn, then one below
/// k_HomeKickHeights for each blow's height. The point is first raised to the land; each blow strikes at that share of
/// its height above the land, at most the reach. The first blow goes the kick's way; the second along the land, ahead
/// turned by its draw less five tenths of a quarter turn, the difference taken unsigned as the game takes it, so that a
/// draw below five turns it by a vast angle
[[nodiscard]] HomeKick HomeKickBlows(glm::vec3 centre, float altitude, glm::vec3 direction, float gameHeading, float size,
                                     float homeRadius, uint32_t turnDraw, uint32_t firstHeightDraw, uint32_t secondHeightDraw);

/// What it needs in its hands to do it
enum class Hands : uint8_t
{
	/// Something to act on
	Holding,
	/// Nothing, or what it holds is put down first
	Empty,
	/// Its hands as they are
	Either,
};
[[nodiscard]] Hands HandsFor(Kind kind);

/// How an action is going
enum class Status : uint8_t
{
	Running,
	/// It has hold of what it reached for, or the blow has landed, and plays on to the end of the animation
	Contact,
	Done,
	Failed,
};

/// Why an action failed, as far as the creature's mind must know
enum class Failure : uint8_t
{
	/// It did not fail, or it failed for another cause
	Other,
	/// It found no way to the thing: the thing can't be picked up, it is out of the map, or the walk to it was refused
	NoWay,
};
/// Whether the creature keeps in mind that it found no way to the thing after the action failed, so that it does not
/// choose the thing again for a while: a pick-up or a stomp that found no way to its thing does
[[nodiscard]] bool LeavesThingAlone(Kind kind, Failure failure);

/// What a creature that walks up to a thing to pick it up or stomp on it does on a turn when it stands still. It goes
/// on until the thing is in reach or the action fails: there is no limit to its walks.
enum class ApproachStep : uint8_t
{
	/// The thing is in the blend of the reaching animations: they start
	Begin,
	/// It stands nearer the thing than it reaches at most: the action starts at once, turning on the spot first so that
	/// the middle of its reach faces the thing, and goes straight on from the turn
	TurnThenBegin,
	/// It walks again to the thing
	Walk,
};
[[nodiscard]] ApproachStep NextApproachStep(bool inBlend, bool nearerThanItReaches);
/// How near the thing a walk up to it ends: within the creature's furthest reach, and never less than a hundredth
[[nodiscard]] float WalkUpToWithin(float furthestReach);
/// A thing higher than this above the creature, in metres, can be too high to pick up
constexpr float k_MostPickUpRise = 5.0f;
/// Whether a thing in reach is too high to pick up: the difference of the heights of the two above the ground is
/// more than k_MostPickUpRise and more than the distance between them. The pick-up then stops.
[[nodiscard]] bool TooHighToPickUp(float rise, float distance);
/// A knock-down crushes what it knocked down every turn from the turn after the blow lands until its animation ends,
/// when the thing is a living one (a villager, an animal or a creature) still in the map: a crush of this much, as
/// wide as this, applied by the creature
constexpr float k_KnockDownCrush = 1.0f;
constexpr float k_KnockDownCrushRadius = 1.0f;
[[nodiscard]] bool KnockDownCrushes(Kind kind, Status status, bool living, bool inMap);
/// A creature kicked by another takes the effect info's crush times this: a tenth of one more than the kicker's height
/// over the kicked one's, that kept to 0..1 (a ratio that is no number taken as 0, as the game compares)
[[nodiscard]] float KickCrushShare(float kickerHeight, float kickedHeight);
/// In the same turns, a knock-down that is the last step of the creature's agenda counts what it knocked down among the
/// creature's kills when the thing, living or not, is still in the map with no life left (a life that is not a number
/// counts too). It counts again every such turn
[[nodiscard]] bool KnockDownKills(Kind kind, Status status, bool inMap, bool lastStep, float life);
/// How a creature decides whether it may pick up a kind of thing
enum class KindRule : uint8_t
{
	/// Never: creatures, buildings, fields, features other than magic mushrooms, lanterns, shields, vortices and teleport
	/// stones
	Never,
	/// Always, whatever else holds: the football and a creed
	Always,
	/// A static other than a rock: only when it is one of the toys, whatever else holds
	Toy,
	/// A bird: only when it is low enough over the land, whatever else holds
	Bird,
	/// A magic mushroom: unless a script said it can't be picked up
	Mushroom,
	/// A pile of food or wood: unless a script said it can't be picked up, and only a kind of pile that may fly about
	Pile,
	/// Anything else that can be picked up (villagers, animals, things lying about, pots, rocks, dead trees, trees): the
	/// test of things in general (creature_pick_up::CreatureMayPickUp)
	Whole,
};

/// The kinds of thing that differ in how a creature picks them up
enum class Thing : uint8_t
{
	/// Anything not named below: buildings, fields, features and the like
	Other,
	Creature,
	Villager,
	/// An animal other than a bird
	Animal,
	Bird,
	/// The football
	Ball,
	Creed,
	/// Anything else lying about
	MobileObject,
	Pot,
	/// A pile of food or wood
	Pile,
	Rock,
	Bonfire,
	DeadTree,
	/// A standing tree
	Tree,
	/// A magic mushroom growing on the land
	MagicMushroom,
	/// Any other static: only the toys among them are picked up
	Static,
	/// Statics with kinds of their own that no hand holds
	Lantern,
	Shield,
	/// Statics with kinds of their own that a hand could hold but a creature never picks up
	Vortex,
	TeleportStone,
	/// A thing built or set into the land that stones can knock about: a building, temple, totem or worship site, a
	/// feature other than a magic mushroom, an animated static or flowers
	Fixture,
};
/// How a creature decides on picking up a kind of thing
[[nodiscard]] KindRule RuleOf(Thing thing);
/// Whether a hand may hold a kind of thing at all
[[nodiscard]] bool Holdable(Thing thing);
/// Whether a kind of thing is alive: villagers, animals, birds and creatures
[[nodiscard]] bool IsLiving(Thing thing);
/// Whether a creature may throw things at a thing to knock it down: a living thing while it lives, and whatever is
/// built or set into the land (shields too), but no tree, lantern, vortex or teleport stone, nor anything lying about
[[nodiscard]] bool CanBeStoned(Thing thing, bool dead);
/// Whether a creature may stomp on a villager or an animal: while it lives, whatever its size or its player. A bird
/// goes by its height over the land (animals::birds::CreatureCanStompOn), and a creature or a thing that is not alive
/// by tests of their own, so this is false for them
[[nodiscard]] bool CanBeStompedOnWhileAlive(Thing thing, bool dead);
/// A thing set on the land (a tree, and what has no test of its own) may be stomped on when the land holds it and it is
/// lower than a fifth of fifteen times the stomper's drawn size
[[nodiscard]] bool CanThingOnTheLandBeStompedOn(bool inMap, float stomperSize, float height);
/// A building (a home, a town's other buildings, a totem, a spell dispenser; not a field) may be stomped on when it is
/// lower than a quarter of the stomper's height
[[nodiscard]] bool CanBuildingBeStompedOn(float stomperHeight, float height);
/// Another creature may be stomped on only when it may be as a thing set on the land, and then only when the stomper is
/// more than four times its height or its plan is of an action of recovery (sleeping, healing itself, resting to get
/// better, going home to recover and the others the game's action table marks so)
[[nodiscard]] bool CanCreatureBeStompedOn(bool inMap, float stomperSize, float stomperHeight, float height, bool recovering);
/// What a thing of no kind of its own is, for the stomp's test of its class, and what tells two things of one kind apart
enum class StompGround : uint8_t
{
	/// Nothing of the kinds below
	None,
	/// A home or any other building of a town: its storage pit, centre, creche, graveyard, workshop, wonder, football
	/// pitch, and the spell dispensers
	Building,
	Field,
	FishFarm,
	/// Set into the land with the test of things on the land: the temple, a worship site, a town's totem, a feature,
	/// an animated static, flowers, a spell's shield and the singing stones' base
	SetIntoLand,
	/// A static of the table that lies about (as the shield's row of the statics table is made)
	LyingAbout,
};
/// The test of its class by which a creature may stomp on a thing
enum class StompRule : uint8_t
{
	/// Fields, fish farms, vortices, teleport stones and what has no test of its own
	Never,
	/// What lies about: mobile objects, pots, piles, the football, creeds, rocks, bonfires, dead trees and the statics
	Always,
	/// Villagers and animals while they live
	WhileAlive,
	/// A bird by its height over the land
	LowBird,
	/// Another creature (CanCreatureBeStompedOn)
	Creature,
	/// A tree, a lantern and what is set into the land (CanThingOnTheLandBeStompedOn)
	OnTheLand,
	/// A building (CanBuildingBeStompedOn)
	LowBuilding,
};
/// The test of its class a kind of thing is stomped on by
[[nodiscard]] StompRule StompRuleOf(Thing thing, StompGround ground);
/// What a stomp's test needs to know of the thing and of the stomper
struct StompTarget
{
	StompRule rule {StompRule::Never};
	bool dead {false};
	/// A bird low enough over the land for the stomper, by the animal system's test
	bool lowBird {false};
	bool inMap {true};
	float height {0.0f};
	/// Another creature whose plan is of an action of recovery
	bool recovering {false};
};
/// Whether a creature of a drawn size and a height may stomp on a thing, by the test of the thing's class
[[nodiscard]] bool CanBeStompedOn(const StompTarget& target, float stomperSize, float stomperHeight);
/// Whether a thing does damage when a creature throws it: the statics, rocks and dead trees, but not the toys
[[nodiscard]] bool DoesThrowingDamage(Thing thing, bool toy);
/// Whether a creature may throw a thing to impress: anything but the living, vortices and teleport stones, as long as
/// the thing's height is less than 0.64 of the creature's weight. The game sets the thing's height against the
/// creature's weight, so in practice any such thing a creature can pick up will do (docs/wiki/project/progress/
/// vanilla_bugs/creature.md, row 66)
[[nodiscard]] bool CanBeThrownToImpress(Thing thing, float thingHeight, float creatureWeight);
constexpr float k_ThrowToImpressWeightShare = 0.64f;
/// Whether a creature may throw a thing into the sea in play: what lies about, the statics, rocks, dead trees and trees
[[nodiscard]] bool CanBeThrownInTheSea(Thing thing);
/// Whether a creature may examine a thing by looking at it, by the thing's kind: anything but a creature, a vortex or a
/// teleport stone. Of the things of no kind of their own, fields and fish farms are refused too, which the caller tells
/// apart
[[nodiscard]] bool CanBeExaminedByLooking(Thing thing);

/// The toys among the statics, known by their models
enum class Toy : uint8_t
{
	Ball,
	BowlingBall,
	Cuddly,
	Die,
	Skittle,
};
/// Which toy a static's model makes it, if any
[[nodiscard]] std::optional<Toy> ToyOf(MeshId mesh);

/// What the kind of a thing says about its being picked up by a creature
struct KindFacts
{
	Thing thing {Thing::Other};
	KindRule rule {KindRule::Never};
	/// A hand may hold it at all: villagers, animals, things lying about, pots and piles, statics and rocks, dead trees and
	/// trees
	bool holdable {false};
	/// One of the toys
	bool toy {false};
	/// A bird low enough over the land
	bool low {false};
	/// A kind of pile that may fly about
	bool mayFly {false};
	/// What a stomp's test of its class makes of it beyond its kind
	StompGround stompGround {StompGround::None};
};
/// Whether a creature may pick the thing up when its kind decides that alone, by the thing's own say (not one a script
/// said can't be picked up): none when the test of things in general decides, for a kind a hand may hold
[[nodiscard]] std::optional<bool> DecidedByKind(const KindFacts& kind, bool thingAllows);
} // namespace openblack::creature_object_actions
