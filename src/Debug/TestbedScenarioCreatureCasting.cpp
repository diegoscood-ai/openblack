/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of creatures casting miracles: a spell at another creature, one on a leash, a lightning bolt,
// a heal, food into a storage pit, water on a field, a try that fizzles and one the creature is too tired for

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

Command Know(MagicType type)
{
	// Once its mind has set up what it knows, on its first turns
	return {.kind = Kind::KnowMiracle, .delaySeconds = 0.5f, .value = static_cast<size_t>(type)};
}

Command CastAtCreature(MagicType type, size_t target, float delay)
{
	return {.kind = Kind::CastMiracle, .delaySeconds = delay, .value = static_cast<size_t>(type), .atCreature = target};
}

Command CastAtObject(MagicType type, size_t object, float delay)
{
	return {.kind = Kind::CastMiracle, .delaySeconds = delay, .value = static_cast<size_t>(type), .object = object};
}

CreatureSetup Caster(glm::vec2 offset, std::string_view label, float exhaustion = 0.0f)
{
	return {.label = label,
	        .species = CreatureType::Tiger,
	        .offset = offset,
	        .facingDegrees = 0.0f,
	        .needs = {.energy = 1.0f, .exhaustion = exhaustion, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}};
}

/// The creature as the one its owner leads on the leash, or not
CreatureSetup Leadable(CreatureSetup setup, bool leadable)
{
	setup.leashable = leadable;
	return setup;
}

CreatureSetup Other(glm::vec2 offset, std::string_view label)
{
	return {.label = label, .species = CreatureType::Cow, .offset = offset, .facingDegrees = 180.0f, .pauseMind = true};
}
} // namespace

