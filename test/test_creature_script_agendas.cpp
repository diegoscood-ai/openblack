/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureLayers.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureScriptAgendas.h"

using namespace openblack;
using namespace openblack::creature_mind;

namespace
{
uint32_t NoRandom(uint32_t /*range*/)
{
	return 0;
}
} // namespace

// "force CreatureCow CREATURE_LOOK_FOREVER CowLookPos": it turns to the thing, holding still two seconds, then watches it
// for a day
TEST(CreatureScriptAgendas, LookingForeverTurnsToTheThingAndWatchesItForADay)
{
	const auto agenda = LookForever(7);
	ASSERT_EQ(agenda.size(), 2);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[0].movement.object, 7u);
	EXPECT_FLOAT_EQ(agenda[0].seconds, 2.0f);
	// its head on the thing as it turns, as after
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_EQ(agenda[0].gaze->object, 7u);
	EXPECT_FALSE(agenda[0].gaze->bottom);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Wait);
	EXPECT_FLOAT_EQ(agenda[1].seconds, 86400.0f);
	ASSERT_TRUE(agenda[1].gaze.has_value());
	EXPECT_EQ(agenda[1].gaze->object, 7u);
	EXPECT_FALSE(agenda[1].gaze->bottom);
}

// Looking without going up to it, it turns to the thing with its head on it, then watches the thing's foot for as long
// as was drawn
TEST(CreatureScriptAgendas, LookingWithoutApproachingWatchesTheFootAWhile)
{
	const auto agenda = LookButDontApproach(3, 1.7f);
	ASSERT_EQ(agenda.size(), 2);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[0].seconds, 2.0f);
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_FALSE(agenda[0].gaze->bottom);
	EXPECT_FLOAT_EQ(agenda[1].seconds, 1.7f);
	ASSERT_TRUE(agenda[1].gaze.has_value());
	EXPECT_TRUE(agenda[1].gaze->bottom);
}

