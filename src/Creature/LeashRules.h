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
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDesires.h"
#include "Creature/LeashRope.h"
#include "Enums.h"

/// How the player leads a creature on a leash. There are three leashes: the learning leash (a plain rope), which makes
/// the creature watch and copy the player more keenly, and the aggression and compassion leashes, which make it angry or
/// kind for as long as it wears them. Held in the hand, the leash is as long as the creature is big; tied to something,
/// it is as long as the creature is far from it. Strayed farther than that from the hand, or from what it is tied to, the
/// creature stops what it was doing and walks back; pulled away from the same thing twice, it goes off wanting it for a
/// while.
namespace openblack::creature_leash
{

/// The leashes in the order the citadel hangs them and the creature knows them
constexpr std::array<LeashType, 3> k_Types = {LeashType::Evil, LeashType::Rope, LeashType::Good};
/// A leash's place in k_Types, or none for no leash
[[nodiscard]] std::optional<size_t> IndexOf(LeashType type);
[[nodiscard]] const char* Name(LeashType type);

/// The rope's length at rest and pulled fully taut
struct Lengths
{
	float slack;
	float max;
};
/// Held in the hand, the leash is as long as the creature is big: with s fifteen times its size, slack is 0.7 s + 22
/// and full length 3 s + 32
[[nodiscard]] Lengths InHand(float creatureSize);
/// Tied to a tree, the leash is six times the creature's height, at most 40, half of that at rest
[[nodiscard]] Lengths TiedToTree(float creatureHeight);
/// Tied to anything but a tree (another creature, a villager, a village, a post), the leash reaches one and a half
/// times as far as the creature is from it, between 180 and 360, half of that at rest. Its lengths are taken when it is
/// tied and stay while it is tied, however the creature grows.
[[nodiscard]] Lengths TiedToObject(float distance);

/// Where the leash meets a creature with no collar bone, as a share of its height
constexpr float k_CollarHeightShare = 0.7f;
/// Where the leash meets the creature, on its body as it is drawn this frame (`drawn`, the matrix the body is drawn
/// with: between its turns, and smaller in its temple's pen): its collar bone posed by `bones`, or, with no such bone,
/// high on the drawn body, `height` being its own height and `sizeShare` how much of its size it is drawn at
[[nodiscard]] glm::vec3 CollarAt(const glm::mat4& drawn, std::span<const glm::mat4> bones, std::optional<uint32_t> bone,
                                 float height, float sizeShare);

/// Where a leash held in the hand meets the hand: its wrist, the hand model's first bone as the hand is drawn this
/// frame (`hand`, the hand's matrix; `bones`, its posed bones), not the fingertip the hand is placed by. None with no
/// posed bones
[[nodiscard]] std::optional<glm::vec3> WristAt(const glm::mat4& hand, std::span<const glm::mat4> bones);

/// The rope's look for each leash: which band of the leash texture it shows, and its width. The compassion leash is
/// thicker.
[[nodiscard]] leash_rope::Look LookFor(LeashType type);

/// What a leash makes the creature feel while it wears one: anger on the aggression leash and compassion on the
/// compassion leash, made dominant over its other desires as it is put on and again every turn; nothing on the learning
/// leash, which instead lets go of any desire made dominant as it is put on
[[nodiscard]] std::optional<creature_desires::Desire> ForcedDesireFor(LeashType type);
/// A leash makes its desire dominant for this many seconds, again every turn, so it lasts as long as the leash is on
constexpr float k_LeashDesireSeconds = 36000.0f;
/// Tied to a village's centre on any leash but aggression, the creature wants to impress the village for this many
/// seconds: once as it is tied, then on every turn the village does not believe in its player enough
constexpr float k_ImpressTownSeconds = 120.0f;
/// What the village a creature is tied to thinks of the creature's player
struct TiedTown
{
	/// How much it believes in the creature's player, and in the other player it believes in most
	float beliefInPlayer;
	float mostInAnother;
	/// Whether the village is the creature's player's
	bool playersOwn;
};
/// Whether the creature wants to impress the village this turn: when the village believes in its player no more than
/// half as much as in the other player it believes in most, or the village is not its player's
[[nodiscard]] bool WantsToImpress(const TiedTown& town);

/// Each sighting of a miracle counts once, three times while the creature wears the learning leash
[[nodiscard]] uint32_t MiracleSightingWeight(bool learningLeash);

/// The plan a leash makes the creature carry out, about itself, for its desire to obey the player, by its row in the
/// game's action table: sent walking by the leash or its home, it walks to the point
constexpr uint32_t k_WalkToPointAction = 1;

/// Free of its home only within 140 of it, and only if its player has a temple
constexpr float k_HomeRange = 140.0f;
[[nodiscard]] bool FreeOfHome(float distanceFromHome, bool playerHasTemple);
/// Kept at home while it starts to grow up, within 12 of it, as the game's debug function sets it
constexpr float k_HomeConfinement = 12.0f;

/// Young, the creature is kept within 10 of its home, until it reaches this development phase, and only on this land
constexpr float k_YoungHomeRadius = 10.0f;
constexpr uint32_t k_YoungUntilPhase = 5;
constexpr int32_t k_YoungHomeLand = 1;
/// What decides, each turn, whether a young creature is kept at its home
struct HomeKeeping
{
	/// It wears a leash
	bool leashed {false};
	uint32_t developmentPhase {0};
	/// It belongs to the player at this machine
	bool localPlayers {false};
	/// Its player is a computer player. Open point: openblack keeps no player kind yet (the script's computer-player
	/// toggle only makes the player's entity), so this is never set
	bool computerPlayer {false};
	/// A multiplayer game, which openblack doesn't have
	bool multiplayer {false};
	/// The land's number, as the map script sets it
	int32_t landNumber {0};
};
/// A creature on no leash, younger than phase 5, of the local player, who is not a computer player, in a game that is
/// not multiplayer, on the first land, is kept within 10 of its home. Nothing else is changed when it isn't: the area it
/// was kept within stays.
[[nodiscard]] bool KeptAtHome(const HomeKeeping& keeping);
/// Moved to a stage of growing up, a creature kept within an area, on no leash, is let go once the stage is one it may
/// leave home from (phase 5 on). Only the area's radius goes: whether it was walking back stays as it was
[[nodiscard]] bool ReleasedFromHome(uint32_t developmentPhase, float radius, bool leashed);
/// The leash a player has in use: the learning leash at the start, then the one last put on or picked, kept while no
/// leash is on
constexpr LeashType k_LeashInUseAtStart = LeashType::Rope;
/// A script letting go of a creature's only desire frees the desires it held down only for a creature with no player,
/// or whose player's leash in use is the learning leash, whether it is on or not
[[nodiscard]] bool OnlyDesireLetsGo(bool hasPlayer, LeashType leashInUse);
/// Kept within an area: a radius is set, unless it is on a leash that doesn't work
[[nodiscard]] bool IsConfined(float radius, bool leashed, bool leashWorks);

/// The area a creature is kept within is one rule for the leash in the hand, the tied leash and the young creature's
/// home. Each turn the point and the radius are taken: the hand's place on the ground, or what the leash is tied to, and
/// the leash's full length; or its home and 10. Strayed farther than the radius, it stops what it was doing and walks
/// back, a little faster the farther it strayed, until it is within its height or the radius of the point, whichever
/// is more.
struct WalkBackCheck
{
	/// Kept within an area (IsConfined)
	bool confined {false};
	/// Busy with something that keeps it where it is: clearing what is in its way, doing what the leash sent it to do,
	/// being teleported, or sent somewhere by its player. Open point: openblack keeps none of these on a creature yet,
	/// so they are never set
	bool clearingWay {false};
	bool sentByLeash {false};
	bool teleporting {false};
	bool sentToPoint {false};
	/// A script controls it
	bool controlledByScript {false};
	/// The point is on the map, and in a cell of land the creature stands on (neither water nor too steep)
	bool pointOnMap {false};
	bool pointOnLand {false};
	/// How far it is from the point, and how near it is kept
	float distance {0.0f};
	float radius {0.0f};
	/// Already walking back, and how far where it walks to is from the point now
	bool walkingBack {false};
	float walkingToFromPoint {0.0f};
};
/// Whether it sets off walking back this turn: kept within an area, busy with nothing that keeps it, farther than the
/// radius from a point on the map's land, and not already walking back to a place near enough the point
[[nodiscard]] bool ShouldWalkBack(const WalkBackCheck& check);
/// Walking back, it sets off again only once where it walks to is farther than this from the point: half the radius,
/// at least 5
constexpr float k_WalkBackRestartFloor = 5.0f;
[[nodiscard]] float WalkBackRestartDistance(float radius);
/// How much faster than walking it goes back: 0.8 of its top speed times how far it strayed over twice the radius, at
/// most all of it
constexpr float k_WalkBackHurry = 0.8f;
[[nodiscard]] float WalkBackHurry(float distance, float radius);
/// It is back within its height or the radius of the point, whichever is more
[[nodiscard]] float WalkBackArrival(float height, float radius);

/// Holding something on the aggression leash, or sent where it can't stand, a creature throws it at the point from
/// eight times its height away
constexpr float k_ThrowFromHeights = 8.0f;
/// Its walk there allows it three times its height more, at least a hundredth beyond where it throws from
constexpr float k_ThrowWalkReachHeights = 3.0f;
constexpr float k_ThrowWalkSpare = 0.01f;
/// Where it walks to before it throws, and from how near to how far from that place it may stop
struct ThrowWalk
{
	glm::vec2 to {0.0f};
	float minDistance {0.0f};
	float maxDistance {0.0f};
};
/// The walk before the throw: nearer than where it throws from, towards the place that far from the point on its own
/// side; further than that, towards the point itself; none when it is that far already. Open point: standing right on
/// the point, the side is undefined (the original divides by the distance, none), and it throws from where it stands
[[nodiscard]] std::optional<ThrowWalk> WalkToThrowAt(glm::vec2 creature, glm::vec2 point, float height);

/// What a creature that the leash sends to a point of the land does there, in the order the game asks
enum class PointOrder : uint8_t
{
	/// It takes no notice
	Refused,
	/// Holding something, it carries it to the point and puts it down there
	CarryAndPutDown,
	/// Holding something on the aggression leash, or with the point where it can't stand, it goes towards the point
	/// with it in another way
	CarryOtherwise,
	/// Nowhere near the point can it stand: it says it can't get there
	CantGetThere,
	/// On water, thirsty, it drinks from the sea there
	DrinkFromSea,
	/// On water, not thirsty, it looks at its reflection there
	LookAtReflection,
	/// A field at the point: it is sent to act on the field
	ActOnField,
	/// Tired, near its home, it goes to sleep
	Sleep,
	/// Anywhere else, it walks there
	WalkThere,
};
/// What decides it
struct PointOrderCheck
{
	/// Its mind thinks, a script doesn't control it, its leash works and the hand doesn't hold it locked
	bool mindActive {true};
	bool controlledByScript {false};
	bool leashWorks {true};
	bool lockedSelect {false};
	float life {1.0f};
	float exhaustion {0.0f};
	/// Something is in its hand, and its leash is the aggression leash
	bool holding {false};
	bool evilLeash {false};
	/// It can stand at the point, or at a place found round it
	bool pointValid {true};
	bool foundValid {true};
	/// Fighting, or busy in another way that keeps it. Open point: what the second stands for is not known, and
	/// openblack never sets it
	bool fighting {false};
	bool busy {false};
	/// The point is in the sea
	bool water {false};
	/// It wants water, and how thirsty it is
	bool wantsWater {false};
	float thirst {0.0f};
	/// A field lies at the point
	bool field {false};
	/// Its player has a temple, how far its home is from the point, and whether its plan is to sleep already
	bool hasTemple {false};
	std::optional<float> homeToPoint;
	bool planIsSleep {false};
};
[[nodiscard]] PointOrder DecidePointOrder(const PointOrderCheck& check);
/// Whether it acknowledges the order with a sound: every order but a refusal and a point it can't get to
[[nodiscard]] bool AcknowledgesOrder(PointOrder order);
/// The in-game bank's sample its player hears, not placed in the world, as the order is acknowledged
constexpr int k_OrderAcknowledgedSample = 1;
/// Where it is sent, the creature target's mark is shown for 20 turns, at its usual strength
constexpr int k_TargetMarkerTurns = 20;
constexpr float k_TargetMarkerMagnitude = 1.0f;
/// The creature target's point is 2 m above the place, and above the top of a thing
constexpr float k_TargetAbove = 2.0f;
/// The point above a target: 2 m up from the place, then the thing's height on top (0 for a point of the land)
[[nodiscard]] constexpr glm::vec3 TargetAbove(const glm::vec3& place, float height)
{
	return {place.x, (place.y + k_TargetAbove) + height, place.z};
}
/// It sleeps when tapped within 12 of its home while its exhaustion is above 0.1
constexpr float k_SleepOrderReach = 12.0f;
constexpr float k_SleepOrderExhaustion = 0.1f;
/// The place it can stand nearest the point is looked for in rings half a metre apart, up to just under a thousand
/// metres away
constexpr float k_PointOrderSearchReach = 999.5f;
/// It walks to within its height of the point, at most 8
constexpr float k_PointOrderArrivalCap = 8.0f;
[[nodiscard]] float PointOrderArrival(float height);
/// Sent to a point farther than 40 away, it tells the player that it wants to obey
constexpr float k_PointOrderTellBeyond = 40.0f;
/// Sent to a point again while it obeys, it runs there and then waits: 2 seconds and a draw of up to 3 more, in whole
/// turns, the draw made as it is sent
constexpr float k_SecondOrderWaitDraw = 3.0f;
constexpr float k_SecondOrderWaitSeconds = 2.0f;
/// The turns of that wait for the draw: the turns a second (a whole division, none at 0 ms) times the seconds, cut
/// towards 0
[[nodiscard]] int32_t SecondOrderWaitTurns(float drawn, uint32_t msPerTurn);
/// A turn of the wait counted: the turns left, and whether it is over (none left)
struct WaitStep
{
	int32_t left {0};
	bool over {false};
};
[[nodiscard]] WaitStep CountWait(int32_t turns);

/// Leashed to another creature on the aggression leash, the other gets angry too within eight times the leashed
/// creature's height
constexpr float k_AngerOtherReach = 8.0f;
/// Leashed to another creature, how nice each finds the other steps down by 0.1 on the aggression leash, up by 0.1 on
/// the compassion leash, and by nothing on the learning leash. The first step is taken at once, the next whenever more
/// than 600 turns have passed since the last. The turn of the last step is kept on the other creature: every creature
/// leashed to it shares it, and untying the leash doesn't forget it.
constexpr uint32_t k_AttitudeTurns = 600;
constexpr float k_AttitudeStep = 0.1f;
/// Whether a step is due this turn, given the turn of the last one (0 for never)
[[nodiscard]] bool AttitudeStepDue(uint32_t lastStep, uint32_t turn);
/// How much nicer each finds the other at each step
[[nodiscard]] float AttitudeStep(LeashType type);

/// A lesson the creature learns about something the player showed it on a leash: how much more the desire is what to
/// act on it with
struct Lesson
{
	creature_desires::Desire desire;
	float change;
};
/// On the aggression leash, what the player shows the creature teaches it anger (+1) over compassion (-1), or over
/// befriending when it is another creature; the compassion leash teaches the reverse; the learning leash nothing
[[nodiscard]] std::vector<Lesson> LessonsFor(LeashType type, bool objectIsCreature);

/// Sent to act on a thing it can pick up with its hands empty, or on a thing its desires find nothing to do with, the
/// creature is made curious about the thing (creature_desires::Desire::Curiosity) and carries out this action on it:
/// it holds up a thing it can pick up (the action table's HoldObject) and looks any other over (ExamineByLooking)
[[nodiscard]] std::string_view OrderedThingAction(bool canPickUp);

/// What the leash tells the creature's mind, for its planner and its learning to act on
struct MindHooks
{
	/// Walking where the leash or its home sends it, its mind obeys the player and leaves its body alone until the walk
	/// is over
	bool obeying {false};
	/// Wearing the learning leash in its player's hand, it copies the player's actions that need it
	bool learningInHand {false};
	/// How much each miracle seen counts towards learning it
	uint32_t miracleSightingWeight {1};
	/// Things the player showed it, by entity number, and the leash they were shown on
	struct Shown
	{
		uint32_t object;
		LeashType type;
		std::vector<Lesson> lessons;
	};
	std::vector<Shown> shown;
	/// Things it was told to act on, by entity number
	std::vector<uint32_t> actOn;
	/// The thing its player last sent it to act on with the leash, by entity number, until its mind takes it up: when its
	/// desires find nothing to do with it, it is made curious about it (OrderedThingAction)
	std::optional<uint32_t> ordered;
	/// Pulled away from what it was doing, its body plays out the end of its own action: the plan that follows does not
	/// cut that end short
	bool bodyEndPlaysOut {false};
	/// How much nicer it finds other creatures, by entity number
	struct Attitude
	{
		uint32_t creature;
		float change;
	};
	std::vector<Attitude> attitudes;
};

} // namespace openblack::creature_leash
