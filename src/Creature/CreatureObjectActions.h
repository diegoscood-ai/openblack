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
};
[[nodiscard]] std::string_view Name(Kind kind);

/// The animations of what it does to what it holds while keeping it: stroke, shake, smell, examine
constexpr size_t k_FirstKeepAnimation = 100;
constexpr size_t k_KeepAnimationCount = 4;
constexpr size_t k_StrokeObject = 100;
constexpr size_t k_ExamineObject = 103;
/// Pointing, low and high to the left and right
constexpr std::array<size_t, 4> k_PointAnimations {208, 209, 210, 211};
/// It points for this long, and turns to face what it points at first when it is more than this far round
constexpr float k_PointSeconds = 5.0f;
constexpr float k_PointTurnRadians = 0.4712f;
/// It turns to face what it throws at first when it is more than this far round
constexpr float k_ThrowTurnRadians = 0.01f;

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
};
/// Whether a creature may pick the thing up when its kind decides that alone, by the thing's own say (not one a script
/// said can't be picked up): none when the test of things in general decides, for a kind a hand may hold
[[nodiscard]] std::optional<bool> DecidedByKind(const KindFacts& kind, bool thingAllows);
} // namespace openblack::creature_object_actions
