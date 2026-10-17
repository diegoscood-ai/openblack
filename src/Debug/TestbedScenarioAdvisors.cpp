/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the advisor spirits: the good and evil spirits flying out from their corners of the
// screen and back, with the rainbow trails they leave as they fly; the advisors told by the scripts to come out, cling
// to the bottom of the screen, point and act, and appear and vanish in smoke; the good spirit pointing at the land, from
// near the screen and from the world; the good spirit saying a line, its mouth moving with its voice, and a line whose
// recording carries gestures; the dialogue's texts under a full screen film; the help's did-you-know bubble over a
// tapped sign; and the advisors' remark on a wrecked building and their answers to the Help key

#include <vector>

#include <SDL_scancode.h>

#include "Input/BindableActions.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

constexpr size_t k_Good = 0;
constexpr size_t k_Evil = 1;

/// Two texts for the dialogue, and the falling spell's film
constexpr size_t k_FirstText = 4431;
constexpr size_t k_SecondText = 4432;
/// The script music type of the missionaries' song's first verse (help::karaoke), and a line of its words (the fourth,
/// five words: HELP_TEXT_MISSIONARY_SONG_VERSE1_04)
constexpr size_t k_FirstVerseMusic = 51;
constexpr size_t k_VerseLineText = 4765;
/// The keypad's Enter key, the dialogue's key in the original
constexpr size_t k_SkipKey = SDL_SCANCODE_KP_ENTER;
constexpr size_t k_FallingSpellFilm = 1;

/// The did-you-know sign's tip, a help text of the miracles' category
constexpr uint32_t k_Tip = 4127;
/// The sign stands 30 units north of the middle; the camera is about 20 units from it
constexpr glm::vec2 k_SignAt {0.0f, 30.0f};
/// Far enough west of the camera's line that the point the bubble hangs from is a little past the left edge of a
/// 1024 x 768 screen, with the horizontal field of view of 70 degrees, while the sign itself is still partly on it
constexpr glm::vec2 k_EdgeSignAt {-14.7f, 30.0f};
/// The spot on the land the good spirit points at: the middle of the map, in the testbed camera's view
constexpr glm::vec2 k_PointedAt {0.0f, 0.0f};
/// The line the good spirit says: the first of the advisors' voices, about five seconds long
constexpr uint32_t k_SpiritLine = 1;
/// A line whose recording's cue labels carry gestures: about two seconds, confused from its start ("[TE confused]" at
/// 0) and a shrug 1.65 s in ("[TA shrug]" at sample 36484 of 22050 a second)
constexpr uint32_t k_GestureLine = 26;

testbed_fixtures::Fixtures TipSign(glm::vec2 at)
{
	return {.highlights = {{.info = ecs::script_highlight::Info::DidYouKnow,
	                        .at = at,
	                        .tip = testbed_fixtures::Highlight::Tip {.text = k_Tip, .category = DykCategory::Miracles}}}};
}

Command Spirit(Kind kind, size_t spirit, float delay)
{
	return {.kind = kind, .delaySeconds = delay, .value = spirit};
}

using Action = Command::AdvisorAction;

Command Tell(size_t advisor, Action action, float delay, glm::vec2 point = glm::vec2(0.0f), uint32_t anim = 0)
{
	return {.kind = Kind::Advisor, .delaySeconds = delay, .point = point, .value = advisor, .advisor = action, .anim = anim};
}

/// Looking over the middle of the map from a little way off
Framing AdvisorsFraming()
{
	return {.shot = Shot::Overview, .include = {{-40.0f, -40.0f}, {40.0f, 40.0f}}, .distance = 1.0f};
}
} // namespace

