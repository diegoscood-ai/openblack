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
#include <vector>

#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"

/// The agendas of the actions a creature does about the things around it. Here so far: looking another creature over,
/// looking at a thing flying through the air, looking at the player's hand, and the actions the first land's lessons
/// force on it: sleeping at home, waving at a thing, being frightened on the spot and lying dead for good (putting down
/// what it holds is the idle mind's PutDownHeld). Pure, tested on made-up values.
namespace openblack::creature_mind
{

/// Looking another creature over: going near it (four times its own height away), turning to face it a tenth of a
/// second, curious, then watching it, curious, for five seconds and up to ten more, after which it has looked it over
[[nodiscard]] std::vector<Step> InspectCreature(uint32_t other, float height);
/// Running away from the player: one time in two first a frightened look, then the run away from the camera, afraid,
/// then one time in two a frightened look again
[[nodiscard]] std::vector<Step> RunFromPlayer(glm::vec2 camera, const Random& random);
/// Running away from a frightening thing at `thing`: the same, one time in two a frightened look before and after the
/// run away from it, afraid
[[nodiscard]] std::vector<Step> RunFromThing(glm::vec2 thing, const Random& random);
/// Pulling silly faces at a villager: going within twice its height of it and facing it, looking at it; two times in
/// three showing it feels playful, then pulling three of the ten faces at it
[[nodiscard]] std::vector<Step> PullSillyFaces(uint32_t villager, float height, const Random& random);
/// Showing a villager an impressive pose: going within three of its heights of it and facing it, looking at it, then
/// the impressive pose, and after it one of two others, a draw of two choosing
[[nodiscard]] std::vector<Step> ShowImpressiveAnimation(uint32_t villager, float height, const Random& random);
constexpr float k_InspectHeights = 4.0f;
constexpr float k_InspectFaceSeconds = 0.1f;
constexpr float k_InspectSeconds = 5.0f;
constexpr float k_InspectExtraSeconds = 10.0f;

/// Looking at a thing flying through the air: half the time it first points at it two seconds, amazed, then it watches
/// it, amazed, while it flies, for up to two seconds and a draw of up to two more each turn; its head follows the thing
/// where it is, unless the creature holds it
[[nodiscard]] std::vector<Step> LookAtFlyingObject(uint32_t thing, const Random& random);
constexpr uint32_t k_PointAtFlyingLots = 2;
constexpr float k_PointAtFlyingSeconds = 2.0f;

/// Looking at the player's hand: watching it, curious, its head on the hand, for whole seconds more than those of a
/// turn and a half (five seconds at most)
[[nodiscard]] std::vector<Step> LookAtHand();
constexpr uint32_t k_LookAtHandTurns = 15;

/// Sleeping at home: half the time a sleepy yawn first; then walking home until within its height, no further than 5
/// (k_SleepAtHomeReach), facing down the slope, sleeping until rested and getting up dazed with a grimace
[[nodiscard]] std::vector<Step> SleepAtHome(glm::vec2 home, float height, const Random& random,
                                            std::optional<float> homeAltitude = std::nullopt);

/// A poo done discreetly, at home: walking home until within two of its heights, looking at home as it goes, then the
/// steps of a poo by a thing (PooBy), the thing the creature itself, as the plan acts on no other
[[nodiscard]] std::vector<Step> PooAtHome(uint32_t self, glm::vec2 home, float height, const Random& random,
                                          std::optional<float> homeAltitude = std::nullopt);
constexpr float k_PooAtHomeHeights = 2.0f;
/// It poos at home only when no further than this from home, in metres
constexpr float k_NearHomeToPoo = 20.0f;

/// Sleeping by a thing: with a clear place found about the thing (k_SleepOnTheSpotHeights times its height wide), it
/// walks to within its 2D radius of it, looking where it goes; then half the time a sleepy yawn (drawn after the walk is
/// planned), the turn down the slope, the sleep until rested and the daze with a grimace. With none, it sleeps as it
/// does on the spot with no clear place: at home (SleepOnTheSpot)
[[nodiscard]] std::vector<Step> SleepByObject(std::optional<glm::vec2> clearArea, float radius, std::optional<glm::vec2> home,
                                              float height, const Random& random);

/// Waving at a thing: going near it (three and a half times its height away), smiling, looking where it goes; turning to
/// face it 1.4 seconds, smiling, watching it; then a friendly wave, watching it with fluttering eyelids
[[nodiscard]] std::vector<Step> WaveAt(uint32_t thing, float height);
constexpr float k_WaveHeights = 3.5f;
constexpr float k_WaveFaceSeconds = 1.4f;

/// Looking a thing over: going near it (two and a half times the creature's height away, or the thing's height when
/// taller), its head on where it goes; turning to face it two seconds, watching it; 1.1 seconds watching its bottom;
/// then taking in what it is (Step::Kind::Observe), the main step. It pulls no face
[[nodiscard]] std::vector<Step> ExamineByLooking(uint32_t thing, float height, float thingHeight);
constexpr float k_ExamineNearHeights = 2.5f;
constexpr float k_ExamineFaceSeconds = 2.0f;
constexpr float k_ExamineWatchSeconds = 1.1f;

/// Being frightened on the spot: a frightened start with wide eyes, three seconds looking frightened, and another start
/// with wide eyes
[[nodiscard]] std::vector<Step> BeFrightenedOnTheSpot();
constexpr float k_FrightenedSeconds = 3.0f;

/// Being sad: waiting two seconds and a float draw of up to three more, looking down at the ground under it with the
/// sad face, then one of the two sad clips, a draw of two choosing, as the main step with no face of its own
[[nodiscard]] std::vector<Step> BeSad(uint32_t self, const Random& random);
constexpr float k_SadSeconds = 2.0f;
constexpr float k_SadExtraSeconds = 3.0f;
/// The two clips of being sad: the sad one on a draw of 0, else the crying one
constexpr size_t k_SadClip = 56;
constexpr size_t k_CryClip = 219;

/// Holding a thing up to its player: with its hand empty it picks the thing up first, its head on the way there; then
/// it turns to the camera showing what it thinks of its player; the first two times it holds a thing of a kind it plays
/// with it, smiling (stroking, shaking, smelling or examining it, drawn); then it watches the camera, showing what it
/// thinks of its player, for 40 seconds and a draw of up to 10 more, in whole turns. Without a camera it does not turn
struct HoldThing
{
	uint32_t thing {0};
	bool handFull {false};
	std::optional<glm::vec2> camera;
	/// It held things of this kind fewer than k_HeldPlaysPerKind times before
	bool playsWithIt {false};
	float turnsPerSecond {0.0f};
};
[[nodiscard]] std::vector<Step> HoldObject(const HoldThing& hold, const Random& random);
constexpr uint32_t k_HeldPlaysPerKind = 2;
constexpr float k_HoldSeconds = 40.0f;
constexpr float k_HoldExtraSeconds = 10.0f;

/// Lying dead for good: fainting (those near it react as to a death), done as it lies, then a wait of ten days lying
/// there, out cold with its eyes closed
[[nodiscard]] std::vector<Step> DeadForever();
constexpr float k_DeadForeverSeconds = 864000.0f;
/// Dying for good, as a script makes a creature: the same faint and lying down, for 999999 seconds
[[nodiscard]] std::vector<Step> DiePermanently();
constexpr float k_DiePermanentlySeconds = 999999.0f;
/// Dying: the creature is knocked out as it is forced to die, so it faints, lies, is carried home and comes round as
/// from a faint, its mind leaving it alone meanwhile; the agenda's one step, its main one, then waits until its body is
/// free
[[nodiscard]] std::vector<Step> Die();

} // namespace openblack::creature_mind
