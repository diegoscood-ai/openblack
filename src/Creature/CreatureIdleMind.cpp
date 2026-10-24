/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureIdleMind.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureEating.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureTownHelp.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;

namespace
{
/// Random fractions of a second are drawn in this many steps
constexpr uint32_t k_FractionSteps = 1000;
/// A face is due again once its wait is within this much of over, so that the turns' rounding doesn't delay it
constexpr float k_FaceDue = 1e-3f;

float RandomFraction(const Random& random)
{
	return static_cast<float>(random(k_FractionSteps)) / static_cast<float>(k_FractionSteps);
}

/// The movements that the creature's movement says are done or have failed
bool IsSubMove(Movement::Kind kind)
{
	return kind == Movement::Kind::GoNearObject || kind == Movement::Kind::GetAwayFromObject ||
	       kind == Movement::Kind::TurnToFaceObject || kind == Movement::Kind::ToThrowPosition;
}

void FinishStep(IdleMind& mind)
{
	++mind.step;
	mind.stepStarted = false;
	mind.stepSeconds = 0.0f;
	mind.sitEnding = false;
}

/// A walk is over: its step's effect, if any, takes place as it ends
void FinishWalk(IdleMind& mind, const Step& step, Commands& commands)
{
	if (step.effect != Effect::None)
	{
		commands.effect = step.effect;
	}
	FinishStep(mind);
}

/// One turn of watching the camera for a set number of turns, as a cut scene has it, the step's first turn counted.
/// Without a player it is done at once. Right on the camera along the ground it does not turn; more than a right angle
/// off, it turns to it. Neither the camera's distance nor the time decide its end: only more than its turns gone by
void WatchCameraForTurns(IdleMind& mind, uint32_t turns, const Senses& senses, Commands& commands)
{
	constexpr float k_OnTheCamera = 0.1f;
	++mind.lookTurns;
	if (!senses.hasPlayer || !senses.camera.has_value())
	{
		FinishStep(mind);
		return;
	}
	if (glm::length(senses.position - *senses.camera) >= k_OnTheCamera)
	{
		const auto off =
		    creature_locomotion::WrapAngle(creature_locomotion::HeadingOf(*senses.camera - senses.position) - senses.heading);
		if (std::abs(off) > std::numbers::pi_v<float> / 2.0f)
		{
			commands.move = Movement {.kind = Movement::Kind::TurnToFace, .point = *senses.camera};
		}
	}
	if (mind.lookTurns > turns)
	{
		FinishStep(mind);
	}
}

/// One turn of watching the player's hand, the turn it starts on counted. Right under the hand along the ground it does
/// not turn; more than a right angle off, it turns to it. Done once it is not turning and has watched long enough
void WatchHand(IdleMind& mind, const Step& step, const Senses& senses, const Random& random, Commands& commands)
{
	constexpr float k_OnTheHand = 0.1f;
	++mind.watchTurns;
	// A creature with no player has no hand to watch, and draws nothing. Otherwise each turn the game first draws the
	// time of a longer watch that this one never uses
	if (senses.hasPlayer)
	{
		static_cast<void>(random(k_HandWatchUnusedDraw));
	}
	if (!senses.hasPlayer || !senses.hand.has_value() || senses.turnsPerSecond == 0)
	{
		FinishStep(mind);
		return;
	}
	bool turning = senses.moving;
	const auto offset = *senses.hand - senses.position;
	if (glm::length(offset) >= k_OnTheHand &&
	    std::abs(creature_locomotion::WrapAngle(creature_locomotion::HeadingOf(offset) - senses.heading)) >
	        std::numbers::pi_v<float> / 2.0f)
	{
		commands.move = Movement {.kind = Movement::Kind::TurnToFace, .point = *senses.hand};
		turning = true;
	}
	if (!turning && WatchedHandEnough(mind.watchTurns, step.turns.value_or(0), senses.turnsPerSecond))
	{
		FinishStep(mind);
	}
}

/// A turn of following the player's hand
void FollowHand(IdleMind& mind, const Senses& senses, Commands& commands)
{
	if (!senses.followHand.has_value())
	{
		FinishStep(mind);
		return;
	}
	switch (creature_player_attention::FollowHandTurn(*senses.followHand, mind.followHandTurns))
	{
	case creature_player_attention::FollowHandAction::Done:
		FinishStep(mind);
		break;
	case creature_player_attention::FollowHandAction::Fail:
		mind.step = mind.agenda.size();
		mind.stepStarted = false;
		mind.gaveUp = true;
		break;
	case creature_player_attention::FollowHandAction::WalkToHand:
		commands.move = Movement {
		    .kind = Movement::Kind::ToPoint, .point = senses.followHand->hand, .maxDistance = senses.followHand->reach};
		break;
	case creature_player_attention::FollowHandAction::Continue:
		break;
	}
}

/// A turn of watching the player while it waits for orders
void WatchPlayer(IdleMind& mind, const Senses& senses, Commands& commands)
{
	const auto action =
	    creature_player_attention::WatchPlayerTurn(senses.watchPlayer.value_or(creature_player_attention::WatchPlayerView {}));
	if (action.done)
	{
		FinishStep(mind);
		return;
	}
	if (action.turnTo.has_value())
	{
		commands.stopMoving = true;
		commands.move = Movement {.kind = Movement::Kind::TurnToFace, .point = *action.turnTo};
	}
}

/// One turn of watching a flying thing, the turn it starts on counted: the limit is picked afresh each turn, before
/// anything else is looked at
void WatchFlying(IdleMind& mind, const Senses& senses, const Random& random)
{
	const auto limit = (static_cast<float>(random(game_random::k_FloatRandRange)) * k_WatchFlyingExtraSeconds *
	                    game_random::FloatRandScale()) +
	                   k_WatchFlyingSeconds;
	++mind.watchTurns;
	if (senses.turnsPerSecond == 0)
	{
		FinishStep(mind);
		return;
	}
	if (!KeepsWatchingFlying(mind.watchTurns, senses.turnsPerSecond, limit, senses.objectInTheAir))
	{
		FinishStep(mind);
	}
}

/// One turn of following the camera's point of focus
void FollowCamera(IdleMind& mind, const Step& step, const Senses& senses, Commands& commands)
{
	const float reach = step.movement.maxDistance;
	// The game asks whether the body is busy at all: walking, or playing an action
	const auto action = creature_emote::FollowCameraTurn({.focus = senses.cameraFocus,
	                                                      .position = senses.position,
	                                                      .moving = senses.bodyBusy,
	                                                      .focusAhead = senses.cameraFocusAhead},
	                                                     mind.followCameraTurns, reach);
	switch (action)
	{
	case creature_emote::FollowCameraAction::Done:
		FinishStep(mind);
		break;
	case creature_emote::FollowCameraAction::Fail:
		mind.step = mind.agenda.size();
		mind.stepStarted = false;
		mind.gaveUp = true;
		break;
	case creature_emote::FollowCameraAction::MoveAgain:
		commands.move = Movement {.kind = Movement::Kind::ToPoint,
		                          .point = {senses.cameraFocus->x, senses.cameraFocus->z},
		                          .maxDistance = reach - creature_emote::k_FollowCameraStopShort};
		break;
	case creature_emote::FollowCameraAction::Continue:
		break;
	}
}

/// A start, loop and end held for some seconds
Step Static(std::array<size_t, 3> sequence, float seconds)
{
	return {.kind = Step::Kind::Static,
	        .seconds = seconds,
	        .animation = sequence[1],
	        .sleepyEyes = false,
	        .movement = {},
	        .sequence = sequence};
}

Step Action(size_t animation, bool sleepyEyes)
{
	return {.kind = Step::Kind::Action, .seconds = 0.0f, .animation = animation, .sleepyEyes = sleepyEyes};
}

/// Every step of an agenda pulls a face for what the creature does or feels, except while its eyes are closed
std::vector<Step> WithFace(std::vector<Step> agenda, creature_face::Cue cue)
{
	for (auto& step : agenda)
	{
		if (!step.closedEyes && step.face == creature_face::Cue::None)
		{
			step.face = cue;
		}
	}
	return agenda;
}

Step Object(ObjectOrder order, Effect effect = Effect::None)
{
	Step step {.kind = Step::Kind::Object};
	step.order = order;
	step.effect = effect;
	return step;
}

Step PickUp(uint32_t object)
{
	return Object({.kind = ObjectOrder::Kind::PickUp, .object = object});
}

/// Going to within a distance of a point, then turning to face what a portion is made from there, holding still a moment
std::vector<Step> GoAndFaceSource(uint32_t source, glm::vec2 point, float distance)
{
	Step go {.kind = Step::Kind::Move};
	go.movement = {.kind = Movement::Kind::ToPoint, .point = point, .maxDistance = distance};
	Step turn {.kind = Step::Kind::Move, .seconds = creature_eating::k_FaceSourceSeconds};
	turn.movement = {.kind = Movement::Kind::TurnToFaceObject, .object = source};
	return {go, turn};
}

/// Making a portion of a kind from a source, picking it up and eating it
void TakePortionAndEat(std::vector<Step>& agenda, uint32_t source, PotInfo portion)
{
	agenda.push_back(Object({.kind = ObjectOrder::Kind::PickUpPortion, .object = source, .portion = portion}));
	agenda.push_back(Object({.kind = ObjectOrder::Kind::Eat}, Effect::Eat));
}
} // namespace