void testbed_scenarios::AddAdvisorScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "advisors.trails",
	    .name = "The advisor spirits' rainbow trails",
	    .facet = Facet::Advisors,
	    .description = "At noon, with the clock standing still, the good spirit flies out from its corner of the screen "
	                   "after two seconds and the evil one half a second later, each to a spot of its own on the screen. "
	                   "Four seconds later both fly back home, and two seconds after that it starts again.",
	    .expected = "Each spirit leaves a rainbow trail behind it as it flies, a ribbon that fades towards its tail, and "
	                "the trail shrinks back into the spirit once it hovers still. The ribbon's faintest fringe, where the "
	                "texture's alpha times the trail's fade is under 5 of 255, is not drawn.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {Spirit(Kind::SpiritOut, k_Good, 2.0f), Spirit(Kind::SpiritOut, k_Evil, 0.5f),
	                 Spirit(Kind::SpiritHome, k_Good, 4.0f), Spirit(Kind::SpiritHome, k_Evil, 0.0f)},
	    .repeatFrom = 0,
	});
	all.push_back({
	    .id = "advisors.point",
	    .name = "The good advisor spirit pointing at the land",
	    .facet = Facet::Advisors,
	    .description = "At noon, with the clock standing still, the good spirit flies out from its corner of the screen "
	                   "after two seconds. Two seconds later it points at the middle of the map, as a script's "
	                   "SPIRIT_POINT_POS does; four seconds after that it points at the same spot again, this time "
	                   "flying out into the world beside it. Four seconds later it flies back home, and two seconds "
	                   "after that it starts again.",
	    .expected = "Near the screen the spirit raises its pointing arm over a quarter of a second, the arm turning as "
	                "it rises, holds it towards the spot, then lowers it the same way as it stops. In the world it flies "
	                "out beside the spot, lit by the land there, and points at it from there.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {Spirit(Kind::SpiritOut, k_Good, 2.0f),
	                 Tell(k_Good, Action::PointAtLand, 2.0f, k_PointedAt),
	                 {.kind = Kind::Advisor,
	                  .delaySeconds = 4.0f,
	                  .point = k_PointedAt,
	                  .value = k_Good,
	                  .gentle = true,
	                  .advisor = Action::PointAtLand},
	                 Spirit(Kind::SpiritHome, k_Good, 4.0f)},
	    .repeatFrom = 0,
	});
	all.push_back({
	    .id = "advisors.talk",
	    .name = "The good advisor spirit saying a line",
	    .facet = Facet::Advisors,
	    .description = "At noon, with the clock standing still, the good spirit flies out from its corner of the screen "
	                   "after two seconds. Two seconds later it says the first line of the advisors' voices, about five "
	                   "seconds long, as a script's text in its voice does. Seven seconds later it flies back home, and "
	                   "two seconds after that it starts again.",
	    .expected = "While the line is heard the spirit comes closer and its mouth moves with the voice, from the first "
	                "frame the line sounds, its three vowel shapes following the syllables; the mouth is worked out in "
	                "the spirit's own update, so it moves on the same frame as the sound it follows. Before the line "
	                "and after it the mouth stays shut.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {Spirit(Kind::SpiritOut, k_Good, 2.0f),
	                 {.kind = Kind::SpiritSay, .delaySeconds = 2.0f, .value = k_Good, .spiritLine = k_SpiritLine},
	                 Spirit(Kind::SpiritHome, k_Good, 7.0f)},
	    .repeatFrom = 0,
	});
	all.push_back({
	    .id = "advisors.gesture",
	    .name = "The good advisor spirit's gestures from its line's recording",
	    .facet = Facet::Advisors,
	    .description = "At noon, with the clock standing still, the good spirit flies out from its corner of the screen "
	                   "after two seconds. Two seconds later it says a line of the advisors' voices, about two seconds "
	                   "long, whose recording marks two gestures: a confused face from its start and a shrug 1.65 s in. "
	                   "Five seconds later it flies back home, and two seconds after that it starts again.",
	    .expected = "As the line starts the spirit's face turns confused, and about a second and a half into it, before "
	                "the line ends, the spirit shrugs; each gesture comes on the frame the line's time first passes its "
	                "mark. Without the recording's labels it would say the line with neither.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {Spirit(Kind::SpiritOut, k_Good, 2.0f),
	                 {.kind = Kind::SpiritSay, .delaySeconds = 2.0f, .value = k_Good, .spiritLine = k_GestureLine},
	                 Spirit(Kind::SpiritHome, k_Good, 5.0f)},
	    .repeatFrom = 0,
	});
	all.push_back({
	    .id = "dialogue.under_falling_film",
	    .name = "The dialogue under the falling spell's film",
	    .facet = Facet::Advisors,
	    .description = "At noon, with the clock standing still, a text is shown in the dialogue after a second, waiting "
	                   "for a click. Two seconds later a second text is given and, on the same frame, the falling "
	                   "spell's film starts: the game pauses and the world is no longer drawn, only the film.",
	    .expected = "While the film plays, neither the dialogue's box nor its words are drawn over it; the click cue "
	                "and the key icons still are.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::ShowText, .delaySeconds = 1.0f, .value = k_FirstText},
	                 {.kind = Kind::ShowText, .delaySeconds = 2.0f, .value = k_SecondText},
	                 {.kind = Kind::PlayFilm, .delaySeconds = 0.0f, .value = k_FallingSpellFilm}},
	});
	all.push_back({
	    .id = "dialogue.song_karaoke",
	    .name = "The missionaries' song sung under its text",
	    .facet = Facet::Advisors,
	    .description = "At noon, with the clock standing still, a line of the missionaries' song (five words) is shown "
	                   "in the dialogue after a second, waiting for a click, and on the same frame the script starts the "
	                   "song's first verse; the log tells the verse's line and beat three times.",
	    .expected = "While the verse plays, in a language that follows the song (English does), a soft glowing box in "
	                "the word's colour at half its alpha sits behind the word of the newest text the verse's beat is "
	                "at, gliding to the next word in under a fifth of a second as the beat moves on; the words before "
	                "it are under the box, the sung word and those after it over it. Without the verse there is no box.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::ShowText, .delaySeconds = 1.0f, .value = k_VerseLineText},
	                 {.kind = Kind::ScriptMusic, .delaySeconds = 0.0f, .value = k_FirstVerseMusic},
	                 {.kind = Kind::MusicState, .delaySeconds = 2.0f},
	                 {.kind = Kind::MusicState, .delaySeconds = 4.0f},
	                 {.kind = Kind::MusicState, .delaySeconds = 6.0f}},
	});
	all.push_back({
	    .id = "film.falling_creature",
	    .name = "The creature falling in the falling spell's film",
	    .facet = Facet::Appearance,
	    .description = "At noon, with the clock standing still, a tiger is held still for a second, then the falling "
	                   "spell's film starts: the game pauses and the world is no longer drawn, only the film and the "
	                   "copy of the creature falling in it.",
	    .expected = "Over the film, the creature's copy falls through the fall's camera, lit from above the film, in white "
	                "at first, then in the colours the spell gives it as it rises; it is hidden from 19.55 s to 27.35 s "
	                "of the film. Its eyes are drawn with it; the sparks and the light bursts in front of it.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {{.species = CreatureType::Tiger, .hold = true, .pauseMind = true}},
	    .commands = {{.kind = Kind::PlayFilm, .delaySeconds = 1.0f, .value = k_FallingSpellFilm}},
	});
	all.push_back({
	    .id = "dialogue.skip_key",
	    .name = "The dialogue's text cut by its key",
	    .facet = Facet::Advisors,
	    .description = "At noon, with the clock standing still, a text is shown in the dialogue after a second, waiting "
	                   "for a click; three seconds later the keypad's Enter key is held down for a second, then let go, "
	                   "with no click.",
	    .expected = "As the original reads that key, held, every frame: while it is down the text is taken as read, "
	                "as a click takes it, without its least time on the screen: its click cue goes and it no longer "
	                "waits for a click (the log's line of the key let go). The words stay, as nothing else is said.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::ShowText, .delaySeconds = 1.0f, .value = k_FirstText},
	                 {.kind = Kind::KeyDown, .delaySeconds = 3.0f, .value = k_SkipKey},
	                 {.kind = Kind::KeyUp, .delaySeconds = 1.0f, .value = k_SkipKey}},
	});
	all.push_back({
	    .id = "help.tip_bubble",
	    .name = "The did-you-know bubble over a tapped sign",
	    .facet = Facet::Advisors,
	    .description = "At noon, with the clock standing still, a did-you-know sign with a tip of the miracles stands "
	                   "on the land north of the middle, with the camera looking at it from about 20 units. After a "
	                   "second the player's hand taps it, as the interface's tap does.",
	    .expected = "The sign lights and turns to face the camera, and the help's bubble opens over it: a rounded, "
	                "see-through blue box fading in towards its bottom, with a tail pointing down at the sign, and in it "
	                "the tip's words under the title, each line over its dark shadow. It stays while the sign is on "
	                "the screen. On the first did-you-know ever tapped the help also starts its script that explains "
	                "the signs, when the land's scripts have it.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, 10.0f}, .look = {0.0f, 1.0f, 30.0f}},
	    .commands = {{.kind = Kind::TapHighlight, .delaySeconds = 1.0f, .point = k_SignAt}},
	    .fixtures = TipSign(k_SignAt),
	});
	all.push_back({
	    .id = "help.tip_bubble_edge",
	    .name = "The did-you-know bubble at the screen's edge",
	    .facet = Facet::Advisors,
	    .description = "As help.tip_bubble, with the sign at the left edge of the screen: the point over it that the "
	                   "bubble hangs from is just past the edge, while the sign itself is still partly on the screen. "
	                   "After a second the player's hand taps it.",
	    .expected = "The bubble is up while the sign is on the screen. As the original, it is drawn wherever the point "
	                "it hangs from falls across the screen, given up only nearer than the near clip, so its right part "
	                "shows at the left edge.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, 10.0f}, .look = {0.0f, 1.0f, 30.0f}},
	    .commands = {{.kind = Kind::TapHighlight, .delaySeconds = 1.0f, .point = k_EdgeSignAt}},
	    .fixtures = TipSign(k_EdgeSignAt),
	});
	all.push_back({
	    .id = "advisors.out",
	    .name = "Both advisors come out",
	    .facet = Facet::Advisors,
	    .description = "The good advisor, then the evil one, is told to come out of its corner, as a challenge's script tells "
	                   "it.",
	    .expected = "Each flies in from beyond its edge of the screen, the good one from the right and the evil one from "
	                "the left, to a place in the middle half of the screen in a second, trailing a faint rainbow. There "
	                "they hover a few units in front of the camera, bobbing and drifting on smooth curves, keeping apart "
	                "from each other, from the pointer and from the middle of the screen, leaning as they move across and "
	                "blinking. The good advisor, a white-bearded sage, wears a flickering halo; the evil one is a small red "
	                "imp. Both are lit from beside the camera, not by the land.",
	    .environment = {.dispenserGrid = false},
	    .framing = AdvisorsFraming(),
	    .commands = {Tell(k_Good, Action::Out, 0.5f), Tell(k_Evil, Action::Out, 0.2f)},
	});
	all.push_back({
	    .id = "advisors.cling",
	    .name = "The advisors cling to the bottom of the screen",
	    .facet = Facet::Advisors,
	    .description = "Both advisors are told to cling near the bottom of the screen, as the help does for a short "
	                   "exchange, then to come out again.",
	    .expected = "Each slides off the bottom edge of the screen and holds on to it, only its upper half showing, turned "
	                "to face up from the edge, swinging a little as it clings. After five seconds each lets go and flies "
	                "back into the middle of the screen.",
	    .environment = {.dispenserGrid = false},
	    .framing = AdvisorsFraming(),
	    .commands = {Tell(k_Good, Action::Cling, 0.5f, {0.25f, 0.95f}), Tell(k_Evil, Action::Cling, 0.0f, {0.75f, 0.95f}),
	                 Tell(k_Good, Action::Out, 5.0f), Tell(k_Evil, Action::Out, 0.0f)},
	});
	all.push_back({
	    .id = "advisors.point_and_act",
	    .name = "The advisors point and act",
	    .facet = Facet::Advisors,
	    .description = "Both come out; the good advisor points at the land in front of the camera and the evil one at a "
	                   "point of the screen; then the good one plays a nod and the evil one laughs, each at a point of the "
	                   "screen, and the evil one turns furious.",
	    .expected = "Each raises the arm on the side of what it points at, the good one turning to the land and the evil "
	                "one to the upper right of the screen, and holds it there, its head turned towards it. Told to act, "
	                "each flies to its point on the screen, plays the anim once and goes back to hovering. The evil one's "
	                "face turns furious and calms down over a few seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = AdvisorsFraming(),
	    .commands = {Tell(k_Good, Action::Out, 0.2f), Tell(k_Evil, Action::Out, 0.0f),
	                 Tell(k_Good, Action::PointAtLand, 2.0f, {0.0f, 20.0f}),
	                 Tell(k_Evil, Action::PointOnScreen, 0.0f, {0.8f, 0.2f}),
	                 Tell(k_Good, Action::PlayAnim, 4.0f, {0.3f, 0.5f}, 20),
	                 Tell(k_Evil, Action::PlayAnim, 0.0f, {0.7f, 0.5f}, 24), Tell(k_Evil, Action::Feel, 3.0f, {}, 7)},
	});
	all.push_back({
	    .id = "advisors.appear_and_vanish",
	    .name = "The advisors appear and vanish in smoke",
	    .facet = Facet::Advisors,
	    .description = "Both advisors appear, as a help script brings them, and after five seconds vanish.",
	    .expected = "Each appears at once a quarter of the way in from its side, halfway down the screen, fading in out of "
	                "a puff of smoke, grey for the good advisor and dark red for the evil one, which spreads and fades in "
	                "about a second. After five seconds each fades out in another puff.",
	    .environment = {.dispenserGrid = false},
	    .framing = AdvisorsFraming(),
	    .commands = {Tell(k_Good, Action::Appear, 0.5f), Tell(k_Evil, Action::Appear, 0.0f), Tell(k_Good, Action::Vanish, 5.0f),
	                 Tell(k_Evil, Action::Vanish, 0.0f)},
	});
	// A hut close in front of the camera, boulders thrown at it from the player's hand until it is wrecked
	constexpr glm::vec2 k_Hut {0.0f, 10.0f};
	std::vector<ObjectSetup> objects {{.type = AbodeInfo::CelticHut, .offset = k_Hut}};
	std::vector<ThrowSetup> throws;
	for (size_t i = 0; i < 4; ++i)
	{
		const float x = static_cast<float>(i % 2) * 4.0f - 2.0f;
		objects.push_back({.type = MobileStaticInfo::Boulder2Chalk, .offset = k_Hut + glm::vec2 {x, -30.0f}});
		throws.push_back({
		    .object = objects.size() - 1,
		    .from = k_Hut + glm::vec2 {x, -30.0f},
		    .height = 4.0f,
		    .velocity = {0.0f, 4.0f, 20.0f},
		    .delaySeconds = 1.0f + static_cast<float>(i) * 1.5f,
		    .repeatSeconds = 6.0f,
		});
	}
	all.push_back({
	    .id = "advisors.guidance_alerts",
	    .name = "The advisors remark on a wrecked building",
	    .facet = Facet::Advisors,
	    .description = "Boulders are thrown from the player's hand at a hut in front of the camera, again and again, "
	                   "with every kind of the advisors' guidance taken as heard long ago, so that the first remark due "
	                   "is said at once rather than after its wait of minutes.",
	    .expected = "Once a blow leaves less than four tenths of the hut standing, an advisor flies in and says one of "
	                "the remarks on destroying a building, with its text in the dialogue box. No second remark follows "
	                "soon: every remark restarts the remarks' common wait.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Hut - glm::vec2 {10.0f}, k_Hut + glm::vec2 {10.0f}},
	                .distance = 0.75f},
	    .objects = objects,
	    .commands = {{.kind = Kind::EndGuidanceWaits, .delaySeconds = 0.2f}},
	    .throws = throws,
	});
	// One thing of each kind in a row in front of the camera, for the Help key
	all.push_back({
	    .id = "advisors.help_key",
	    .name = "The Help key on one thing of each kind",
	    .facet = Facet::Advisors,
	    .description = "A row in front of the camera, from left to right: a villager of no town, a sheep, an oak, a "
	                   "Celtic hut, an Aztec statue, a rock, a food pile, a scroll and the player's tiger. The Help key "
	                   "is pressed once after two seconds; then put the hand over each, and over the bare ground and the "
	                   "sea, and press F1.",
	    .expected = "Each press has the advisors say the thing's help text in the dialogue box: the villager, belonging "
	                "to no town, gets one of the remarks about vagrants; the sheep, the oak, the hut, the statue, the "
	                "rock, the food pile (\"a food pile\") and the scroll their own descriptions; the player's tiger its "
	                "species' text. Over bare ground the text tells what the ground is made of (grass, earth, sand...); "
	                "over land at sea level, shallow water. A new press cuts short the one before. The debug bar's "
	                "consciences window shows the last answer.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-45.0f, 0.0f}, {45.0f, 20.0f}}, .distance = 0.8f},
	    .creatures = {{.species = CreatureType::Tiger, .offset = {40.0f, 10.0f}}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-40.0f, 10.0f}},
	                {.type = AnimalInfo::Sheep, .offset = {-30.0f, 10.0f}},
	                {.type = TreeInfo::Oak, .offset = {-20.0f, 10.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {-7.0f, 10.0f}},
	                {.type = FeatureInfo::AztcStatue, .offset = {6.0f, 10.0f}},
	                {.type = MobileStaticInfo::Rock, .offset = {15.0f, 10.0f}},
	                {.type = PotInfo::FoodPile, .offset = {22.0f, 10.0f}}},
	    .commands = {{.kind = Kind::PressKey,
	                  .delaySeconds = 2.0f,
	                  .value = static_cast<size_t>(input::BindableActionMap::HELP)}},
	    .fixtures = {.highlights = {{.info = ecs::script_highlight::Info::Bronze, .at = glm::vec2 {30.0f, 10.0f}}}},
	});
}
