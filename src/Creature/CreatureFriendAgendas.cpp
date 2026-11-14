/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFriendAgendas.h"

#include <algorithm>
#include <array>
#include <memory>
#include <span>

#include <glm/vec3.hpp>

#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureKiss.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/CreatureThrow.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;
using creature_face::Cue;

namespace
{
Step PlayOnce(size_t animation)
{
	return {.kind = Step::Kind::Action, .animation = animation};
}

/// Turning to face a thing, holding still a while once it does
Step FaceThing(uint32_t thing, float seconds, Cue face = Cue::None, std::optional<Gaze> gaze = std::nullopt)
{
	return {.kind = Step::Kind::Move,
	        .seconds = seconds,
	        .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = thing},
	        .face = face,
	        .gaze = gaze};
}

Step PointAt(uint32_t thing, float seconds, Cue face)
{
	return {.kind = Step::Kind::Object,
	        .order = {.kind = ObjectOrder::Kind::PointAt, .object = thing, .seconds = seconds},
	        .face = face,
	        .gaze = Gaze {.object = thing}};
}

/// An order to the partner. The partner does the step with no face of its own and looks about as it does it; the face
/// the order is given with is the asker's own, pulled as it gives the order
Step Order(uint32_t partner, Step ordered, Cue face = Cue::None)
{
	ordered.face = Cue::None;
	ordered.gaze.reset();
	return {.kind = Step::Kind::Order,
	        .object = partner,
	        .face = face,
	        .ordered = std::make_shared<const Step>(std::move(ordered))};
}
} // namespace

std::vector<Step> creature_mind::WaveAtFriend(uint32_t self, uint32_t friendly)
{
	auto wave = PlayOnce(animations::k_FriendlyWave);
	wave.main = true;
	return {
	    {.kind = Step::Kind::RequestPartner,
	     .object = friendly,
	     .face = Cue::Playfulness,
	     .gaze = Gaze {.object = friendly},
	     .request = creature_friendship::RequestKind::Play},
	    {.kind = Step::Kind::Demobilise, .object = friendly},
	    Order(friendly, FaceThing(self, k_FriendFacesWaverSeconds), Cue::Grimace),
	    FaceThing(friendly, k_FaceFriendToWaveSeconds, Cue::Smile, Gaze {.object = friendly}),
	    Order(friendly, PlayOnce(animations::k_FriendlyWave)),
	    wave,
	    {.kind = Step::Kind::Remobilise, .object = friendly},
	};
}

std::vector<Step> creature_mind::SmileAtFriend(uint32_t friendly, float height, const Random& random)
{
	Step goNear {
	    .kind = Step::Kind::Move,
	    .movement = {.kind = Movement::Kind::GoNearObject, .object = friendly, .maxDistance = k_SmileNearHeights * height},
	    .face = Cue::Smile};
	goNear.gaze = Gaze {.object = friendly};
	goNear.flutterEyelids = true;
	// The time it holds its smile is drawn before whether it shows it is happy
	auto face = FaceThing(friendly, k_SmileFaceSeconds + creature_emote::FloatRandom(random, k_SmileFaceExtraSeconds),
	                      Cue::Smile, Gaze {.object = friendly});
	face.flutterEyelids = true;
	face.main = true;
	std::vector<Step> agenda {goNear, face};
	if (random(2) == 0)
	{
		agenda.push_back(PlayOnce(animations::k_Happy));
	}
	return agenda;
}

std::vector<Step> creature_mind::FightCreature(uint32_t other)
{
	return {
	    {.kind = Step::Kind::RequestPartner,
	     .object = other,
	     .face = Cue::Anger,
	     .gaze = Gaze {.object = other},
	     .request = creature_friendship::RequestKind::Fight},
	    {.kind = Step::Kind::Demobilise, .object = other},
	    {.kind = Step::Kind::Fight, .object = other, .face = Cue::Anger, .main = true},
	    {.kind = Step::Kind::Remobilise, .object = other},
	};
}

std::vector<Step> creature_mind::ShowCreatureItHatesIt(uint32_t other, float height, float otherHeight, const Random& random)
{
	std::vector<Step> agenda {
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject,
	                  .object = other,
	                  .maxDistance = std::max(height * k_HateNearHeights, otherHeight * k_HateNearHeights)},
	     .face = Cue::Anger,
	     .gaze = Gaze {.object = other, .goingTowards = true}},
	    FaceThing(other, k_HateFaceSeconds, Cue::Anger, Gaze {.object = other}),
	};
	if (random(2) == 0)
	{
		agenda.push_back(PointAt(other, k_HatePointSeconds, Cue::Grimace));
	}
	const auto show = Individual(random(2) != 0 ? k_HateShows[0] : k_HateShows[1]);
	agenda.insert(agenda.end(), show.begin(), show.end());
	if (random(2) == 0)
	{
		const auto last = Individual(k_HateLastShow);
		agenda.insert(agenda.end(), last.begin(), last.end());
	}
	return agenda;
}

std::optional<std::vector<Step>> creature_mind::ShowHowNiceItReckons(uint32_t other, std::optional<float> howNice,
                                                                     const Random& random)
{
	if (!howNice.has_value())
	{
		return std::nullopt;
	}
	std::vector<Step> agenda {FaceThing(other, k_NiceFaceSeconds, Cue::None, Gaze {.object = other})};
	if (*howNice == 0.0f)
	{
		agenda.push_back({.kind = Step::Kind::Face,
		                  .seconds = creature_player_attention::k_SillyFaceSeconds,
		                  .animation = k_NeitherNiceFace,
		                  .gaze = Gaze {.object = other},
		                  .main = true});
		return agenda;
	}
	if (*howNice > 0.0f)
	{
		auto show = Individual(k_NiceShow);
		show.front().main = true;
		agenda.insert(agenda.end(), show.begin(), show.end());
		return agenda;
	}
	if (*howNice < 0.0f)
	{
		auto show = Individual(random(2) != 0 ? k_HateShows[0] : k_HateShows[1]);
		show.front().main = true;
		agenda.insert(agenda.end(), show.begin(), show.end());
	}
	return agenda;
}