glm::vec2 creature_mind::FeetLookPoint(glm::vec2 from, float bodyYaw)
{
	const auto angle = static_cast<uint16_t>(gutils::ConvertScawenAngleToGameAngle(bodyYaw));
	const auto here = map_coords::FromMetres(from);
	const auto x = map_coords::ToFixedGUtils(gutils::GetXByAngle(angle, k_FeetLookMetres) + map_coords::ToMetres(here.x));
	const auto z = map_coords::ToFixedGUtils(gutils::GetZByAngle(angle, k_FeetLookMetres) + map_coords::ToMetres(here.z));
	return {map_coords::ToMetres(x), map_coords::ToMetres(z)};
}

std::string_view creature_mind::Name(Activity activity)
{
	switch (activity)
	{
	case Activity::BeIdle:
		return "Being idle";
	case Activity::Sit:
		return "Sitting";
	case Activity::ShowDesire:
		return "Showing a desire";
	case Activity::HangAround:
		return "Hanging around";
	case Activity::Told:
		return "Doing as told";
	case Activity::Eat:
		return "Eating";
	case Activity::Drink:
		return "Drinking";
	case Activity::Sleep:
		return "Sleeping";
	case Activity::Poo:
		return "Having a poo";
	case Activity::Puke:
		return "Being sick";
	case Activity::Faint:
		return "Out cold";
	case Activity::Examine:
		return "Looking something over";
	case Activity::PlayWithObject:
		return "Throwing something about";
	case Activity::Hurl:
		return "Hurling something";
	case Activity::PutDown:
		return "Putting something down";
	case Activity::Planned:
		return "Acting on a desire";
	case Activity::None:
	default:
		return "Nothing";
	}
}

std::vector<Step> creature_mind::BeIdle(const Random& random)
{
	std::vector<Step> agenda;
	for (int i = 0; i < k_IdleRepeats; ++i)
	{
		Step wait {.kind = Step::Kind::Wait, .seconds = k_IdleWaitSeconds + RandomFraction(random)};
		wait.face = creature_face::Cue::Idle;
		agenda.push_back(wait);
		auto yawn = Action(animations::k_Tired, true);
		yawn.face = creature_face::Cue::Idle;
		agenda.push_back(yawn);
	}
	return agenda;
}

Step creature_mind::SitDown(const Random& random)
{
	auto sit = Static({animations::k_StartSit, animations::k_Sit, animations::k_EndSit},
	                  static_cast<float>(k_SitSeconds + random(k_SitExtraSeconds)));
	sit.face = creature_face::Cue::Idle;
	return sit;
}

std::vector<Step> creature_mind::Sleep(const Random& random)
{
	std::vector<Step> agenda;
	if (random(k_YawnBeforeSleepLots) == 0)
	{
		agenda.push_back(Action(animations::k_Tired, true));
	}
	auto sleep = Static({animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep}, 0.0f);
	sleep.untilRested = true;
	sleep.closedEyes = true;
	sleep.effect = Effect::Slept;
	agenda.push_back(sleep);
	agenda.push_back(Action(animations::k_Confused, true));
	return WithFace(std::move(agenda), creature_face::Cue::Grimace);
}

std::vector<Step> creature_mind::SleepOnTheSpot(std::optional<glm::vec2> clearArea, float radius, std::optional<glm::vec2> home,
                                                float height, const Random& random)
{
	std::vector<Step> agenda;
	if (random(k_YawnBeforeSleepLots) == 0)
	{
		agenda.push_back(Action(animations::k_Tired, true));
	}
	const auto walkTo = [&agenda](glm::vec2 point, float within) {
		Step go {.kind = Step::Kind::Move};
		go.movement = {.kind = Movement::Kind::ToPoint, .point = point, .maxDistance = within};
		go.gaze = Gaze {.point = glm::vec3 {point.x, 0.0f, point.y}, .goingTowards = true};
		agenda.push_back(go);
	};
	if (clearArea.has_value())
	{
		walkTo(*clearArea, radius);
	}
	else
	{
		// No clear place: the game's plain sleep, at home facing down the slope
		if (home.has_value())
		{
			walkTo(*home, std::min(k_SleepAtHomeReach, height));
		}
		Step face {.kind = Step::Kind::Move};
		face.movement = {.kind = Movement::Kind::FaceDownSlope};
		agenda.push_back(face);
	}
	auto sleep = Static({animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep}, 0.0f);
	sleep.untilRested = true;
	sleep.closedEyes = true;
	sleep.effect = Effect::Slept;
	agenda.push_back(sleep);
	// Only the waking pulls a face: the yawn, the walk and the turn down the slope pull none
	auto waking = Action(animations::k_Confused, true);
	waking.face = creature_face::Cue::Grimace;
	agenda.push_back(waking);
	return agenda;
}