void testbed_scenarios::AddCreatureCastingScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "miracles.creature_casts_itchy",
	    .name = "Creature casting: itchy at another creature",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the itchy spell is told to cast it at a cow a hundred metres away.",
	    .expected = "One time in five it pulls a playful face first. It walks until it is within five times its height "
	                "(about 75 metres) of the cow, backs off if nearer than twice its height, turns to face it and "
	                "holds still a tenth of a second, then raises its arms into the casting pose; as the pose loops the "
	                "spell is cast with the sound a creature's cast makes, wisps fly to the cow and flies gather round "
	                "its head; three seconds later the tiger lowers its arms. The cow starts scratching two seconds "
	                "after the cast. The tiger tires a little.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 110.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster"), Other({0.0f, 100.0f}, "target")},
	    .commands = {Know(MagicType::CreatureSpellItchy), CastAtCreature(MagicType::CreatureSpellItchy, 1, 0.5f)},
	});

	// The same spell at a creature that thinks for itself and is on a leash, so that what the spell does to its wants
	// and to the leash shows
	all.push_back({
	    .id = "miracles.creature_itchy_on_leashed_creature",
	    .name = "Creature casting: itchy at a creature on a leash",
	    .facet = Facet::Miracles,
	    .description =
	        "A tiger that knows the itchy spell is told to cast it at a second tiger sixty metres away, which "
	        "has its mind running and is the creature the player leads, put on the rope leash half a second in, the hand "
	        "held still between them.",
	    .expected = "The caster turns to the leashed tiger and casts without walking, as in the itchy cast at a cow. Once "
	                "the spell holds on it, the leash comes off the target every turn and stays off, and scratching is "
	                "what it wants above all else: it gives up what it was doing and scratches. The shot, after the "
	                "cast, shows no rope to its collar.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -5.0f}, {15.0f, 65.0f}}, .distance = 0.6f},
	    .creatures = {Leadable(Caster({0.0f, 0.0f}, "caster"), false), Leadable(Caster({0.0f, 60.0f}, "leashed target"), true)},
	    .commands = {{.kind = Kind::PutOnLeash, .creature = 1, .delaySeconds = 0.5f, .leash = LeashType::Rope},
	                 Know(MagicType::CreatureSpellItchy),
	                 CastAtCreature(MagicType::CreatureSpellItchy, 1, 0.5f),
	                 // Logged a few seconds after the cast has landed, and again just before the shot
	                 {.kind = Kind::LeashState, .creature = 1, .delaySeconds = 4.0f},
	                 {.kind = Kind::LeashState, .creature = 1, .delaySeconds = 8.0f}},
	    // The hand held still in view between them, so that the rope runs to it and holds the target in the shot
	    .hand = HandHold {.offset = {0.0f, 45.0f}, .height = 8.0f},
	});

	all.push_back({
	    .id = "miracles.creature_casts_lightning",
	    .name = "Creature casting: a lightning bolt at a rock",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the lightning bolt is told to cast it at a pillar of rock eighty metres away.",
	    .expected = "One time in five it shows its anger first. It walks up to within fifty metres of the rock, turns "
	                "to it, raises its arms and a bolt crackles from between its hands to the rock for three seconds, "
	                "then stops as it lowers its arms.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 90.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = FeatureInfo::FatPilarChalk, .offset = {0.0f, 80.0f}, .scale = 0.5f}},
	    .commands = {Know(MagicType::LightningBolt), CastAtObject(MagicType::LightningBolt, 0, 0.5f)},
	});

	// The same cast, and the player's hand takes hold of the tiger while its bolt crackles. The bolt starts about 5.5
	// seconds in and runs three seconds; the Action button goes down on the tiger at 6.5 seconds. The camera stands
	// south-west of where the tiger stops to cast, 50 metres short of the rock, and looks at its chest, in the middle of
	// the shot, where the pointer goes
	all.push_back({
	    .id = "miracles.hand_grips_casting_creature",
	    .name = "Creature casting: the hand grips a creature mid-cast",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the lightning bolt is told to cast it at a pillar of rock eighty metres away, "
	                   "as in the lightning scenario. A second after its bolt starts, the pointer is on the tiger and "
	                   "the Action (right) button goes down on it; it is let go a second later.",
	    .expected = "The press takes hold of the tiger, and the bolt it is casting stops there, at the grip, rather than "
	                "running its three seconds: a bolt lasts only while it is held, and a creature the hand grips lets "
	                "go of the miracle it casts. Without that, the bolt runs to its normal end as the tiger lowers its "
	                "arms.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-12.0f, 20.0f, 0.0f}, .look = {0.0f, 7.0f, 31.0f}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = FeatureInfo::FatPilarChalk, .offset = {0.0f, 80.0f}, .scale = 0.5f}},
	    .commands = {Know(MagicType::LightningBolt),
	                 CastAtObject(MagicType::LightningBolt, 0, 0.5f),
	                 {.kind = Kind::PointerTo, .delaySeconds = 4.5f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.0f, .value = 3}},
	});

	all.push_back({
	    .id = "miracles.creature_casts_heal",
	    .name = "Creature casting: a heal at a hurt villager",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the heal is told to cast it at a villager hurt to a third of its life, sixty "
	                   "metres away.",
	    .expected = "It walks up to within twice its height of the villager, backing off if nearer, faces it and casts: "
	                "the chakra lights over the villager, whose life fills up. The heal runs its own ten seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 70.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 60.0f}, .life = 0.33f}},
	    .commands = {Know(MagicType::Heal), CastAtObject(MagicType::Heal, 0, 0.5f)},
	});

	all.push_back({
	    .id = "miracles.creature_casts_food_on_town",
	    .name = "Creature casting: food into a town's storage pit",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the food miracle is told to cast it into a storage pit sixty metres away, "
	                   "which is its town's, beside two huts of the same town.",
	    .expected = "It shows no feeling first. It walks up to within two metres of the pit, kind as it goes, turns to it "
	                "with a kind face and holds still a tenth of a second, then raises its arms into the scattering pose "
	                "(start_scatter, then scatter) with a strained face. As the pose loops two beams run from its hands "
	                "to a point above the pit, 10 m above it, or 1.3 x its height if that is more, and food falls from "
	                "that point into the pit for three seconds: the pit's food goes up with each grain that lands, as "
	                "the miracle's table gives it (200 for the first grain and 18 for each after, times the tribal "
	                "power). Then it lowers its arms (end_scatter) and the beams fade out, still following its hands. It "
	                "tires a little.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 75.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, 60.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {-14.0f, 66.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {14.0f, 66.0f}}},
	    .commands = {Know(MagicType::Food), CastAtObject(MagicType::Food, 0, 0.5f)},
	});

	all.push_back({
	    .id = "miracles.creature_waters_field",
	    .name = "Creature casting: water on a dry field",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the water miracle is told to cast it on a newly sown wheat field, which has no "
	                   "food in it yet, sixty metres away.",
	    .expected = "It shows no feeling first and pulls no face as it walks up to within twice its height of the field, "
	                "then turns to it smiling and holds still a tenth of a second, and smiles as it raises its arms into "
	                "the scattering pose. As the pose loops two beams run from its hands to a point 10 m above the field "
	                "and the water falls from there onto the field for three seconds; then it lowers its arms and the "
	                "beams fade out, still following its hands.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 75.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = FieldTypeInfo::Wheat, .offset = {0.0f, 60.0f}}},
	    .commands = {Know(MagicType::Water), CastAtObject(MagicType::Water, 0, 0.5f)},
	});

	all.push_back({
	    .id = "miracles.creature_cast_fizzles",
	    .name = "Creature casting: a spell it hasn't learnt fizzles",
	    .facet = Facet::Miracles,
	    .description = "A tiger that has never seen the freeze spell is told to cast it at a cow.",
	    .expected = "It goes up to the cow and raises its arms, but nothing is cast: it lowers them at once, looks "
	                "embarrassed, and one time in two is sad as well. Its count of sightings of the freeze goes up by "
	                "one.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 60.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster"), Other({0.0f, 50.0f}, "target")},
	    .commands = {CastAtCreature(MagicType::CreatureSpellFreeze, 1, 1.0f)},
	});

	all.push_back({
	    .id = "miracles.creature_too_tired_to_cast",
	    .name = "Creature casting: too tired to cast",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the freeze spell but is all but exhausted is told to cast it at a cow.",
	    .expected = "It goes up to the cow and raises its arms, but can't pay for the spell: nothing is cast, and it "
	                "wants food above everything else.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 60.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster", 0.84f), Other({0.0f, 50.0f}, "target")},
	    .commands = {Know(MagicType::CreatureSpellFreeze), CastAtCreature(MagicType::CreatureSpellFreeze, 1, 0.5f)},
	});

	all.push_back({
	    .id = "miracles.creature_heals_by_itself",
	    .name = "Creature casting: a hurt villager of no town is not healed",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the heal, and wants nothing so much as to be kind, stands forty metres from a "
	                   "villager hurt to a third of its life, who belongs to no town. Nobody tells it to cast.",
	    .expected = "It does not heal the villager: a creature's compassion is always for a town it helps, which says "
	                "what it does, and this villager has none. It goes on with what it would do anyway. Run with 1800 "
	                "frames, so as to see that nothing comes of it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 50.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 40.0f}, .life = 0.33f}},
	    .commands = {Know(MagicType::Heal),
	                 {.kind = Kind::SetDesire,
	                  .creature = 0,
	                  .delaySeconds = 0.5f,
	                  .value = static_cast<size_t>(creature_desires::Desire::Compassion),
	                  .amount = 1.0f}},
	});

	all.push_back({
	    .id = "miracles.creature_heals_town_villager",
	    .name = "Creature casting: compassion for a village with someone hurt",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the heal, and wants nothing so much as to be kind, stands forty metres from a "
	                   "hut whose farmer is hurt to a third of his life; his neighbour is well. Nobody tells it to cast.",
	    .expected = "Its compassion is for the village. Each time it plans for it, the hurt farmer may make it think of "
	                "healing first, on a coin's toss; it then goes up to the farmer and casts the heal, as when told. "
	                "Otherwise it helps with what the village wants most. Run with 450 frames, so that the shot catches "
	                "the heal on the farmer, cast some five seconds in.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 50.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, 46.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 40.0f}, .joinTown = true, .life = 0.33f},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, 41.0f}, .joinTown = true}},
	    .commands = {Know(MagicType::Heal),
	                 {.kind = Kind::SetDesire,
	                  .creature = 0,
	                  .delaySeconds = 0.5f,
	                  .value = static_cast<size_t>(creature_desires::Desire::Compassion),
	                  .amount = 1.0f}},
	});

	all.push_back({
	    .id = "miracles.hand_itchy_seed",
	    .name = "Creature spells: the itchy spell held in the hand",
	    .facet = Facet::Miracles,
	    .description = "The player's hand is given the itchy creature spell and held still over the land.",
	    .expected = "Ten flies swarm round the hand's grasp point, circling a point that goes round it on a sphere of two "
	                "metres. The freeze and compassion spells would show nothing in the hand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, 20.0f}, {10.0f, 40.0f}}, .distance = 0.3f},
	    .commands = {{.kind = Kind::HoldSeed,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(SpellSeedType::CreatureSpellItchy)}},
	    .hand = HandHold {.offset = {0.0f, 30.0f}, .height = 8.0f},
	});

	all.push_back({
	    .id = "creature.compassion_empty_town",
	    .name = "Creature compassion: an empty town beside a village",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the heal, and wants nothing so much as to be kind, stands five metres from a town "
	                   "with nothing in it and thirty from the middle of a village, whose hut's farmer is hurt to a third "
	                   "of his life; his neighbour is well. Both towns believe in the player alike. Nobody tells it to "
	                   "cast.",
	    .expected = "An empty town is of no use to compassion: the tiger passes it over, its compassion is for the village, "
	                "and on a coin's toss it goes up to the farmer and casts the heal. Run with 450 frames.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 0.0f}, {20.0f, 70.0f}}},
	    .creatures = {Caster({-30.0f, 0.0f}, "caster")},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, 66.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 60.0f}, .joinTown = true, .life = 0.33f},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, 61.0f}, .joinTown = true}},
	    .commands = {Know(MagicType::Heal),
	                 {.kind = Kind::SetDesire,
	                  .creature = 0,
	                  .delaySeconds = 0.5f,
	                  .value = static_cast<size_t>(creature_desires::Desire::Compassion),
	                  .amount = 1.0f}},
	    .townBelief = 0.5f,
	    .bareTowns = {{.offset = {-35.0f, 0.0f}, .belief = 0.5f}},
	});

	all.push_back({
	    .id = "creature.compassion_far_hurt_villager",
	    .name = "Creature compassion: a village's hurt farmer far from it",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the heal, and wants nothing so much as to be kind, stands forty metres from a "
	                   "hut with one of its villagers well beside it; the village's other villager, a farmer hurt to a "
	                   "third of his life, is two hundred and fifty metres off. Nobody tells it to cast.",
	    .expected = "Its compassion is for the village, and what it does for it may be done to any of the village's own "
	                "people, however far: on a coin's toss it goes the long way to the farmer and casts the heal. Run "
	                "with 6000 frames.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -20.0f}, {260.0f, 50.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, 46.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, 41.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {250.0f, 0.0f}, .joinTown = true, .life = 0.33f}},
	    .commands = {Know(MagicType::Heal),
	                 {.kind = Kind::SetDesire,
	                  .creature = 0,
	                  .delaySeconds = 0.5f,
	                  .value = static_cast<size_t>(creature_desires::Desire::Compassion),
	                  .amount = 1.0f}},
	});
}