std::vector<Step> creature_mind::HowlAtFriend(uint32_t friendly)
{
	return {
	    FaceThing(friendly, k_FaceToHowlSeconds, Cue::None, Gaze {.object = friendly}),
	    PlayOnce(k_Howl),
	};
}

std::vector<Step> creature_mind::PineForFriend(uint32_t friendly)
{
	auto pine = PlayOnce(animations::k_Sad);
	pine.main = true;
	return {
	    FaceThing(friendly, k_PineSeconds, Cue::Sad, Gaze {.object = friendly}),
	    PointAt(friendly, k_PineSeconds, Cue::Sad),
	    pine,
	};
}

std::vector<Step> creature_mind::LookForFriend(uint32_t friendly, float height)
{
	auto pine = PlayOnce(animations::k_Sad);
	pine.main = true;
	return {
	    PointAt(friendly, k_PineSeconds, Cue::Sad),
	    pine,
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = friendly, .maxDistance = k_LookForFriendHeights * height},
	     .gaze = Gaze {.object = friendly, .goingTowards = true}},
	};
}

namespace
{
/// The look at a thing, with fluttering eyelids
Step Fluttering(Step step, uint32_t thing)
{
	step.gaze = Gaze {.object = thing};
	step.flutterEyelids = true;
	return step;
}

/// The look of the walk to a point on the ground
Gaze GoingLook(glm::vec2 point)
{
	return Gaze {.point = glm::vec3 {point.x, 0.0f, point.y}, .goingTowards = true};
}

Step GoTo(glm::vec2 point, float within, std::optional<Gaze> gaze = std::nullopt)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::ToPoint, .point = point, .maxDistance = within},
	        .gaze = gaze};
}

Step WaitForStill(uint32_t thing)
{
	return {.kind = Step::Kind::WaitForStill, .object = thing, .gaze = Gaze {.object = thing}};
}

Step AskToBeFriends(uint32_t friendly, Cue face = Cue::None)
{
	return {.kind = Step::Kind::RequestPartner,
	        .object = friendly,
	        .face = face,
	        .gaze = Gaze {.object = friendly},
	        .request = creature_friendship::RequestKind::BeFriends};
}

/// The first steps of the joint actions side by side: it asks the friend, both go to their places, and it waits for the
/// friend to stop
std::vector<Step> MeetAt(const Friends& friends, glm::vec2 selfPlace, glm::vec2 friendPlace)
{
	return {
	    AskToBeFriends(friends.friendly),
	    Order(friends.friendly, GoTo(friendPlace, friends.friendHeight)),
	    GoTo(selfPlace, friends.height, GoingLook(selfPlace)),
	    WaitForStill(friends.friendly),
	};
}

Step SitFor(float seconds)
{
	return {.kind = Step::Kind::Static,
	        .seconds = seconds,
	        .animation = animations::k_Sit,
	        .sequence = {animations::k_StartSit, animations::k_Sit, animations::k_EndSit},
	        .mirrorAtRandom = true};
}
} // namespace

std::vector<Step> creature_mind::KissFriend(const Friends& friends, float kissingDistance, const Random& random)
{
	const auto [selfPlace, friendPlace] = creature_kiss::KissPlaces(friends.selfPoint, friends.friendPoint, kissingDistance);
	auto goToPlace = GoTo(selfPlace, k_KissPlaceWithin, GoingLook(selfPlace));
	goToPlace.face = Cue::Compassion;
	auto face = FaceThing(friends.friendly, k_FaceToKissSeconds, Cue::Compassion, Gaze {.object = friends.friendly});
	face.flutterEyelids = true;
	auto friendFaces = FaceThing(friends.self, k_FaceToKissSeconds);
	friendFaces.flutterEyelids = true;
	std::vector<Step> agenda {
	    AskToBeFriends(friends.friendly),
	    Order(friends.friendly, GoTo(friendPlace, k_KissPlaceWithin), Cue::Puzzled),
	    goToPlace,
	    Order(friends.friendly, friendFaces),
	    face,
	    Order(friends.friendly, {.kind = Step::Kind::Kiss, .object = friends.self}),
	    {.kind = Step::Kind::Kiss, .object = friends.friendly, .main = true},
	};
	if (random(2) == 0)
	{
		auto smile = FaceThing(friends.friendly, k_FaceAfterKissSeconds, Cue::Smile, Gaze {.object = friends.friendly});
		smile.flutterEyelids = true;
		agenda.push_back(smile);
		agenda.push_back(PlayOnce(animations::k_Happy));
	}
	return agenda;
}

std::pair<glm::vec2, glm::vec2> creature_mind::SideBySide(const Friends& friends)
{
	const auto middle = friends.selfPoint + (friends.friendPoint - friends.selfPoint) * 0.5f;
	return {middle + glm::vec2(friends.height, 0.0f), middle - glm::vec2(friends.friendHeight, 0.0f)};
}

std::pair<glm::vec2, glm::vec2> creature_mind::AboutPlace(glm::vec2 middle, float radius)
{
	const auto offset = glm::vec2(radius * 0.25f, radius * 0.25f);
	return {middle + offset, middle - offset};
}