std::vector<Step> creature_mind::Eat(uint32_t food)
{
	return WithFace({PickUp(food),
	                 Object({.kind = ObjectOrder::Kind::Keep, .animation = creature_object_actions::k_ExamineObject}),
	                 Object({.kind = ObjectOrder::Kind::Eat}, Effect::Eat)},
	                creature_face::Cue::Curiosity);
}

std::vector<Step> creature_mind::EatPortion(uint32_t pile, glm::vec2 point, float distance)
{
	Step go {.kind = Step::Kind::Move};
	go.movement = {.kind = Movement::Kind::ToPoint, .point = point, .maxDistance = distance};
	Step turn {.kind = Step::Kind::Move};
	turn.movement = {.kind = Movement::Kind::TurnToFace, .point = point};
	return WithFace({go, turn, Object({.kind = ObjectOrder::Kind::PickUpPortion, .object = pile}),
	                 Object({.kind = ObjectOrder::Kind::Eat}, Effect::Eat)},
	                creature_face::Cue::Curiosity);
}

std::vector<Step> creature_mind::EatFromStoragePit(uint32_t pile, glm::vec2 point, float height)
{
	auto agenda = GoAndFaceSource(pile, point, height * creature_eating::k_PitApproachHeights);
	agenda.push_back({.kind = Step::Kind::ClearObjectToActOn});
	TakePortionAndEat(agenda, pile, PotInfo::WheatInHand);
	return agenda;
}

std::vector<Step> creature_mind::EatFromField(uint32_t field, glm::vec2 point, float height)
{
	auto agenda = GoAndFaceSource(field, point, height);
	agenda.push_back({.kind = Step::Kind::ClearObjectToActOn});
	TakePortionAndEat(agenda, field, PotInfo::WheatInHand);
	return agenda;
}

std::vector<Step> creature_mind::EatFromTree(uint32_t tree, glm::vec2 point, float distance)
{
	auto agenda = GoAndFaceSource(tree, point, distance);
	TakePortionAndEat(agenda, tree, PotInfo::TreeFruitInHand);
	return agenda;
}

std::vector<Step> creature_mind::ExamineByPickingUp(uint32_t object, bool handFull, const Random& random)
{
	const auto keep = creature_object_actions::k_FirstKeepAnimation +
	                  random(static_cast<uint32_t>(creature_object_actions::k_KeepAnimationCount));
	const auto letGo = random(k_PutDownLots) >= k_TossLots ? ObjectOrder::Kind::PutDown : ObjectOrder::Kind::Discard;
	std::vector<Step> agenda;
	// It picks the thing up only when its hand is empty; holding something, it examines what it holds
	if (!handFull)
	{
		agenda.push_back(PickUp(object));
	}
	agenda.push_back(Object({.kind = ObjectOrder::Kind::Keep, .animation = keep}, Effect::Examined));
	agenda.push_back(Object({.kind = letGo}));
	return WithFace(std::move(agenda), creature_face::Cue::Curiosity);
}

std::vector<Step> creature_mind::ThrowAbout(uint32_t object, const Random& random)
{
	constexpr uint32_t k_Degrees = 360;
	const auto angle = static_cast<float>(random(k_Degrees)) * std::numbers::pi_v<float> / 180.0f;
	const auto distance = k_ThrowAroundDistance + static_cast<float>(random(k_ThrowAroundExtra));
	return WithFace({PickUp(object), Object({.kind = ObjectOrder::Kind::ThrowNearby,
	                                         .point = distance * glm::vec2(std::cos(angle), std::sin(angle))},
	                                        Effect::ThrewAbout)},
	                creature_face::Cue::Playfulness);
}

std::vector<Step> creature_mind::TurnToFacePoint(glm::vec2 point)
{
	Step turn {.kind = Step::Kind::TurnToFacePoint};
	turn.movement = {.kind = Movement::Kind::TurnToFace, .point = point};
	return {turn};
}

std::vector<Step> creature_mind::PutDownHeld()
{
	return {Object({.kind = ObjectOrder::Kind::PutDown})};
}

std::vector<Step> creature_mind::Poo(const Random& random)
{
	std::vector<Step> agenda;
	if (random(k_ShowPooLots) == 0)
	{
		agenda.push_back(Action(animations::k_NeedAPoo, false));
	}
	auto poo = Static({animations::k_StartPoo, animations::k_Poo, animations::k_EndPoo}, k_PooSeconds);
	poo.effect = Effect::Poo;
	agenda.push_back(poo);
	return WithFace(std::move(agenda), creature_face::Cue::Smile);
}

std::vector<Step> creature_mind::Puke()
{
	auto puke = Static({animations::k_StartPuke, animations::k_Puke, animations::k_EndPuke}, k_PukeSeconds);
	puke.effect = Effect::Puke;
	return WithFace({puke}, creature_face::Cue::Grimace);
}

std::vector<Step> creature_mind::Faint()
{
	auto faint = Static({animations::k_Faint, animations::k_Faint, animations::k_GetUp}, k_FaintSeconds);
	faint.holdLoop = true;
	faint.closedEyes = true;
	faint.effect = Effect::CameRound;
	return {faint};
}

std::vector<Step> creature_mind::EatHeld()
{
	return WithFace({Object({.kind = ObjectOrder::Kind::Eat}, Effect::Eat)}, creature_face::Cue::Curiosity);
}

std::vector<Step> creature_mind::TakeFromHand(uint32_t object, glm::vec2 camera, bool leftHand)
{
	Step turn {.kind = Step::Kind::Move};
	turn.movement = {.kind = Movement::Kind::TurnToFace, .point = camera};
	auto take = Object({.kind = ObjectOrder::Kind::TakeFromHand, .object = object, .mirrored = leftHand}, Effect::TookFromHand);
	std::vector<Step> agenda {turn, take};
	for (auto& step : agenda)
	{
		step.gaze = Gaze {.object = object};
	}
	return WithFace(std::move(agenda), creature_face::Cue::Curiosity);
}

std::vector<Step> creature_mind::ExamineGiven(uint32_t object)
{
	auto examine =
	    Object({.kind = ObjectOrder::Kind::Keep, .animation = creature_object_actions::k_ExamineObject}, Effect::Examined);
	examine.gaze = Gaze {.object = object};
	return WithFace({examine}, creature_face::Cue::Curiosity);
}

std::vector<Step> creature_mind::EatGiven()
{
	return {Object({.kind = ObjectOrder::Kind::Eat}, Effect::Eat)};
}

std::vector<Step> creature_mind::ThrowGiven(glm::vec3 camera)
{
	auto toss =
	    Object({.kind = ObjectOrder::Kind::Throw, .point = {camera.x, camera.z}, .pointHeight = camera.y}, Effect::ThrewAbout);
	toss.gaze = Gaze {.camera = true};
	return {toss};
}

std::vector<Step> creature_mind::Emote(size_t animation)
{
	return WithFace({Action(animation, false)}, creature_face::Cue::AttitudeToPlayer);
}

