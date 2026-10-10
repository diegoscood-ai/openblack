# Rocks and features

The fixed and loose scenery on the land: rocks and boulders of each land's stone, pillars and spikes, ruins and
monuments, gates, and the small things the land scripts scatter.

**Progress: 12/19 done, 4 partial — 74%**

## Rocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place rocks and boulders (flat, long, sharp, square and round) in chalk, limestone, sandstone and volcanic stone | done | `MobileStaticArchetype` (`src/ECS/Archetypes/MobileStaticArchetype.cpp`) |
| Rocks can be picked up by the hand and thrown | done | `Rocks::ValidForPlaceInHand` (2D radius up to 3.6, `src/ECS/Rocks.cpp`), thrown into the physics (`src/ECS/Physics/FromHand.cpp`); see ../hand/ |
| A thrown rock tumbles, lands and hurts what it hits | done | `src/ECS/Physics/PhysicsObjects.cpp` and the impact reactions; see ../physics/ |
| A rock hit hard enough splits in two | done | `Rocks::SplitInTwo` from the impact and `Rocks::Tap` from the hand (`HandSystem.cpp`); the creature's blow does not split them; detail in [rocks_splitting_and_heat.md](rocks_splitting_and_heat.md) |
| The creature picks up rocks, throws them and attacks with them | partial | The creature's object actions pick up and throw what it may lift (`CreatureObjectActionSystem.cpp`); attacking with rocks is not checked; see ../creature/ |
| Footpaths go round rocks | done | An obstacle entering the map cells sends the footpaths round it (`footpaths::RerouteFootpathsAroundObstacle` from `src/ECS/MapCells.cpp`); see ../terrain/footpaths.md |
| Challenge scripts make rocks | done | CHL CREATE of a mobile static makes a rock (`src/CHLApi.cpp`) |

## Features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place features: pillars and spikes in each stone, temples, statues, pyramids, the needle, the acropolis, the mine, the ark and its dock and wreck, the whale, craters, tombstones | done | `FeatureArchetype` (`src/ECS/Archetypes/FeatureArchetype.cpp`) |
| Features with a physics shape are solid | todo | Features are not among the obstacles of `PhysicsObjects::InteractsWithPhysicsObjects`, so thrown things pass through them; our wiki counts multi-map fixed objects as obstacles ([physics.md](../../bw1-notes/physics.md#manager-physicsobjectgameturnupdate-0x644fc0)) |
| Planned features are built up over time (unconfirmed which) | done | `src/ECS/FeatureBuild.cpp` (BUILT_PERCENTAGE: the Land 1 ark's dry dock drawn partly built) |
| Features catch fire and burn | done | Fire receivers by their info (combustion 2000, `src/ECS/Fire/FireObjectTraits.cpp`) |
| Features cast shadows | done | The static shadows of fixed objects (`RenderPass::StaticShadow`) |
| Mushrooms and toadstools the creature can eat (magic mushrooms change it) | todo | see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) and ../creature/ |

## Animated scenery

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place animated scenery: gates and land transitions | done | `AnimatedStaticArchetype` places them and they play their info's clip (`src/ECS/Archetypes/AnimatedStaticArchetype.cpp`) |
| Gates open when enough gate stones are laid (unconfirmed exact rule) | todo | see ../story/ |
| Gate totems (ape, cow, tiger, blank) stand by the creature gates | partial | Placed as scenery only |

## Loose objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fences, lanterns, barrels and other loose objects placed by the land scripts | done | `MobileObjectArchetype`, `MobileStaticArchetype` |
| Loose objects can be picked up, thrown and broken | partial | Picked up and thrown into the physics (`src/ECS/Physics/FromHand.cpp`); a thrown fence goes into a wood store; breaking them is not ported; see ../physics/ |
| Some loose objects crush new buildings or block them (unconfirmed which) | partial | A scaffold set down clears the things in its way (`DestroyThingsInWay` in `src/ECS/Scaffolds.cpp`); which objects block a building is not confirmed |
