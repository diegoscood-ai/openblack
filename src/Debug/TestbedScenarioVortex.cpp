/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the vortices between the lands: a way out of a land opening on the lake's bank, levelling
// the bank and the water's edge towards their average, and closing again; and the volcano's mouth, which leaves the land
// as it is; and a way into a land throwing out what a crossing file of its own holds

#include <string>

#include "3D/FlatLand.h"
#include "ECS/VortexSaveLines.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// On the lake's south bank: the levelled square takes in the plane, the bank and the shallows
constexpr glm::vec2 k_Lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;
constexpr glm::vec2 k_OnTheBank = k_Lake - glm::vec2(0.0f, flat_land::k_LakeHalfExtent.y + 5.0f);
/// At the top of the lake's south bank, where the slope begins, in the middle of its land block: the volcano's mouth is
/// only ever opened high above the water (its rock under the land would show through the shallows' clear land as it
/// would in the game), and its hole and ring are only on the block under its middle
constexpr glm::vec2 k_AboveTheBank = k_OnTheBank + glm::vec2(80.0f, -80.0f);
/// Out on the plane, where the land is already flat
constexpr glm::vec2 k_OnThePlane {-120.0f, 60.0f};

using Kind = Command::Kind;

/// The vortex kinds as the vortex command takes them
constexpr size_t k_In = 0;
constexpr size_t k_Out = 1;
constexpr size_t k_Volcano = 2;

/// A vortex opened at the point as a land's script opens one, after the last command
Command Open(size_t type, glm::vec2 point, float delay)
{
	return {.kind = Kind::CreateVortex, .delaySeconds = delay, .point = point, .value = type};
}

/// The vortex opened so many before told to fade out, after the last command
Command FadeOut(size_t vortex, float delay)
{
	return {.kind = Kind::VortexFadeOut, .delaySeconds = delay, .value = vortex};
}

/// A crossing file as a way out of a land writes it, every thing where the vortex stood: a farmer, a cow, a blank
/// line, a rock, and last a second farmer whose line has no new line after it
const std::string& CrossingFile()
{
	using namespace openblack::vortex::crossing;
	static const std::string k_File = [] {
		const auto text = [](const Line& thing) { return ecs::vortex_save_lines::Text(thing).value_or(std::string {}); };
		const map_coords::MapCoords here {};
		auto last = text(VillagerLine {.offset = here, .villagerType = "CELTIC_FARMER", .age = 40});
		if (!last.empty() && last.back() == '\n')
		{
			last.pop_back();
		}
		return text(VillagerLine {.offset = here, .villagerType = "CELTIC_FARMER", .age = 30}) +
		       text(AnimalLine {.offset = here, .type = static_cast<int32_t>(AnimalInfo::Cow), .age = 2}) + "\n" +
		       text(MobileStaticLine {.offset = here, .row = static_cast<int32_t>(MobileStaticInfo::Rock)}) + last;
	}();
	return k_File;
}
} // namespace

