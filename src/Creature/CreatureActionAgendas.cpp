/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureActionAgendas.h"

#include <algorithm>

#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;

namespace
{
/// An animation played once, with sleepy eyes when it is the yawn or the daze of waking
Step PlayOnce(size_t animation, bool sleepyEyes = false, creature_face::Cue face = creature_face::Cue::None)
{
	return {.kind = Step::Kind::Action, .animation = animation, .sleepyEyes = sleepyEyes, .face = face};
}

/// Going to within a distance of a thing
Step GoNear(uint32_t thing, float distance, creature_face::Cue face = creature_face::Cue::None)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::GoNearObject, .object = thing, .maxDistance = distance},
	        .face = face};
}

/// Turning to face a thing, holding still a while once it does
Step FaceThing(uint32_t thing, float seconds, creature_face::Cue face, std::optional<Gaze> gaze)
{
	return {.kind = Step::Kind::Move,
	        .seconds = seconds,
	        .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = thing},
	        .face = face,
	        .gaze = gaze};
}
} // namespace

std::vector<Step> creature_mind::InspectCreature(uint32_t other, float height)
{
	// The game also weighs, each turn of the watch, which of its desires it would act on with the other creature, and
	// which the other would act on with it, and on finding one drops the watch to act on it then (anger, and fear unless
	// anger is the stronger, settled first); the planner can't yet weigh a desire for one given thing, so the watch
	// always runs its time
	return {
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = other, .maxDistance = height * k_InspectHeights}},
	    {.kind = Step::Kind::Move,
	     .seconds = k_InspectFaceSeconds,
	     .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = other},
	     .face = creature_face::Cue::Curiosity},
	    {.kind = Step::Kind::Inspect,
	     .seconds = k_InspectSeconds,
	     .extraSeconds = k_InspectExtraSeconds,
	     .object = other,
	     .face = creature_face::Cue::Curiosity,
	     .gaze = Gaze {.object = other}},
	};
}

std::vector<Step> creature_mind::LookAtFlyingObject(uint32_t thing, const Random& random)
{
	const Gaze follow {.object = thing, .itself = true};
	std::vector<Step> agenda;
	if (random(k_PointAtFlyingLots) == 0)
	{
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PointAt, .object = thing, .seconds = k_PointAtFlyingSeconds},
		                  .face = creature_face::Cue::Amazed,
		                  .gaze = follow});
	}
	agenda.push_back({.kind = Step::Kind::WatchFlying, .object = thing, .face = creature_face::Cue::Amazed, .gaze = follow});
	return agenda;
}

std::vector<Step> creature_mind::LookAtHand()
{
	return {{.kind = Step::Kind::WatchHand,
	         .turns = k_LookAtHandTurns,
	         .face = creature_face::Cue::Curiosity,
	         .gaze = Gaze {.hand = true}}};
}

std::vector<Step> creature_mind::SleepAtHome(glm::vec2 home, float height, const Random& random)
{
	std::vector<Step> agenda;
	if (random(k_YawnBeforeSleepLots) == 0)
	{
		agenda.push_back(PlayOnce(animations::k_Tired, true));
	}
	agenda.push_back(
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::ToPoint, .point = home, .maxDistance = std::min(k_SleepAtHomeReach, height)},
	     .gaze = Gaze {.point = glm::vec3 {home.x, 0.0f, home.y}, .goingTowards = true}});
	agenda.push_back({.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::FaceDownSlope}});
	agenda.push_back({.kind = Step::Kind::Static,
	                  .sequence = {animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep},
	                  .untilRested = true,
	                  .closedEyes = true,
	                  .effect = Effect::Slept});
	agenda.push_back(PlayOnce(animations::k_Confused, true, creature_face::Cue::Grimace));
	return agenda;
}

