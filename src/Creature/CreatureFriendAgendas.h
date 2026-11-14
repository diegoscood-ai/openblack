/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureIdleMind.h"

/// The agendas of the actions that a creature does with or for another creature: waving at a friend, which the friend
/// does too, howling at a friend, pining for and looking for a friend that it misses, and the joint actions that two
/// friends do side by side: being happy, having a poo, sitting, sleeping, drinking, and helping or attacking a town.
namespace openblack::creature_mind
{

/// Waving at a friend, as a joint action. The agenda has these steps:
/// 1. It asks the friend to play with it, with a playful face. If the friend refuses, the agenda stops.
/// 2. It makes the friend stand still in its mind.
/// 3. It tells the friend to turn to face it for a tenth of a second.
/// 4. It turns to face the friend for 1.4 seconds, with a smile.
/// 5. It tells the friend to wave.
/// 6. It waves. This is the main step.
/// 7. It lets the friend's mind go free again.
/// It waves from where it is: it does not walk to the friend.
[[nodiscard]] std::vector<Step> WaveAtFriend(uint32_t self, uint32_t friendly);

/// The creature smiles at a friend. The agenda has these steps:
/// 1. It goes near the friend, to within three times its own height, smiling and fluttering its eyelids at it.
/// 2. It turns to face the friend and holds still for 4 seconds and a draw of up to 4 more, smiling and fluttering its
///    eyelids. This is the main step.
/// 3. Half the time, by a draw made after the first, it shows it is happy.
/// It does not ask the friend first, and the friend does nothing of its own.
[[nodiscard]] std::vector<Step> SmileAtFriend(uint32_t friendly, float height, const Random& random);
constexpr float k_SmileNearHeights = 3.0f;
constexpr float k_SmileFaceSeconds = 4.0f;
constexpr float k_SmileFaceExtraSeconds = 4.0f;
constexpr float k_FriendFacesWaverSeconds = 0.1f;
constexpr float k_FaceFriendToWaveSeconds = 1.4f;

/// Showing another creature it hates it:
/// 1. It goes near the other, to within two and a half times the taller one's height, angry, looking where it goes.
/// 2. It turns to face the other for two seconds, angry, looking at it.
/// 3. Half the time it points at the other for two seconds with a grimace.
/// 4. It plays one of two angry actions, by a draw (k_HateShows), with no face of its own.
/// 5. Half the time it plays one more (k_HateLastShow).
/// The draws are made in that order.
[[nodiscard]] std::vector<Step> ShowCreatureItHatesIt(uint32_t other, float height, float otherHeight, const Random& random);
constexpr float k_HateNearHeights = 2.5f;
constexpr float k_HateFaceSeconds = 2.0f;
constexpr float k_HatePointSeconds = 2.0f;
/// A draw of two that is not 0 plays the first, else the second
constexpr std::array<size_t, 2> k_HateShows {222, 70};
constexpr size_t k_HateLastShow = 53;

/// Showing another creature how nice it reckons it is: it turns to face the other for two seconds, looking at it, then,
/// as its main step, by what it thinks of it (`howNice`, -1 to 1): neither nice nor nasty, it pulls the first silly face
/// at it; nice, it plays k_NiceShow; nasty, one of the two angry actions by a draw (k_HateShows). None for a thing that
/// is not a creature (`howNice` absent)
[[nodiscard]] std::optional<std::vector<Step>> ShowHowNiceItReckons(uint32_t other, std::optional<float> howNice,
                                                                    const Random& random);
constexpr float k_NiceFaceSeconds = 2.0f;
constexpr size_t k_NiceShow = 55;
constexpr size_t k_NeitherNiceFace = 16;

/// Howling at a friend: it turns to face the friend for two seconds, then howls
[[nodiscard]] std::vector<Step> HowlAtFriend(uint32_t friendly);
constexpr float k_FaceToHowlSeconds = 2.0f;
constexpr size_t k_Howl = 217;

/// Pining for a friend: with a sad face it turns to face the friend for two seconds and points at it for two seconds.
/// Then it plays its sad action. This is the main step.
[[nodiscard]] std::vector<Step> PineForFriend(uint32_t friendly);
/// Looking for a friend: with a sad face it points at the friend for two seconds and plays its sad action, which is the
/// main step. Then it goes near the friend, to three times its own height, as it looks where it goes.
[[nodiscard]] std::vector<Step> LookForFriend(uint32_t friendly, float height);
constexpr float k_PineSeconds = 2.0f;
constexpr float k_LookForFriendHeights = 3.0f;

/// Fighting another creature, as a script forces it:
/// 1. With an angry face it asks the other creature to fight it. If the other refuses, the rest is given up.
/// 2. It makes the other stand still in its mind.
/// 3. It fights the other. This is the main step: the fight takes both to their places in an arena, they taunt each
///    other, duel, and respond to how it went.
/// 4. It lets the other's mind go free again.
[[nodiscard]] std::vector<Step> FightCreature(uint32_t other);

/// Telling another creature to go away. By a first draw of two, half the time it fights it instead (FightCreature).
/// Otherwise, a joint action asked as a fight is:
/// 1. It asks the other, playful, looking at it; the other is made to stand still in its mind.
/// 2. The other turns to face it; it faces the other for 1.4 seconds, smiling, then is amazed (k_GoAwayAmazed).
/// 3. Its main step: for 3 seconds it points, with a grimace, past the other, as far beyond it as the other is from it.
/// 4. The other plays k_GoAwayOtherShow; it is amazed again; the other walks to that point, to within two of the other's
///    heights; it waits for the other to stand still; the other plays k_GoAwayOtherShow again; it is amazed once more.
/// 5. The other's mind is let go again.
/// `standing` and `otherStanding` are where the two stand.
[[nodiscard]] std::vector<Step> TellCreatureToGoAway(uint32_t self, uint32_t other, glm::vec3 standing, glm::vec3 otherStanding,
                                                     float otherHeight, const Random& random);
constexpr float k_GoAwayFaceSeconds = 1.4f;
constexpr float k_GoAwayPointSeconds = 3.0f;
constexpr float k_GoAwayOtherFacesSeconds = 0.1f;
constexpr float k_GoAwayHeights = 2.0f;
constexpr size_t k_GoAwayAmazed = 53;
constexpr size_t k_GoAwayOtherShow = 56;

/// Two creatures in a joint action: the creature that asks and its friend, by their entities' numbers, where each one
/// stands on the ground and how tall each one is
struct Friends
{
	uint32_t self {0};
	uint32_t friendly {0};
	glm::vec2 selfPoint {0.0f};
	glm::vec2 friendPoint {0.0f};
	float height {0.0f};
	float friendHeight {0.0f};
};

/// The places of two friends side by side: from the point halfway between them, the creature stands its own height
/// along the first axis of the map, and the friend stands its own height the other way. The first of the pair is the
/// creature's place.
[[nodiscard]] std::pair<glm::vec2, glm::vec2> SideBySide(const Friends& friends);
/// The places of two friends about a clear place: the creature stands a quarter of the radius along both axes of the
/// map from the middle, and the friend stands the same distance the other way. The first of the pair is the creature's
/// place.
[[nodiscard]] std::pair<glm::vec2, glm::vec2> AboutPlace(glm::vec2 middle, float radius);
/// The radius of the clear place that two friends sit in, and the radius of the clear place that they sleep in, from
/// their heights
[[nodiscard]] float SitTogetherRadius(float height, float friendHeight);
[[nodiscard]] float SleepTogetherRadius(float height, float friendHeight);
constexpr float k_SitTogetherHeights = 1.7f;

/// Kissing a friend, the two standing the kissing distance from the point halfway between them (creature_kiss::
/// KissPlaces, the first place the creature's). The agenda has these steps:
/// 1. It asks the friend to be friends, looking at it. If the friend refuses, the agenda stops.
/// 2. It tells the friend, puzzled, to go to its place, to within a tenth of a metre.
/// 3. It goes to its own place, to within a tenth of a metre, with a face of compassion.
/// 4. It tells the friend to turn to face it, a tenth of a second.
/// 5. It turns to face the friend a tenth of a second with a face of compassion, fluttering its eyelids.
/// 6. It tells the friend to kiss it.
/// 7. It kisses the friend. This is the main step.
/// 8. Half the time, it then faces the friend ten seconds with a smile, fluttering its eyelids, and shows it is happy.
[[nodiscard]] std::vector<Step> KissFriend(const Friends& friends, float kissingDistance, const Random& random);
constexpr float k_KissPlaceWithin = 0.1f;
constexpr float k_FaceToKissSeconds = 0.1f;
constexpr float k_FaceAfterKissSeconds = 10.0f;
constexpr float k_SleepTogetherHeights = 2.0f;

/// Being happy with a friend. The agenda has these steps:
/// 1. It asks the friend to be its friend, as it looks at it. If the friend refuses, the agenda stops.
/// 2. It tells the friend to go to its place (see SideBySide), to within the friend's height.
/// 3. It goes to its own place, to within its own height, as it looks where it goes.
/// 4. It waits until the friend does not move, as it looks at it.
/// 5. It tells the friend to turn to face it, for a tenth of a second.
/// 6. It turns to face the friend for a tenth of a second, and flutters its eyelids as it looks at it.
/// 7. It tells the friend to play the happy action.
/// 8. It plays the happy action. No step is the main step, so the last step is.
[[nodiscard]] std::vector<Step> BeHappyWithFriend(const Friends& friends);
constexpr float k_FaceFriendSeconds = 0.1f;

/// Having a poo with a friend: the first four steps of BeHappyWithFriend, then:
/// 5. It tells the friend to squat for a poo for four seconds, with a smile.
/// 6. It squats for a poo for four seconds, with a smile. This is the main step.
/// 7. It tells the friend to do its poo, with a smile.
/// 8. It does its poo, with a smile.
/// 9. It turns to face the friend for four seconds and up to four seconds more, with a smile, as it looks at it.
/// 10. It plays the happy action.
[[nodiscard]] std::vector<Step> PooWithFriend(const Friends& friends, const Random& random);
constexpr float k_PooTogetherSeconds = 4.0f;
constexpr float k_FaceAfterPooSeconds = 4.0f;
constexpr float k_FaceAfterPooExtraSeconds = 4.0f;

/// Sitting with a friend about a clear place (see AboutPlace): the first three steps of BeHappyWithFriend to these
/// places, then:
/// 4. It waits until the friend does not move, and flutters its eyelids as it looks at it.
/// 5. It tells the friend to turn to face it, for a tenth of a second.
/// 6. It turns to face the friend for a tenth of a second, as it looks at it.
/// 7. It tells the friend to sit for ten seconds and up to five seconds more.
/// 8. It sits for ten seconds and up to five seconds more, as it looks at the friend. This is the main step.
/// 9. It turns to face the friend for a tenth of a second, and flutters its eyelids as it looks at it.
/// 10. It plays the happy action.
[[nodiscard]] std::vector<Step> SitWithFriend(const Friends& friends, glm::vec2 middle, float radius, const Random& random);
constexpr float k_SitTogetherSeconds = 10.0f;
constexpr float k_SitTogetherExtraSeconds = 5.0f;

/// Sleeping with a friend about a clear place (see AboutPlace). Half the time it first yawns with sleepy eyes. Then the
/// first four steps of BeHappyWithFriend to these places, and:
/// 5. It tells the friend to turn to face it, for a tenth of a second.
/// 6. It turns to face the friend for a tenth of a second, and flutters its eyelids as it looks at it.
/// 7. It tells the friend to sleep for seventeen seconds.
/// 8. It sleeps for seventeen seconds. This is the main step. This sleep does not rest it.
/// 9. It plays the confused action with sleepy eyes and a grimace.
[[nodiscard]] std::vector<Step> SleepWithFriend(const Friends& friends, glm::vec2 middle, float radius, const Random& random);
constexpr float k_SleepTogetherSeconds = 17.0f;

/// Drinking with a friend at the coast nearest the creature, facing the sea nearest that coast. The friend's place is
/// its own height along the first axis of the map from the coast. The agenda has these steps:
/// 1. It asks the friend to be its friend, as it looks at it. If the friend refuses, the agenda stops.
/// 2. It tells the friend to go to its place, to within the friend's height.
/// 3. It goes to the coast, to within its own height, as it looks where it goes.
/// 4. It waits until the friend does not move, as it looks at it.
/// 5. It tells the friend to turn to face the sea.
/// 6. It turns to face the sea.
/// 7. It tells the friend to play the drink action.
/// 8. It plays the drink action. This is the main step.
/// 9. It tells the friend to drink.
/// 10. It drinks.
/// 11. It turns to face the friend for four seconds and up to four seconds more, with a smile, and flutters its eyelids
///     as it looks at it.
[[nodiscard]] std::vector<Step> DrinkWithFriend(const Friends& friends, glm::vec2 coast, glm::vec2 sea, const Random& random);
/// Eating from a field with a friend: it asks the friend to be friends, playfully; the friend goes to 10 m west of the
/// field and it to 10 m east, each to within the creature's height; each turns to face the field a tenth of a second
/// and takes a handful of wheat from it; the friend eats, then it eats, the main step; it turns to face the friend a tenth
/// of a second and plays the happy action, lets the friend go, then turns to face it again fluttering its eyelids and
/// plays the happy action. fieldPoint is the field's place
[[nodiscard]] std::vector<Step> EatFromFieldWithFriend(const Friends& friends, uint32_t field, glm::vec2 fieldPoint);
constexpr float k_FaceAfterDrinkSeconds = 4.0f;
constexpr float k_FaceAfterDrinkExtraSeconds = 4.0f;

/// The town of a joint action with a town: the town and its storage pit, by their entities' numbers, where the town is
/// and the radius of the pit on the ground
struct FriendsTown
{
	uint32_t town {0};
	uint32_t pit {0};
	glm::vec2 townPoint {0.0f};
	float pitRadius {0.0f};
};
/// Helping a town with a friend (desire Compassion), or attacking it with a friend (desire Anger). The agenda has these
/// steps:
/// 1. It asks the friend to be its friend, with a playful face, as it looks at it. If the friend refuses, the agenda
///    stops.
/// 2. It turns to face the friend for a tenth of a second, and flutters its eyelids as it looks at it.
/// 3. It plays the happy action.
/// 4. It points at the town for two seconds, with a playful face, as it looks at the town. This is the main step.
/// 5. It tells the friend to go near the storage pit, to twice the pit's radius and the creature's own height.
/// 6. It goes near the pit to the same distance, as it looks where it goes.
/// 7. It tells the friend to turn to face it, for a tenth of a second.
/// 8. It turns to face the friend for a tenth of a second.
/// 9. It tells the friend to have the desire about the town.
/// 10. It has the desire about the town.
/// The first of the two creatures to take up the desire stops the joint action, and so the agenda of the other one.
[[nodiscard]] std::vector<Step> TownWithFriend(const Friends& friends, const FriendsTown& town,
                                               creature_desires::Desire desire);
constexpr float k_PointAtTownSeconds = 2.0f;
constexpr float k_NearPitTimes = 2.0f;

/// Talking to a friend, and telling a friend a joke. The agenda has these steps:
/// 1. It goes near the friend, to three times its own height, with a smile, and flutters its eyelids as it looks at it.
/// 2. It turns to face the friend for four seconds and up to four seconds more, with a smile, fluttering its eyelids.
///    This is the main step.
/// 3. It asks the friend to be its friend, as it looks at it. If the friend refuses, the agenda stops.
/// 4. It tells the friend to turn to face it for four seconds.
/// 5. It talks, and flutters its eyelids as it looks at the friend.
/// 6. To talk, it tells the friend to talk too. To tell a joke, it tells the friend to laugh.
[[nodiscard]] std::vector<Step> TalkToFriend(uint32_t self, uint32_t friendly, float height, const Random& random);
[[nodiscard]] std::vector<Step> TellFriendAJoke(uint32_t self, uint32_t friendly, float height, const Random& random);
constexpr float k_TalkNearHeights = 3.0f;
constexpr float k_FaceToTalkSeconds = 4.0f;
constexpr float k_FaceToTalkExtraSeconds = 4.0f;
constexpr float k_FriendFacesTalkerSeconds = 4.0f;
constexpr size_t k_Laugh = 218;

/// Dancing with a friend. The agenda has these steps:
/// 1. It goes near the friend, to two times its own height, as it looks at it.
/// 2. It turns to face the friend for a tenth of a second, and flutters its eyelids as it looks at it.
/// 3. It plays the happy action.
/// 4. It asks the friend to be its friend, as it looks at it. If the friend refuses, the agenda stops.
/// 5. It makes the friend stand still in its mind.
/// 6. It tells the friend to turn to face it, for a tenth of a second.
/// 7. It waits until the friend does not move, with a playful face, as it looks at it.
/// 8. It tells the friend to dance alone for twenty seconds.
/// 9. It dances alone for twenty seconds, with a playful face. This is the main step.
/// 10. It lets the mind of the friend go free again.
[[nodiscard]] std::vector<Step> DanceWithFriend(uint32_t self, uint32_t friendly, float height);
constexpr float k_DanceNearHeights = 2.0f;
constexpr float k_DanceTogetherSeconds = 20.0f;

/// The first steps of arguing with, moping about with and showing off to a friend:
/// 1. It asks the friend to be its friend, as it looks at it. If the friend refuses, the agenda stops.
/// 2. It tells the friend to go near it, to three times the height of the creature.
/// 3. It goes near the friend, to three times its own height, with the face of the action, as it looks at the friend.
/// 4. It tells the friend to turn to face it for four seconds and up to four seconds more.
/// Arguing with a friend then has these steps:
/// 5. It turns to face the friend for a second with a smile, and flutters its eyelids as it looks at it.
/// 6. It tells the friend to turn to face it for a second.
/// 7. It turns to face the friend for four seconds and up to four seconds more, angry, as it looks at it.
/// 8. It points at the friend for two seconds, angry.
/// 9. It tells the friend to play the angry action. Then it plays the angry action, angry.
/// 10. It tells the friend to wait for five seconds. Then it waits for five seconds.
/// 11. It tells the friend to wait for two seconds. Then it waits for two seconds with a smile. This is the main step.
/// 12. It turns to face the friend for a tenth of a second, fluttering its eyelids, and plays the happy action.
[[nodiscard]] std::vector<Step> ArgueWithFriend(uint32_t self, uint32_t friendly, float height, const Random& random);
/// Moping about with a friend, after the first four steps with a sad face:
/// 5. It turns to face the friend for four seconds and up to four seconds more, sad, fluttering its eyelids.
/// 6. It tells the friend to play the sad action.
/// 7. It plays the sad action, sad. This is the main step.
/// 8. Half the time, it turns to face the friend for a tenth of a second, fluttering its eyelids, and plays the happy
///    action.
[[nodiscard]] std::vector<Step> MopeAboutWithFriend(uint32_t self, uint32_t friendly, float height, const Random& random);
/// Showing off to a friend, after the first four steps with a smile:
/// 5. It turns to face the friend for four seconds and up to four seconds more, with a smile, fluttering its eyelids.
/// 6. It points at its home for three seconds and grimaces, as it looks at its home. This is the main step.
/// 7. It tells the friend to turn to face its home.
/// 8. It points at the friend for two seconds, sad.
/// 9. It tells the friend to play the confused action.
/// 10. It turns to face the friend for a tenth of a second, fluttering its eyelids, and plays the happy action.
[[nodiscard]] std::vector<Step> ShowOffToFriend(uint32_t self, uint32_t friendly, float height, glm::vec3 home,
                                                const Random& random);
/// Confusing a friend, after the first four steps with a smile:
/// 5. It turns to face the friend for four seconds and up to four seconds more, with a smile, looking at it.
/// 6. It points at its home for three seconds and grimaces, as it looks at its home. This is the main step.
/// 7. It tells the friend to turn to face its home.
/// 8. It points at the friend for two seconds, sad, fluttering its eyelids.
/// 9. It tells the friend to play the confused action.
/// Showing a friend a miracle: going near the friend first when it cannot see it (1.5 heights for a destructive one,
/// 3.5 for a creating one), it turns to face it; then, with a thing to cast at (the nearest it knows that can be set
/// on fire, or that it can help), it asks the friend to be shown, both go near the thing, and it casts the miracle at
/// the thing (the main step) while the friend watches, the friend then turning to it amazed. With no such thing it
/// stops after turning to the friend.
/// - A destructive miracle (fireball, lightning, storm): the friend goes within 2.5 heights of the thing and it within
///   5, backs off to 2, turns to the thing in fear, points at it two seconds grimacing, the friend turns to its head,
///   it waits for the friend to be free, the friend turns to the thing, it casts.
/// - A creating miracle (food, wood): both go within 5 heights of the thing, it waits for the friend, grimacing; the
///   friend turns to the thing, it turns to it with compassion, points at it two seconds grimacing, the friend turns to
///   its head, it waits for the friend to be free, the friend turns to the thing, it casts, with compassion.
struct ShowSpell
{
	uint32_t magicType {0};
	bool destructive {true};
	/// The thing it casts at, none when it knows no such thing
	std::optional<uint32_t> target;
	/// Where its head is, which the friend turns to
	glm::vec3 head {0.0f};
	bool seesFriend {true};
};
[[nodiscard]] std::vector<Step> ShowFriendSpell(const Friends& friends, const ShowSpell& spell);
[[nodiscard]] std::vector<Step> ConfuseFriend(uint32_t self, uint32_t friendly, float height, glm::vec3 home,
                                              const Random& random);
constexpr float k_TogetherNearHeights = 3.0f;
constexpr float k_FaceTogetherSeconds = 4.0f;
constexpr float k_FaceTogetherExtraSeconds = 4.0f;
constexpr float k_FaceToArgueSeconds = 1.0f;
constexpr float k_PointAtFriendSeconds = 2.0f;
constexpr float k_ArgueWaitSeconds = 5.0f;
constexpr float k_AfterArgueSeconds = 2.0f;
constexpr float k_PointAtHomeSeconds = 3.0f;

/// Getting the attention of a friend that does something with another creature:
/// 1. It turns to face the friend for a tenth of a second, and flutters its eyelids as it looks at it.
/// 2. Half the time, it points at the friend for 0.4 seconds, sad.
/// 3. Half the time, it waves.
/// 4. A third of the time, it plays the action that asks the player to look at it.
/// 5. It turns to face the friend for a tenth of a second, fluttering its eyelids.
/// 6. It plays the sad action. This is the main step.
[[nodiscard]] std::vector<Step> GetAttentionFromFriend(uint32_t friendly, const Random& random);
constexpr float k_PointForAttentionSeconds = 0.4f;
constexpr size_t k_LookAtMe = 69;

/// Examining another creature with a friend. The agenda has these steps:
/// 1. It asks the friend to play, with a playful face, as it looks at it. If the friend refuses, the agenda stops.
/// 2. It makes the friend stand still in its mind.
/// 3. It tells the friend to turn to face the other creature, for a tenth of a second.
/// 4. It turns to face the other creature for a tenth of a second, with a playful face, as it looks at it.
/// 5. It tells the friend to point at the other creature for a second.
/// 6. It points at the other creature for a second, with a playful face.
/// 7. Half the time, it tells the friend to play the happy action. Half the time, it plays the happy action.
/// It does not let the mind of the friend go free. The end of the joint action does that.
[[nodiscard]] std::vector<Step> ExamineOtherCreatureWithFriend(uint32_t friendly, uint32_t other, const Random& random);
constexpr float k_PointAtExaminedSeconds = 1.0f;

/// Sitting with a friend on top of a hill, about a clear place on the hill. The creature sits a quarter of the radius
/// along the first axis of the map from the middle, and the friend the same distance the other way. The agenda has
/// these steps:
/// 1. It asks the friend to be its friend, as it looks at it. If the friend refuses, the agenda stops.
/// 2. When it cannot see the friend, it goes near the friend, to three times its own height, as it looks at it.
/// 3. It turns to face the friend for a tenth of a second, and flutters its eyelids as it looks at it.
/// 4. Half the time, it plays the happy action.
/// 5. It tells the friend to go to its place, to within two and a half times the height of the creature.
/// 6. It goes to its own place, to within the same distance, as it looks where it goes.
/// 7. It waits until the friend does not move, as it looks at it.
/// 8. It tells the friend to face down the slope, and to sit for ten seconds and up to five seconds more.
/// 9. It faces down the slope, and sits for ten seconds and up to five seconds more. This is the main step.
[[nodiscard]] std::vector<Step> SitOnTopOfHillWithFriend(const Friends& friends, glm::vec2 middle, float radius,
                                                         bool seesFriend, const Random& random);
/// The radius of the clear place on the hill, from the radius of each of the two on the ground
[[nodiscard]] float HillTogetherRadius(float radius, float friendRadius);
constexpr float k_HillTogetherRadii = 4.0f;
constexpr float k_HillPlaceWithinHeights = 2.5f;
constexpr float k_HillFriendNearHeights = 3.0f;

/// Whether a creature can see an object: when the object is less than ten metres away, or less than fifty times the
/// height of the object
[[nodiscard]] bool CanSee(float distance, float objectHeight);
constexpr float k_SeenWithinMetres = 10.0f;
constexpr float k_SeenWithinHeights = 50.0f;

/// Playing a throwing game with a friend, with a toy. The agenda has these steps:
/// 1. It goes near the friend, to five times its own height, as it looks at it.
/// 2. It turns to face the friend for a tenth of a second, with a smile, as it looks at it.
/// 3. It asks the friend to play, with a playful face, as it looks at it. If the friend refuses, the agenda stops.
/// 4. It makes the friend stand still in its mind.
/// 5. It turns to face the friend for a tenth of a second, fluttering its eyelids, and plays the happy action.
/// 6. It tells the friend to turn to face it for a tenth of a second, grimacing as it gives the order.
/// 7. With nothing in its hand, it picks the toy up, as it looks at it on the way.
/// 8. It throws the toy at the friend's point to throw at. This is the main step.
/// 9. It waits three seconds.
/// 10. It tells the friend to pick the toy up, and waits until the friend does not move, as it looks at it.
/// 11. It tells the friend to throw the toy at its own point to throw at, and waits until the friend does not move.
/// 12. It lets the mind of the friend go free again.
/// Each point to throw at is where a creature stands, raised by 0.82 of its standing height.
[[nodiscard]] std::vector<Step> PlayThrowingGameWithFriend(const Friends& friends, uint32_t toy, glm::vec3 atSelf,
                                                           glm::vec3 atFriend, bool handFull);
/// Where to throw at a creature that stands at a place, from its drawn size: 0.82 of its standing height, fifteen
/// times its drawn size, above where it stands
[[nodiscard]] glm::vec3 ThrowPointAt(glm::vec3 standing, float drawnSize);
constexpr float k_ThrowGameNearHeights = 5.0f;

/// Giving a friend a toy. The agenda has these steps:
/// 1. It asks the friend to be friends, as it looks at it. If the friend refuses, the agenda stops.
/// 2. It tells the friend to point at it for five seconds, smiling as it gives the order.
/// 3. With nothing in its hand, it picks the toy up, as it looks at it on the way.
/// 4. It goes up to the friend, as it looks at it.
/// 5. It turns to face the friend for a tenth of a second, fluttering its eyelids, with a face of compassion.
/// 6. It puts the toy down, with a smile. This is the main step.
/// 7. It waits until the toy is on the map, as it looks at it.
/// 8. It tells the friend to pick the toy up, smiling as it gives the order.
/// 9. It turns to face the friend for a tenth of a second, with a face of compassion, as it looks at it.
/// 10. It waits five seconds with a smile.
/// 11. It waits four turns, and then until the friend is free.
/// 12. It tells the friend to play with what it holds, one of the four ways chosen at random, smiling as it gives the
///     order.
[[nodiscard]] std::vector<Step> GiveFriendToy(uint32_t self, uint32_t friendly, uint32_t toy, bool handFull,
                                              const Random& random);
constexpr float k_FriendPointsAtGiverSeconds = 5.0f;
constexpr float k_WaitAfterGiftSeconds = 5.0f;
constexpr uint32_t k_WaitForFreeTurns = 4;

/// The citadel that a creature shows a friend: where it stands on the ground, and the radius of its heart on the ground
struct Citadel
{
	glm::vec3 point {0.0f};
	float radius {0.0f};
};
/// Showing its citadel to a friend. The agenda has these steps:
/// 1. When it cannot see the friend, it goes near the friend, to three times its own height, with a face of compassion.
/// 2. It turns to face the friend for a tenth of a second, with a face of compassion, as it looks at it.
/// 3. It asks the friend to be shown something, as it looks at it. If the friend refuses, the agenda stops.
/// 4. It tells the friend to go near the citadel, to five times the friend's radius and 1.2 times the heart's radius,
///    looking puzzled as it gives the order.
/// 5. It goes near the citadel, to five times its own radius and 1.2 times the heart's radius, as it looks there.
/// 6. It waits until the friend does not move, as it looks at it, and grimaces.
/// 7. It points at the citadel for three seconds, and grimaces, as it looks at it. This is the main step.
/// 8. It tells the friend to turn to face its head.
/// 9. It waits four turns, and then until the friend is free.
/// 10. It tells the friend to turn to face the citadel, and to wait five seconds and up to twenty more, looking
///     puzzled as it gives the order.
/// 11. It waits five seconds and up to twenty more, with a face of compassion, as it looks at its feet.
[[nodiscard]] std::vector<Step> ShowFriendCitadel(const Friends& friends, const Citadel& citadel, float radius,
                                                  float friendRadius, glm::vec3 head, bool seesFriend, const Random& random);
constexpr float k_CitadelNearRadii = 5.0f;
constexpr float k_CitadelHeartRadii = 1.2f;
constexpr float k_PointAtCitadelSeconds = 3.0f;
constexpr float k_ShowCitadelWaitSeconds = 5.0f;
constexpr float k_ShowCitadelWaitExtraSeconds = 20.0f;
constexpr float k_ThrowAtHeightFraction = 0.82f;
constexpr float k_StandingHeightOfSizeOne = 15.0f;
constexpr float k_WaitAfterThrowSeconds = 3.0f;

} // namespace openblack::creature_mind
