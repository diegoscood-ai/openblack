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
#include <string_view>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"
#include "Enums.h"

/// The rules of the god hand taking hold of things, carrying them and letting them go: what it may pick up and how long
/// it waits to, how each kind of thing hangs in it, the spring that drags the hand along with what it holds, and whether
/// a release is a throw, a put-down or a pour. Pure functions, tested on their own; the hand's system applies them.
namespace openblack::hand_grab
{

/// Scooping, the hand hovers this far above the top of what it scoops from
inline constexpr float k_ScoopHoverAbove = 3.0f;
/// Scooping, the hand is tipped down this far towards what it scoops from, in radians (78.75 degrees)
inline constexpr float k_ScoopTip = 1.3744469f;
/// Below this height the hand tips further down while scooping
inline constexpr float k_ScoopTipLowHeight = 2.5f;
/// The hand's length, which a scoop low over the land tips it further by
inline constexpr float k_HandLength = 3.2f;
/// How far a hand scooping at a height is tipped down: further when it is low, by the angle of its drop below the
/// low height over the hand's own length
[[nodiscard]] float ScoopTip(float height, float handLength);

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
/// A release faster than this (squared) is laid along the slope only when it is a tree
inline constexpr float k_FastReleaseSpeedSquared = 1.0f;

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
/// People, animals and fences put down on ground steeper than this (the up of the land's normal) slide off instead
inline constexpr float k_LeastLandingNormalY = 0.7f;
/// A put-down stands where it is over dry land, or where the nearest cell stands higher than this
inline constexpr uint8_t k_LowestLandingAltitude = 1;

/// The kinds of thing as the hand treats them
enum class GrabKind : uint8_t
{
	/// Nothing the hand may take
	None,
	Villager,
	Animal,
	/// A rock, a heavy static or a dead tree: refused only when a rock wider than the hand can lift
	Rock,
	/// Any other static: toys, fences, totems, stones
	MobileStatic,
	/// Loose things: pots, balls, mushrooms, logs
	MobileObject,
	/// The one loose thing held on the palm
	Poo,
	Tree,
	DeadTree,
};

/// What deciding whether the hand may take a thing needs to know of it
struct Holdable
{
	GrabKind kind {GrabKind::None};
	/// It is still in the world, not being taken away
	bool available {true};
	/// A villager inside its home
	bool atHome {false};
	/// Held by a hand already
	bool inHand {false};
	/// A villager on its way to hide in a building
	bool hiding {false};
	/// An animal whose kind the player may pick up
	bool speciesAllows {false};
	/// Its radius across the ground, at its scale
	float radius {0.0f};
	/// A rock (not another heavy static): its size limits it
	bool isRock {false};
};

/// Whether a thing of its kind says the hand may hold it
[[nodiscard]] bool ValidForPlaceInHand(const Holdable& thing);

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

/// The checks a press goes through, in the game's order, before the hand takes a thing
struct Gate
{
	bool spaceInHand {true};
	bool alreadyInHand {false};
	bool valid {false};
	/// A script said it can't be picked up
	bool cannotBePickedUp {false};
	/// Something else carries it (a tornado)
	bool carried {false};
	/// The hand is in its player's influence (or the thing doesn't need it to be)
	bool inInfluence {true};
};
/// Whether a press on a thing takes it; otherwise the press is a tap
[[nodiscard]] bool PassesGate(const Gate& gate);
/// Why a press on a thing doesn't take it, in a few words; empty when it does
[[nodiscard]] std::string_view GateRefusal(const Gate& gate);

/// How long a press has been held: by the clock, but no more than the game turns gone by allow
[[nodiscard]] uint32_t ElapsedMs(uint32_t nowMs, uint32_t pressMs, uint32_t turn, uint32_t pressTurn);

/// How a kind of thing hangs in the hand
struct HoldFacts
{
	HoldType type {HoldType::Above};
	/// Its height times this is how far its model hangs below the hand
	float loweringMultiplier {0.0f};
	/// How far it spreads out of the hand, which opens the hand's pose
	float holdRadius {0.0f};
};
/// How a thing hangs: from its kind, its static's row and model, and its size at its scale
[[nodiscard]] HoldFacts HoldOf(GrabKind kind, MobileStaticInfo staticType, MeshId mesh, float height, float radius);

/// How far a thing hangs below the hand as the hand takes hold of it to pull it free: its own hang, but no closer than a
/// share of the hand's height
[[nodiscard]] float HoldDistance(float loweringMultiplier, float height, float handSize);
/// How far the hand rises for what it holds: a little for the palm, else its hang (its lowering times its height as the
/// hand last measured it) at the least; a standing tree a share of its height more
[[nodiscard]] float HandRise(HoldType hold, float lowering, float handSize, std::optional<float> rootedHeight);
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
/// Whether a release is fast enough that only a tree is laid along the slope as it leaves the hand
[[nodiscard]] bool IsFastRelease(glm::vec3 velocity);
/// Whether a pot let go at a velocity is poured out where it is: not faster than the pour speed (a speed that is not a
/// number pours)
[[nodiscard]] bool PotPours(glm::vec3 velocity);
/// Whether an object let go at a velocity puts its resource down where it is: a pot of any class, poured
[[nodiscard]] bool PotDissolvesOnRelease(bool isPot, glm::vec3 velocity);
/// The twist a throw gets from how far the hand moved since letting go, for a body of a mass at a speed: a turning force
/// about the level axis across the hand's motion
[[nodiscard]] glm::vec3 ReleaseSpinTorque(float mass, float speed, glm::vec3 handMoved);

/// What deciding whether a put-down stands where it is needs
struct Landing
{
	bool thrown {false};
	/// It had to be raised over something under it
	bool raised {false};
	/// A villager let go by a computer player stands even when raised
	bool computerVillager {false};
	bool dryLand {false};
	/// The altitude of the cell nearest it, none off the map
	std::optional<uint8_t> nearestAltitude;
	/// A person, an animal or a fence, which needs ground that isn't too steep
	bool needsGentleSlope {false};
	/// The up of the land's normal under it
	float normalY {1.0f};
};
/// Whether a release stands where it is put down rather than flying
[[nodiscard]] bool LandsOnRelease(const Landing& landing);

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

/// What a handful's table says of scooping it up
struct ScoopFacts
{
	/// The first scoop, and what each game turn scoops as the scoop ramps up from the one to the other
	uint32_t initial {0};
	uint32_t perTurn {0};
	uint32_t perTurnEnd {0};
	/// The most a handful can hold, nothing for no limit
	uint32_t maxPickedUp {0};
	/// How long the scoop takes to ramp up, in seconds
	float rampSeconds {0.0f};
};
/// How far a scoop has ramped up after some game turns, 0 to 1: the share of its ramp gone, squared
[[nodiscard]] float ScoopRamp(uint32_t turns, const ScoopFacts& facts);
/// What a game turn of scooping takes, after some turns of it
[[nodiscard]] uint32_t ScoopAmount(uint32_t turns, const ScoopFacts& facts);
/// What a scoop takes, no more than the source has nor than the handful has room for
[[nodiscard]] uint32_t ScoopTaken(uint32_t wanted, uint32_t sourceHas, uint32_t held, const ScoopFacts& facts);

} // namespace openblack::hand_grab