// Pointing at a thing lasts five seconds and follows it; at the camera, a second at the camera's height, amazed, its
// head on the camera's point, turning first only with a player, its head on the camera and pulling no face
TEST(CreatureScriptAgendas, PointingAtAThingOrTheCamera)
{
	const auto thing = PointAtThing(9);
	ASSERT_EQ(thing.size(), 1);
	EXPECT_EQ(thing[0].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(thing[0].order.object, 9u);
	EXPECT_EQ(thing[0].order.seconds, 5.0f);
	EXPECT_EQ(thing[0].face, creature_face::Cue::Amazed);
	ASSERT_TRUE(thing[0].gaze.has_value());
	EXPECT_EQ(thing[0].gaze->object, std::optional<uint32_t>(9));
	EXPECT_TRUE(thing[0].gaze->itself);
	// A script's CREATURE_DO_ACTION of the row reaches it, with a thing to point at
	const auto* row = creature_plan_actions::For("PointAtObject");
	ASSERT_NE(row, nullptr);
	EXPECT_EQ(row->target, creature_plan_actions::Target::Anything);
	EXPECT_FALSE(creature_plan_actions::Agenda(*row, std::nullopt, {}, {}, [](uint32_t) { return 0u; }).has_value());
	const auto forced = creature_plan_actions::Agenda(*row, 9, {}, {}, [](uint32_t) { return 0u; });
	ASSERT_TRUE(forced.has_value());
	ASSERT_EQ(forced->size(), 1u);
	EXPECT_EQ((*forced)[0].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ((*forced)[0].order.object, 9u);

	const glm::vec3 camera {10.0f, 40.0f, 20.0f};
	const auto unowned = PointAtCamera(false, camera);
	ASSERT_EQ(unowned.size(), 1);
	EXPECT_EQ(unowned[0].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(unowned[0].order.point, glm::vec2(10.0f, 20.0f));
	EXPECT_EQ(unowned[0].order.pointHeight, 40.0f);
	EXPECT_EQ(unowned[0].order.seconds, 1.0f);
	EXPECT_EQ(unowned[0].face, creature_face::Cue::Amazed);
	ASSERT_TRUE(unowned[0].gaze.has_value());
	EXPECT_EQ(unowned[0].gaze->point, camera);
	const auto owned = PointAtCamera(true, camera);
	ASSERT_EQ(owned.size(), 2);
	EXPECT_EQ(owned[0].kind, Step::Kind::Move);
	EXPECT_EQ(owned[0].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(owned[0].movement.point, glm::vec2(10.0f, 20.0f));
	EXPECT_EQ(owned[0].face, creature_face::Cue::None);
	ASSERT_TRUE(owned[0].gaze.has_value());
	EXPECT_TRUE(owned[0].gaze->camera);
	EXPECT_EQ(owned[1].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(owned[1].order.seconds, 1.0f);
}

// PointAtCamera, which a creature idling with its player may choose by itself, is that agenda: chosen with no camera to
// point at, its agenda can't be made. LookAtCamera's always can, curious, its head on the camera
TEST(CreatureScriptAgendas, TheCameraActionsAreCarriedOutAsTheirAgendas)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const auto* point = plan_actions::For("PointAtCamera");
	const auto* look = plan_actions::For("LookAtCamera");
	ASSERT_NE(point, nullptr);
	ASSERT_NE(look, nullptr);
	plan_actions::Situation situation;
	EXPECT_TRUE(plan_actions::Possible(*point, situation));
	EXPECT_FALSE(plan_actions::Agenda(*point, std::nullopt, {}, situation, NoRandom).has_value());
	const auto blind = plan_actions::Agenda(*look, std::nullopt, {}, situation, NoRandom);
	ASSERT_TRUE(blind.has_value());
	ASSERT_EQ(blind->size(), 1u);
	EXPECT_EQ(blind->front().kind, Step::Kind::LookAtCamera);
	EXPECT_EQ(blind->front().face, creature_face::Cue::Curiosity);
	ASSERT_TRUE(blind->front().gaze.has_value());
	EXPECT_TRUE(blind->front().gaze->camera);

	situation.eye = {10.0f, 40.0f, 20.0f};
	situation.camera = glm::vec2(10.0f, 20.0f);
	const auto unowned = plan_actions::Agenda(*point, std::nullopt, {}, situation, NoRandom);
	ASSERT_TRUE(unowned.has_value());
	ASSERT_EQ(unowned->size(), 1u);
	EXPECT_EQ(unowned->front().order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(unowned->front().order.pointHeight, 40.0f);
	situation.hasPlayer = true;
	const auto owned = plan_actions::Agenda(*point, std::nullopt, {}, situation, NoRandom);
	ASSERT_TRUE(owned.has_value());
	ASSERT_EQ(owned->size(), 2u);
	EXPECT_EQ(owned->front().movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(owned->back().order.kind, ObjectOrder::Kind::PointAt);
}

// "force CreatureTiger CREATURE_SIT TigerBasePos": walks there, faces the place, then down the slope, and sits
TEST(CreatureScriptAgendas, SittingWhereAScriptSays)
{
	const auto agenda = SitDownAt(4, {100.0f, 200.0f}, 3.0f, 12.5f);
	ASSERT_EQ(agenda.size(), 4);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(100.0f, 200.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 3.0f);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[1].seconds, 0.1f);
	ASSERT_TRUE(agenda[1].gaze.has_value());
	EXPECT_EQ(agenda[1].gaze->object, 4u);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::FaceDownSlope);
	// the game's sit, 38 with its start and end, for the seconds drawn, pulling no face
	EXPECT_EQ(agenda[3].kind, Step::Kind::Static);
	EXPECT_EQ(agenda[3].sequence, (std::array<size_t, 3> {37, 38, 39}));
	EXPECT_FLOAT_EQ(agenda[3].seconds, 12.5f);
	EXPECT_EQ(agenda[3].face, creature_face::Cue::None);
	// facing down the slope and sitting, the head looks about throughout: a gaze that names nothing
	for (const auto& step : {agenda[2], agenda[3]})
	{
		ASSERT_TRUE(step.gaze.has_value());
		EXPECT_FALSE(step.gaze->object.has_value());
		EXPECT_FALSE(step.gaze->point.has_value());
		EXPECT_FALSE(step.gaze->camera);
	}
}

// The actions scripts force: looking for a day, looking without going up (only with its time drawn), and sitting at a
// thing under a script's control (else on the spot, as a creature sits by itself)
TEST(CreatureScriptAgendas, TheScriptsLookAndSitActionsAreCarriedOutAsTheirAgendas)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const auto* forever = plan_actions::For("LookForever");
	const auto* look = plan_actions::For("LookButDontApproach");
	const auto* sit = plan_actions::For("SitDown");
	ASSERT_NE(forever, nullptr);
	ASSERT_NE(look, nullptr);
	ASSERT_NE(sit, nullptr);
	EXPECT_EQ(forever->target, plan_actions::Target::Anything);
	EXPECT_EQ(look->target, plan_actions::Target::Anything);
	plan_actions::Situation situation;
	const glm::vec2 at {30.0f, 40.0f};
	const auto day = plan_actions::Agenda(*forever, 7u, at, situation, NoRandom);
	ASSERT_TRUE(day.has_value());
	EXPECT_EQ(day->size(), 2u);
	EXPECT_FALSE(plan_actions::Agenda(*look, 7u, at, situation, NoRandom).has_value());
	EXPECT_FALSE(plan_actions::Agenda(*forever, std::nullopt, at, situation, NoRandom).has_value());
	situation.drawnSeconds = 1.5f;
	const auto awhile = plan_actions::Agenda(*look, 7u, at, situation, NoRandom);
	ASSERT_TRUE(awhile.has_value());
	EXPECT_FLOAT_EQ(awhile->back().seconds, 1.5f);
	// by itself, or with no time drawn, it sits on the spot
	const auto onTheSpot = plan_actions::Agenda(*sit, 7u, at, situation, NoRandom);
	ASSERT_TRUE(onTheSpot.has_value());
	EXPECT_EQ(onTheSpot->size(), 1u);
	situation.controlledByScript = true;
	situation.radius = 4.0f;
	situation.drawnSeconds = 11.0f;
	const auto there = plan_actions::Agenda(*sit, 7u, at, situation, NoRandom);
	ASSERT_TRUE(there.has_value());
	ASSERT_EQ(there->size(), 4u);
	EXPECT_EQ(there->front().movement.point, at);
	EXPECT_FLOAT_EQ(there->front().movement.maxDistance, 4.0f);
	EXPECT_FLOAT_EQ(there->back().seconds, 11.0f);
}

// "move CreatureCow position to [CowBasePos]": a sixth of the way first (at least its height, at most 0.7 of it and
// 22), then within its height; with a distance, within it twice
TEST(CreatureScriptAgendas, AScriptSendsACreatureInTwoGoes)
{
	const glm::vec3 to {600.0f, 0.0f, 0.0f};
	const auto far = MoveForScript({0.0f, 0.0f}, to, 15.0f, 0.0f);
	ASSERT_EQ(far.size(), 2);
	EXPECT_FLOAT_EQ(far[0].movement.maxDistance, (0.7f * 15.0f) + 22.0f);
	EXPECT_FLOAT_EQ(far[1].movement.maxDistance, 15.0f);
	EXPECT_EQ(far[0].movement.point, glm::vec2(600.0f, 0.0f));
	ASSERT_TRUE(far[0].gaze.has_value());

	const auto middling = MoveForScript({0.0f, 0.0f}, {120.0f, 0.0f, 0.0f}, 15.0f, 0.0f);
	EXPECT_FLOAT_EQ(middling[0].movement.maxDistance, 20.0f);
	const auto near = MoveForScript({0.0f, 0.0f}, {30.0f, 0.0f, 0.0f}, 15.0f, 0.0f);
	EXPECT_FLOAT_EQ(near[0].movement.maxDistance, 15.0f);
	// A sixth of the way just its height: the game takes its height only when that is more, so the cap
	const auto even = MoveForScript({0.0f, 0.0f}, {600.0f, 0.0f, 0.0f}, 100.0f, 0.0f);
	EXPECT_FLOAT_EQ(even[0].movement.maxDistance, (0.7f * 100.0f) + 22.0f);
	const auto given = MoveForScript({0.0f, 0.0f}, to, 15.0f, 4.0f);
	EXPECT_FLOAT_EQ(given[0].movement.maxDistance, 4.0f);
	EXPECT_FLOAT_EQ(given[1].movement.maxDistance, 4.0f);
}

// A script's focus: it turns only when more than an eighth of a half turn off, and not on top of the point
TEST(CreatureScriptAgendas, FacingAScriptsPointTurnsOnlyWhenWellOff)
{
	EXPECT_FALSE(NeedsToTurnToFace(50.0f, 0.3f));
	EXPECT_FALSE(NeedsToTurnToFace(50.0f, -0.39f));
	EXPECT_TRUE(NeedsToTurnToFace(50.0f, 0.4f));
	EXPECT_TRUE(NeedsToTurnToFace(50.0f, -3.0f));
	EXPECT_FALSE(NeedsToTurnToFace(0.05f, 3.0f));
	const auto face = FaceForScript({1.0f, 2.0f, 3.0f});
	ASSERT_EQ(face.size(), 1);
	EXPECT_EQ(face[0].movement.kind, Movement::Kind::FacePoint);
	EXPECT_EQ(face[0].movement.point, glm::vec2(1.0f, 3.0f));
	// facing a thing, it turns to it and holds still two seconds once it does, its head on it
	const auto thing = FaceThingForScript(42);
	ASSERT_EQ(thing.size(), 1);
	EXPECT_EQ(thing[0].movement.kind, Movement::Kind::TurnToFaceObject);
	ASSERT_TRUE(thing[0].movement.object.has_value());
	EXPECT_EQ(*thing[0].movement.object, 42u);
	EXPECT_FLOAT_EQ(thing[0].seconds, k_FaceThingSeconds);
	ASSERT_TRUE(thing[0].gaze.has_value() && thing[0].gaze->object.has_value());
	EXPECT_EQ(*thing[0].gaze->object, 42u);
}

// Down the slope: the lowest of eight points ten away, the first when several are as low, the map's corner when none
// is on the land
TEST(CreatureScriptAgendas, DownTheSlopeIsTheLowestGroundAbout)
{
	const auto lowest = DownSlope({100.0f, 100.0f}, [](glm::vec2 at) -> std::optional<float> { return at.x + at.y; });
	EXPECT_EQ(lowest, glm::vec2(90.0f, 90.0f));
	const auto flat = DownSlope({100.0f, 100.0f}, [](glm::vec2) -> std::optional<float> { return 5.0f; });
	EXPECT_EQ(flat, glm::vec2(110.0f, 100.0f));
	const auto off = DownSlope({100.0f, 100.0f}, [](glm::vec2) -> std::optional<float> { return std::nullopt; });
	EXPECT_EQ(off, glm::vec2(0.0f, 0.0f));
}

// The look on the way to a scripted sit, with the species' numbers in info.dat: nothing of its own within 8, the point
// within 25; farther, the point, what interests it and a sweep, two seconds and a turn each
TEST(CreatureScriptAgendas, OnTheWayToAPointItLooksAtThePointAtWhatInterestsItAndAboutInTurn)
{
	constexpr uint32_t k_TurnsPerSecond = 10;
	const WalkLookInfo info {.sweepAngle = 1.5f,
	                         .sweepStep = 0.1f,
	                         .firstDistance = 15.0f,
	                         .farthest = 95.0f,
	                         .distanceStep = 10.0f,
	                         .pointWithin = 25.0f};
	auto look = StartWalkLook(info);
	EXPECT_EQ(look.look, 0u);
	EXPECT_FLOAT_EQ(look.step, 0.1f);
	EXPECT_FLOAT_EQ(look.distance, 15.0f);
	const glm::vec2 from {100.0f, 100.0f};
	// near the point: nothing of its own within 8, the point within 25, and the turns are not counted
	EXPECT_EQ(LookWhileGoing(look, info, from, {105.0f, 100.0f}, 0.0f, true, k_TurnsPerSecond).kind, WalkLookAt::Kind::Nothing);
	EXPECT_EQ(LookWhileGoing(look, info, from, {120.0f, 100.0f}, 0.0f, true, k_TurnsPerSecond).kind, WalkLookAt::Kind::Point);
	EXPECT_EQ(look.turns, 0u);
	// farther: 21 turns at the point, 21 at what interests it (the point when nothing does), then the sweep
	const glm::vec2 far {200.0f, 100.0f};
	for (int turn = 0; turn < 21; ++turn)
	{
		EXPECT_EQ(LookWhileGoing(look, info, from, far, 0.0f, true, k_TurnsPerSecond).kind, WalkLookAt::Kind::Point);
	}
	EXPECT_EQ(look.look, 1u);
	const auto none = LookWhileGoing(look, info, from, far, 0.0f, false, k_TurnsPerSecond);
	EXPECT_EQ(none.kind, WalkLookAt::Kind::Point);
	// with nothing to watch the moving thing it last saw is forgotten once it is two seconds old
	EXPECT_TRUE(none.forgetMovingSeen);
	for (int turn = 1; turn < 21; ++turn)
	{
		EXPECT_EQ(LookWhileGoing(look, info, from, far, 0.0f, true, k_TurnsPerSecond).kind, WalkLookAt::Kind::Interesting);
	}
	EXPECT_EQ(look.look, 2u);
	// the sweep: 15 ahead (a body's yaw of 0 faces -z), a tenth of a radian round
	const auto first = LookWhileGoing(look, info, from, far, 0.0f, true, k_TurnsPerSecond);
	ASSERT_EQ(first.kind, WalkLookAt::Kind::Sweep);
	EXPECT_NEAR(glm::distance(first.sweep, from), 15.0f, 0.01f);
	EXPECT_LT(first.sweep.y, from.y - 14.0f);
	EXPECT_FLOAT_EQ(look.angle, 0.1f);
	// held in the sweep: at either end it turns back and looks 10 further, back to 15 once at 95
	float farthest = 0.0f;
	bool backToFirst = false;
	for (int turn = 0; turn < 400; ++turn)
	{
		look.look = 2;
		look.turns = 0;
		const auto previous = look.distance;
		static_cast<void>(LookWhileGoing(look, info, from, far, 0.0f, true, k_TurnsPerSecond));
		EXPECT_LE(look.angle, 1.61f);
		EXPECT_GE(look.angle, -1.61f);
		farthest = std::max(farthest, look.distance);
		backToFirst = backToFirst || (previous == 85.0f && look.distance == 15.0f);
	}
	EXPECT_FLOAT_EQ(farthest, 85.0f);
	EXPECT_TRUE(backToFirst);
	// the scripted sit's walk takes this look
	const auto sit = SitDownAt(4, {130.0f, 140.0f}, 3.0f, 12.0f);
	ASSERT_TRUE(sit.front().gaze.has_value());
	EXPECT_TRUE(sit.front().gaze->goingTowards);
	ASSERT_TRUE(sit.front().gaze->point.has_value());
	EXPECT_EQ(glm::vec2(sit.front().gaze->point->x, sit.front().gaze->point->z), glm::vec2(130.0f, 140.0f));
}

// The look a cut scene forces draws four chances every time, in the game's order: the point (one in two), the wave (one
// in two), the talk (one in two), then the confused look and talk (one in three); the camera is only needed to point
TEST(CreatureScriptAgendas, TheCutSceneLookDrawsItsFourChancesInTheGamesOrder)
{
	namespace animations = openblack::creature_layers::animations;
	const glm::vec3 camera {10.0f, 40.0f, 20.0f};
	// The fake hands out the draws given, in turn, and keeps the ranges asked for
	const auto drawing = [](std::vector<uint32_t> draws, std::vector<uint32_t>& ranges) {
		return [draws = std::move(draws), &ranges](uint32_t range) {
			const auto index = ranges.size();
			ranges.push_back(range);
			return index < draws.size() ? draws[index] : 1u;
		};
	};

	std::vector<uint32_t> ranges;
	const auto all = LookAtCameraInCutScene(camera, drawing({0, 0, 0, 0}, ranges));
	EXPECT_EQ(ranges, (std::vector<uint32_t> {2, 2, 2, 3}));
	ASSERT_EQ(all.size(), 7u);
	EXPECT_EQ(all[0].kind, Step::Kind::Object);
	EXPECT_EQ(all[0].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(all[0].order.point, glm::vec2(10.0f, 20.0f));
	EXPECT_EQ(all[0].order.pointHeight, 40.0f);
	EXPECT_EQ(all[0].order.seconds, 1.0f);
	EXPECT_EQ(all[0].face, creature_face::Cue::Amazed);
	ASSERT_TRUE(all[0].gaze.has_value());
	EXPECT_EQ(all[0].gaze->point, camera);
	for (const auto index : {1u, 6u})
	{
		EXPECT_EQ(all[index].kind, Step::Kind::LookAtCamera);
		EXPECT_EQ(all[index].turns, 20u);
		EXPECT_EQ(all[index].face, creature_face::Cue::Curiosity);
		ASSERT_TRUE(all[index].gaze.has_value());
		EXPECT_TRUE(all[index].gaze->camera);
	}
	EXPECT_EQ(all[2].kind, Step::Kind::Action);
	EXPECT_EQ(all[2].animation, animations::k_FriendlyWave);
	EXPECT_EQ(all[2].face, creature_face::Cue::Amazed);
	EXPECT_EQ(all[4].kind, Step::Kind::Action);
	EXPECT_EQ(all[4].animation, animations::k_Confused);
	EXPECT_EQ(all[4].face, creature_face::Cue::Amazed);
	for (const auto index : {2u, 4u})
	{
		ASSERT_TRUE(all[index].gaze.has_value());
		EXPECT_TRUE(all[index].gaze->camera);
	}
	for (const auto index : {3u, 5u})
	{
		EXPECT_EQ(all[index].kind, Step::Kind::Overlay);
		EXPECT_EQ(all[index].animation, animations::k_Talk);
		EXPECT_EQ(all[index].face, creature_face::Cue::Curiosity);
		ASSERT_TRUE(all[index].gaze.has_value());
		EXPECT_TRUE(all[index].gaze->camera);
	}

	// None of the four: the two looks only, the chances drawn all the same
	ranges.clear();
	const auto none = LookAtCameraInCutScene(camera, drawing({1, 1, 1, 1}, ranges));
	EXPECT_EQ(ranges.size(), 4u);
	ASSERT_EQ(none.size(), 2u);
	EXPECT_EQ(none[0].kind, Step::Kind::LookAtCamera);
	EXPECT_EQ(none[1].kind, Step::Kind::LookAtCamera);

	// Each draw stands for its own extra: only the talk, then only the confused look and its talk
	ranges.clear();
	const auto talk = LookAtCameraInCutScene(camera, drawing({1, 1, 0, 2}, ranges));
	ASSERT_EQ(talk.size(), 3u);
	EXPECT_EQ(talk[1].kind, Step::Kind::Overlay);
	ranges.clear();
	const auto confused = LookAtCameraInCutScene(camera, drawing({1, 1, 1, 0}, ranges));
	ASSERT_EQ(confused.size(), 4u);
	EXPECT_EQ(confused[1].animation, animations::k_Confused);
	EXPECT_EQ(confused[2].kind, Step::Kind::Overlay);

	// With no camera the point's chance is drawn, and there is no point
	ranges.clear();
	const auto blind = LookAtCameraInCutScene(std::nullopt, drawing({0, 1, 1, 1}, ranges));
	EXPECT_EQ(ranges.size(), 4u);
	ASSERT_EQ(blind.size(), 2u);
	EXPECT_EQ(blind[0].kind, Step::Kind::LookAtCamera);
}

// Only a script's bars force the look (the game's own, as a film's, have no owner), and not on a creature already
// carrying it out
TEST(CreatureScriptAgendas, OnlyAScriptsCutSceneForcesTheLook)
{
	EXPECT_TRUE(StartsLookAtCameraInCutScene(true, 3, false));
	EXPECT_FALSE(StartsLookAtCameraInCutScene(true, 3, true));
	EXPECT_FALSE(StartsLookAtCameraInCutScene(true, 0, false));
	EXPECT_FALSE(StartsLookAtCameraInCutScene(false, 3, false));
	EXPECT_EQ(k_CutSceneLookAction, "LookAtCameraInWideScreen");
}

// LookAtCameraInWideScreen is carried out as the cut scene's look, with or without a camera: it never fails
TEST(CreatureScriptAgendas, TheWideScreenActionIsTheCutSceneLook)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const auto* look = plan_actions::For("LookAtCameraInWideScreen");
	ASSERT_NE(look, nullptr);
	EXPECT_EQ(look->build, plan_actions::Build::LookAtCameraInCutScene);
	plan_actions::Situation situation;
	EXPECT_TRUE(plan_actions::Possible(*look, situation));
	const auto blind = plan_actions::Agenda(*look, std::nullopt, {}, situation, NoRandom);
	ASSERT_TRUE(blind.has_value());
	ASSERT_EQ(blind->size(), 6u);
	EXPECT_EQ(blind->front().kind, Step::Kind::LookAtCamera);
	situation.eye = {10.0f, 40.0f, 20.0f};
	situation.camera = glm::vec2(10.0f, 20.0f);
	const auto seen = plan_actions::Agenda(*look, std::nullopt, {}, situation, NoRandom);
	ASSERT_TRUE(seen.has_value());
	ASSERT_EQ(seen->size(), 7u);
	EXPECT_EQ(seen->front().order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(seen->front().order.pointHeight, 40.0f);
}

// The second of the three looks on the way to a far point: what interests it, else the last moving thing it came
// across while that is fresh, else the point (and the moving thing is forgotten); the other two looks leave it be
TEST(CreatureScriptAgendas, WithNothingToWatchTheSecondLookTakesTheLastMovingThingSeen)
{
	constexpr uint32_t k_TurnsPerSecond = 10;
	const WalkLookInfo info {.sweepAngle = 1.5f,
	                         .sweepStep = 0.1f,
	                         .firstDistance = 15.0f,
	                         .farthest = 95.0f,
	                         .distanceStep = 10.0f,
	                         .pointWithin = 25.0f};
	const glm::vec2 from {100.0f, 100.0f};
	const glm::vec2 far {200.0f, 100.0f};
	auto look = StartWalkLook(info);
	const auto first = LookWhileGoing(look, info, from, far, 0.0f, false, k_TurnsPerSecond, true);
	EXPECT_EQ(first.kind, WalkLookAt::Kind::Point);
	EXPECT_FALSE(first.forgetMovingSeen);
	look.look = 1;
	const auto seen = LookWhileGoing(look, info, from, far, 0.0f, false, k_TurnsPerSecond, true);
	EXPECT_EQ(seen.kind, WalkLookAt::Kind::MovingSeen);
	EXPECT_FALSE(seen.forgetMovingSeen);
	const auto watched = LookWhileGoing(look, info, from, far, 0.0f, true, k_TurnsPerSecond, true);
	EXPECT_EQ(watched.kind, WalkLookAt::Kind::Interesting);
	EXPECT_FALSE(watched.forgetMovingSeen);
	const auto old = LookWhileGoing(look, info, from, far, 0.0f, false, k_TurnsPerSecond, false);
	EXPECT_EQ(old.kind, WalkLookAt::Kind::Point);
	EXPECT_TRUE(old.forgetMovingSeen);
	// near the point it has no look of its own and nothing is forgotten
	EXPECT_FALSE(LookWhileGoing(look, info, from, {105.0f, 100.0f}, 0.0f, false, k_TurnsPerSecond, false).forgetMovingSeen);
	look.look = 2;
	EXPECT_FALSE(LookWhileGoing(look, info, from, far, 0.0f, false, k_TurnsPerSecond, false).forgetMovingSeen);
}
