/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/
// Miracles under way kept in a saved game, as a land timeline on the game's own Land 1 with the SkipBox answered 3
// (docs/wiki/project/scenarios.md, "Playing a timeline on a land"): one seed after another given to the hand as a
// bubble's would be and poured on the field where the creatures are chosen, the quick save taken with them still
// running, so a load of the save shows what comes back. A forest, a teleport stone and a flock of birds are poured,
// and the game saved while the flock flies; a storm and a spiritual shield, which a pour does not cast, are sized by a
// circle drawn with the Action button held; then the game is saved again; then a lightning bolt is poured and the game
// saved a third time while it strikes; then food, wood, a heal, a fireball and water are poured along a second row, the
// game saved as each is let go while it is under way, a physical shield and a tornado (the storm's second power-up)
// sized by circles, and a blast poured and saved at once. Each save takes the place of the one before in the quick
// save's slot, so a run cut just after a save leaves that one in it.
#include <cstddef>

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "Enums.h"
#include "Input/BindableActions.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// The Action (right) button
constexpr size_t k_ActionButton = 3;
/// A point of the field where the creatures are chosen, inside the camera's fence at the land's start (a camera put
/// outside it is pushed back in, and the hand then misses the point), where the miracles are poured
constexpr glm::vec2 k_Field {2120.0f, 3090.0f};
/// How far apart along the field the miracles are poured
constexpr float k_Step = 15.0f;
/// How long the Action button pours a miracle
constexpr float k_PourSeconds = 3.5f;
/// The storm seed's power-up that makes a tornado (its base is the storm, its first the lightning storm)
constexpr int k_TornadoPowerUp = 1;

/// The hand lets go of what it holds and the timeline waits for it to hold nothing: a seed the hand keeps while its
/// chants last (the bolt, the water) stays in it until it is pressed or let go, and the hand takes no other seed
/// meanwhile
void EmptyTheHand(std::vector<Command>& all)
{
	all.push_back({.kind = Kind::HandDrop, .delaySeconds = 0.5f});
	all.push_back({.kind = Kind::WaitHandEmpty, .waitUntilFree = true});
}

/// The camera put 70 south of a point and 45 up, looking at it
Command CameraSouthOf(glm::vec2 at, float delay)
{
	return {.kind = Kind::CameraTo, .delaySeconds = delay, .eye = {at.x, 45.0f, at.y - 70.0f}, .look = {at.x, 5.0f, at.y}};
}

/// The quick save, as its keys ask for it
Command QuickSave(float delay)
{
	return {.kind = Kind::PressKey, .delaySeconds = delay, .value = static_cast<size_t>(input::BindableActionMap::QUICK_SAVE)};
}

/// A seed given to the hand as a bubble's would be, the hand held over the point, the Action button held while it
/// pours, then let go, quick saved at once if asked (with the miracle just under way); then the hand lets go of what it
/// still keeps and the timeline waits for it to be empty, so that the next seed, a circle's too, comes to an empty hand
void Pour(std::vector<Command>& all, SpellSeedType seed, glm::vec2 at, float delay, bool saveAtLetGo = false)
{
	all.push_back({.kind = Kind::HoldSeed, .delaySeconds = delay, .value = static_cast<size_t>(seed)});
	all.push_back(CameraSouthOf(at, 1.0f));
	all.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = at});
	all.push_back({.kind = Kind::PointerPress, .delaySeconds = 1.0f, .waitUntilFree = true, .value = k_ActionButton});
	all.push_back({.kind = Kind::PointerRelease, .delaySeconds = k_PourSeconds, .value = k_ActionButton});
	if (saveAtLetGo)
	{
		all.push_back(QuickSave(0.1f));
	}
	all.push_back({.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1});
	EmptyTheHand(all);
}

/// A seed that a circle sizes (a storm, a tornado, a shield): given to the hand, at a power-up if asked, the pointer
/// put on the land at the middle of the screen, where the camera looks at the point, and the circle drawn round it with
/// the Action button held; letting go casts it there at the circle's size
void Circle(std::vector<Command>& all, SpellSeedType seed, glm::vec2 at, float delay, std::optional<int> powerUp = std::nullopt)
{
	all.push_back({.kind = Kind::HoldSeed, .delaySeconds = delay, .value = static_cast<size_t>(seed)});
	all.back().powerUp = powerUp;
	all.push_back(CameraSouthOf(at, 1.0f));
	all.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}});
	all.push_back({.kind = Kind::DrawGesture, .delaySeconds = 1.0f, .value = static_cast<size_t>(GestureType::Circle)});
}