std::vector<Step> creature_mind::FaceAndEmote(glm::vec2 point, size_t animation)
{
	Step turn {.kind = Step::Kind::Move};
	turn.movement = {.kind = Movement::Kind::TurnToFace, .point = point};
	return WithFace({turn, Action(animation, false)}, creature_face::Cue::AttitudeToPlayer);
}

std::vector<Step> creature_mind::ApproachAndEmote(uint32_t object, size_t animation)
{
	Step approach {.kind = Step::Kind::Move};
	approach.movement = {.kind = Movement::Kind::ToObject, .object = object, .maxDistance = k_ApproachDistance};
	return WithFace({approach, Action(animation, false)}, creature_face::Cue::AttitudeToPlayer);
}

std::vector<Step> creature_mind::LookAt(glm::vec2 point, float seconds)
{
	Step turn {.kind = Step::Kind::Move};
	turn.movement = {.kind = Movement::Kind::TurnToFace, .point = point};
	return WithFace({turn, {.kind = Step::Kind::Wait, .seconds = seconds}}, creature_face::Cue::Amazed);
}

std::vector<Step> creature_mind::LookAtCamera()
{
	return {{.kind = Step::Kind::LookAtCamera, .face = creature_face::Cue::Curiosity, .gaze = Gaze {.camera = true}}};
}

std::vector<Step> creature_mind::FollowFor(uint32_t object, float seconds)
{
	Step follow {.kind = Step::Kind::Move, .seconds = seconds};
	follow.movement = {.kind = Movement::Kind::Follow, .object = object, .maxDistance = k_FollowDistance};
	return WithFace({follow}, creature_face::Cue::Amazed);
}

std::vector<Step> creature_mind::RunFrom(glm::vec2 point)
{
	Step flee {.kind = Step::Kind::Move};
	flee.movement = {.kind = Movement::Kind::FleeFrom, .point = point, .run = true};
	return WithFace({flee}, creature_face::Cue::Fear);
}

std::vector<Step> creature_mind::DestroyThing(uint32_t object)
{
	return WithFace({Object({.kind = ObjectOrder::Kind::Destroy, .object = object})}, creature_face::Cue::Anger);
}

std::vector<Step> creature_mind::KickThing(uint32_t object)
{
	const Gaze watch {.object = object};
	return WithFace({{.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::GoNearObject, .object = object, .maxDistance = 0.0f},
	                  .gaze = watch},
	                 {.kind = Step::Kind::Move,
	                  .seconds = k_FaceKickSeconds,
	                  .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = object},
	                  .gaze = watch},
	                 {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::Kick, .object = object}, .gaze = watch}},
	                creature_face::Cue::Anger);
}

std::vector<Step> creature_mind::CatchThing(uint32_t object)
{
	return WithFace({Object({.kind = ObjectOrder::Kind::Catch, .object = object})}, creature_face::Cue::Smile);
}

std::vector<Step> creature_mind::LookAbout(float seconds)
{
	return WithFace({{.kind = Step::Kind::Wait, .seconds = seconds}}, creature_face::Cue::Idle);
}

namespace
{
const Step* CurrentStep(const IdleMind& mind)
{
	return mind.stepStarted && mind.step < mind.agenda.size() ? &mind.agenda[mind.step] : nullptr;
}
} // namespace

bool creature_mind::KeepsWatchingFlying(uint32_t turns, uint32_t turnsPerSecond, float limit, bool flying)
{
	return flying && turnsPerSecond != 0 && static_cast<float>(turns / turnsPerSecond) < limit;
}

bool creature_mind::WatchedHandEnough(uint32_t turns, uint32_t limitTurns, uint32_t turnsPerSecond)
{
	if (turnsPerSecond == 0)
	{
		return true;
	}
	const auto limit = std::min(static_cast<float>(limitTurns / turnsPerSecond), k_LongestHandWatchSeconds);
	return static_cast<float>(turns / turnsPerSecond) > limit;
}

bool creature_mind::IsAsleep(const IdleMind& mind)
{
	const auto* step = CurrentStep(mind);
	return step != nullptr && step->kind == Step::Kind::Static && step->untilRested;
}

bool creature_mind::IsResting(const IdleMind& mind)
{
	const auto* step = CurrentStep(mind);
	return step != nullptr && step->kind == Step::Kind::Static && step->untilBetter && !mind.sitEnding;
}

bool creature_mind::IsUnconscious(const IdleMind& mind)
{
	const auto* step = CurrentStep(mind);
	// A faint step's creature is up again from the turn it starts to get up, though it stays on the step
	return step != nullptr && step->kind == Step::Kind::Static && step->holdLoop && !(step->faint && mind.sitEnding);
}

bool creature_mind::ComesTo(IdleMind& mind)
{
	if (!mind.outCold || mind.step < mind.agenda.size())
	{
		return false;
	}
	mind.outCold = false;
	return true;
}

bool creature_mind::SitCounting(const IdleMind& mind)
{
	const auto* step = CurrentStep(mind);
	return step != nullptr && step->kind == Step::Kind::Static && step->sequence[1] == animations::k_Sit && !mind.sitEnding;
}

std::vector<Step> creature_mind::HangAround(const Random& random)
{
	constexpr uint32_t k_Degrees = 360;
	const auto angle = static_cast<float>(random(k_Degrees)) * std::numbers::pi_v<float> / 180.0f;
	const auto distance = k_HangAroundDistance + static_cast<float>(random(k_HangAroundExtra));
	std::vector<Step> agenda {{.kind = Step::Kind::Move,
	                           .seconds = 0.0f,
	                           .animation = 0,
	                           .sleepyEyes = false,
	                           .movement = {.kind = Movement::Kind::Nearby,
	                                        .point = distance * glm::vec2(std::cos(angle), std::sin(angle)),
	                                        .object = std::nullopt,
	                                        .run = false,
	                                        .minDistance = 0.0f,
	                                        .maxDistance = 1.0f}},
	                          SitDown(random)};
	return WithFace(std::move(agenda), creature_face::Cue::Idle);
}

void creature_mind::Plan(IdleMind& mind, Activity activity, std::vector<Step> agenda)
{
	mind.activity = activity;
	mind.agenda = std::move(agenda);
	++mind.serial;
	mind.step = 0;
	mind.stepStarted = false;
	mind.stepSeconds = 0.0f;
	mind.sitEnding = false;
	mind.wakeWanted = false;
	mind.gaveUp = false;
	// Taking up something else, its eyes look as usual again
	mind.eyesToClear = mind.eyesHeld;
}

std::optional<creature_face::Request> creature_mind::PullFace(IdleMind& mind, creature_face::Cue cue, const Senses& senses,
                                                              const Random& random)
{
	mind.faceSeconds = k_FaceRepeatSeconds;
	auto feelings = senses.feelings;
	feelings.variety = mind.faceVariety;
	// Only the idle face draws, for its pick, before the variety moves on; every other face is set by the feelings
	if (cue == creature_face::Cue::Idle)
	{
		feelings.idlePick = random(creature_face::k_IdleFaceCount);
	}
	const auto face = creature_face::Choose(cue, feelings);
	if (face.has_value())
	{
		mind.faceVariety += random(k_FaceVarietyStep);
	}
	return face;
}

