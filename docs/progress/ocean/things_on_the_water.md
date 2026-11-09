# Things on the water

What happens on and under the sea's surface: rings where things splash, the hand's light at night, fish, boats and
nets, and what floats or sinks.

**Progress: 20/20 done, 0 chosen, 0 partial, 0 inferred — 100%**

How the original does it, in our wiki: [Water](../../../world/water.md) and [Sea life and fixed scenery](../../../world/sea-life-and-scenery.md).

## Rings and splashes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A ring of the smoke texture spreads and fades on the surface over 0.7 seconds where something splashes | done | `src/3D/WaterRings.cpp`, `Renderer::DrawWaterRings`; test `Water.ringTimingInSinglePrecision` (`test/test_water.cpp`) |
| Rings are capped; when full a new one is not made | done | The 1024-ring pool (`src/3D/WaterRings.h`); test `WaterRingSystem.HoldsAtMostItsRingsAndLetsThemGo` |
| Rings keep the land's light of when they were made | done | `ecs::AddWaterRing` fixes the colour; test `LandLightTable.RingColourFixedAtCreation` |
| Rings stop growing while the game is paused | done | The rings age with the game time (`src/3D/WaterRings.cpp`); test `WaterRings.AgesAtItsRateInWholeMilliseconds` |
| The hand splashes where it grips the land at the water | done | `src/ECS/Systems/Implementations/HandFish.cpp`, from the grip (`HandPlacement.cpp`) |
| Things thrown or dropped into the sea splash and ring | done | The water hit's ring, foam and sound (`src/ECS/Systems/Implementations/DynamicsSystem.cpp`) and the bobbing ripples; see ../physics/ and [water.md](../../../world/water.md#impacts-and-objects-that-fall-into-the-water) |
| Rain makes rings on the water | done | Heavy rain's drops ring the sea, and the land, every frame a tile is drawn (`RainSystem::Splash`, `rain::Splash`; tests `Rain.SplashFallsOnItsBlockInTheGamesOrder`); scenes `light.rain_splashes(_night)`; see ../weather/rain.md |
| The water miracle splashes and ripples where it falls | done | The miracle's drop rings (`src/Magic/Spells/SpellWater.cpp`, ../miracles/water.md). The game's rings never drift, so the ring list draws what its slots did ([water.md](../../../world/water.md#water-rings)); test `WaterRingSystem.RingsStayWhereTheyWereMade` |

## Light on the water

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At night the hand's light glows warm on the water under it | done | `src/Graphics/HandWaterGlow.cpp`; test `HandWaterGlow.WarmsThePaletteColourTowardsOrange` |
| The glow only shows where there is water near the hand and the hand's light is strong enough | done | `src/Graphics/HandWaterGlow.cpp`; test `HandWaterGlow.ShowsOnlyNearWater` |
| Lightning lights the sea as it lights the land | done | The sea takes the light table's last entry, which the flash lifts (`LandLightTable::Build`), as our wiki describes; test `test/test_land_light.cpp` (flash cases), scene `examples.storm_lightning` ([weather.md](../../../world/weather.md#climate)) |

## Life and objects at sea

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Objects thrown into the sea float or sink | done | `Body::TouchGround`; test `PhysicsBodyWaterTest.*` (`test/test_physics_body_water.cpp`); see ../physics/water_physics.md |
| Villagers thrown into the sea drown | done | `src/ECS/VillagerDrowning.cpp`; test `test/test_villager_death.cpp`; see ../physics/water_physics.md |
| Sharks swim off the coast along the land script's walk path, with their wake rings; they take nothing | done | `src/ECS/Sharks.cpp`, `SharkSystem.cpp`; tests `SharkRules.*` (`test/test_shark_rules.cpp`); [sea-life-and-scenery.md](../../../world/sea-life-and-scenery.md#sharks) |
| Seagulls fly where the land places them, like the other birds | done | The birds (`src/ECS/AnimalBirds.cpp`); test `test/test_bird_rules.cpp`; see ../animal/ |
| Shoals of fish scatter round things in the water | done | `src/ECS/FishShoals.cpp` (a splash scares them: the hand's, a thing falling in, and a creature's foot set down below a height of 1, `FootprintSystem::Step`); tests `FishShoal.*` (`test/test_fish_farm.cpp`), `FootprintSystemTest.AFootSetDownInTheWaterSplashesTheFish` |
| Fishermen's nets and fish farms stand in the water, cut by the sea plane | done | `src/ECS/FishFarms.cpp`, `DrawFishPlots` (`src/Graphics/RendererFishPlot.cpp`); tests `FishFarm.*`; see ../resources/ and ../building/ |
| The missionaries' boat sails off Land 1 with its sailor and missionaries | done | `src/ECS/MissionaryBoat.cpp`, `src/Graphics/RendererBoat.cpp`; [sea-life-and-scenery.md](../../../world/sea-life-and-scenery.md#the-missionaries-boat-petitnavire); see ../story/ |
| The people on the boat take the hull's colour, lit as the boat is | done | `missionary_boat::PeopleLightOf` / `PeopleColoursAt` (`src/ECS/MissionaryBoat.cpp`), set as the deck's colour in `DrawColoursOf` (`RenderingSystem.cpp`): the land light at the hull's place, no haze, specular 0; test `MissionaryBoatDeck.TheDeckTakesTheHullsColour`; debug: World > Missionaries' boat ([sea-life-and-scenery.md](../../../world/sea-life-and-scenery.md#the-missionaries-boat-petitnavire)) |
| The sailors launching the boat take the land light of their own place, without the haze or a specular | done | `missionary_boat::PeopleLightOf` / `PeopleColoursAt` as a set colour in `DrawColoursOf`; black off the map, where the specular is written over the colour; test `MissionaryBoatDeck.TheSailorsAtTheLaunchTakeTheLightOfTheirPlace` ([sea-life-and-scenery.md](../../../world/sea-life-and-scenery.md#the-missionaries-boat-petitnavire)) |