std::vector<Command> MiraclesKept()
{
	std::vector<Command> all;
	float along = 0.0f;
	for (const auto seed : {SpellSeedType::Nature, SpellSeedType::Teleport, SpellSeedType::FlockFlying})
	{
		Pour(all, seed, {k_Field.x + along, k_Field.y}, all.empty() ? 3.0f : 1.0f);
		along += k_Step;
		// the flock flies for 25 seconds only, so it is saved as soon as it is poured, before any other miracle
		if (seed == SpellSeedType::FlockFlying)
		{
			all.push_back(QuickSave(0.5f));
		}
	}
	for (const auto seed : {SpellSeedType::Storm, SpellSeedType::Shield})
	{
		// the seed is given once the drawing before it has ended
		Circle(all, seed, {k_Field.x + along, k_Field.y}, 3.0f);
		all.at(all.size() - 4).waitUntilFree = true;
		along += 2.0f * k_Step;
	}
	all.push_back(QuickSave(3.0f));
	all.back().waitUntilFree = true;
	// saved again with the bolt still striking, as soon as the pour is let go
	Pour(all, SpellSeedType::LightningBolt, {k_Field.x - k_Step, k_Field.y}, 3.0f, true);
	// the other miracles a hand pours, along a second row north of the first, each quick saved as soon as its pour is let
	// go, while it is under way (a later save finds none of them, as they end within seconds); a physical shield at the
	// row's east end and a tornado west of the bolt, each sized by a circle; the blast last, quick saved at its let go too
	const float row = k_Field.y + 2.0f * k_Step;
	along = 0.0f;
	for (const auto seed :
	     {SpellSeedType::Food, SpellSeedType::Wood, SpellSeedType::Heal, SpellSeedType::Fire, SpellSeedType::Water})
	{
		Pour(all, seed, {k_Field.x + along, row}, 3.0f, true);
		along += k_Step;
	}
	Circle(all, SpellSeedType::PhysicalShield, {k_Field.x + along, row}, 3.0f);
	all.at(all.size() - 4).waitUntilFree = true;
	Circle(all, SpellSeedType::Storm, {k_Field.x - 2.0f * k_Step, k_Field.y}, 3.0f, k_TornadoPowerUp);
	all.at(all.size() - 4).waitUntilFree = true;
	// the blast's seed given once the circle's drawing has ended, poured at the row's west end as the bolt was
	const auto blast = all.size();
	Pour(all, SpellSeedType::BeamExplosion, {k_Field.x - k_Step, row}, 3.0f, true);
	all.at(blast).waitUntilFree = true;
	return all;
}
} // namespace

void testbed_scenarios::AddLandMiraclesKeptScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.miracles_kept_land1",
	    .name = "Land 1: miracles under way kept in a quick save",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 with the SkipBox answered 3, as land:Land1.txt:<frames>:skip=3/30:"
	                   "timeline=story.miracles_kept_land1, in a run that keeps the player's profile. Pours a forest, a "
	                   "teleport stone and a flock of birds along the field where the creatures are chosen, quick saving "
	                   "while the flock flies, then sizes a storm and a spiritual shield there with a circle, each seed "
	                   "given to the hand as a bubble's would be, and quick saves; then pours a lightning bolt and quick "
	                   "saves again while it strikes; then pours food, wood, a heal, a fireball and water along a second "
	                   "row, sizes a physical shield there and a tornado (the storm at its second power-up) west of the "
	                   "bolt with circles, pours a blast; it quick saves as each of the second row's pours and the "
	                   "blast's is let go.",
	    .expected = "Each seed comes into the hand and its miracle starts where it is poured; each quick save is "
	                "written into slot 19 with its miracles running, the first with the flock flying, the third with "
	                "the bolt under way, each of the second row's with its own miracle just under way, the last with the "
	                "blast, the physical shield and the tornado. A load of any of them has them running again.",
	    .commands = MiraclesKept(),
	    .onLand = true,
	});
}
