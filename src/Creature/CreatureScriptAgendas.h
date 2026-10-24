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
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureIdleMind.h"

/// The agendas of the actions scripts force on creatures to stage their scenes (the creatures of the glade looking,
/// pointing and sitting), and of a script turning a creature to face a point or sending it somewhere.
///
/// Facing the camera only means anything to a creature with a player: one without (as the glade's creatures) doesn't
/// turn to the camera or look at it, but still points at it.
namespace openblack::creature_mind
{
/// Looking at a thing until something else is given: it turns to face it, holding still two seconds, then watches it
/// for a day, its head on the thing's top throughout
constexpr float k_FaceThingSeconds = 2.0f;
constexpr float k_LookForeverSeconds = 86400.0f;
/// Looking at a thing without going up to it: turning to face it, then watching its foot for 1.1 seconds and up to 1.2
/// more, drawn as the agenda is made
constexpr float k_LookButDontApproachSeconds = 1.1f;
constexpr float k_LookButDontApproachExtraSeconds = 1.2f;
/// Pointing at a thing lasts five seconds, and at the camera one; pointing never lasts less than a second
constexpr float k_PointAtThingSeconds = 5.0f;
constexpr float k_PointAtCameraSeconds = 1.0f;
constexpr float k_ShortestPointSeconds = 1.0f;
/// Sitting where a script says, it turns to face the place, holding still a tenth of a second, then sits for ten
/// seconds and up to five more, drawn as the agenda is made
constexpr float k_FacePlaceSeconds = 0.1f;
constexpr float k_SitAtSeconds = 10.0f;
constexpr float k_SitAtExtraSeconds = 5.0f;
/// The action of the game's table the look in a cut scene is
constexpr std::string_view k_CutSceneLookAction = "LookAtCameraInWideScreen";
/// Watching the camera in a cut scene ends once more than this many turns have gone by
constexpr uint32_t k_CutSceneWatchTurns = 20;
/// The chances of each extra in the look: pointing, waving and talking are one in two, looking confused one in three
constexpr uint32_t k_CutScenePointLots = 2;
constexpr uint32_t k_CutSceneWaveLots = 2;
constexpr uint32_t k_CutSceneTalkLots = 2;
constexpr uint32_t k_CutSceneConfusedLots = 3;

/// Turning to face a thing and watching it for a day
[[nodiscard]] std::vector<Step> LookForever(uint32_t thing);
/// Turning to face a thing and watching its foot for the seconds given
[[nodiscard]] std::vector<Step> LookButDontApproach(uint32_t thing, float seconds);
/// Pointing at a thing for five seconds wherever it goes, amazed, its head on the thing itself
[[nodiscard]] std::vector<Step> PointAtThing(uint32_t thing);
/// Pointing at the camera for a second, amazed, its head on the point, having turned to face the camera first, its head
/// on it, when it has a player
[[nodiscard]] std::vector<Step> PointAtCamera(bool hasPlayer, glm::vec3 camera);
/// While a script's cut scene is on, a creature free to think for itself is made to look at the camera, over and over
/// until the cut scene is over. Half the time it first points at the camera for a second, amazed; then it watches the
/// camera, curious, for 21 turns; half the time it waves, amazed; half the time it talks, curious; one time in three it
/// looks confused, amazed, and talks; then it watches the camera for 21 turns again. Its head is on the camera
/// throughout. The four chances are drawn in that order every time. Without a camera it doesn't point; without a player
/// it doesn't watch, nor has its head on the camera.
[[nodiscard]] std::vector<Step> LookAtCameraInCutScene(const std::optional<glm::vec3>& camera, const Random& random);
/// Whether a cut scene makes a creature free to think for itself look at the camera this turn: the cinema bars are in,
/// a script brought them (owner 0 is the game's own bars, as a film's), and it isn't carrying out the look already.
/// Once a look is over it is made to look again while the bars stay in.
[[nodiscard]] bool StartsLookAtCameraInCutScene(bool barsIn, uint32_t barsOwner, bool lookingAlready);
/// Sitting down where a script says: it walks there until within its own radius with the look on the way to a point
/// (LookWhileGoing), turns to face the place, its head on it, turns to face down the slope, then sits for the seconds
/// given, looking about
[[nodiscard]] std::vector<Step> SitDownAt(uint32_t place, glm::vec2 point, float radius, float seconds);
/// Turning to face a point, watching it, as a script asks
[[nodiscard]] std::vector<Step> FaceForScript(glm::vec3 point);
/// Turning to face a thing, as a script asks: it turns, holding still two seconds once it does
[[nodiscard]] std::vector<Step> FaceThingForScript(uint32_t thing);

/// A creature a script sends to a point walks there in two goes, watching the point. With no distance given, it first
/// gets within a sixth of the way (its height when that is more, else no more than 0.7 of its height and 22), then
/// within its height; with one, it gets within that distance twice.
[[nodiscard]] std::vector<Step> MoveForScript(glm::vec2 from, glm::vec3 point, float height, float distance);
constexpr float k_FirstLegShare = 1.0f / 6.0f;
constexpr float k_FirstLegHeightShare = 0.7f;
constexpr float k_FirstLegMost = 22.0f;

/// Facing a point as a script asks: whether it has to turn, given how far the point is and how far round from the way
/// it faces (radians, either way)
[[nodiscard]] bool NeedsToTurnToFace(float distance, float turn);
/// It is right on top of a point nearer than this, and faces it within this much of a turn
constexpr float k_OnTopOf = 0.1f;
constexpr float k_FacingWithin = 0.392699093f;

/// The ground around a creature it looks down the slope at: ten units away in the eight directions about it
constexpr float k_SlopeLookDistance = 10.0f;
/// The lowest of the ground about a point, the first of the lowest when several are as low. heightAt says how high the
/// ground is at a point, none off the land; with all of them off the land the game takes the corner of the map at the
/// origin.
[[nodiscard]] glm::vec2 DownSlope(glm::vec2 from, const std::function<std::optional<float>(glm::vec2)>& heightAt);

/// The look on the way to a point: nearer than k_NoWalkLookWithin it has no look of its own, and within the species'
/// pointWithin it looks at the point. Farther, it takes three looks in turn, each for two seconds and a turn in whole
/// turns: at the point, at what interests it most (else at the last moving thing it came across while that was seen
/// less than two seconds ago, else at the point, and the moving thing is forgotten), and a sweep from side to side of
/// the way its body
/// faces, a step further each turn up to sweepAngle either way, turning back there and then looking distanceStep
/// further away, from firstDistance until farthest, then from firstDistance again
constexpr float k_NoWalkLookWithin = 8.0f;
/// The species' numbers for it
struct WalkLookInfo
{
	float sweepAngle {0.0f};
	float sweepStep {0.0f};
	float firstDistance {0.0f};
	float farthest {0.0f};
	float distanceStep {0.0f};
	float pointWithin {0.0f};
};
/// Where a creature is in it: which of the three looks, the turns it has had of it, and the sweep. Kept from walk to
/// walk, from when the creature is made
struct WalkLook
{
	uint32_t look {0};
	uint32_t turns {0};
	float angle {0.0f};
	float step {0.0f};
	float distance {0.0f};
	float distanceStep {0.0f};
};
[[nodiscard]] WalkLook StartWalkLook(const WalkLookInfo& info);
/// What it looks at this turn: nothing of its own, the point, what interests it most, the last moving thing it came
/// across, or the sweep's point on the ground (x, z, in metres); and whether the moving thing is to be forgotten
struct WalkLookAt
{
	enum class Kind : uint8_t
	{
		Nothing,
		Point,
		Interesting,
		MovingSeen,
		Sweep,
	};
	Kind kind {Kind::Nothing};
	glm::vec2 sweep {0.0f};
	bool forgetMovingSeen {false};
};
/// One turn of it, from where the creature stands to the point, the way its body faces (a body's yaw), whether
/// anything interests it and whether the last moving thing it came across was seen less than two seconds ago;
/// turnsPerSecond is the game's whole turns in a second
[[nodiscard]] WalkLookAt LookWhileGoing(WalkLook& look, const WalkLookInfo& info, glm::vec2 from, glm::vec2 point,
                                        float bodyYaw, bool interesting, uint32_t turnsPerSecond,
                                        bool movingSeenRecently = false);
} // namespace openblack::creature_mind
