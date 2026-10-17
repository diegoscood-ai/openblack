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
// a worship site and with one

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
	                "hand goes to the icon, tipped forwards over it in its side hold, and the pointer stays on the hand. "
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
