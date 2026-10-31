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

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"
#include "Enums.h"

/// The rules of the god hand taking hold of things, carrying them and letting them go: what it may pick up and how long
/// it waits to, how each kind of thing hangs in it, the spring that drags the hand along with what it holds, and whether
/// a release is a throw, a put-down or a pour. Pure functions, tested on their own; the hand's system applies them.
namespace openblack::hand_grab
{

/// A press shorter than this is a tap; untuggable things and things in flight are only taken once it is held this long
inline constexpr uint32_t k_GrabWaitMs = 225;
/// A game turn, in milliseconds, as the wait counts the turns gone by
inline constexpr uint32_t k_TurnMs = 100;
/// The hand's fade into its pulling pose; a pull only starts once it is over
inline constexpr float k_PullBlendSeconds = 0.13f;
/// Rocks wider than this across the ground can't be picked up
inline constexpr float k_RockMaxPickUpRadius = 3.6f;

/// The hand's spring, dragging the hand after where it should be while it is ready to throw
inline constexpr float k_SpringStiffness = 260.0f;
inline constexpr float k_SpringDamping = 40.0f;
/// The spring is stepped in steps of this many milliseconds of game time
inline constexpr uint32_t k_SpringStepMs = 10;
/// Nothing leaves the hand faster than this
inline constexpr float k_MaxThrowSpeed = 124.0f;

/// A release is a throw when its speed across the ground, squared, is more than this; slower it is a put-down
inline constexpr float k_HandThrowSpeedSquared = 4.0f;
/// The same for what a creature lets go of
inline constexpr float k_CreatureThrowSpeedSquared = 1.0f;
/// A pot let go no faster than this (its whole speed squared) is poured out where it is
inline constexpr float k_PourSpeedSquared = 5.0f;
/// After a throw the hand's motion gives what it threw a twist: this long after letting go...
inline constexpr uint32_t k_ReleaseSpinDelayMs = 180;
/// A handful poured out of the hand pours for three quarters of a second; a scoop's stream flows for at most a minute
inline constexpr float k_PourSeconds = 0.75f;
inline constexpr float k_ScoopStreamSeconds = 60.0f;
/// ...by this much times its mass and speed, for one game turn
inline constexpr float k_ReleaseSpinFactor = 1.6f;

/// What the hand holds hangs this far below it on the palm
inline constexpr float k_AboveHang = 0.2f;
/// What it holds any other way hangs at least this far below it
inline constexpr float k_MinimumHang = 1.9f;
/// A rooted thing (a standing tree) hangs a further share of its height lower
inline constexpr float k_RootedHangShare = 0.1f;
/// The point the hand picks on the land under the cursor is raised by this share of the hang
inline constexpr float k_CursorRaiseShare = 0.6f;
/// What the hand takes hangs at least this share of the hand's own height below it
inline constexpr float k_LeastLoweringShare = 0.3f;
/// The hand's standard height
inline constexpr float k_StandardHandHeight = 3.2f;

/// Put down tilted more than this (radians, either way), a tree isn't planted again
inline constexpr float k_UprightTilt = 0.2f;
/// A put-down stands where it is over dry land, or where the nearest cell stands higher than this
inline constexpr uint8_t k_LowestLandingAltitude = 1;

/// Which rule decides whether a thing may be placed in the hand. It is an allowlist: what is not named never goes in
enum class PlaceInHandRule : uint8_t
{
	/// Buildings, totems, features, animated statics, creatures, temple parts, icons and everything else
	Never,
	/// Loose things, simple pots, one-shot orbs, standing trees, forests, dead and felled trees, statics
	Always,
	/// A rock: no wider across the ground than the hand can lift
	Rock,
	/// A villager it can reach
	Villager,
	/// An animal whose species the player may pick up
	Animal,
	/// A field with growth and food
	Field,
	/// A fish farm whose food is worth something
	FishFarm,
	/// A pile only when it is the hand's own kind of pile with something in it
	Pile,
	/// A scaffold by its own rule
	Scaffold,
	/// A spell seed or a teleport stone, by the seed
	Seed,
	/// A fireball unless it is the hand's own player's
	FireBall,
};

/// The classes a thing belongs to, as far as placing it in the hand cares. One entity may carry several (a villager is
/// a mobile too, a scaffold a loose object); the most particular one decides
struct PlaceInHandClasses
{
	bool creature {false};
	bool villager {false};
	bool animal {false};
	bool scaffold {false};
	bool seedOrTeleport {false};
	bool fireBall {false};
	bool field {false};
	bool fishFarm {false};
	/// A pot or pile, and whether it is a pile (food or wood) rather than a simple pot
	bool pot {false};
	bool pile {false};
	bool deadTree {false};
	bool mobileStatic {false};
	/// A static that counts as a rock
	bool rock {false};
	/// The bonfire, and a piece knocked off a building: never placed in the hand
	bool bonfire {false};
	bool fragment {false};
	bool tree {false};
	bool bigForest {false};
	bool oneOffSpellSeed {false};
	/// Any other loose thing
	bool mobile {false};
};
/// The rule that decides for a thing of these classes
[[nodiscard]] PlaceInHandRule PlaceInHandRuleOf(const PlaceInHandClasses& thing);
/// A field may be placed in the hand while both its growth and its food are above nothing
[[nodiscard]] bool FieldValidForPlaceInHand(float growth, float food);
/// A pile may be placed in the hand only when its row is the hand's own row for its kind (the hand's food for a food
/// pile, the hand's wood for a wood pile) and it holds something. A stored or dropped pile of another row never
[[nodiscard]] bool PileValidForPlaceInHand(PotType kind, PotInfo row, uint32_t resource);
/// An empty hand's press locks onto a pile (the scoop) when it is a pile of food or wood of any row but the hand's own
/// for its kind, empty or not. A pot (food or wood pot, the things carried in the hand) is no pile
[[nodiscard]] bool PileValidForLockedSelect(PotType kind, PotInfo row);

/// How long a press has been held: by the clock, but no more than the game turns gone by allow
[[nodiscard]] uint32_t ElapsedMs(uint32_t nowMs, uint32_t pressMs, uint32_t turn, uint32_t pressTurn);

/// Whether the grab takes what it pressed on into the hand this frame. A thing in flight, a forest or a fireball waits
/// for the grab's wait; a resting thing for the end of its tug hold as the frame before left it, so it is taken the frame
/// after the hold ends and a release on the frame the hold ends is still a tap; a standing tree only once it is pulled
/// out of the ground, which takes it by itself
[[nodiscard]] constexpr bool GrabPicksUp(bool timer, uint32_t elapsedMs, bool holdOverBefore, bool standingTree)
{
	if (standingTree)
	{
		return false;
	}
	return timer ? elapsedMs >= k_GrabWaitMs : holdOverBefore;
}

/// How a grab other than a standing tree's starts: whether it waits the grab's time, whether the hand holds the thing
/// with a short pull meanwhile, and whether the drawn hand takes hold of it at the press
struct GrabStart
{
	bool timer;
	bool tugHold;
	bool drawnHold;
};
/// A thing free in the air, or one that can't be pulled (a big forest, a magic fireball, a field, a pile), waits the
/// grab's time; anything else is held with a short pull, but a miracle's seed. The drawn hand takes hold at the press of
/// anything that can be pulled, free or not
[[nodiscard]] constexpr GrabStart GrabStartOf(bool free, bool tuggable, bool seed)
{
	const bool timer = free || !tuggable;
	return {.timer = timer, .tugHold = !timer && !seed, .drawnHold = tuggable};
}

/// What letting go of a standing tree the hand is pulling at does
enum class TreeLetGo : uint8_t
{
	/// Nothing more: it stays rooted as it stood
	Nothing,
	/// It is tapped, as anything grabbed and let go within the grab's wait
	Tap,
};
/// A tree let go no later than the grab's wait after the press is tapped; one pulled free is held, and letting go of it
/// is a throw or a put-down instead
[[nodiscard]] constexpr TreeLetGo TreeRelease(uint32_t elapsedMs, bool uprooted)
{
	return !uprooted && elapsedMs <= k_GrabWaitMs ? TreeLetGo::Tap : TreeLetGo::Nothing;
}

/// How far a thing hangs below the hand as the hand takes hold of it to pull it free: its own hang, but no closer than a
/// share of the hand's height
[[nodiscard]] float HoldDistance(float loweringMultiplier, float height, float handSize);
/// How far the point the hand picks on the land is raised for what it holds
[[nodiscard]] float CursorRaise(float handRise);

/// The spring that drags the hand after where it should be while it is ready to throw. It is stepped in fixed steps of
/// game time, at least one each frame however short, and what it holds leaves the hand at its speed. The game time the
/// hand spends holding things is counted all along, the spring on or not, and the spring catches up with all of it when
/// it next steps.
class HandSpring
{
public:
	/// The spring takes hold at the hand, at rest; the time counted so far is kept
	void Start(glm::vec3 position);
	/// A frame of holding some game milliseconds long goes by, whether the spring is on or not
	void Count(uint32_t frameMs);
	/// The spring is stepped towards where the hand should be, as many steps as the time counted allows and at least one
	void Step(glm::vec3 target);
	[[nodiscard]] glm::vec3 Position() const { return _position; }
	[[nodiscard]] glm::vec3 Velocity() const { return _velocity; }
	[[nodiscard]] uint32_t StepsTaken() const { return _steps; }

private:
	glm::vec3 _position {0.0f};
	glm::vec3 _velocity {0.0f};
	uint32_t _elapsedMs {0};
	uint32_t _steppedMs {0};
	/// How many steps the last Step took
	uint32_t _steps {0};
};

/// A countdown in game milliseconds: still to run, run out this frame, or used up without running out (it reached
/// exactly nothing, or had nothing left), which cancels what it counts down to
enum class Countdown : uint8_t
{
	Running,
	RunOut,
	Cancelled,
};
/// A frame of some game milliseconds passes on a countdown
[[nodiscard]] Countdown CountDown(int32_t& msLeft, uint32_t frameMs);

/// Pulling a rooted thing free
inline constexpr float k_TugStiffness = 1000.0f;
/// The hand's greatest pull, for each step of its strength (which is never more than nothing)
inline constexpr float k_MaxForcePerStrength = 600000.0f;
/// The pulled thing turns about its base with this much heaviness, and is damped by this much times its spin
inline constexpr float k_TugTurnInertia = 1000.0f;
inline constexpr float k_TugTurnDamping = 4000.0f;
/// It stretches towards the hand by no more than this, easing to it over this long
inline constexpr float k_TugMaxStretch = 1.3f;
inline constexpr float k_TugStretchSeconds = 0.3f;

/// The hand's greatest pull at a strength
[[nodiscard]] float MaxForce(float strength);

/// What a pulled thing is as the hand pulls at it
struct Tug
{
	/// Its axes, scaled, as it leans; and its base on the land, which it turns about
	glm::mat3 axes {1.0f};
	glm::vec3 base {0.0f};
	/// How it is turning, about the axis the pull turns it about (its turning force is the grip's place from the base
	/// across the pull)
	glm::vec3 spin {0.0f};
	/// How fast the point at half its height moved last frame, as its unstretched axes moved it
	glm::vec3 pullVelocity {0.0f};
};
/// A thing as it stands, about to be pulled: its axes are its rotation's, each times its scale on that axis
[[nodiscard]] Tug TugOf(const glm::mat3& rotation, glm::vec3 scale, glm::vec3 base);
/// The rotation a pulled thing is drawn with as it leans: its axes made unit length again
[[nodiscard]] glm::mat3 TugRotation(const Tug& tug);

/// What the hand pulls with and against, a frame long
struct TugFrame
{
	/// Where the hand's line meets the plane of the land under the thing
	glm::vec3 hand {0.0f};
	/// How far up the thing the hand grips it
	float holdDistance {0.0f};
	float maxForce {k_MaxForcePerStrength};
	/// Its weight (its mass) and its height
	float weight {0.0f};
	float height {0.0f};
	/// Only trees are really pulled at; anything else comes free at once
	bool tree {false};
	/// Heavy things of the rock material neither lean nor stretch
	bool leans {true};
	float seconds {0.0f};
};

/// What a frame of pulling came to
struct TugResult
{
	/// The pull, capped at the hand's greatest
	glm::vec3 force {0.0f};
	/// How far it stretches towards the hand next
	float stretchTarget {1.0f};
	bool comesFree {false};
};

/// Where the hand grips the thing: up its axis from its base, the axis made unit length with the game's inverse square
/// root
[[nodiscard]] glm::vec3 TugGrip(const Tug& tug, float holdDistance);
/// A frame of the hand pulling at a rooted thing: the pull, whether it comes free, and how it leans. It turns by its spin
/// over the frame, each of its axes turned about the spin's axis with the game's axis-angle turn, and its middle's speed
/// is how far that turn moved it times one over the frame's time
[[nodiscard]] TugResult PullAt(Tug& tug, const TugFrame& frame);
/// Where the hand is drawn as it pulls, on the thing as it leans and stretches: its unit up times the stretch, then times
/// how far up the hand grips it, from its base
[[nodiscard]] glm::vec3 TugHandPoint(const Tug& tug, float holdDistance, float stretch);

/// The hand's fade into its pulling pose, from the start of its pulling state
struct TugFade
{
	/// How long it has faded, counted up to the fade's length
	float seconds {0.0f};
	/// The pull waits until the frame after the fade is over
	bool waiting {true};
};
/// A frame of the pulling state: whether the hand pulls this frame, then the fade moved on by the frame's time. The
/// first frames wait while the fade runs, the frame that finds it over waits too, and every frame after pulls
[[nodiscard]] bool StepTugFade(TugFade& fade, float frameSeconds);

/// How far apart two points are across the ground, their heights left out
[[nodiscard]] float GroundRange(glm::vec3 from, glm::vec3 to);
/// The plane the hand moves on while it pulls: through the thing's base, along the land there, at the height the line
/// from the eye through the hand (where the hand was before the press) has once it is as far from the eye as the base
struct TugPlane
{
	glm::vec3 point {0.0f};
	glm::vec3 normal {0.0f, 1.0f, 0.0f};
};
/// The plane a pull moves the hand on, set once as the pull starts
[[nodiscard]] TugPlane TugPlaneAt(glm::vec3 eye, glm::vec3 hand, glm::vec3 base, glm::vec3 landNormal);
/// Where the hand is while it pulls: where the line from the eye through the pointer meets the pull's plane
[[nodiscard]] glm::vec3 TugHandOnPlane(const TugPlane& plane, glm::vec3 eye, glm::vec3 direction);

/// Whether a release at a velocity is a throw rather than a put-down (a creature's at a lower speed)
[[nodiscard]] bool IsThrow(glm::vec3 velocity, bool byCreature);
/// Whether a pot let go at a velocity is poured out where it is: not faster than the pour speed (a speed that is not a
/// number pours)
[[nodiscard]] bool PotPours(glm::vec3 velocity);
/// Whether an object let go at a velocity puts its resource down where it is: a pot of any class, poured
[[nodiscard]] bool PotDissolvesOnRelease(bool isPot, glm::vec3 velocity);
/// What happens to a put-down that stands
enum class LandedOutcome : uint8_t
{
	/// It leaves the physics at once, its kind's landing run (people, animals, fences, trees planted again)
	LeavesPhysics,
	/// It stays in the physics, settling where it is put
	Settles,
	/// A tree held tilted or not to be planted again: it stays in the physics and falls, not counted as put down
	Falls,
};
struct LandedThing
{
	bool living {false};
	bool fence {false};
	bool tree {false};
	bool burning {false};
	bool onLand {false};
	/// Its tilt about its two level axes as the hand held it
	float tiltX {0.0f};
	float tiltZ {0.0f};
	/// The hand was made to let go, or the thing mustn't be planted again
	bool dontReplant {false};
	/// A creature let it go: its tilt and planting aren't asked
	bool byCreature {false};
};
[[nodiscard]] LandedOutcome OutcomeOfLanding(const LandedThing& thing);

/// What starting a land grip throws up at the press's point
enum class GripStartEffect : uint8_t
{
	/// A puff of dust and the grab sound, over the land
	Dust,
	/// The ring, the splash sound and the fish scared, over the water
	Splash,
	/// Nothing at all
	None,
};
/// Over the land, dust unless a script has the widescreen on (then no sound either); over the water or off the map, a
/// splash unless the game is paused or the hand draws a held thing
[[nodiscard]] GripStartEffect GripStartEffectAt(bool land, bool wideScreen, bool paused, bool drawsHeld);

} // namespace openblack::hand_grab