float creature_mind::SitTogetherRadius(float height, float friendHeight)
{
	return (height + friendHeight) * k_SitTogetherHeights;
}

float creature_mind::SleepTogetherRadius(float height, float friendHeight)
{
	return k_SleepTogetherHeights * (height + friendHeight);
}

std::vector<Step> creature_mind::TellCreatureToGoAway(uint32_t self, uint32_t other, glm::vec3 standing,
                                                      glm::vec3 otherStanding, float otherHeight, const Random& random)
{
	if (random(2) == 0)
	{
		return FightCreature(other);
	}
	const auto amazed = [] {
		auto step = PlayOnce(k_GoAwayAmazed);
		step.face = Cue::Amazed;
		return step;
	};
	// As far past the other as the other is from it
	const auto beyond = otherStanding + (otherStanding - standing);
	const glm::vec2 point {beyond.x, beyond.z};
	return {
	    {.kind = Step::Kind::RequestPartner,
	     .object = other,
	     .face = Cue::Playfulness,
	     .gaze = Gaze {.object = other},
	     .request = creature_friendship::RequestKind::Fight},
	    {.kind = Step::Kind::Demobilise, .object = other},
	    Order(other, FaceThing(self, k_GoAwayOtherFacesSeconds), Cue::Grimace),
	    FaceThing(other, k_GoAwayFaceSeconds, Cue::Smile, Gaze {.object = other}),
	    amazed(),
	    {.kind = Step::Kind::Object,
	     .order =
	         {.kind = ObjectOrder::Kind::PointAt, .point = point, .pointHeight = beyond.y, .seconds = k_GoAwayPointSeconds},
	     .face = Cue::Grimace,
	     .gaze = Gaze {.point = beyond},
	     .main = true},
	    Order(other, PlayOnce(k_GoAwayOtherShow)),
	    amazed(),
	    Order(other,
	          {.kind = Step::Kind::Move,
	           .movement = {.kind = Movement::Kind::ToPoint, .point = point, .maxDistance = k_GoAwayHeights * otherHeight},
	           .gaze = Gaze {.point = beyond, .goingTowards = true}}),
	    WaitForStill(other),
	    Order(other, PlayOnce(k_GoAwayOtherShow)),
	    amazed(),
	    {.kind = Step::Kind::Remobilise, .object = other},
	};
}

std::vector<Step> creature_mind::BeHappyWithFriend(const Friends& friends)
{
	const auto [selfPlace, friendPlace] = SideBySide(friends);
	auto agenda = MeetAt(friends, selfPlace, friendPlace);
	agenda.push_back(Order(friends.friendly, FaceThing(friends.self, k_FaceFriendSeconds)));
	agenda.push_back(Fluttering(FaceThing(friends.friendly, k_FaceFriendSeconds), friends.friendly));
	agenda.push_back(Order(friends.friendly, PlayOnce(animations::k_Happy)));
	agenda.push_back(PlayOnce(animations::k_Happy));
	return agenda;
}

std::vector<Step> creature_mind::PooWithFriend(const Friends& friends, const Random& random)
{
	const auto [selfPlace, friendPlace] = SideBySide(friends);
	auto agenda = MeetAt(friends, selfPlace, friendPlace);
	auto squat = Step {.kind = Step::Kind::Static,
	                   .seconds = k_PooTogetherSeconds,
	                   .animation = animations::k_Poo,
	                   .sequence = {animations::k_StartPoo, animations::k_Poo, animations::k_EndPoo},
	                   .mirrorAtRandom = true,
	                   .face = Cue::Smile};
	const auto poo = Step {.kind = Step::Kind::Apply, .effect = Effect::Poo, .face = Cue::Smile};
	agenda.push_back(Order(friends.friendly, squat, Cue::Smile));
	squat.main = true;
	agenda.push_back(squat);
	agenda.push_back(Order(friends.friendly, poo, Cue::Smile));
	agenda.push_back(poo);
	const float seconds = k_FaceAfterPooSeconds + creature_emote::FloatRandom(random, k_FaceAfterPooExtraSeconds);
	agenda.push_back(FaceThing(friends.friendly, seconds, Cue::Smile, Gaze {.object = friends.friendly}));
	agenda.push_back(PlayOnce(animations::k_Happy));
	return agenda;
}

std::vector<Step> creature_mind::SitWithFriend(const Friends& friends, glm::vec2 middle, float radius, const Random& random)
{
	const auto [selfPlace, friendPlace] = AboutPlace(middle, radius);
	auto agenda = MeetAt(friends, selfPlace, friendPlace);
	agenda.back().flutterEyelids = true;
	agenda.push_back(Order(friends.friendly, FaceThing(friends.self, k_FaceFriendSeconds)));
	agenda.push_back(FaceThing(friends.friendly, k_FaceFriendSeconds, Cue::None, Gaze {.object = friends.friendly}));
	agenda.push_back(
	    Order(friends.friendly, SitFor(k_SitTogetherSeconds + creature_emote::FloatRandom(random, k_SitTogetherExtraSeconds))));
	auto sit = SitFor(k_SitTogetherSeconds + creature_emote::FloatRandom(random, k_SitTogetherExtraSeconds));
	sit.gaze = Gaze {.object = friends.friendly};
	sit.main = true;
	agenda.push_back(sit);
	agenda.push_back(Fluttering(FaceThing(friends.friendly, k_FaceFriendSeconds), friends.friendly));
	agenda.push_back(PlayOnce(animations::k_Happy));
	return agenda;
}

