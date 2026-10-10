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
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"
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

/// Pulled away from the same desire's action a second time, the desire is held back for this many seconds a pull
constexpr float k_SuppressSecondsPerPull = 30.0f;
/// How many times the creature has been pulled away from acting on each desire
struct PullMemory
{
	std::array<uint8_t, creature_desires::k_DesireCount> counts {};
};
/// The creature is pulled away from acting on a desire: on the second pull the desire is held back, for the returned
/// seconds, and the count starts again
[[nodiscard]] std::optional<float> RecordPull(PullMemory& memory, creature_desires::Desire desire);

/// A tug on the leash held in the hand. Nothing in the game tugs it: the original's sender of the tug is never called,
/// so this is reached only from the debug windows.
///
/// A creature walking back into the area it is kept within, clearing its way or held by a script takes no notice of a
/// tug. Otherwise, if it was not being led yet, it is pulled away from the plan it carried out. Already led, with a
/// plan about something other than itself, it stays where it is when the leash is tied or it is within 10 of the hand.
/// Still led, it keeps going while the end of its route is less than half as far from the hand as it is. Otherwise it
/// sets off for the hand, unless where it was last sent walking is within 1 of it.
constexpr float k_CloseToHand = 10.0f;
constexpr float k_CloserShare = 0.5f;
constexpr float k_HandMoved = 1.0f;
struct TugCheck
{
	/// Clearing its way of things in its way. Open point: openblack keeps no such state on a creature yet, so it is
	/// never set
	bool clearingWay {false};
	bool walkingBack {false};
	bool controlledByScript {false};
	/// Being led by the leash already
	bool led {false};
	/// Carrying out a plan about something other than itself
	bool planOnOther {false};
	bool tied {false};
	/// How far its body is from the hand, in the world
	float bodyToHand {0.0f};
	/// How far it, and the end of its route, are from the hand on the map; no route end when it has no route
	float creatureToHand {0.0f};
	std::optional<float> routeEndToHand;
	/// How far where it was last sent walking is from the hand on the map
	float sentToFromHand {0.0f};
};
enum class Lead : uint8_t
{
	/// It takes no notice of the tug
	Ignored,
	/// It stays where it is
	Stay,
	/// It keeps going where it was going
	KeepGoing,
	/// It sets off for the hand
	GoToHand,
};
struct Tug
{
	/// Whether it is pulled away from the plan it carried out, which happens before it decides where to go
	bool pulledAway {false};
	Lead lead {Lead::Ignored};
};
[[nodiscard]] Tug DecideTug(const TugCheck& check);

/// The plans a leash makes the creature carry out, about itself, for its desire to obey the player, by their rows in the
/// game's action table: pulled away from what it did, it goes to the hand; sent walking by the leash or its home, it
/// walks to the point
constexpr uint32_t k_GoToHandAction = 89;
constexpr uint32_t k_WalkToPointAction = 1;

/// How hard the creature is being pulled, for its speed: none at first, all once it is on its way. Once a turn, led or
/// not, while it is more than 0.05 it fades by 0.95, and is gone below 0.3
constexpr float k_PullFadesAbove = 0.05f;
constexpr float k_PullFade = 0.95f;
constexpr float k_PullGone = 0.3f;
[[nodiscard]] float FadePull(float pull);

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
	/// How much nicer it finds other creatures, by entity number
	struct Attitude
	{
		uint32_t creature;
		float change;
	};
	std::vector<Attitude> attitudes;
};

} // namespace openblack::creature_leash