void testbed_scenarios::AddVortexScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.vortex_levels_the_bank",
	    .name = "A vortex out of the land levels the lake's bank",
	    .facet = Facet::Miracles,
	    .description = "A way out of a land opens on the lake's south bank after a second, as a land's script opens one, "
	                   "and another out on the flat plane; both are told to fade out 20 seconds after they open.",
	    .expected =
	        "For 2 seconds after each opens nothing happens; then over 5 seconds the bank and the water's edge in "
	        "an 11 by 11 cell square round the first are pulled towards the square's average height, the slope "
	        "easing out as it goes, wholly within 50 of the middle and less towards 56; the plane's stays flat. "
	        "Fading out does not undo it: the levelled bank stays after the vortices have gone, 7 seconds after "
	        "they were told to fade out. Each opens a hole in its land block under its middle, the swirl showing through it, "
	        "with its ring of light and glow on the ground.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_OnTheBank, k_OnThePlane, k_Lake}, .distance = 0.8f},
	    .commands = {Open(k_In, k_OnTheBank, 1.0f), Open(k_In, k_OnThePlane, 0.0f), FadeOut(0, 20.0f), FadeOut(1, 0.0f)},
	});

	all.push_back({
	    .id = "story.vortex_levels_under_the_living",
	    .name = "The living go down with the land a vortex levels",
	    .facet = Facet::Miracles,
	    .description = "A way out of a land opens on the lake's south bank after a second, as Land 1's opens beside the "
	                   "fisherman's wife waiting on the cliff. On the slope above it, within the square it levels, a "
	                   "villager, a cow, an oak and a hut.",
	    .expected = "As the bank is levelled over 5 seconds, the villager and the cow go down (or up) with the land under "
	                "them, their feet and shadows on the ground the whole time, never floating. The oak and the hut keep "
	                "the height they were put at, as in the game, which draws them where they were made.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OnTheBank.x - 30.0f, 13.0f, k_OnTheBank.y - 22.0f},
	                .look = {k_OnTheBank.x - 8.0f, 3.0f, k_OnTheBank.y - 22.0f}},
	    .objects = {{.type = VillagerInfo::NorseHousewifeFemale, .offset = k_OnTheBank + glm::vec2(-8.0f, -18.0f)},
	                {.type = AnimalInfo::Cow, .offset = k_OnTheBank + glm::vec2(-8.0f, -25.0f)},
	                {.type = TreeInfo::Oak, .offset = k_OnTheBank + glm::vec2(-8.0f, -11.0f)},
	                {.type = AbodeInfo::NorseHut, .offset = k_OnTheBank + glm::vec2(-8.0f, -34.0f), .yawDegrees = 90.0f}},
	    .commands = {Open(k_In, k_OnTheBank, 1.0f)},
	});

	all.push_back({
	    .id = "story.vortex_pulls_things_in",
	    .name = "A vortex out of the land pulls things in",
	    .facet = Facet::Miracles,
	    .description = "A way out of a land opens on the flat plane after a second, as Land 1's opens for the player to "
	                   "leave by. Within 25 of it stand a villager, a cow, an oak, a rock, a pile of food and a hut; a "
	                   "second cow grazes 45 to the east.",
	    .expected = "Nothing is taken while the vortex fades in (7 seconds). Once it is fully open, the villager, the cow, "
	                "the oak and the rock leave the land and are drawn round the middle, slowly at first, faster and "
	                "faster as they come nearer, rising or sinking onto the funnel, tumbling, and gone into the middle "
	                "about 8 seconds after each was taken. The pile gives up pots of food, a smaller pot every few turns, "
	                "that go the same way, until it is empty. The hut stays, and so does the far cow: the vortex only "
	                "reaches 30 along its sides (42 to its corners). The testbed is a run of the checks, so the crossing "
	                "file is not written (the Vortices window says so).",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OnThePlane.x - 30.0f, 45.0f, k_OnThePlane.y - 60.0f},
	                .look = {k_OnThePlane.x, 0.0f, k_OnThePlane.y}},
	    .objects = {{.type = VillagerInfo::NorseHousewifeFemale, .offset = k_OnThePlane + glm::vec2(-10.0f, 8.0f)},
	                {.type = AnimalInfo::Cow, .offset = k_OnThePlane + glm::vec2(12.0f, -6.0f)},
	                {.type = TreeInfo::Oak, .offset = k_OnThePlane + glm::vec2(6.0f, 14.0f)},
	                {.type = MobileStaticInfo::Rock, .offset = k_OnThePlane + glm::vec2(-14.0f, -10.0f)},
	                {.type = PotInfo::FoodPile, .offset = k_OnThePlane + glm::vec2(0.0f, -18.0f), .amount = 3000},
	                {.type = AbodeInfo::NorseHut, .offset = k_OnThePlane + glm::vec2(20.0f, 18.0f), .yawDegrees = 90.0f},
	                {.type = AnimalInfo::Cow, .offset = k_OnThePlane + glm::vec2(45.0f, 0.0f)}},
	    .commands = {Open(k_In, k_OnThePlane, 1.0f)},
	});

	all.push_back({
	    .id = "story.vortex_throws_things_out",
	    .name = "A vortex into the land throws out its new people",
	    .facet = Facet::Miracles,
	    .description = "A way into a land opens on the flat plane after a second, as Land 2's opens when the player "
	                   "arrives, with no crossing file to read (the testbed's game folder holds none). A hut stands 40 "
	                   "off. It is told to fade out 3 seconds after it opens, long before all are out.",
	    .expected = "Every fifth of a second, in a random direction, a new villager is thrown out of the vortex about 16 "
	                "from its middle, flying 8 to 13 a second outwards and 10 to 15 upwards, tumbling and flailing its "
	                "arms as it flies, until 30 have come out, every fifth a child. They join the hut's town. Nothing "
	                "thrown out can be destroyed while the vortex lasts. Though told to fade out, the vortex stays open "
	                "until the last villager is out, then fades over 5 seconds and goes 7 seconds after the last.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OnThePlane.x - 40.0f, 50.0f, k_OnThePlane.y - 70.0f},
	                .look = {k_OnThePlane.x, 0.0f, k_OnThePlane.y}},
	    .objects = {{.type = AbodeInfo::NorseHut, .offset = k_OnThePlane + glm::vec2(40.0f, 0.0f), .yawDegrees = 90.0f}},
	    .commands = {Open(k_Out, k_OnThePlane, 1.0f), FadeOut(0, 3.0f)},
	});

	all.push_back({
	    .id = "story.vortex_throws_out_the_last_crossing",
	    .name = "A vortex into the land throws out what the last one took",
	    .facet = Facet::Miracles,
	    .description = "A way into a land opens on the flat plane after a second, as Land 2's opens when the player "
	                   "arrives, and reads a crossing file of the scenario's own, as the way out of the land before would "
	                   "have written it: a farmer, a cow, a blank line, a rock, and a second farmer on a last line with no "
	                   "new line after it. A hut stands 40 off.",
	    .expected = "Every fifth of a second, in a random direction about 16 from the vortex's middle, the next line comes "
	                "out: the first farmer, thrown out flying 8 to 13 a second outwards and 10 to 15 upwards, tumbling and "
	                "flailing; then the cow, thrown the same way; then nothing new (the blank line hands the cow over "
	                "again, and it is already out); then the rock, thrown; then the second farmer, who stands where he "
	                "is made and is not thrown, and the file is done. Then new villagers come out as in "
	                "story.vortex_throws_things_out, 29 of them, so that 30 villagers have been thrown out in all with "
	                "the first farmer; those whose count is a multiple of five are children (six of them). The villagers "
	                "join the hut's town.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OnThePlane.x - 40.0f, 50.0f, k_OnThePlane.y - 70.0f},
	                .look = {k_OnThePlane.x, 0.0f, k_OnThePlane.y}},
	    .objects = {{.type = AbodeInfo::NorseHut, .offset = k_OnThePlane + glm::vec2(40.0f, 0.0f), .yawDegrees = 90.0f}},
	    .commands = {Open(k_Out, k_OnThePlane, 1.0f),
	                 {.kind = Kind::VortexCrossingFile, .delaySeconds = 0.0f, .value = 0, .crossingFile = CrossingFile()}},
	});

	all.push_back({
	    .id = "story.vortex_takes_what_is_thrown_in",
	    .name = "A vortex out of the land takes what is thrown into it",
	    .facet = Facet::Miracles,
	    .description = "A way out of a land opens on the flat plane after a second. A rock lies 60 west of it, out of its "
	                   "reach; 10 seconds in, once the vortex is fully open, the rock is thrown at it from 6 above the "
	                   "land, as the hand throws.",
	    .expected = "The rock flies into the vortex's unseen cylinder and is taken as it strikes it: it leaves the "
	                "physics, is drawn round the middle with the rest and is gone there. Nothing is lifted over the "
	                "cylinder. The rock is never taken from the land, out of reach.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OnThePlane.x - 50.0f, 40.0f, k_OnThePlane.y - 60.0f},
	                .look = {k_OnThePlane.x - 20.0f, 0.0f, k_OnThePlane.y}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = k_OnThePlane + glm::vec2(-60.0f, 0.0f)}},
	    .commands = {Open(k_In, k_OnThePlane, 1.0f)},
	    .throws = {{.object = 0, .height = 6.0f, .velocity = {30.0f, 10.0f, 0.0f}, .delaySeconds = 10.0f}},
	});

	all.push_back({
	    .id = "story.vortex_opens_a_hole",
	    .name = "A vortex out of the land opens a hole in it",
	    .facet = Facet::Miracles,
	    .description = "A way out of a land opens on the flat plane after a second, seen from close above, and is told "
	                   "to fade out 40 seconds after it opens.",
	    .expected = "For 2 seconds nothing shows; then over 5 seconds a hole opens in the land from the swirl's darkest "
	                "strokes outwards until the whole disc, 45 across, is open, the spinning funnel showing down through "
	                "it over the sea's colour, with a ring of light laid on the land around it, the glow on the ground "
	                "and the stars spinning over it. The hole ends at the edge of its land block. Fading out closes it "
	                "again over 5 seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OnThePlane.x - 25.0f, 60.0f, k_OnThePlane.y - 70.0f},
	                .look = {k_OnThePlane.x - 25.0f, 0.0f, k_OnThePlane.y + 10.0f}},
	    .commands = {Open(k_In, k_OnThePlane, 1.0f), FadeOut(0, 40.0f)},
	});

	all.push_back({
	    .id = "story.volcano_vortex_keeps_the_land",
	    .name = "The volcano's mouth leaves the land as it is",
	    .facet = Facet::Miracles,
	    .description = "The glowing mouth of a volcano is opened at the top of the lake's south bank, out of the water, "
	                   "as the last land's script opens one on its mountain.",
	    .expected = "It starts fully open and the slope of the bank under it is kept: the volcano's mouth never levels "
	                "the land. Its fiery hole, ring and glow show on the ground and it rumbles.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_AboveTheBank, k_Lake}, .distance = 0.8f},
	    .commands = {Open(k_Volcano, k_AboveTheBank, 1.0f)},
	});
}