std::vector<Step> creature_mind::SleepWithFriend(const Friends& friends, glm::vec2 middle, float radius, const Random& random)
{
	std::vector<Step> agenda;
	if (random(2) == 0)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = animations::k_Tired, .sleepyEyes = true});
	}
	const auto [selfPlace, friendPlace] = AboutPlace(middle, radius);
	const auto meet = MeetAt(friends, selfPlace, friendPlace);
	agenda.insert(agenda.end(), meet.begin(), meet.end());
	agenda.push_back(Order(friends.friendly, FaceThing(friends.self, k_FaceFriendSeconds)));
	agenda.push_back(Fluttering(FaceThing(friends.friendly, k_FaceFriendSeconds), friends.friendly));
	auto sleep = Step {.kind = Step::Kind::Static,
	                   .seconds = k_SleepTogetherSeconds,
	                   .animation = animations::k_Sleep,
	                   .sequence = {animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep},
	                   .mirrorAtRandom = true};
	agenda.push_back(Order(friends.friendly, sleep));
	sleep.main = true;
	agenda.push_back(sleep);
	agenda.push_back(
	    {.kind = Step::Kind::Action, .animation = animations::k_Confused, .sleepyEyes = true, .face = Cue::Grimace});
	return agenda;
}

std::vector<Step> creature_mind::DrinkWithFriend(const Friends& friends, glm::vec2 coast, glm::vec2 sea, const Random& random)
{
	const auto friendPlace = coast + glm::vec2(friends.friendHeight, 0.0f);
	auto agenda = MeetAt(friends, coast, friendPlace);
	const auto faceSea =
	    Step {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::TurnToFace, .point = sea}, .gaze = GoingLook(sea)};
	agenda.push_back(Order(friends.friendly, faceSea));
	agenda.push_back(faceSea);
	agenda.push_back(Order(friends.friendly, PlayOnce(animations::k_Drink)));
	auto drinkAction = PlayOnce(animations::k_Drink);
	drinkAction.main = true;
	agenda.push_back(drinkAction);
	const auto drink = Step {.kind = Step::Kind::Apply, .effect = Effect::Drink};
	agenda.push_back(Order(friends.friendly, drink));
	agenda.push_back(drink);
	const float seconds = k_FaceAfterDrinkSeconds + creature_emote::FloatRandom(random, k_FaceAfterDrinkExtraSeconds);
	agenda.push_back(Fluttering(FaceThing(friends.friendly, seconds, Cue::Smile), friends.friendly));
	return agenda;
}

std::vector<Step> creature_mind::TownWithFriend(const Friends& friends, const FriendsTown& town,
                                                creature_desires::Desire desire)
{
	const float distance = k_NearPitTimes * (town.pitRadius + friends.height);
	const auto nearPit = Step {.kind = Step::Kind::Move,
	                           .movement = {.kind = Movement::Kind::GoNearObject, .object = town.pit, .maxDistance = distance},
	                           .gaze = Gaze {.object = town.pit, .goingTowards = true}};
	const auto apply = Step {.kind = Step::Kind::ApplyDesire, .object = town.town, .desire = desire};
	return {
	    AskToBeFriends(friends.friendly, Cue::Playfulness),
	    Fluttering(FaceThing(friends.friendly, k_FaceFriendSeconds), friends.friendly),
	    PlayOnce(animations::k_Happy),
	    {.kind = Step::Kind::Object,
	     .order = {.kind = ObjectOrder::Kind::PointAt,
	               .point = town.townPoint,
	               .pointHeight = 0.0f,
	               .seconds = k_PointAtTownSeconds},
	     .face = Cue::Playfulness,
	     .gaze = Gaze {.point = glm::vec3 {town.townPoint.x, 0.0f, town.townPoint.y}},
	     .main = true},
	    Order(friends.friendly, nearPit),
	    nearPit,
	    Order(friends.friendly, FaceThing(friends.self, k_FaceFriendSeconds)),
	    FaceThing(friends.friendly, k_FaceFriendSeconds),
	    Order(friends.friendly, apply),
	    apply,
	};
}

namespace
{
Step GoNear(uint32_t thing, float within, std::optional<Gaze> gaze = std::nullopt, Cue face = Cue::None)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::GoNearObject, .object = thing, .maxDistance = within},
	        .face = face,
	        .gaze = gaze};
}

Step GoNearLooking(uint32_t thing, float within, Cue face = Cue::None)
{
	return GoNear(thing, within, Gaze {.object = thing, .goingTowards = true}, face);
}

Step Ask(uint32_t friendly, creature_friendship::RequestKind kind, Cue face = Cue::None)
{
	return {.kind = Step::Kind::RequestPartner,
	        .object = friendly,
	        .face = face,
	        .gaze = Gaze {.object = friendly},
	        .request = kind};
}

Step WaitFor(float seconds, Cue face = Cue::None, std::optional<Gaze> gaze = std::nullopt)
{
	return {.kind = Step::Kind::Wait, .seconds = seconds, .face = face, .gaze = gaze};
}

Step PointAtPlace(glm::vec3 place, float seconds, Cue face)
{
	return {
	    .kind = Step::Kind::Object,
	    .order = {.kind = ObjectOrder::Kind::PointAt, .point = {place.x, place.z}, .pointHeight = place.y, .seconds = seconds},
	    .face = face,
	    .gaze = Gaze {.point = place}};
}

Step TurnToFacePlace(glm::vec3 place)
{
	return {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::TurnToFace, .point = {place.x, place.z}}};
}

