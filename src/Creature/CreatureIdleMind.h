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

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Enums.h"

/// What a creature does with itself, decided once a game turn. It works through an agenda of steps: waiting while it
/// looks about, playing an action, sitting for a while, going somewhere. Each step pulls a face for what it does or
/// feels as it starts, and again every few seconds while it lasts: idling, a random face for three seconds. The agendas
/// are built by the planner's actions; when one runs out the creature stands as it is until the planner gives it
/// another, as the game's creature does.
namespace openblack::creature_mind
{
/// The seconds between showing desires
constexpr float k_ShowDesireSeconds = 60.0f;
/// Each step pulls a face as it starts, and again this often for as long as it lasts: every four seconds and two turns
constexpr float k_FaceRepeatSeconds = 4.2f;
/// After each face it pulls, the creature's variety of faces moves on by up to this much less one
constexpr uint32_t k_FaceVarietyStep = 7;
/// Idle waits are one second and up to one more
constexpr float k_IdleWaitSeconds = 1.0f;
/// Being idle is waiting then yawning, this many times
constexpr int k_IdleRepeats = 2;
/// Sitting lasts ten seconds and up to four more
constexpr uint32_t k_SitSeconds = 10;
constexpr uint32_t k_SitExtraSeconds = 5;
/// A desire weaker than this isn't worth showing. The game shows desires when its planner picks the desire to show how
/// it is over all others; this stands in for that.
constexpr float k_MinDesireShown = 0.2f;
/// Hanging around, the creature walks this far away and up to this much further, in a random direction
constexpr float k_HangAroundDistance = 20.0f;
constexpr uint32_t k_HangAroundExtra = 21;
/// Off to sleep, the creature yawns first one time in this many; off for a poo, it shows it needs one first one time in
/// this many
constexpr uint32_t k_YawnBeforeSleepLots = 2;
constexpr uint32_t k_ShowPooLots = 3;
/// A poo and being sick each last this many seconds
constexpr float k_PooSeconds = 4.0f;
constexpr float k_PukeSeconds = 4.0f;
/// Fainted, it lies out cold this long before it gets up. The game's time is not known; this stands in for it.
constexpr float k_FaintSeconds = 10.0f;
/// Looking something over, it puts it down gently most times, out of this many, and tosses it away the rest
constexpr uint32_t k_PutDownLots = 10;
constexpr uint32_t k_TossLots = 2;
/// Throwing something about, it throws it this far away and up to this much further, in a random direction
constexpr float k_ThrowAroundDistance = 30.0f;
constexpr uint32_t k_ThrowAroundExtra = 21;
/// Going up to something to act at it, it stops this close; following something, it keeps this close
constexpr float k_ApproachDistance = 12.0f;
constexpr float k_FollowDistance = 25.0f;

/// What the creature is doing
enum class Activity : uint8_t
{
	None,
	BeIdle,
	Sit,
	ShowDesire,
	/// Walking somewhere nearby and sitting there
	HangAround,
	/// Something it was told to do
	Told,
	/// Seeing to its body: eating, drinking, sleeping, having a poo, being sick, and lying out cold
	Eat,
	Drink,
	Sleep,
	Poo,
	Puke,
	Faint,
	/// Doing things with what is nearby: picking something up to look it over, throwing it about, hurling it at
	/// something, and putting down what it holds
	Examine,
	PlayWithObject,
	Hurl,
	PutDown,
	/// Something else the planner chose to satisfy a desire: an emote, looking at or following something, running away,
	/// destroying something, greeting something
	Planned,
};
[[nodiscard]] std::string_view Name(Activity activity);

/// Something done with the creature's hands to a thing about it
struct ObjectOrder
{
	enum class Kind : uint8_t
	{
		PickUp,
		PutDown,
		Discard,
		Eat,
		/// Stroking, shaking, smelling or examining what it holds, by the animation
		Keep,
		/// Throwing what it holds at a point, or at a point as far from where the creature is as the point is from the
		/// origin
		Throw,
		ThrowNearby,
		Destroy,
		/// Pointing at a point
		PointAt,
		/// Making a portion of food from the object, as much as its drawn size eats, and picking the portion up
		PickUpPortion,
		/// Catching something flying at it, once its body is free
		Catch,
		/// Reaching out to take what the player's hand gives it, with its left hand when mirrored
		TakeFromHand,
		/// Throwing what it holds into a store at the store's own place, taking as long to fly there as it would take to
		/// fall the distance along the ground
		ThrowInStore,
		/// Kicking the object, which flies off ahead of it
		Kick,
	};
	Kind kind {Kind::PickUp};
	/// What it acts on, by its entity's number
	std::optional<uint32_t> object;
	glm::vec2 point {0.0f};
	size_t animation {0};
	/// Pointing: how high the point is, when it isn't on the ground, and how long it points, when not for its usual while
	std::optional<float> pointHeight;
	std::optional<float> seconds;
	/// Making a portion: what it is, by its row of the pot table
	PotInfo portion {PotInfo::WheatInHand};
	/// Taking from the player's hand with its left hand
	bool mirrored {false};
};

/// Going somewhere, or turning to face something
struct Movement
{
	enum class Kind : uint8_t
	{
		/// To a point, or to a point as far from where the creature is as the point is from the origin
		ToPoint,
		Nearby,
		/// Up to an object, or following it until the step's time is up
		ToObject,
		Follow,
		/// Running away from a point
		FleeFrom,
		TurnToFace,
		/// Going near an object, keeping the step's distance from it; getting away from it to that distance; turning
		/// to face it and holding still a moment, the step's seconds. Each goes on until the creature's movement says
		/// it is done or has failed.
		GoNearObject,
		GetAwayFromObject,
		TurnToFaceObject,
		/// Turning to face a point as a script asks: within an eighth of a half turn of facing it, or right on top of it,
		/// it counts as facing it already and doesn't turn
		FacePoint,
		/// Turning to face the lowest of the ground around it, done once its body is free again: the turn over and no
		/// action playing. Standing on top of it, it doesn't turn
		FaceDownSlope,
		/// Going to where it can throw into an object: nearer than twice its height, the step's distance and the object's
		/// radius together it backs away, further than three and a half times its height it walks up; the step's distance
		/// is the creature's height. Done or failed by what the creature's movement says, as going near an object is
		ToThrowPosition,
	};
	Kind kind {Kind::ToPoint};
	glm::vec2 point {0.0f};
	/// The object walked up to or followed, by its entity's number
	std::optional<uint32_t> object;
	bool run {false};
	/// Arriving anywhere from min to max from the point, or keeping within max of what it follows
	float minDistance {0.0f};
	float maxDistance {0.0f};
};

/// A miracle cast at a thing or at a place, by its magic type's number
struct CastOrder
{
	uint32_t magicType {0};
	/// What it is cast at, by its entity's number
	std::optional<uint32_t> object;
	/// The place that it is cast at, when the miracle goes to a place and not to a thing
	std::optional<glm::vec3> place;
};

/// One stage of a creature's dance with the villagers of a town
struct DanceOrder
{
	enum class Kind : uint8_t
	{
		/// Make a new dance of a row of the dance table at a point. The creature's last dance goes.
		Set,
		/// Call the free grown up people of a villager's town to the dance, at least the step's count of them
		Summon,
		/// Stop the head that looks about. No later step of the agenda turns the head.
		StopLooking,
		/// Send each called villager to its place in the dance, at a run
		SendToPlaces,
		/// Wait until none of the dancers moves
		WaitUntilStill,
		/// Start the dance from the start of its clock
		Start,
		/// Tell each called villager to dance and worship. The step fails when its villager is gone.
		TellToDance,
		/// Join the dance and dance in it for the step's count of turns, then stop the dance
		Dance,
		/// Stop the dance, at the end of the last step
		Stop,
		/// Remove the creature's last dance, the turn before it makes a new one
		Clear,
	};
	Kind kind {Kind::Set};
	/// Where the dance is, and its row of the dance table
	glm::vec2 point {0.0f};
	uint32_t type {0};
	/// The villager the dance is for, by its entity's number
	std::optional<uint32_t> villager;
	/// The fewest villagers to call, or the turns to dance
	uint32_t count {0};
};

/// What a step does to the creature's body
enum class Effect : uint8_t
{
	None,
	/// Has eaten what it held
	Eat,
	Drink,
	Poo,
	Puke,
	/// Wakes up rested
	Slept,
	/// Comes round from a faint
	CameRound,
	/// Has looked something over, thrown something about, and hurled something
	Examined,
	ThrewAbout,
	Hurled,
	/// The action it is carrying out counts as done here, before the steps that finish it
	Completed,
	/// Has taken what the player's hand gave it (the step's object), and decides what to do with it
	TookFromHand,
	/// Has looked another creature (the effect's object) over, and waits a while before it does again
	Inspected,
};

/// What a creature's head is made to look at: a thing (its top, or its foot), the player's hand, a point, the player's
/// camera, or the ground at its feet. When it gives nothing this turn, or names nothing, the head looks about, as for a step of
/// the game's with no look of its own
struct Gaze
{
	std::optional<uint32_t> object;
	bool bottom {false};
	/// The thing itself where it is, as up in the air, rather than its top or foot
	bool itself {false};
	/// The player's hand nearest the creature
	bool hand {false};
	/// The player: the hand when it moved much lately, else the camera
	bool player {false};
	std::optional<glm::vec3> point;
	bool camera {false};
	/// On the way to the point, the look of the walk there (creature_mind::LookWhileGoing) in place of the point
	bool goingTowards {false};
	/// The ground at its feet: a few metres ahead of the way its body faces (FeetLookPoint)
	bool feet {false};
};

/// Looking at its feet, a creature looks at the ground this many metres ahead of the way its body faces
inline constexpr float k_FeetLookMetres = 4.0f;
/// Where a creature at `from` whose body faces `bodyYaw` (a body's yaw) looks when it looks at its feet: k_FeetLookMetres
/// along the game angle of the way it faces, each axis added to its position in metres and stored as a map position
/// again (truncated), as the original's utilities step a position. The head looks at the ground there
[[nodiscard]] glm::vec2 FeetLookPoint(glm::vec2 from, float bodyYaw);

/// How the eyes should look
enum class Eyes : uint8_t
{
	Unchanged,
	/// Drooping and slowly blinking, as if just woken up
	Sleepy,
	Normal,
	Closed,
	/// Wide open, as when frightened
	Wide,
	/// Wide open and darting about
	Stoned,
};

struct Step
{
	enum class Kind : uint8_t
	{
		/// Waiting while looking about
		Wait,
		/// An action played once
		Action,
		/// A start, a loop for a while and an end, such as sitting down, sitting while looking about, and getting up
		Static,
		/// Going somewhere until it arrives or gives up, or for the step's seconds when they are more than 0
		Move,
		/// Doing something with a thing until it is done; if it can't, the rest of the agenda is given up
		Object,
		/// Casting a miracle: the start of the casting pose, then the miracle as its loop begins, held for the step's
		/// seconds counted in whole turns, then let go of as the end plays; if it can't be cast, the rest of the agenda
		/// is given up
		Cast,
		/// Drawing a gesture in the air with its hand, by the gesture's number in the step's animation, until the
		/// creature's movement says it is done or has failed
		Gesture,
		/// Waiting for the step's object to be back on the map (out of any hand, flight or tornado); if the object is
		/// gone, the rest of the agenda is given up
		WaitInMap,
		/// Turning to face the step's movement point, as a script's focus has it: no turn when it stands right on the
		/// point or already faces it to within an eighth of a right angle each way; done once the turn is over. It
		/// pulls no face
		TurnToFacePoint,
		/// Looking at the camera: turning to it each turn it is more than a right angle off, until five whole seconds
		/// (the step's seconds in whole seconds, when it gives some) are past, or the camera is farther than a hundred
		/// metres or right on it. A creature without a player is done
		/// at once. With the step's turns set, as in a cut scene, it watches for more than that many turns whatever the
		/// camera does, turning to it unless it is right on it
		LookAtCamera,
		/// Letting go of the thing the plan acts on, done as it starts. It pulls no face
		ClearObjectToActOn,
		/// Letting go of the thing the plan uses, done as it starts: the creature itself is used from then on. It pulls
		/// no face
		ClearObjectToUse,
		/// Waiting until the body is free: it starts even while the body is busy, and is done on the turn after the
		/// first turn the body is seen free, its start's own turn included. It pulls no face
		WaitUntilFree,
		/// Playing the step's animation on top of the body, as talking: it starts unless something already plays there,
		/// and the step is done once nothing plays on top of the body
		Overlay,
		/// The creature heals itself at once: its life becomes full, and its wounds and blood age as if much time went by
		HealItself,
		/// Pulling a face, by the face's animation, held for the step's seconds. It is done at once, and the next step
		/// starts on the next turn.
		Face,
		/// Following the point of focus of its player's camera, for the step's seconds counted in whole turns, until it
		/// is within the movement's distance of the point (see creature_emote::FollowCameraTurn)
		FollowCamera,
		/// Watching the player's hand: each turn, from the one it starts on, it turns to face the hand when it is more
		/// than a quarter turn away (unless right under it), and once it isn't turning it is done when the whole
		/// seconds it has watched come to more than those of the step's turns, at most five; a creature without a
		/// player is done at once
		WatchHand,
		/// The step that follows the player's hand. From the next turn, for the step's seconds in whole turns, the creature
		/// walks to the hand when the hand is further than its radius and a little more (see
		/// creature_player_attention::FollowHandTurn). The step is done when the creature is near the hand or the time is
		/// up. If the hand is off the map, the creature stops the rest of the agenda.
		FollowHand,
		/// The step that watches the player while the creature waits for orders after a leash order to a place. The
		/// creature turns to the hand or to the camera (see creature_player_attention::WatchPlayerTurn). The step is done
		/// when the creature does not wait for orders.
		WatchPlayer,
		/// Stopping where it is, done once its body plays no action. It starts even while the body moves.
		StopMoving,
		/// A stage of a dance with the villagers of a town (see DanceOrder)
		Dance,
		/// Dancing alone: as it starts, once its body plays no action, the creature starts a dance with moves of one
		/// second each. The step lasts for the step's seconds counted in whole turns. The dance stops when the step
		/// ends, or when the creature does something else.
		PracticeDance,
		/// Raising or lowering the step's object, a town's totem: from the next turn, each turn the share of the town's
		/// people at worship moves by a step, until the totem is all the way up or down. If the totem has no town that
		/// can worship, the rest of the agenda is given up.
		RaiseTotem,
		LowerTotem,
		/// Making the step's object, a villager, a breeder at once
		MakeDisciple,
		/// Kissing: once its body plays no action, it plays the step's animation once, with whatever its hand holds
		Kiss,
		/// The creature's own fire stops at once, as it cools to the temperature of the air; done as it starts. It pulls
		/// no face
		CoolDown,
		/// Looking another creature (the step's object) over: as it starts it picks a time of the step's seconds and up
		/// to the step's extra seconds more, counted in whole turns from the next, after which it has looked it over
		Inspect,
		/// Watching a thing flying through the air (the step's object): each turn, from the one it starts on, it goes on
		/// while the thing is still really in the air and the whole seconds it has watched come under a limit picked
		/// afresh each turn, from two seconds to two seconds more: not in the first three seconds, half the time each
		/// turn in the fourth, never after
		WatchFlying,
		/// Asking another creature (the step's object) to be its partner in a joint action, for the step's request.
		/// When the other creature agrees, the two are partners and the step is done at once. When it refuses, the rest
		/// of the agenda is given up.
		RequestPartner,
		/// Telling the partner (the step's object) to do the step's order at once. The partner stops what it does, and
		/// does the order and then waits, watching this creature. The step is done at once.
		Order,
		/// Making the partner (the step's object) stand still in its mind, or letting its mind go free again, at once
		Demobilise,
		Remobilise,
		/// Waiting while the step's object moves. The step is done on a turn when the object does not move.
		WaitForStill,
		/// The step's effect occurs at once, with no animation, as the drink or the poo after the animation of it
		Apply,
		/// Making the step's desire fully strong at once and planning it on the step's object, a town. The current
		/// action stops as a failure. When no plan is found, the creature decides again.
		ApplyDesire,
		/// Swapping minds with another creature: every spell on it is brought to its end until none is on it or on the
		/// other creature; then it plays the step's animation (not while its body plays another), starting the beam
		/// between them, and turn by turn it takes on the shape the swap gives it until it has
		SwapMind,
		/// Breaking the step's object, a rock, in two at once; when it isn't a rock, the rest of the agenda is given up
		Shatter,
		/// Mending the step's object, a building, by a fifth of its life at once
		Repair,
		/// Doing away with the step's object at once, as with the tree used up mending a building
		UseUp,
		/// Fighting another creature (the step's object): as it starts, the fight between the two is started, and when
		/// it can't start the rest of the agenda is given up. The fight takes the two to their places, and the mind
		/// leaves the body alone while it lasts; the step is done on the first turn the mind thinks again.
		Fight,
	};
	Kind kind {Kind::Wait};
	float seconds {0.0f};
	/// Up to how many seconds more an inspect step may last, picked as it starts
	float extraSeconds {0.0f};
	/// Watching the camera for a set number of turns, rather than until the camera goes or the step's seconds are up
	std::optional<uint32_t> turns;
	size_t animation {0};
	/// Played with drooping, slowly blinking eyes, as if just woken up
	bool sleepyEyes {false};
	Movement movement {};
	/// A static step's start, loop and end
	std::array<size_t, 3> sequence {};
	/// A static step's loop lasts until the creature is rested, rather than for the step's seconds
	bool untilRested {false};
	/// A static step's loop lasts until the creature is better (see k_FewestRestTurns). The body heals and rests during
	/// the loop, and its fire stops each turn.
	bool untilBetter {false};
	/// A static step's loop holds the last frame of its start, as lying out cold
	bool holdLoop {false};
	/// A static step plays left to right half the time, chosen as it starts
	bool mirrorAtRandom {false};
	/// Played with the eyes closed
	bool closedEyes {false};
	/// What the step does to the body: when an action is done (eating, as it starts), or a static step's loop ends
	Effect effect {Effect::None};
	/// The object eaten or waited for, by its entity's number
	std::optional<uint32_t> object;
	/// What an object step does
	ObjectOrder order {};
	/// What a cast step casts
	CastOrder cast {};
	/// The face pulled as the step starts
	creature_face::Cue face {creature_face::Cue::None};
	/// How an action's eyes look while it plays: set as it starts, and back to normal as it ends
	Eyes heldEyes {Eyes::Unchanged};
	/// What its head looks at while the step lasts, in place of what the step's kind has it look at
	std::optional<Gaze> gaze;
	/// It flutters its eyelids, blinking far more often, from the step's start until it next takes up something else
	bool flutterEyelids {false};
	/// What a dance step does
	DanceOrder dance {};
	/// A static step in which the creature faints. As it starts, those near it react as to a death, and it is out cold.
	/// It gets up on the first turn that it is seen lying, with no wait, and the step then never ends: the creature
	/// stays on it, standing, until something else takes it over. The step's seconds are the wait it never has.
	bool faint {false};
	/// A faint step in which the creature lies dead: it is not got up once it lies, but lies there for the step's
	/// seconds, then gets up as a static step ends
	bool staysDown {false};
	/// The main step of the agenda: the step that does what the action is for. The player sees the action only at or
	/// after this step.
	bool main {false};
	/// What a step that asks for a partner asks the other creature to do
	creature_friendship::RequestKind request {creature_friendship::RequestKind::Play};
	/// The step that an order step tells the partner to do
	std::shared_ptr<const Step> ordered;
	/// The desire that a step that applies a desire makes strong
	creature_desires::Desire desire {creature_desires::Desire::Compassion};
};

struct IdleMind
{
	Activity activity {Activity::None};
	std::vector<Step> agenda;
	size_t step {0};
	bool stepStarted {false};
	float stepSeconds {0.0f};
	bool sitEnding {false};
	/// Seconds until the step's face is pulled again, and the count that varies the faces it pulls
	float faceSeconds {0.0f};
	uint32_t faceVariety {0};
	/// Seconds until it may show a desire again
	float showDesireSeconds {0.0f};
	/// Told to wake up, or to end whatever static step it is in
	bool wakeWanted {false};
	/// Counts the agendas planned, so that a new one can be told from the last
	uint32_t serial {0};
	/// The agenda was given up before its end, as a step it needed couldn't be done
	bool gaveUp {false};
	/// A cast step: whether its miracle has been cast, and the turns it is held for yet
	bool castDone {false};
	uint32_t castTurns {0};
	/// The turns a look at the camera has lasted
	uint32_t lookTurns {0};
	/// A wait until the body is free has seen it free
	bool bodyWasFree {false};
	/// A step that rests to get better: the turns since it started, the first turn included
	uint32_t restTurns {0};
	/// A step that follows the camera: the turns it follows for yet
	uint32_t followCameraTurns {0};
	/// A step watching the hand or a flying thing: the turns it has watched it
	uint32_t watchTurns {0};
	/// For a step that follows the hand: the number of turns that are left
	uint32_t followHandTurns {0};
	/// An inspect step: the turns it watches for yet
	uint32_t inspectTurns {0};
	/// A step changed how its eyes blink, which lasts until it takes up something else; and it has, so the eyes go back
	/// to their usual look as the next step starts
	bool eyesHeld {false};
	bool eyesToClear {false};
	/// A dance step: the turns it dances yet, and whether its dance stops next turn
	uint32_t danceTurns {0};
	bool danceEnding {false};
	/// A step that makes a dance: whether it ordered the new dance yet
	bool danceSet {false};
	/// Out cold from a faint step: set as it faints and kept, even once it is up again, until the creature has nothing
	/// left to do. Meanwhile it chooses nothing, does not faint for its needs and takes up no reaction
	bool outCold {false};
};

/// How what its hands were told to do is going
enum class HandsState : uint8_t
{
	/// Told nothing, or it was stopped
	Idle,
	Busy,
	Done,
	Failed,
};

/// How going near, getting away from or turning to face an object is going
enum class SubMove : uint8_t
{
	Running,
	Done,
	Failed,
};

/// What the mind knows this turn
struct Senses
{
	float seconds {0.0f};
	/// The game's whole turns in a second: a thousand over its milliseconds a turn, none when that is none
	uint32_t turnsPerSecond {0};
	/// Whether the body plays an action or is on the move, and whether it is in the loop of an action, such as sitting
	bool bodyBusy {false};
	bool bodyLooping {false};
	/// Whether its body plays an action, whether it moves or not
	bool bodyPlaying {false};
	/// Whether the body dances and does not play the finish of the dance yet
	bool bodyDancing {false};
	/// Whether it holds something in its hand
	bool holding {false};
	/// Whether it is on its way somewhere or turning, and where it stands
	bool moving {false};
	/// How going near, getting away from or turning to face an object, or drawing a gesture, is going, by the
	/// creature's movement
	SubMove subMove {SubMove::Running};
	glm::vec2 position {0.0f};
	/// Whether it has slept enough to wake
	bool rested {false};
	/// Whether its life is full and it is not tired at all
	bool better {false};
	HandsState hands {HandsState::Idle};
	/// Whether the object a step waits for is on the map, none when the object is gone
	std::optional<bool> objectInMap;
	/// Whether the object that a step waits to be still moves
	bool objectMoving {false};
	/// Where the player looks from on the ground, how far the camera is from the creature (through the air, not along
	/// the ground), whether the creature has a player, and the way the creature faces
	std::optional<glm::vec2> camera;
	float cameraDistance {0.0f};
	bool hasPlayer {false};
	/// The game turns its player's hand has stayed still
	uint32_t handStillTurns {0};
	float heading {0.0f};
	/// What picks the faces it pulls
	creature_face::Feelings feelings {};
	/// Whether something plays on top of the body, as talking or nodding
	bool overlayPlaying {false};
	/// The point of focus of its player's camera on the land, while a step follows the camera; none without a player
	std::optional<glm::vec3> cameraFocus;
	/// Whether that point is less than a sixth of a turn from the way its body faces, or in its own map cell
	bool cameraFocusAhead {false};
	/// Where its player's hand nearest it is on the ground, while a step watches it: where the hand last had a place,
	/// also while it has none. None for a creature without a player
	std::optional<glm::vec2> hand;
	/// The player's hand while a step follows the hand, and the player while a step watches the player
	std::optional<creature_player_attention::FollowHandView> followHand;
	std::optional<creature_player_attention::WatchPlayerView> watchPlayer;
	/// Its dance with villagers, while a dance step is next or under way
	struct DanceView
	{
		/// Whether its dance is still there, and whether a villager of the dance moved in the last turn
		bool exists {false};
		bool dancersMoving {false};
		/// Whether the creature dances in a group of the dance, and that group's move: its number in the dance files
		/// and its two arguments
		bool inGroup {false};
		uint32_t action {0};
		uint32_t first {0};
		float second {0.0f};
		/// The way the dance faces (radians), the middle of the creature's group, and its own place in the group
		float danceAngle {0.0f};
		/// How many times their keyed speed the groups of the dance move at
		float danceRate {1.0f};
		glm::vec2 groupCentre {0.0f};
		glm::vec2 place {0.0f};
		/// How far it is from its place, in metres, and the way it faces (radians, as its movement measures it)
		float placeDistance {0.0f};
		float heading {0.0f};
	};
	std::optional<DanceView> dance;
	/// The share of the town's people at worship, while a step moves the town's totem; none when the totem has no town
	/// that can worship
	std::optional<float> totemShare;
	/// While a step watches a flying thing: whether the thing is still really in the air
	bool objectInTheAir {false};
	/// A swap of minds: whether no spell is on it or on the other creature any more, and whether its body has taken on
	/// the shape the swap gives it
	struct SwapView
	{
		bool spellsGone {false};
		bool settled {false};
	};
	SwapView swap {};
};

/// What the mind tells the body
struct Commands
{
	std::optional<size_t> playOnce;
	bool mirrored {false};
	/// A start, loop and end to play, the loop held on its last frame or not, and the end of the loop playing now
	std::optional<std::array<size_t, 3>> startSequence;
	bool holdLoop {false};
	bool endSit {false};
	/// A dance to start, with the milliseconds of each move; the dance to stop; only a start, loop and end to stop
	std::optional<float> startDance;
	bool endDance {false};
	bool endStaticLoop {false};
	/// A face to pull, or to relax the face pulled
	std::optional<creature_face::Request> face;
	bool relaxFace {false};
	Eyes eyes {Eyes::Unchanged};
	/// Whether the head turns to whatever is interesting this turn
	bool lookAbout {false};
	/// What the head looks at this turn, for a step that gives it something, in place of looking about
	std::optional<Gaze> gaze;
	/// Where to go, or to stop going
	std::optional<Movement> move;
	bool stopMoving {false};
	/// What happens to the body this turn, and the object it eats
	Effect effect {Effect::None};
	std::optional<uint32_t> effectObject;
	/// What to do with a thing
	std::optional<ObjectOrder> object;
	/// A gesture to draw, by its number
	std::optional<uint32_t> gesture;
	/// An animation to play on top of the body
	std::optional<size_t> playOverlay;
	/// A miracle to cast, and whether to let go of the one it holds
	std::optional<CastOrder> cast;
	bool releaseCast {false};
	/// The creature's own fire stops
	bool coolDown {false};
	/// The creature heals itself (see Step::Kind::HealItself)
	bool healItself {false};
	/// The thing the plan acts on becomes what the creature knows of itself, or nothing when it knows nothing of itself
	bool clearObjectToActOn {false};
	/// The thing the plan uses becomes the creature itself
	bool clearObjectToUse {false};
	/// How often its eyes now blink, when that changes
	std::optional<int32_t> blinkIntervalMs;
	/// A stage of a dance with villagers to carry out this turn
	std::optional<DanceOrder> dance;
	/// The share of the town's people at worship that a totem (by its entity's number) is set to this turn
	struct TotemShare
	{
		uint32_t totem {0};
		float share {0.0f};
	};
	std::optional<TotemShare> totem;
	/// A villager to make a breeder at once, by its entity's number
	std::optional<uint32_t> makeDisciple;
	/// The creature faints this turn, so those near it react as to a death
	bool fainted {false};
	/// A creature to ask to be its partner, and what for
	struct PartnerRequest
	{
		uint32_t creature {0};
		creature_friendship::RequestKind kind {creature_friendship::RequestKind::Play};
	};
	std::optional<PartnerRequest> requestPartner;
	/// An order to give the partner: the partner, by its entity's number, and the step it is to do
	struct PartnerOrder
	{
		uint32_t partner {0};
		std::shared_ptr<const Step> step;
	};
	std::optional<PartnerOrder> order;
	/// The partner to make stand still in its mind (true) or to let go free again (false)
	std::optional<std::pair<uint32_t, bool>> demobilise;
	/// A desire to make fully strong and to plan on a thing, by its entity's number
	struct DesireOn
	{
		creature_desires::Desire desire {creature_desires::Desire::Compassion};
		uint32_t thing {0};
	};
	std::optional<DesireOn> applyDesire;
	/// A swap of minds: bring every spell on it to its end, start the beam to the other creature, take on a turn more of
	/// the shape the swap gives it
	bool finishSpells {false};
	bool startSwapBeam {false};
	bool easeSwap {false};
	/// A rock to break in two, a building to mend, and a thing to do away with, by their entities' numbers
	std::optional<uint32_t> shatter;
	std::optional<uint32_t> repair;
	std::optional<uint32_t> useUp;
	/// A fight to start with another creature, by its entity's number
	std::optional<uint32_t> fight;
};

/// random(n) is a whole number from 0 to n - 1
using Random = std::function<uint32_t(uint32_t)>;

/// Fluttering its eyelids, the creature blinks every 0.625 to 1.875 seconds
constexpr int32_t k_FlutterBlinkIntervalMs = 1250;

/// The limit of a watch of a flying thing: two seconds, and a draw of up to two more each turn
constexpr float k_WatchFlyingSeconds = 2.0f;
constexpr float k_WatchFlyingExtraSeconds = 2.0f;

/// Being idle: wait a second or two looking about, then a tired yawn with sleepy eyes, twice
[[nodiscard]] std::vector<Step> BeIdle(const Random& random);
/// Sitting for ten to fourteen seconds
[[nodiscard]] Step SitDown(const Random& random);
/// Hanging around: walking 20 to 40 units away in a random direction, then sitting there
[[nodiscard]] std::vector<Step> HangAround(const Random& random);
/// Sleeping on the spot: half the time a tired yawn first, then sleeping with its eyes closed until rested, then a dazed
/// look about with sleepy eyes
[[nodiscard]] std::vector<Step> Sleep(const Random& random);
/// The planner's sleep on the spot, as the game builds it: half the time a yawn first; then, with a clear place found
/// near it, it walks to within its 2D radius of it; without one, it walks home to within its height (at most
/// k_SleepAtHomeReach) and turns to face down the slope; then it sleeps until rested and wakes confused, grimacing. Only
/// the waking pulls a face
[[nodiscard]] std::vector<Step> SleepOnTheSpot(std::optional<glm::vec2> clearArea, float radius, std::optional<glm::vec2> home,
                                               float height, const Random& random);
/// Sleeping at home, it walks to within this of its home at most
constexpr float k_SleepAtHomeReach = 5.0f;
/// It sleeps on the spot in a clear place about it this many times as wide as it is tall
constexpr float k_SleepOnTheSpotHeights = 4.0f;
/// Picking food up, examining it and eating it
[[nodiscard]] std::vector<Step> Eat(uint32_t food);
/// Going to within the distance of a pile of food at a point, turning to it, then making a portion from it, picking that
/// up and eating it
[[nodiscard]] std::vector<Step> EatPortion(uint32_t pile, glm::vec2 point, float distance);
/// Going to within its height and two fifths of a storage pit's food pile at a point, turning to face the pile, letting
/// go of the thing the plan acts on, then making a portion of wheat from the pile, picking that up and eating it
[[nodiscard]] std::vector<Step> EatFromStoragePit(uint32_t pile, glm::vec2 point, float height);
/// Going to within its height of a field at a point, turning to face it, letting go of the thing the plan acts on, then
/// making a portion of wheat from the field, picking that up and eating it
[[nodiscard]] std::vector<Step> EatFromField(uint32_t field, glm::vec2 point, float height);
/// Going to within the distance of a tree at a point, turning to face it, then making a portion of fruit from the tree,
/// picking that up and eating it
[[nodiscard]] std::vector<Step> EatFromTree(uint32_t tree, glm::vec2 point, float distance);
/// Picking something up, stroking, shaking, smelling or examining it, then mostly putting it down and sometimes tossing it
/// away
[[nodiscard]] std::vector<Step> ExamineByPickingUp(uint32_t object, bool handFull, const Random& random);
/// Picking something up and throwing it about, 30 to 50 units away in a random direction
[[nodiscard]] std::vector<Step> ThrowAbout(uint32_t object, const Random& random);
/// Putting down what it holds
[[nodiscard]] std::vector<Step> PutDownHeld();
/// A poo on the spot, a third of the time showing it needs one first
[[nodiscard]] std::vector<Step> Poo(const Random& random);
/// Being sick on the spot
[[nodiscard]] std::vector<Step> Puke();
/// Fainting where it stands, lying out cold a while, then getting up
[[nodiscard]] std::vector<Step> Faint();
/// Eating what it holds
[[nodiscard]] std::vector<Step> EatHeld();
/// Taking a thing the player's hand gives it: turning to the player's camera, then reaching out to take it from the
/// hand, looking at it and curious all along
[[nodiscard]] std::vector<Step> TakeFromHand(uint32_t object, glm::vec2 camera, bool leftHand);
/// What it does with a thing it was given and holds: looks it over, curious; eats it; or throws it at the player's
/// camera, looking at the camera
[[nodiscard]] std::vector<Step> ExamineGiven(uint32_t object);
[[nodiscard]] std::vector<Step> EatGiven();
[[nodiscard]] std::vector<Step> ThrowGiven(glm::vec3 camera);
/// Playing an action once on the spot
[[nodiscard]] std::vector<Step> Emote(size_t animation);
/// Turning to face a point, then playing an action
[[nodiscard]] std::vector<Step> FaceAndEmote(glm::vec2 point, size_t animation);
/// Walking up to something, then playing an action at it
[[nodiscard]] std::vector<Step> ApproachAndEmote(uint32_t object, size_t animation);
/// Turning to face something and watching it for some seconds
[[nodiscard]] std::vector<Step> LookAt(glm::vec2 point, float seconds);
/// Turning to face a point, as a script's focus has it
[[nodiscard]] std::vector<Step> TurnToFacePoint(glm::vec2 point);
/// Looking at the camera for five whole seconds, curious, its head on the camera, turning to it whenever it is more than
/// a right angle off; done at once for a creature without a player
[[nodiscard]] std::vector<Step> LookAtCamera();
/// Following something about for some seconds
[[nodiscard]] std::vector<Step> FollowFor(uint32_t object, float seconds);
/// Running away from a point
[[nodiscard]] std::vector<Step> RunFrom(glm::vec2 point);
/// Walking up to something and destroying it
[[nodiscard]] std::vector<Step> DestroyThing(uint32_t object);
/// Walking right up to something, turning a tenth of a second to face it and kicking it, watching it all the while,
/// angry
[[nodiscard]] std::vector<Step> KickThing(uint32_t object);
constexpr float k_FaceKickSeconds = 0.1f;
/// Catching something flying at it, smiling
[[nodiscard]] std::vector<Step> CatchThing(uint32_t object);
/// Waiting a while, looking about
[[nodiscard]] std::vector<Step> LookAbout(float seconds);
/// A creature that rests to get better gets up only when its life is full, it is not tired at all, and the step has
/// lasted more than this many turns
constexpr uint32_t k_FewestRestTurns = 30;
/// Whether the creature is asleep, or out cold, in its current step
[[nodiscard]] bool IsAsleep(const IdleMind& mind);
/// Whether the creature rests to get better in its current step, until it starts to get up
[[nodiscard]] bool IsResting(const IdleMind& mind);
[[nodiscard]] bool IsUnconscious(const IdleMind& mind);
/// A creature out cold with nothing left to do comes to: it chooses nothing on that turn, and is no longer out cold
/// after it. Returns whether it came to this turn
[[nodiscard]] bool ComesTo(IdleMind& mind);
/// Whether the creature's sit is counting its time this turn: a static step whose loop is sitting, past the turn it
/// started on and before its end is asked for. Each such turn rests its body, from the sitting down to the getting up
[[nodiscard]] bool SitCounting(const IdleMind& mind);

/// A new agenda in place of the current one
void Plan(IdleMind& mind, Activity activity, std::vector<Step> agenda);
/// Pulls a face for a reason, if the reason calls for one, and moves the variety of faces on. Only the idle face draws
/// for its pick, before the variety's step; a reason that pulls no face draws nothing
[[nodiscard]] std::optional<creature_face::Request> PullFace(IdleMind& mind, creature_face::Cue cue, const Senses& senses,
                                                             const Random& random);
/// One game turn of the mind
/// The world carried out a dance stage that the mind ordered: the step is done, or the rest of the agenda is no use
void DanceOrderDone(IdleMind& mind, bool done);
[[nodiscard]] Commands Think(IdleMind& mind, const Senses& senses, const Random& random);
/// Whether a creature watching the hand has watched long enough: its whole seconds are more than the whole seconds of
/// the step's turns, but never more than five
[[nodiscard]] bool WatchedHandEnough(uint32_t turns, uint32_t limitTurns, uint32_t turnsPerSecond);
/// The longest a creature watches the hand, in whole seconds
constexpr float k_LongestHandWatchSeconds = 5.0f;
/// Each turn of watching the hand takes one draw of this range from the game's numbers, whose value it does not use
constexpr uint32_t k_HandWatchUnusedDraw = 30;
/// Whether a creature goes on watching a flying thing: while the thing is still flying and the whole seconds of the
/// turns it has watched come under the limit
[[nodiscard]] bool KeepsWatchingFlying(uint32_t turns, uint32_t turnsPerSecond, float limit, bool flying);
} // namespace openblack::creature_mind
