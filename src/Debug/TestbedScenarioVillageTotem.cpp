/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of a town centre's totem: the hand takes hold of it with the Action button, slides it up the
// screen and lets go, as a player sets the share of the town's people at worship; and the totem as it stands, without
// a worship site and with one. With the share set as the drag sets it, the need signs over the worship site's altar

#include <vector>

#include "ScriptHeaders/ScriptEnums.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// Where the town centre stands; its totem stands in its middle, 4.6 above the land, its icon 2.7 above that
constexpr glm::vec2 k_Centre {0.0f, 40.0f};
/// Where the player's temple stands, well clear of the town centre
constexpr glm::vec2 k_Temple {0.0f, -60.0f};
/// Where the player's creature is penned, west of the camera and out of its shot
constexpr glm::vec2 k_Creature {-60.0f, 15.0f};
/// Where the player's temple stands for the altar's signs: north east of the town centre, short of the lake's shore. Its
/// town's worship site takes the temple's first slot, which lays it south east of the temple: its altar 12 m east and
/// 26 m south of the temple, the hunger sign's point 3 m east and 44 m south, the rest sign's 8 m east of that, and the
/// arrival point 18 m east and 39 m south
constexpr glm::vec2 k_TempleAhead {40.0f, 100.0f};
/// Where the worship site's villagers arrive, from the temple
constexpr glm::vec2 k_SiteArrival {18.0f, -39.0f};
} // namespace