Step Talk(uint32_t friendly)
{
	return Fluttering({.kind = Step::Kind::Overlay, .animation = animations::k_Talk}, friendly);
}

Step PickUpThing(uint32_t thing)
{
	return {.kind = Step::Kind::Object,
	        .order = {.kind = ObjectOrder::Kind::PickUp, .object = thing},
	        .gaze = Gaze {.object = thing, .goingTowards = true}};
}

Step ThrowAt(glm::vec3 place)
{
	return {.kind = Step::Kind::Object,
	        .order = {.kind = ObjectOrder::Kind::Throw, .point = {place.x, place.z}, .pointHeight = place.y}};
}

Step WaitForFree(uint32_t friendly)
{
	return {.kind = Step::Kind::WaitForFree, .turns = k_WaitForFreeTurns, .object = friendly};
}

Step Demobilise(uint32_t friendly, bool still)
{
	return {.kind = still ? Step::Kind::Demobilise : Step::Kind::Remobilise, .object = friendly};
}

Step Main(Step step)
{
	step.main = true;
	return step;
}

/// The happy action, after the creature turns to face the friend for a tenth of a second and flutters its eyelids
std::array<Step, 2> HappyAtFriend(uint32_t friendly)
{
	return {Fluttering(FaceThing(friendly, k_FaceFriendSeconds), friendly), PlayOnce(animations::k_Happy)};
}

void Append(std::vector<Step>& agenda, std::span<const Step> steps)
{
	agenda.insert(agenda.end(), steps.begin(), steps.end());
}

/// The first four steps of arguing, moping and showing off, with the face of the action
std::vector<Step> ComeTogether(uint32_t self, uint32_t friendly, float height, Cue face, const Random& random)
{
	const float near = k_TogetherNearHeights * height;
	return {
	    AskToBeFriends(friendly),
	    Order(friendly, GoNear(self, near), face),
	    GoNearLooking(friendly, near, face),
	    Order(friendly,
	          FaceThing(self, k_FaceTogetherSeconds + creature_emote::FloatRandom(random, k_FaceTogetherExtraSeconds)), face),
	};
}
} // namespace

namespace
{
std::vector<Step> TalkAgenda(uint32_t self, uint32_t friendly, float height, const Random& random, Step answer)
{
	const float seconds = k_FaceToTalkSeconds + creature_emote::FloatRandom(random, k_FaceToTalkExtraSeconds);
	return {
	    Fluttering(GoNear(friendly, k_TalkNearHeights * height, std::nullopt, Cue::Smile), friendly),
	    Main(Fluttering(FaceThing(friendly, seconds, Cue::Smile), friendly)),
	    AskToBeFriends(friendly),
	    Order(friendly, FaceThing(self, k_FriendFacesTalkerSeconds)),
	    Talk(friendly),
	    Order(friendly, std::move(answer)),
	};
}
} // namespace

std::vector<Step> creature_mind::EatFromFieldWithFriend(const Friends& friends, uint32_t field, glm::vec2 fieldPoint)
{
	constexpr float k_FieldSideMetres = 10.0f;
	const auto take =
	    Step {.kind = Step::Kind::Object,
	          .order = {.kind = ObjectOrder::Kind::PickUpPortion, .object = field, .portion = PotInfo::WheatInHand}};
	const auto eat = Step {.kind = Step::Kind::Object, .effect = Effect::Eat, .order = {.kind = ObjectOrder::Kind::Eat}};
	std::vector<Step> agenda {
	    AskToBeFriends(friends.friendly, Cue::Playfulness),
	    Order(friends.friendly, GoTo(fieldPoint - glm::vec2(k_FieldSideMetres, 0.0f), friends.height)),
	    GoTo(fieldPoint + glm::vec2(k_FieldSideMetres, 0.0f), friends.height),
	    Order(friends.friendly, FaceThing(field, k_FaceFriendSeconds)),
	    FaceThing(field, k_FaceFriendSeconds),
	    Order(friends.friendly, take),
	    take,
	    Order(friends.friendly, eat),
	    Main(eat),
	    FaceThing(friends.friendly, k_FaceFriendSeconds),
	    PlayOnce(animations::k_Happy),
	    Demobilise(friends.friendly, false),
	};
	Append(agenda, HappyAtFriend(friends.friendly));
	return agenda;
}

std::vector<Step> creature_mind::TalkToFriend(uint32_t self, uint32_t friendly, float height, const Random& random)
{
	return TalkAgenda(self, friendly, height, random, {.kind = Step::Kind::Overlay, .animation = animations::k_Talk});
}

std::vector<Step> creature_mind::TellFriendAJoke(uint32_t self, uint32_t friendly, float height, const Random& random)
{
	return TalkAgenda(self, friendly, height, random, PlayOnce(k_Laugh));
}

std::vector<Step> creature_mind::DanceWithFriend(uint32_t self, uint32_t friendly, float height)
{
	const Step dance {.kind = Step::Kind::PracticeDance, .seconds = k_DanceTogetherSeconds, .face = Cue::Playfulness};
	auto wait = WaitForStill(friendly);
	wait.face = Cue::Playfulness;
	std::vector<Step> agenda {GoNearLooking(friendly, k_DanceNearHeights * height)};
	Append(agenda, HappyAtFriend(friendly));
	Append(agenda, std::array {
	                   AskToBeFriends(friendly),
	                   Demobilise(friendly, true),
	                   Order(friendly, FaceThing(self, k_FaceFriendSeconds)),
	                   wait,
	                   Order(friendly, dance, Cue::Playfulness),
	                   Main(dance),
	                   Demobilise(friendly, false),
	               });
	return agenda;
}

