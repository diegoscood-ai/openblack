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
[[nodiscard]] std::vector<Step> SleepAtHome(glm::vec2 home, float height, const Random& random);

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

/// Being frightened on the spot: a frightened start with wide eyes, three seconds looking frightened, and another start
/// with wide eyes
[[nodiscard]] std::vector<Step> BeFrightenedOnTheSpot();
constexpr float k_FrightenedSeconds = 3.0f;

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

/// Lying dead for good: fainting (those near it react as to a death) and lying there, out cold with its eyes closed,
/// for ten days
[[nodiscard]] std::vector<Step> DeadForever();
constexpr float k_DeadForeverSeconds = 864000.0f;

} // namespace openblack::creature_mind