void testbed_scenarios::AddVillageTotemScenarios(std::vector<Scenario>& all)
{
	// The scenario's town is a Celtic town of the player's, which believes in the player as a land's script makes its
	// player's town believe. The temple, built as the fixtures make it, opens the town's worship site, which the map
	// script's command then builds at once, as the lands' scripts do. The camera looks north at the totem from 25 m
	// south of it; the middle of the screen is on the centre's own roof, so the pointer goes a tenth of the screen above
	// it, onto the icon standing on the totem's sunk plinth
	all.push_back({
	    .id = "objects.village_totem_drag",
	    .name = "The hand slides a town centre's totem up",
	    .facet = Facet::Objects,
	    .description = "A Celtic town centre of the player's town, which has one villager, with the player's temple "
	                   "built south of it and the town's worship site built. The pointer goes to the totem in the middle "
	                   "of the centre, the Action (right) button goes down on it, the mouse moves up 30% of the screen "
	                   "over two seconds, and the button is let go half a second later.",
	    .expected = "The press takes hold of the totem, and the Select Lock tooltip shows its up and down arrows. "
	                "Sliding the mouse up raises the totem with it: a tenth of the town's share for each unit the hand "
	                "slides, three icon heights a screen. Let go, it drops back to where it stood and rises again to the "
	                "share it was let go at, 5.2 seconds for the whole way, and the town's villager sets off to worship. "
	                "While it holds the totem the hand stays on it, in its side hold round the icon, tipped over it and "
	                "rising and sinking with it, not following the pointer; let go, it goes back to the pointer. "
	                "While the two stand apart a second, see-through totem at half alpha shows the other share: where "
	                "the town's share stood while the hand holds it, then the share it was let go at while it rises; and "
	                "on every frame it is placed again the tooltip shows the share of the town's people worshipping.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 20.0f, 15.0f}, .look = {0.0f, 7.0f, 40.0f}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	                // A town with no one in it has no worship site
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_Centre + glm::vec2(10.0f, -10.0f),
	                 .joinTown = true}},
	    .commands = {{.kind = Kind::CreateWorshipSite, .delaySeconds = 1.0f, .value = static_cast<size_t>(Tribe::CELTIC)},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.4f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.0f, -0.3f}, .amount = 2.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 2.5f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.15f, 0.85f}}},
	    // As a land's script after it makes the player's town: the most the town's belief cap lets it believe
	    .townBelief = 10.0f,
	    .fixtures = {.temples = {{.at = k_Temple}}},
	});

	// The temple ahead of the town centre, and three villagers of the town waiting a few metres south of its worship
	// site's arrival point. The town's share is set to all of its people, as the totem's drag sets it once the hand lets
	// go: those it sends dance at the worship site, which has no food, so the altar's hunger sign rises. The camera stands
	// south of the site and looks north at it, the altar in the middle of the shot and the temple behind it. The debug
	// window's Temple, Altar need signs, shows each sign's desire and how full it shows
	all.push_back({
	    .id = "worship.altar_needs",
	    .name = "The need signs over a worship site's altar",
	    .facet = Facet::Objects,
	    .description = "A Celtic town centre of the player's town, which has three villagers, with the player's temple "
	                   "built north east of it and the town's worship site built at one second. The villagers stand a few "
	                   "metres south of the site's arrival point. Half a second later the town's share of people at "
	                   "worship is set to all of them, as the hand letting go of its totem sets it. The camera looks "
	                   "north at the site's altar from about 50 m south of it.",
	    .expected = "Before anyone comes to worship no sign shows over the altar. The three villagers the share sends "
	                "walk to the site's arrival point and on to the dance round the altar; the site's food pot is empty, "
	                "so once the first of them joins the dance the hunger sign (the citadel's hungry desire mesh, at the "
	                "site's first point, left of the altar and nearest the camera) rises out of the ground to its height "
	                "in a second, two and a half times its size as it starts and its own size from 15% of the way up, "
	                "turned to face the camera, and follows the dancers' hunger every turn. The rest sign shows only once "
	                "the dancers' lives are down by more than a quarter, and the hiding sign only once more than ten of "
	                "the site's people do not dance, so neither shows here. Under the hand, the hunger sign shows its "
	                "own tooltip with how full it is.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {50.0f, 16.0f, 25.0f}, .look = {52.0f, 3.0f, 62.0f}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_TempleAhead + k_SiteArrival + glm::vec2(-6.0f, -8.0f),
	                 .joinTown = true},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_TempleAhead + k_SiteArrival + glm::vec2(0.0f, -9.0f),
	                 .joinTown = true},
	                {.type = VillagerInfo::CelticForesterMale,
	                 .offset = k_TempleAhead + k_SiteArrival + glm::vec2(6.0f, -8.0f),
	                 .joinTown = true}},
	    .commands = {{.kind = Kind::CreateWorshipSite, .delaySeconds = 1.0f, .value = static_cast<size_t>(Tribe::CELTIC)},
	                 {.kind = Kind::SetWorshipShare, .delaySeconds = 0.5f, .amount = 1.0f}},
	    .townBelief = 10.0f,
	    .fixtures = {.temples = {{.at = k_TempleAhead}}},
	});

	// The same town centre, its totem's life and built percentage set by a land's script's SET_PROPERTY
	constexpr auto k_Health = static_cast<size_t>(script::ObjectPropertyType::Health);
	constexpr auto k_Built = static_cast<size_t>(script::ObjectPropertyType::BuiltPercentage);
	all.push_back({
	    .id = "objects.village_totem_hidden",
	    .name = "A town centre's totem without life or not built",
	    .facet = Facet::Objects,
	    .description = "A Celtic town centre of the player's town, which has one villager, seen from the south. A "
	                   "script sets its totem's HEALTH to 0 at two seconds and back to 1 at four, then its "
	                   "BUILT_PERCENTAGE to 0.5 at six and back to 1 at eight; the pointer then presses on the totem.",
	    .expected = "With no life, and again half built, nothing of the totem shows: neither its plinth nor its icon, "
	                "while the town centre stands as it was. Each time it is set back it shows again where it stood. "
	                "The last press takes hold of the totem as usual.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 20.0f, 15.0f}, .look = {0.0f, 7.0f, 40.0f}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_Centre + glm::vec2(10.0f, -10.0f),
	                 .joinTown = true}},
	    .commands = {{.kind = Kind::ScriptTotemProperty, .delaySeconds = 2.0f, .value = k_Health, .object = 0, .amount = 0.0f},
	                 {.kind = Kind::ScriptTotemProperty, .delaySeconds = 2.0f, .value = k_Health, .object = 0, .amount = 1.0f},
	                 {.kind = Kind::ScriptTotemProperty, .delaySeconds = 2.0f, .value = k_Built, .object = 0, .amount = 0.5f},
	                 {.kind = Kind::ScriptTotemProperty, .delaySeconds = 2.0f, .value = k_Built, .object = 0, .amount = 1.0f},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.4f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.0f, .value = 3}},
	    .townBelief = 10.0f,
	    .fixtures = {.temples = {{.at = k_Temple}}},
	});

	// The same town centre with the player's creature in the world: the icon on the centre's totem is the player's.
	// The camera stands 25 m south of the totem and looks straight at its icon, which is in the middle of the shot. The
	// creature is penned well west of the camera, out of the shot
	all.push_back({
	    .id = "objects.village_totem_creature_icon",
	    .name = "A town centre's totem wearing your creature's icon",
	    .facet = Facet::Objects,
	    .description = "A Celtic town centre of the player's town, which has one villager, seen from the south with its "
	                   "totem in the middle of the shot. The player's creature, the profile's, is penned 60 m west of "
	                   "the camera.",
	    .expected = "The icon standing on the totem is the icon of the species of the player's creature, as on the "
	                "totems of every town of a player who has a creature. Where the totem does not yet take its "
	                "player's creature's icon, it shows the hand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 20.0f, 15.0f}, .look = {0.0f, 9.0f, 40.0f}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_Centre + glm::vec2(10.0f, -10.0f),
	                 .joinTown = true}},
	    .townBelief = 10.0f,
	    .fixtures = {.creatures = {{.at = k_Creature, .hold = testbed_fixtures::Hold::Penned}}},
	});

	// raffclar's two: the totem as a town centre stands it, without a worship site and with one
	all.push_back({
	    .id = "objects.village_totem",
	    .name = "A town centre's totem",
	    .facet = Facet::Objects,
	    .description = "A Norse town centre of the player's town, which has no worship site.",
	    .expected = "The tribe's totem, the column in the middle of the centre, stands at the centre's totem point, turned "
	                "as the centre is, with the hand's icon on its pole (the player has no creature), sunk in the centre "
	                "with no rise: with nowhere to worship the town has no one at worship. Holding the Action button on "
	                "it takes no hold, as there is no worship site.",
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Centre - glm::vec2(15.0f), k_Centre + glm::vec2(15.0f)},
	                .distance = 0.5f},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre}},
	});
	all.push_back({
	    .id = "objects.village_totem_worship",
	    .name = "A town centre's totem with a temple",
	    .facet = Facet::Objects,
	    .description = "The same Norse town centre of the player's town, with the player's temple standing built "
	                   "beside it, which opens the worship site of the town, which has one villager.",
	    .expected = "Once the town's worship site is built, holding the Action button on the totem takes hold of it: the "
	                "hand goes to the icon, tipped forwards over it in its side hold, with no pointer drawn: the cursor's "
	                "image is pinned on the hand (the Game Details overlay's Cursor Image Pinned line follows it). "
	                "Sliding the mouse up raises it, a see-through second totem staying at the town's share, with the "
	                "People Worshipping tooltip by the hand. Let go, the share floats up over it in grey and it eases "
	                "there, the second totem waiting where it is going.",
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Centre - glm::vec2(15.0f), k_Centre + glm::vec2(15.0f)},
	                .distance = 0.5f},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre},
	                // A town with no one in it has no worship site
	                {.type = VillagerInfo::NorseHousewifeFemale,
	                 .offset = k_Centre + glm::vec2(10.0f, -10.0f),
	                 .joinTown = true}},
	    .fixtures = {.temples = {{.at = k_Temple}}},
	});
}