namespace
{
/// The rest of the agenda is no use
void GiveUpAgenda(IdleMind& mind)
{
	mind.step = mind.agenda.size();
	mind.stepStarted = false;
	mind.gaveUp = true;
}

/// Waiting for the dancers to stand still: with no dance there is nothing to wait for
void WaitForDancers(IdleMind& mind, const Senses& senses)
{
	if (!senses.dance.has_value() || !senses.dance->exists)
	{
		GiveUpAgenda(mind);
	}
	else if (!senses.dance->dancersMoving)
	{
		FinishStep(mind);
	}
}

/// A dance step starts: the world carries out its order this turn
void StartDanceStep(IdleMind& mind, const Step& step, const Senses& senses, Commands& commands)
{
	switch (step.dance.kind)
	{
	case DanceOrder::Kind::WaitUntilStill:
		WaitForDancers(mind, senses);
		return;
	case DanceOrder::Kind::StopLooking:
		// Its head looks about this last turn, then stays still
		commands.lookAbout = true;
		break;
	case DanceOrder::Kind::Set:
		// The last dance goes first, and the new one comes the turn after
		mind.danceSet = false;
		commands.dance = DanceOrder {.kind = DanceOrder::Kind::Clear};
		return;
	case DanceOrder::Kind::Dance:
		mind.danceTurns = step.dance.count;
		mind.danceEnding = false;
		break;
	default:
		break;
	}
	commands.dance = step.dance;
}

/// A turn of a dance step under way
void DanceStepTurn(IdleMind& mind, const Step& step, const Senses& senses, Commands& commands, const Random& random)
{
	if (step.dance.kind == DanceOrder::Kind::WaitUntilStill)
	{
		WaitForDancers(mind, senses);
		return;
	}
	if (step.dance.kind == DanceOrder::Kind::Set && !mind.danceSet)
	{
		commands.dance = step.dance;
		mind.danceSet = true;
		return;
	}
	if (step.dance.kind != DanceOrder::Kind::Dance)
	{
		// The world says when it has carried out the order
		return;
	}
	if (mind.danceEnding)
	{
		commands.dance = DanceOrder {.kind = DanceOrder::Kind::Stop};
		FinishStep(mind);
		return;
	}
	if (!senses.dance.has_value() || !senses.dance->inGroup)
	{
		GiveUpAgenda(mind);
		return;
	}
	const auto turn = creature_dance::Turn(*senses.dance, senses.position);
	// A turn already under way is not started again
	if (turn.facePoint.has_value() && !senses.moving)
	{
		commands.move = Movement {.kind = Movement::Kind::TurnToFace, .point = *turn.facePoint};
	}
	// The move that plays an action or starts a pose draws its mirroring every turn it is the move, whether or not
	// the body is free to start it; an action while the creature holds something takes no draw
	if (turn.playOnce.has_value())
	{
		const bool mirrored = !senses.holding && random(2) == 1;
		if (!senses.bodyPlaying)
		{
			commands.playOnce = *turn.playOnce;
			commands.mirrored = mirrored;
		}
	}
	if (turn.overlay.has_value() && !senses.overlayPlaying)
	{
		commands.playOverlay = *turn.overlay;
	}
	commands.endStaticLoop = turn.endPose;
	commands.endDance = turn.endDance;
	if (turn.danceMoveMs.has_value() && !senses.bodyPlaying)
	{
		commands.startDance = *turn.danceMoveMs;
	}
	if (turn.pose.has_value())
	{
		const bool mirrored = random(2) == 1;
		if (!senses.bodyPlaying)
		{
			commands.startSequence = std::array {*turn.pose - 1, *turn.pose, *turn.pose + 1};
			commands.mirrored = mirrored;
		}
	}
	if (turn.face.has_value())
	{
		if (const auto face = creature_face::FaceOf(*turn.face))
		{
			commands.face = creature_face::Request {.face = *face, .milliseconds = turn.faceMs};
		}
	}
	if (turn.walkTo.has_value())
	{
		commands.move =
		    Movement {.kind = Movement::Kind::ToPoint, .point = *turn.walkTo, .maxDistance = creature_dance::k_NearPlace};
	}
	--mind.danceTurns;
	if (mind.danceTurns == 0)
	{
		mind.danceEnding = true;
	}
}
} // namespace

void creature_mind::DanceOrderDone(IdleMind& mind, bool done)
{
	if (mind.step >= mind.agenda.size() || mind.agenda[mind.step].kind != Step::Kind::Dance)
	{
		return;
	}
	if (done)
	{
		FinishStep(mind);
	}
	else
	{
		GiveUpAgenda(mind);
	}
}

