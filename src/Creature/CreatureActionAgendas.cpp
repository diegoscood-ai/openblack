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
#include <iterator>

#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreaturePlayerAttention.h"

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

/// Facing a villager to act at it, it holds still this long once it does
constexpr float k_FaceVillagerSeconds = 0.1f;

std::vector<Step> creature_mind::PullSillyFaces(uint32_t villager, float height, const Random& random)
{
	constexpr float k_SillyFacesHeights = 2.0f;
	constexpr uint32_t k_PlayfulLots = 3;
	constexpr size_t k_Faces = 3;
	std::vector<Step> agenda {
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = villager, .maxDistance = k_SillyFacesHeights * height},
	     .gaze = Gaze {.goingTowards = true}},
	    FaceThing(villager, k_FaceVillagerSeconds, creature_face::Cue::None, Gaze {.object = villager}),
	};
	if (random(k_PlayfulLots) > 0)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = creature_layers::animations::k_FeelPlayful});
	}
	for (size_t face = 0; face < k_Faces; ++face)
	{
		agenda.push_back(
		    {.kind = Step::Kind::Face,
		     .seconds = creature_player_attention::k_SillyFaceSeconds,
		     .animation = creature_player_attention::k_FirstSillyFace + random(creature_player_attention::k_SillyFaceCount),
		     .gaze = Gaze {.object = villager}});
	}
	return agenda;
}

std::vector<Step> creature_mind::ShowImpressiveAnimation(uint32_t villager, float height, const Random& random)
{
	constexpr float k_ImpressHeights = 3.0f;
	constexpr size_t k_ImpressivePose = 69;
	constexpr size_t k_AfterOne = 65;
	constexpr size_t k_AfterOther = 52;
	return {
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = villager, .maxDistance = k_ImpressHeights * height},
	     .gaze = Gaze {.goingTowards = true}},
	    FaceThing(villager, k_FaceVillagerSeconds, creature_face::Cue::None, Gaze {.object = villager}),
	    {.kind = Step::Kind::Action, .animation = k_ImpressivePose},
	    // Both poses are main steps; the later one, the last made main, is the agenda's main step
	    {.kind = Step::Kind::Action, .animation = random(2) == 0 ? k_AfterOne : k_AfterOther, .main = true},
	};
}

std::vector<Step> creature_mind::InspectCreature(uint32_t other, float height)
{
	// Each turn of the look the mind weighs which of its desires it would act on with the other creature, and which the
	// other would act on with it, and on finding one drops the look to act on it (CreatureMindSystem::WeighWatchedCreature)
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

std::vector<Step> creature_mind::SleepAtHome(glm::vec2 home, float height, const Random& random,
                                             std::optional<float> homeAltitude)
{
	std::vector<Step> agenda;
	if (random(k_YawnBeforeSleepLots) == 0)
	{
		agenda.push_back(PlayOnce(animations::k_Tired, true));
	}
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToPoint,
	                               .point = home,
	                               .maxDistance = std::min(k_SleepAtHomeReach, height),
	                               .pointHeight = homeAltitude},
	                  .gaze = Gaze {.point = glm::vec3 {home.x, 0.0f, home.y}, .goingTowards = true}});
	agenda.push_back({.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::FaceDownSlope}});
	agenda.push_back({.kind = Step::Kind::Static,
	                  .sequence = {animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep},
	                  .untilRested = true,
	                  .mirrorAtRandom = true,
	                  .closedEyes = true,
	                  .effect = Effect::Slept,
	                  .main = true});
	agenda.push_back(PlayOnce(animations::k_Confused, true, creature_face::Cue::Grimace));
	return agenda;
}