std::vector<Step> creature_mind::ArgueWithFriend(uint32_t self, uint32_t friendly, float height, const Random& random)
{
	auto agenda = ComeTogether(self, friendly, height, Cue::Smile, random);
	const float seconds = k_FaceTogetherSeconds + creature_emote::FloatRandom(random, k_FaceTogetherExtraSeconds);
	auto angry = PlayOnce(animations::k_Angry);
	angry.face = Cue::Anger;
	Append(agenda, std::array {
	                   Fluttering(FaceThing(friendly, k_FaceToArgueSeconds, Cue::Smile), friendly),
	                   Order(friendly, FaceThing(self, k_FaceToArgueSeconds), Cue::Anger),
	                   FaceThing(friendly, seconds, Cue::Anger, Gaze {.object = friendly}),
	                   PointAt(friendly, k_PointAtFriendSeconds, Cue::Anger),
	                   Order(friendly, PlayOnce(animations::k_Angry), Cue::Anger),
	                   angry,
	                   Order(friendly, WaitFor(k_ArgueWaitSeconds), Cue::Anger),
	                   WaitFor(k_ArgueWaitSeconds),
	                   Order(friendly, WaitFor(k_AfterArgueSeconds), Cue::Smile),
	                   Main(WaitFor(k_AfterArgueSeconds, Cue::Smile)),
	               });
	Append(agenda, HappyAtFriend(friendly));
	return agenda;
}

std::vector<Step> creature_mind::MopeAboutWithFriend(uint32_t self, uint32_t friendly, float height, const Random& random)
{
	auto agenda = ComeTogether(self, friendly, height, Cue::Sad, random);
	const float seconds = k_FaceTogetherSeconds + creature_emote::FloatRandom(random, k_FaceTogetherExtraSeconds);
	auto sad = PlayOnce(animations::k_Sad);
	sad.face = Cue::Sad;
	Append(agenda, std::array {
	                   Fluttering(FaceThing(friendly, seconds, Cue::Sad), friendly),
	                   Order(friendly, PlayOnce(animations::k_Sad), Cue::Sad),
	                   Main(sad),
	               });
	if (random(2) == 0)
	{
		Append(agenda, HappyAtFriend(friendly));
	}
	return agenda;
}

std::vector<Step> creature_mind::ShowOffToFriend(uint32_t self, uint32_t friendly, float height, glm::vec3 home,
                                                 const Random& random)
{
	auto agenda = ComeTogether(self, friendly, height, Cue::Smile, random);
	const float seconds = k_FaceTogetherSeconds + creature_emote::FloatRandom(random, k_FaceTogetherExtraSeconds);
	Append(agenda, std::array {
	                   Fluttering(FaceThing(friendly, seconds, Cue::Smile), friendly),
	                   Main(PointAtPlace(home, k_PointAtHomeSeconds, Cue::Grimace)),
	                   Order(friendly, TurnToFacePlace(home), Cue::Puzzled),
	                   PointAt(friendly, k_PointAtFriendSeconds, Cue::Sad),
	                   Order(friendly, PlayOnce(animations::k_Confused), Cue::Puzzled),
	               });
	Append(agenda, HappyAtFriend(friendly));
	return agenda;
}

std::vector<Step> creature_mind::ShowFriendSpell(const Friends& friends, const ShowSpell& spell)
{
	constexpr float k_DestructiveNearHeights = 1.5f;
	constexpr float k_CreatingNearHeights = 3.5f;
	constexpr float k_FriendToFlammableHeights = 2.5f;
	constexpr float k_ToTargetHeights = 5.0f;
	constexpr float k_BackOffHeights = 2.0f;
	const auto friendly = friends.friendly;
	const float height = friends.height;
	const auto mood = spell.destructive ? Cue::None : Cue::Compassion;
	std::vector<Step> agenda;
	if (!spell.seesFriend)
	{
		agenda.push_back(
		    GoNearLooking(friendly, (spell.destructive ? k_DestructiveNearHeights : k_CreatingNearHeights) * height, mood));
	}
	agenda.push_back(FaceThing(friendly, k_FaceFriendSeconds, mood, Gaze {.object = friendly}));
	if (!spell.target.has_value())
	{
		return agenda;
	}
	const auto target = *spell.target;
	const auto cast = Main({.kind = Step::Kind::Cast,
	                        .seconds = k_CastHoldSeconds,
	                        .animation = k_CastPose[1],
	                        .sequence = k_CastPose,
	                        .cast = {.magicType = spell.magicType, .object = target},
	                        .face = spell.destructive ? Cue::Grimace : Cue::Compassion});
	if (spell.destructive)
	{
		Append(agenda, std::array {
		                   Ask(friendly, creature_friendship::RequestKind::BeShown, Cue::Playfulness),
		                   Order(friendly, GoNearLooking(target, k_FriendToFlammableHeights * height)),
		                   GoNearLooking(target, k_ToTargetHeights * height),
		                   Step {.kind = Step::Kind::Move,
		                         .movement = {.kind = Movement::Kind::GetAwayFromObject,
		                                      .object = target,
		                                      .maxDistance = k_BackOffHeights * height},
		                         .gaze = Gaze {.object = target}},
		                   FaceThing(target, k_FaceFriendSeconds, Cue::Fear, Gaze {.object = target}),
		                   PointAt(target, k_PointAtFriendSeconds, Cue::Grimace),
		                   Order(friendly, TurnToFacePlace(spell.head)),
		                   WaitForFree(friendly),
		                   Order(friendly, FaceThing(target, k_FaceFriendSeconds)),
		                   cast,
		                   Order(friendly, FaceThing(friends.self, k_FaceFriendSeconds), Cue::Amazed),
		               });
		return agenda;
	}
	Append(
	    agenda,
	    std::array {
	        Ask(friendly, creature_friendship::RequestKind::BeShown),
	        Order(friendly, GoNearLooking(target, k_ToTargetHeights * height)),
	        GoNearLooking(target, k_ToTargetHeights * height),
	        Step {
	            .kind = Step::Kind::WaitForStill, .object = friendly, .face = Cue::Grimace, .gaze = Gaze {.object = friendly}},
	        Order(friendly, FaceThing(target, k_FaceFriendSeconds)),
	        FaceThing(target, k_FaceFriendSeconds, Cue::Compassion, Gaze {.object = target}),
	        PointAt(target, k_PointAtFriendSeconds, Cue::Grimace),
	        Order(friendly, TurnToFacePlace(spell.head)),
	        WaitForFree(friendly),
	        Order(friendly, FaceThing(target, k_FaceFriendSeconds)),
	        cast,
	        Order(friendly, FaceThing(friends.self, k_FaceFriendSeconds), Cue::Amazed),
	    });
	return agenda;
}