std::vector<Step> creature_mind::SleepByObject(std::optional<glm::vec2> clearArea, float radius, std::optional<glm::vec2> home,
                                               float height, const Random& random)
{
	if (!clearArea.has_value())
	{
		return SleepOnTheSpot(std::nullopt, radius, home, height, random);
	}
	std::vector<Step> agenda;
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToPoint, .point = *clearArea, .maxDistance = radius},
	                  .gaze = Gaze {.point = glm::vec3 {clearArea->x, 0.0f, clearArea->y}, .goingTowards = true}});
	// Unlike the other sleeps, the yawn comes after the walk
	if (random(k_YawnBeforeSleepLots) == 0)
	{
		agenda.push_back(PlayOnce(animations::k_Tired, true));
	}
	agenda.push_back({.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::FaceDownSlope}});
	agenda.push_back({.kind = Step::Kind::Static,
	                  .sequence = {animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep},
	                  .untilRested = true,
	                  .closedEyes = true,
	                  .effect = Effect::Slept,
	                  .main = true});
	agenda.push_back(PlayOnce(animations::k_Confused, true, creature_face::Cue::Grimace));
	return agenda;
}

std::vector<Step> creature_mind::WaveAt(uint32_t thing, float height)
{
	const Gaze look {.object = thing};
	auto go = GoNear(thing, k_WaveHeights * height, creature_face::Cue::Smile);
	go.gaze = Gaze {.object = thing, .goingTowards = true};
	// The wave pulls no face of its own; the creature watches the thing with fluttering eyelids
	auto wave = PlayOnce(animations::k_FriendlyWave);
	wave.gaze = look;
	wave.flutterEyelids = true;
	wave.main = true;
	return {go, FaceThing(thing, k_WaveFaceSeconds, creature_face::Cue::Smile, look), wave};
}

std::vector<Step> creature_mind::BeFrightenedOnTheSpot()
{
	// The two starts look about with wide eyes and pull no face; the wait between them looks frightened
	auto start = PlayOnce(animations::k_Frightened);
	start.heldEyes = Eyes::Wide;
	auto last = start;
	last.main = true;
	return {
	    start,
	    {.kind = Step::Kind::Wait, .seconds = k_FrightenedSeconds, .face = creature_face::Cue::Frightened},
	    last,
	};
}

std::vector<Step> creature_mind::HoldObject(const HoldThing& hold, const Random& random)
{
	std::vector<Step> agenda;
	if (!hold.handFull)
	{
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PickUp, .object = hold.thing},
		                  .gaze = Gaze {.object = hold.thing, .goingTowards = true}});
	}
	const Gaze atCamera {.camera = true};
	if (hold.camera.has_value())
	{
		agenda.push_back({.kind = Step::Kind::Move,
		                  .movement = {.kind = Movement::Kind::TurnToFace, .point = *hold.camera},
		                  .face = creature_face::Cue::AttitudeToPlayer,
		                  .gaze = atCamera,
		                  .main = true});
	}
	// The play is drawn before the watch's seconds
	if (hold.playsWithIt)
	{
		const auto play = creature_object_actions::k_FirstKeepAnimation +
		                  random(static_cast<uint32_t>(creature_object_actions::k_KeepAnimationCount));
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::Keep, .animation = play},
		                  .face = creature_face::Cue::Smile});
	}
	const auto seconds = k_HoldSeconds + creature_emote::FloatRandom(random, k_HoldExtraSeconds);
	agenda.push_back({.kind = Step::Kind::Wait,
	                  .seconds = creature_emote::WholeTurnSeconds(seconds, hold.turnsPerSecond),
	                  .face = creature_face::Cue::AttitudeToPlayer,
	                  .gaze = atCamera});
	return agenda;
}

std::vector<Step> creature_mind::DeadForever()
{
	// It faints, so those near it react as to a death, and it is out cold; being dead for good, it is not got up as it
	// lies, as from any other faint
	return {{.kind = Step::Kind::Static,
	         .seconds = k_DeadForeverSeconds,
	         .sequence = {animations::k_Faint, animations::k_Faint, animations::k_GetUp},
	         .holdLoop = true,
	         .closedEyes = true,
	         .faint = true,
	         .staysDown = true,
	         .main = true}};
}