std::vector<Step> creature_mind::PooAtHome(uint32_t self, glm::vec2 home, float height, const Random& random,
                                           std::optional<float> homeAltitude)
{
	std::vector<Step> agenda {{.kind = Step::Kind::Move,
	                           .movement = {.kind = Movement::Kind::ToPoint,
	                                        .point = home,
	                                        .maxDistance = k_PooAtHomeHeights * height,
	                                        .pointHeight = homeAltitude},
	                           .gaze = Gaze {.point = glm::vec3 {home.x, 0.0f, home.y}, .goingTowards = true}}};
	std::ranges::move(PooBy(self, random), std::back_inserter(agenda));
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
	                  .mirrorAtRandom = true,
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

namespace
{

// Running away from a point, afraid: one time in two first the frightened clip with wide eyes, then the run, then one
// time in two the frightened clip again
std::vector<Step> RunAwayLookingFrightened(glm::vec2 point, const Random& random)
{
	auto frightened = PlayOnce(animations::k_Frightened);
	frightened.heldEyes = Eyes::Wide;
	frightened.frightenedLook = true;
	std::vector<Step> agenda;
	if (random(2) == 0)
	{
		agenda.push_back(frightened);
	}
	Step flee {.kind = Step::Kind::Move, .face = creature_face::Cue::Fear};
	flee.movement = {.kind = Movement::Kind::FleeFrom, .point = point, .run = true};
	flee.main = true;
	agenda.push_back(flee);
	if (random(2) == 0)
	{
		agenda.push_back(frightened);
	}
	return agenda;
}

} // namespace

std::vector<Step> creature_mind::RunFromPlayer(glm::vec2 camera, const Random& random)
{
	return RunAwayLookingFrightened(camera, random);
}

std::vector<Step> creature_mind::RunFromThing(glm::vec2 thing, const Random& random)
{
	return RunAwayLookingFrightened(thing, random);
}

std::vector<Step> creature_mind::ExamineByLooking(uint32_t thing, float height, float thingHeight)
{
	auto go = GoNear(thing, std::max(height * k_ExamineNearHeights, thingHeight));
	go.gaze = Gaze {.object = thing, .goingTowards = true};
	Step observe {.kind = Step::Kind::Observe, .object = thing};
	observe.main = true;
	return {
	    go,
	    FaceThing(thing, k_ExamineFaceSeconds, creature_face::Cue::None, Gaze {.object = thing}),
	    {.kind = Step::Kind::Wait, .seconds = k_ExamineWatchSeconds, .gaze = Gaze {.object = thing, .bottom = true}},
	    observe,
	};
}

std::vector<Step> creature_mind::BeFrightenedOnTheSpot()
{
	// The two starts look about with wide eyes and pull no face; the wait between them looks frightened
	auto start = PlayOnce(animations::k_Frightened);
	start.heldEyes = Eyes::Wide;
	start.frightenedLook = true;
	auto last = start;
	last.main = true;
	return {
	    start,
	    {.kind = Step::Kind::Wait, .seconds = k_FrightenedSeconds, .face = creature_face::Cue::Frightened},
	    last,
	};
}

std::vector<Step> creature_mind::BeSad(uint32_t self, const Random& random)
{
	// The wait's seconds are drawn before the clip's choice
	const float seconds = k_SadSeconds + creature_emote::FloatRandom(random, k_SadExtraSeconds);
	const Step wait {.kind = Step::Kind::Wait,
	                 .seconds = seconds,
	                 .face = creature_face::Cue::Sad,
	                 .gaze = Gaze {.object = self, .bottom = true}};
	auto clip = PlayOnce(random(2) == 0 ? k_SadClip : k_CryClip);
	clip.main = true;
	return {wait, clip};
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

namespace
{
/// Lying dead for good for so long: it faints, so those near it react as to a death, and it is out cold; the faint is
/// done once it lies, and being dead for good it is not got up: it lies on through the wait after it
std::vector<Step> LieDeadForGood(float seconds)
{
	Step faint {.kind = Step::Kind::Static,
	            .sequence = {animations::k_Faint, animations::k_Faint, animations::k_GetUp},
	            .holdLoop = true,
	            .closedEyes = true,
	            .faint = true,
	            .staysDown = true,
	            .main = true};
	faint.liesOn = true;
	return {faint, {.kind = Step::Kind::Wait, .seconds = seconds, .staysDown = true}};
}
} // namespace

std::vector<Step> creature_mind::DeadForever()
{
	return LieDeadForGood(k_DeadForeverSeconds);
}

std::vector<Step> creature_mind::DiePermanently()
{
	return LieDeadForGood(k_DiePermanentlySeconds);
}

std::vector<Step> creature_mind::Die()
{
	return {{.kind = Step::Kind::WaitUntilFree, .main = true}};
}