std::vector<Step> creature_mind::ConfuseFriend(uint32_t self, uint32_t friendly, float height, glm::vec3 home,
                                               const Random& random)
{
	auto agenda = ComeTogether(self, friendly, height, Cue::Smile, random);
	const float seconds = k_FaceTogetherSeconds + creature_emote::FloatRandom(random, k_FaceTogetherExtraSeconds);
	Append(agenda, std::array {
	                   FaceThing(friendly, seconds, Cue::Smile, Gaze {.object = friendly}),
	                   Main(PointAtPlace(home, k_PointAtHomeSeconds, Cue::Grimace)),
	                   Order(friendly, TurnToFacePlace(home), Cue::Puzzled),
	                   Fluttering(PointAt(friendly, k_PointAtFriendSeconds, Cue::Sad), friendly),
	                   Order(friendly, PlayOnce(animations::k_Confused), Cue::Puzzled),
	               });
	return agenda;
}

std::vector<Step> creature_mind::GetAttentionFromFriend(uint32_t friendly, const Random& random)
{
	std::vector<Step> agenda {Fluttering(FaceThing(friendly, k_FaceFriendSeconds), friendly)};
	if (random(2) == 0)
	{
		agenda.push_back(PointAt(friendly, k_PointForAttentionSeconds, Cue::Sad));
	}
	if (random(2) == 0)
	{
		agenda.push_back(PlayOnce(animations::k_FriendlyWave));
	}
	if (random(3) == 0)
	{
		agenda.push_back(PlayOnce(k_LookAtMe));
	}
	agenda.push_back(Fluttering(FaceThing(friendly, k_FaceFriendSeconds), friendly));
	agenda.push_back(Main(PlayOnce(animations::k_Sad)));
	return agenda;
}

std::vector<Step> creature_mind::ExamineOtherCreatureWithFriend(uint32_t friendly, uint32_t other, const Random& random)
{
	std::vector<Step> agenda {
	    Ask(friendly, creature_friendship::RequestKind::Play, Cue::Playfulness),
	    Demobilise(friendly, true),
	    Order(friendly, FaceThing(other, k_FaceFriendSeconds), Cue::Playfulness),
	    FaceThing(other, k_FaceFriendSeconds, Cue::Playfulness, Gaze {.object = other}),
	    Order(friendly, PointAt(other, k_PointAtExaminedSeconds, Cue::None), Cue::Playfulness),
	    PointAt(other, k_PointAtExaminedSeconds, Cue::Playfulness),
	};
	if (random(2) == 0)
	{
		agenda.push_back(Order(friendly, PlayOnce(animations::k_Happy)));
	}
	if (random(2) == 0)
	{
		agenda.push_back(PlayOnce(animations::k_Happy));
	}
	return agenda;
}

float creature_mind::HillTogetherRadius(float radius, float friendRadius)
{
	return (radius + friendRadius) * k_HillTogetherRadii;
}

std::vector<Step> creature_mind::SitOnTopOfHillWithFriend(const Friends& friends, glm::vec2 middle, float radius,
                                                          bool seesFriend, const Random& random)
{
	std::vector<Step> agenda {AskToBeFriends(friends.friendly)};
	if (!seesFriend)
	{
		agenda.push_back(GoNearLooking(friends.friendly, k_HillFriendNearHeights * friends.height));
	}
	agenda.push_back(Fluttering(FaceThing(friends.friendly, k_FaceFriendSeconds), friends.friendly));
	if (random(2) == 0)
	{
		agenda.push_back(PlayOnce(animations::k_Happy));
	}
	const float within = k_HillPlaceWithinHeights * friends.height;
	const auto friendPlace = middle - glm::vec2(radius * 0.25f, 0.0f);
	const auto selfPlace = middle + glm::vec2(radius * 0.25f, 0.0f);
	const Step faceDown {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::FaceDownSlope}};
	const auto friendSits = SitFor(k_SitTogetherSeconds + creature_emote::FloatRandom(random, k_SitTogetherExtraSeconds));
	const auto sits = SitFor(k_SitTogetherSeconds + creature_emote::FloatRandom(random, k_SitTogetherExtraSeconds));
	Append(agenda, std::array {
	                   Order(friends.friendly, GoTo(friendPlace, within)),
	                   GoTo(selfPlace, within, GoingLook(selfPlace)),
	                   WaitForStill(friends.friendly),
	                   Order(friends.friendly, faceDown),
	                   Order(friends.friendly, friendSits),
	                   faceDown,
	                   Main(sits),
	               });
	return agenda;
}