Commands creature_mind::Think(IdleMind& mind, const Senses& senses, const Random& random)
{
	Commands commands;
	mind.showDesireSeconds = std::max(mind.showDesireSeconds - senses.seconds, 0.0f);
	if (mind.step >= mind.agenda.size())
	{
		return commands;
	}
	const auto& step = mind.agenda[mind.step];
	// A step with something to look at has the head on it for as long as it lasts
	commands.gaze = step.gaze;
	// A cast step holds its pose's loop as a still step holds a sit. The creature's moves in a dance with villagers
	// start and stop its poses and its dance themselves
	const bool villageDance = step.kind == Step::Kind::Dance && step.dance.kind == DanceOrder::Kind::Dance;
	const bool sitting =
	    ((step.kind == Step::Kind::Static || step.kind == Step::Kind::Cast) && mind.stepStarted) || villageDance;
	const bool dancing = step.kind == Step::Kind::PracticeDance || villageDance;
	// A step that goes on pulls its face again every few seconds
	if (mind.stepStarted && step.face != creature_face::Cue::None)
	{
		mind.faceSeconds -= senses.seconds;
		if (mind.faceSeconds < k_FaceDue)
		{
			commands.face = PullFace(mind, step.face, senses, random);
		}
	}
	// A sit nobody is waiting on any more ends
	if (senses.bodyLooping && !sitting)
	{
		commands.endSit = true;
	}
	// A dance nobody waits on any more stops
	if (senses.bodyDancing && !dancing)
	{
		commands.endDance = true;
	}

	if (!mind.stepStarted)
	{
		commands.lookAbout = step.kind == Step::Kind::Wait && !step.gaze.has_value();
		// A swap of minds waits until no spell is on either creature, bringing those on itself to their end meanwhile
		if (step.kind == Step::Kind::SwapMind && !senses.swap.spellsGone)
		{
			commands.finishSpells = true;
			return commands;
		}
		// Nothing starts while the body still plays an action, but a wait. A stop starts while the body moves, to stop
		// it, and a dance alone waits only while the body plays an action
		if (step.kind != Step::Kind::Wait && step.kind != Step::Kind::WaitUntilFree && step.kind != Step::Kind::StopMoving &&
		    step.kind != Step::Kind::PracticeDance && senses.bodyBusy)
		{
			return commands;
		}
		if (step.kind == Step::Kind::PracticeDance && senses.bodyPlaying)
		{
			return commands;
		}
		mind.stepStarted = true;
		mind.stepSeconds = 0.0f;
		if (step.kind == Step::Kind::WaitUntilFree)
		{
			// Its first look at the body is on the turn it starts; it pulls no face
			mind.bodyWasFree = !senses.bodyBusy;
			return commands;
		}
		if (step.kind == Step::Kind::TurnToFacePoint)
		{
			// Right on the point, or facing it to within an eighth of a right angle, it has no need to turn
			constexpr float k_OnThePoint = 0.1f;
			constexpr float k_FacingWithin = std::numbers::pi_v<float> / 8.0f;
			const auto offset = step.movement.point - senses.position;
			if (glm::length(offset) >= k_OnThePoint &&
			    std::abs(creature_locomotion::WrapAngle(creature_locomotion::HeadingOf(offset) - senses.heading)) >=
			        k_FacingWithin)
			{
				commands.move = step.movement;
			}
			return commands;
		}
		if (step.kind == Step::Kind::ClearObjectToActOn)
		{
			commands.clearObjectToActOn = true;
			FinishStep(mind);
			return commands;
		}
		if (step.kind == Step::Kind::ClearObjectToUse)
		{
			commands.clearObjectToUse = true;
			FinishStep(mind);
			return commands;
		}
		if (step.kind == Step::Kind::CoolDown)
		{
			commands.coolDown = true;
			FinishStep(mind);
			return commands;
		}
		// Each step pulls the face it is meant to as it starts, held for as long as the face goes with
		commands.face = PullFace(mind, step.face, senses, random);
		if (mind.eyesToClear)
		{
			commands.eyes = Eyes::Normal;
			mind.eyesHeld = false;
			mind.eyesToClear = false;
		}
		if (step.flutterEyelids)
		{
			// Its eyes go back to their usual blinking once it takes up something else
			commands.blinkIntervalMs = k_FlutterBlinkIntervalMs;
			mind.eyesHeld = true;
		}
		switch (step.kind)
		{
		case Step::Kind::Action:
			commands.playOnce = step.animation;
			commands.mirrored = random(2) == 1;
			if (step.sleepyEyes)
			{
				commands.eyes = Eyes::Sleepy;
			}
			else if (step.heldEyes != Eyes::Unchanged)
			{
				commands.eyes = step.heldEyes;
			}
			break;
		case Step::Kind::Static:
			commands.startSequence = step.sequence;
			commands.holdLoop = step.holdLoop;
			// The turn that the rest starts is its first turn
			mind.restTurns = 1;
			// Only a step that may be mirrored draws for it, so that the others keep their draws
			if (step.mirrorAtRandom)
			{
				commands.mirrored = random(2) == 1;
			}
			commands.fainted = step.faint;
			if (step.faint)
			{
				mind.outCold = true;
			}
			if (step.closedEyes)
			{
				commands.eyes = Eyes::Closed;
				// Asleep or out cold, it pulls no face
				commands.face.reset();
				commands.relaxFace = true;
			}
			break;
		case Step::Kind::Move:
			commands.move = step.movement;
			if (step.movement.kind == Movement::Kind::Nearby)
			{
				commands.move->kind = Movement::Kind::ToPoint;
				commands.move->point = senses.position + step.movement.point;
			}
			break;
		case Step::Kind::Gesture:
			commands.gesture = static_cast<uint32_t>(step.animation);
			break;
		case Step::Kind::Cast:
			commands.startSequence = step.sequence;
			mind.castDone = false;
			mind.castTurns = 0;
			// The pose's start draws a variant as an action's start does; what the variant chooses is not known, so it
			// is drawn and left unused
			static_cast<void>(random(2));
			break;
		case Step::Kind::Object:
			commands.object = step.order;
			if (step.order.kind == ObjectOrder::Kind::ThrowNearby)
			{
				commands.object->kind = ObjectOrder::Kind::Throw;
				commands.object->point = senses.position + step.order.point;
			}
			break;
		case Step::Kind::LookAtCamera:
			mind.lookTurns = 0;
			// Watching for a set number of turns, its first turn is one of them
			if (step.turns.has_value())
			{
				WatchCameraForTurns(mind, *step.turns, senses, commands);
			}
			break;
		case Step::Kind::Overlay:
			// Nothing starts on top of the body while something already plays there; the step waits for it all the same
			commands.playOverlay = step.animation;
			break;
		case Step::Kind::HealItself:
			commands.healItself = true;
			FinishStep(mind);
			break;
		case Step::Kind::Face:
			// The face is held for the step's seconds, and the next step starts on the next turn
			if (const auto face = creature_face::FaceOf(step.animation))
			{
				commands.face =
				    creature_face::Request {.face = *face, .milliseconds = step.seconds * 1000.0f, .cue = step.face};
			}
			commands.effect = step.effect;
			FinishStep(mind);
			break;
		case Step::Kind::FollowCamera:
			// The time is counted in whole turns from the next
			mind.followCameraTurns = static_cast<uint32_t>(step.seconds * static_cast<float>(senses.turnsPerSecond));
			break;
		case Step::Kind::WatchHand:
			// Its first turn is one of those it watches for
			mind.watchTurns = 0;
			WatchHand(mind, step, senses, random, commands);
			break;
		case Step::Kind::FollowHand:
			// The time is set as the step starts, and the hand is followed from the next turn
			mind.followHandTurns = creature_player_attention::FollowHandTurns(senses.turnsPerSecond);
			break;
		case Step::Kind::WatchPlayer:
			WatchPlayer(mind, senses, commands);
			break;
		case Step::Kind::StopMoving:
			commands.stopMoving = true;
			break;
		case Step::Kind::Dance:
			StartDanceStep(mind, step, senses, commands);
			break;
		case Step::Kind::PracticeDance:
			commands.startDance = creature_dance::k_PracticeMoveMs;
			mind.danceTurns = static_cast<uint32_t>(step.seconds * static_cast<float>(creature_dance::k_TurnsPerSecond));
			break;
		case Step::Kind::RaiseTotem:
		case Step::Kind::LowerTotem:
			// The totem starts to move on the next turn
			break;
		case Step::Kind::MakeDisciple:
			commands.makeDisciple = step.object;
			commands.effect = step.effect;
			FinishStep(mind);
			break;
		case Step::Kind::Kiss:
			commands.playOnce = step.animation;
			break;
		case Step::Kind::WatchFlying:
			// Its first turn is one of those it watches for
			mind.watchTurns = 0;
			WatchFlying(mind, senses, random);
			break;
		case Step::Kind::Inspect:
		{
			// The time is picked as the step starts, and counted in whole turns from the next
			const auto extra =
			    static_cast<float>(random(game_random::k_FloatRandRange)) * step.extraSeconds * game_random::FloatRandScale();
			mind.inspectTurns = static_cast<uint32_t>((extra + step.seconds) * static_cast<float>(senses.turnsPerSecond));
			break;
		}
		// The steps of a joint action are done as they start. When the other creature refuses to be the partner, the
		// mind's owner gives up the rest of the agenda.
		case Step::Kind::RequestPartner:
			if (step.object.has_value())
			{
				commands.requestPartner = Commands::PartnerRequest {.creature = *step.object, .kind = step.request};
			}
			FinishStep(mind);
			break;
		case Step::Kind::Order:
			if (step.object.has_value() && step.ordered != nullptr)
			{
				commands.order = Commands::PartnerOrder {.partner = *step.object, .step = step.ordered};
			}
			FinishStep(mind);
			break;
		case Step::Kind::Demobilise:
		case Step::Kind::Remobilise:
			if (step.object.has_value())
			{
				commands.demobilise = std::pair {*step.object, step.kind == Step::Kind::Demobilise};
			}
			FinishStep(mind);
			break;
		case Step::Kind::WaitForStill:
			// The step looks at the object on the turn that it starts too
			if (!senses.objectMoving)
			{
				FinishStep(mind);
			}
			break;
		case Step::Kind::Apply:
			commands.effect = step.effect;
			FinishStep(mind);
			break;
		case Step::Kind::ApplyDesire:
			// The step does not end by itself: the new plan takes the place of the agenda, and with no plan the agenda
			// stops. Without a thing to apply the desire to, the rest of the agenda is given up.
			if (step.object.has_value())
			{
				commands.applyDesire = Commands::DesireOn {.desire = step.desire, .thing = *step.object};
			}
			else
			{
				mind.step = mind.agenda.size();
				mind.stepStarted = false;
				mind.gaveUp = true;
			}
			break;
		case Step::Kind::SwapMind:
			commands.playOnce = step.animation;
			commands.mirrored = random(2) == 1;
			commands.startSwapBeam = true;
			break;
		case Step::Kind::Shatter:
			commands.shatter = step.object;
			FinishStep(mind);
			break;
		case Step::Kind::Repair:
			commands.repair = step.object;
			FinishStep(mind);
			break;
		case Step::Kind::UseUp:
			commands.useUp = step.object;
			FinishStep(mind);
			break;
		case Step::Kind::Fight:
			// Without the other creature, the rest of the agenda is no use
			if (step.object.has_value())
			{
				commands.fight = *step.object;
			}
			else
			{
				mind.step = mind.agenda.size();
				mind.stepStarted = false;
				mind.gaveUp = true;
			}
			break;
		case Step::Kind::Wait:
		case Step::Kind::WaitInMap:
		case Step::Kind::TurnToFacePoint:
		case Step::Kind::ClearObjectToActOn:
		case Step::Kind::ClearObjectToUse:
		case Step::Kind::WaitUntilFree:
		case Step::Kind::CoolDown:
			break;
		}
		return commands;
	}

	mind.stepSeconds += senses.seconds;
	switch (step.kind)
	{
	case Step::Kind::ClearObjectToActOn:
	case Step::Kind::ClearObjectToUse:
	case Step::Kind::HealItself:
	case Step::Kind::Face:
	case Step::Kind::CoolDown:
	case Step::Kind::RequestPartner:
	case Step::Kind::Order:
	case Step::Kind::Demobilise:
	case Step::Kind::Remobilise:
	case Step::Kind::Shatter:
	case Step::Kind::Repair:
	case Step::Kind::UseUp:
		// Done as it starts
		FinishStep(mind);
		break;
	case Step::Kind::FollowCamera:
		FollowCamera(mind, step, senses, commands);
		break;
	case Step::Kind::WatchHand:
		WatchHand(mind, step, senses, random, commands);
		break;
	case Step::Kind::FollowHand:
		FollowHand(mind, senses, commands);
		break;
	case Step::Kind::WatchPlayer:
		WatchPlayer(mind, senses, commands);
		break;
	case Step::Kind::WatchFlying:
		WatchFlying(mind, senses, random);
		break;
	case Step::Kind::Inspect:
		if (mind.inspectTurns > 0)
		{
			--mind.inspectTurns;
		}
		if (mind.inspectTurns == 0)
		{
			commands.effect = Effect::Inspected;
			commands.effectObject = step.object;
			FinishStep(mind);
		}
		break;
	case Step::Kind::SwapMind:
		// From the turn after the beam starts it takes on the shape, done once it has, whether or not the action has
		// played out
		if (senses.swap.settled)
		{
			FinishStep(mind);
		}
		else
		{
			commands.easeSwap = true;
		}
		break;
	case Step::Kind::TurnToFacePoint:
		// Done once the turn is over, or on the turn after its start when it had no need to turn
		if (!senses.moving)
		{
			FinishStep(mind);
		}
		break;
	case Step::Kind::LookAtCamera:
	{
		if (step.turns.has_value())
		{
			WatchCameraForTurns(mind, *step.turns, senses, commands);
			break;
		}
		// In the game's order: without a player it is done; right on the camera along the ground it is done; more
		// than a right angle off, it turns to it; then its five whole seconds, a camera farther than a hundred metres
		// through the air, and the player's hand still for more than five seconds of turns, end it
		constexpr float k_OnTheCamera = 0.1f;
		constexpr float k_CameraFarthest = 100.0f;
		constexpr uint32_t k_LookSeconds = 5;
		// A step that gives its own seconds looks that many whole seconds
		const uint32_t lookSeconds = step.seconds > 0.0f ? static_cast<uint32_t>(step.seconds) : k_LookSeconds;
		++mind.lookTurns;
		if (!senses.hasPlayer || !senses.camera.has_value())
		{
			FinishStep(mind);
			break;
		}
		if (glm::length(senses.position - *senses.camera) < k_OnTheCamera)
		{
			FinishStep(mind);
			break;
		}
		const auto off =
		    creature_locomotion::WrapAngle(creature_locomotion::HeadingOf(*senses.camera - senses.position) - senses.heading);
		if (std::abs(off) > std::numbers::pi_v<float> / 2.0f)
		{
			commands.move = Movement {.kind = Movement::Kind::TurnToFace, .point = *senses.camera};
		}
		if ((senses.turnsPerSecond != 0 && mind.lookTurns / senses.turnsPerSecond > lookSeconds) ||
		    senses.cameraDistance > k_CameraFarthest || senses.handStillTurns > senses.turnsPerSecond * k_LookSeconds)
		{
			FinishStep(mind);
		}
		break;
	}
	case Step::Kind::Overlay:
		if (!senses.overlayPlaying)
		{
			FinishStep(mind);
		}
		break;
	case Step::Kind::StopMoving:
		if (!senses.bodyPlaying)
		{
			FinishStep(mind);
		}
		break;
	case Step::Kind::WaitForStill:
		if (!senses.objectMoving)
		{
			FinishStep(mind);
		}
		break;
	case Step::Kind::Dance:
		DanceStepTurn(mind, step, senses, commands, random);
		break;
	case Step::Kind::PracticeDance:
		if (mind.danceTurns > 0)
		{
			--mind.danceTurns;
		}
		// The dance alone ends its action, which stops the dance
		if (mind.danceTurns == 0)
		{
			commands.endDance = true;
			FinishStep(mind);
		}
		break;
	case Step::Kind::RaiseTotem:
	case Step::Kind::LowerTotem:
	{
		if (!senses.totemShare.has_value() || !step.object.has_value())
		{
			// No town to worship: the rest of the agenda is no use without it
			mind.step = mind.agenda.size();
			mind.stepStarted = false;
			mind.gaveUp = true;
			break;
		}
		const auto turn = creature_town_help::MoveTotemTurn(*senses.totemShare, step.kind == Step::Kind::RaiseTotem);
		commands.totem = Commands::TotemShare {.totem = *step.object, .share = turn.share};
		if (turn.done)
		{
			commands.effect = step.effect;
			FinishStep(mind);
		}
		break;
	}
	case Step::Kind::MakeDisciple:
		FinishStep(mind);
		break;
	case Step::Kind::Kiss:
		if (!senses.bodyBusy)
		{
			FinishStep(mind);
		}
		break;
	case Step::Kind::Apply:
		FinishStep(mind);
		break;
	case Step::Kind::ApplyDesire:
		break;
	case Step::Kind::Fight:
		// The mind thinks again only once the creature is out of its fight and back on its feet
		FinishStep(mind);
		break;
	case Step::Kind::Wait:
		commands.lookAbout = !step.gaze.has_value();
		if (mind.stepSeconds >= step.seconds)
		{
			FinishStep(mind);
		}
		break;
	case Step::Kind::WaitUntilFree:
		// Done on the turn after the body was first seen free
		if (mind.bodyWasFree)
		{
			FinishStep(mind);
		}
		else
		{
			mind.bodyWasFree = !senses.bodyBusy;
		}
		break;
	case Step::Kind::WaitInMap:
		if (!senses.objectInMap.has_value())
		{
			// What it waited for is gone: the rest of the agenda is no use without it
			mind.step = mind.agenda.size();
			mind.stepStarted = false;
			mind.gaveUp = true;
		}
		else if (*senses.objectInMap)
		{
			FinishStep(mind);
		}
		break;
	case Step::Kind::Action:
		if (!senses.bodyBusy)
		{
			if (step.sleepyEyes || step.heldEyes != Eyes::Unchanged)
			{
				commands.eyes = Eyes::Normal;
			}
			if (step.effect != Effect::None)
			{
				commands.effect = step.effect;
			}
			FinishStep(mind);
		}
		break;
	case Step::Kind::Static:
	{
		commands.lookAbout = senses.bodyLooping && !step.closedEyes;
		// Each turn of the rest its fire stops, until it starts to get up
		if (step.untilBetter && !mind.sitEnding)
		{
			++mind.restTurns;
			commands.coolDown = true;
		}
		if (step.faint && !step.staysDown)
		{
			// The creature gets up on the first turn that it is seen lying, or when told to wake: it does not wait, its
			// life is not changed and it is still out cold. Its eyes open as it starts to get up. The step then never
			// ends, whatever the body does, so the creature stays on it once it is up
			if (!mind.sitEnding && (senses.bodyLooping || mind.wakeWanted))
			{
				commands.endSit = true;
				commands.eyes = Eyes::Normal;
				mind.sitEnding = true;
				mind.wakeWanted = false;
			}
			break;
		}
		const bool better = senses.better && mind.restTurns > k_FewestRestTurns;
		const bool done = mind.wakeWanted || (step.untilBetter   ? better
		                                      : step.untilRested ? senses.rested
		                                                         : mind.stepSeconds >= step.seconds);
		if (!senses.bodyBusy)
		{
			if (step.closedEyes)
			{
				commands.eyes = Eyes::Normal;
			}
			// The body played the whole sequence before its time was up: it still takes effect. The rest takes effect
			// once the creature is up
			if (!mind.sitEnding || step.untilBetter)
			{
				commands.effect = step.effect;
			}
			FinishStep(mind);
		}
		// A faint is ended only once the creature lies: its fall, slow for a big creature, can outlast its time
		else if (!mind.sitEnding && done && (!step.holdLoop || senses.bodyLooping))
		{
			commands.endSit = true;
			if (!step.untilBetter)
			{
				commands.effect = step.effect;
			}
			mind.sitEnding = true;
			mind.wakeWanted = false;
		}
		break;
	}
	case Step::Kind::Object:
		if (senses.hands == HandsState::Done)
		{
			commands.effect = step.effect;
			if (step.effect == Effect::TookFromHand)
			{
				commands.effectObject = step.order.object;
			}
			FinishStep(mind);
		}
		else if (senses.hands != HandsState::Busy)
		{
			// It couldn't, or was stopped: the rest of the agenda is no use without it
			mind.step = mind.agenda.size();
			mind.stepStarted = false;
			mind.gaveUp = true;
		}
		break;
	case Step::Kind::Cast:
		if (!mind.castDone)
		{
			// The miracle is cast as the loop of the pose begins, and held from the next turn for the step's seconds in
			// the game's whole turns, the fraction dropped
			if (senses.bodyLooping)
			{
				commands.cast = step.cast;
				mind.castDone = true;
				mind.castTurns = static_cast<uint32_t>(step.seconds * static_cast<float>(senses.turnsPerSecond));
			}
			else if (!senses.bodyBusy)
			{
				// The pose ended before it could cast
				mind.step = mind.agenda.size();
				mind.stepStarted = false;
				mind.gaveUp = true;
			}
		}
		else if (mind.castTurns > 0)
		{
			--mind.castTurns;
		}
		else
		{
			// Its time up, it lets the miracle go and ends the pose, done once the end has played
			if (!mind.sitEnding)
			{
				commands.releaseCast = true;
				commands.endSit = true;
				mind.sitEnding = true;
			}
			if (!senses.bodyBusy)
			{
				FinishStep(mind);
			}
		}
		break;
	case Step::Kind::Gesture:
	case Step::Kind::Move:
		if (step.kind == Step::Kind::Gesture || IsSubMove(step.movement.kind))
		{
			if (senses.subMove == SubMove::Done)
			{
				// Done, its step's effect, if any, takes place, as a walk's does
				FinishWalk(mind, step, commands);
			}
			else if (senses.subMove == SubMove::Failed)
			{
				mind.step = mind.agenda.size();
				mind.stepStarted = false;
				mind.gaveUp = true;
			}
			break;
		}
		// Facing down the slope is done once the body is free: the turn over and no action playing
		if (step.movement.kind == Movement::Kind::FaceDownSlope)
		{
			if (!senses.bodyBusy)
			{
				FinishStep(mind);
			}
			break;
		}
		// Done once it has arrived or given up, or its time is up. A walk that counts the action as done says so then
		if (!senses.moving)
		{
			FinishWalk(mind, step, commands);
		}
		else if (step.seconds > 0.0f && mind.stepSeconds >= step.seconds)
		{
			commands.stopMoving = true;
			FinishWalk(mind, step, commands);
		}
		break;
	}
	return commands;
}