bool creature_mind::CanSee(float distance, float objectHeight)
{
	return distance < k_SeenWithinMetres || distance < objectHeight * k_SeenWithinHeights;
}

glm::vec3 creature_mind::ThrowPointAt(glm::vec3 standing, float drawnSize)
{
	return standing + glm::vec3(0.0f, k_ThrowAtHeightFraction * (k_StandingHeightOfSizeOne * drawnSize), 0.0f);
}

std::vector<Step> creature_mind::PlayThrowingGameWithFriend(const Friends& friends, uint32_t toy, glm::vec3 atSelf,
                                                            glm::vec3 atFriend, bool handFull)
{
	std::vector<Step> agenda {
	    GoNearLooking(friends.friendly, k_ThrowGameNearHeights * friends.height),
	    FaceThing(friends.friendly, k_FaceFriendSeconds, Cue::Smile, Gaze {.object = friends.friendly}),
	    Ask(friends.friendly, creature_friendship::RequestKind::Play, Cue::Playfulness),
	    Demobilise(friends.friendly, true),
	};
	Append(agenda, HappyAtFriend(friends.friendly));
	agenda.push_back(Order(friends.friendly, FaceThing(friends.self, k_FaceFriendSeconds), Cue::Grimace));
	if (!handFull)
	{
		agenda.push_back(PickUpThing(toy));
	}
	Append(agenda, std::array {
	                   Main(ThrowAt(atFriend)),
	                   WaitFor(k_WaitAfterThrowSeconds),
	                   Order(friends.friendly, PickUpThing(toy)),
	                   WaitForStill(friends.friendly),
	                   Order(friends.friendly, ThrowAt(atSelf)),
	                   WaitForStill(friends.friendly),
	                   Demobilise(friends.friendly, false),
	               });
	return agenda;
}

std::vector<Step> creature_mind::GiveFriendToy(uint32_t self, uint32_t friendly, uint32_t toy, bool handFull,
                                               const Random& random)
{
	std::vector<Step> agenda {
	    AskToBeFriends(friendly),
	    Order(friendly, PointAt(self, k_FriendPointsAtGiverSeconds, Cue::None), Cue::Smile),
	};
	if (!handFull)
	{
		agenda.push_back(PickUpThing(toy));
	}
	Append(agenda, std::array {
	                   GoNearLooking(friendly, 0.0f),
	                   Fluttering(FaceThing(friendly, k_FaceFriendSeconds, Cue::Compassion), friendly),
	                   Main({.kind = Step::Kind::Object,
	                         .order = {.kind = ObjectOrder::Kind::Discard, .animation = creature_throw::k_PutDown},
	                         .face = Cue::Smile}),
	                   Step {.kind = Step::Kind::WaitInMap, .object = toy, .gaze = Gaze {.object = toy}},
	                   Order(friendly, PickUpThing(toy), Cue::Smile),
	                   FaceThing(friendly, k_FaceFriendSeconds, Cue::Compassion, Gaze {.object = friendly}),
	                   WaitFor(k_WaitAfterGiftSeconds, Cue::Smile),
	                   WaitForFree(friendly),
	               });
	// The way the friend plays with it is drawn as the agenda is made
	const auto keep = creature_object_actions::k_FirstKeepAnimation +
	                  random(static_cast<uint32_t>(creature_object_actions::k_KeepAnimationCount));
	agenda.push_back(Order(
	    friendly, {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::Keep, .animation = keep}}, Cue::Smile));
	return agenda;
}

std::vector<Step> creature_mind::ShowFriendCitadel(const Friends& friends, const Citadel& citadel, float radius,
                                                   float friendRadius, glm::vec3 head, bool seesFriend, const Random& random)
{
	std::vector<Step> agenda;
	if (!seesFriend)
	{
		agenda.push_back(GoNearLooking(friends.friendly, k_TogetherNearHeights * friends.height, Cue::Compassion));
	}
	const glm::vec2 heart {citadel.point.x, citadel.point.z};
	auto wait = WaitForStill(friends.friendly);
	wait.face = Cue::Grimace;
	// The friend's wait is drawn before its own
	const float friendWaits = k_ShowCitadelWaitSeconds + creature_emote::FloatRandom(random, k_ShowCitadelWaitExtraSeconds);
	const float waits = k_ShowCitadelWaitSeconds + creature_emote::FloatRandom(random, k_ShowCitadelWaitExtraSeconds);
	Append(agenda,
	       std::array {
	           FaceThing(friends.friendly, k_FaceFriendSeconds, Cue::Compassion, Gaze {.object = friends.friendly}),
	           Ask(friends.friendly, creature_friendship::RequestKind::BeShown),
	           Order(friends.friendly,
	                 GoTo(heart, (friendRadius * k_CitadelNearRadii) + (citadel.radius * k_CitadelHeartRadii)), Cue::Puzzled),
	           GoTo(heart, (radius * k_CitadelNearRadii) + (citadel.radius * k_CitadelHeartRadii), GoingLook(heart)),
	           wait,
	           Main(PointAtPlace(citadel.point, k_PointAtCitadelSeconds, Cue::Grimace)),
	           Order(friends.friendly, TurnToFacePlace(head)),
	           WaitForFree(friends.friendly),
	           Order(friends.friendly, TurnToFacePlace(citadel.point)),
	           Order(friends.friendly, WaitFor(friendWaits), Cue::Puzzled),
	           WaitFor(waits, Cue::Compassion, Gaze {.feet = true}),
	       });
	return agenda;
}
