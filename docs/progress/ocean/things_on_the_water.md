# Things on the water

What happens on and under the sea's surface: rings where things splash, the hand's light at night, fish, boats and
nets, and what floats or sinks.

**Progress: 16/19 done, 1 partial — 87%**

How the original does it, in our wiki: [Water in the game](../../bw1-notes/water.md).

## Rings and splashes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A ring of the smoke texture spreads and fades on the surface over 0.7 seconds where something splashes | done | `src/ECS/WaterRings.cpp`, `Renderer::DrawWaterRings`; test `Water.ringTimingInSinglePrecision` (`test/test_water.cpp`) |
| Rings are capped; when full a new one is not made | done | The 1024-ring pool (`src/ECS/WaterRings.h`) |
| Rings keep the land's light of when they were made | done | `ecs::AddWaterRing` fixes the colour; test `LandLightTable.RingColourFixedAtCreation` |
| Rings stop growing while the game is paused | done | The rings age with the game time (`src/ECS/WaterRings.cpp`) |
| The hand splashes where it grips the land at the water | done | `src/ECS/Systems/Implementations/HandFish.cpp`, from the grip (`HandPlacement.cpp`) |
| Things thrown or dropped into the sea splash and ring | done | The water hit's ring, foam and sound (`src/ECS/Physics/CollisionSounds.cpp`) and the bobbing ripples (`src/ECS/Physics/PhysicsObjects.cpp`); see ../physics/ |
| Rain makes rings on the water | todo | Not in our tree; see ../weather/rain.md |
| The water miracle splashes and ripples where it falls | partial | see ../miracles/water.md |
| Big splashes of spray when something heavy lands in the sea | todo | Not in our tree |

## Light on the water

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At night the hand's light glows warm on the water under it | done | `src/Graphics/HandWaterGlow.cpp`; test `HandWaterGlow.WarmsThePaletteColourTowardsOrange` |
| The glow only shows where there is water near the hand and the hand's light is strong enough | done | `src/Graphics/HandWaterGlow.cpp`; test `HandWaterGlow.ShowsOnlyNearWater` |
| Lightning lights the sea as it lights the land | done | The sea takes the light table's last entry, which the flash lifts (`LandLightTable::Build`), as our wiki describes ([day-night-weather.md](../../bw1-notes/day-night-weather.md#climate)) |

## Life and objects at sea

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Objects thrown into the sea float or sink | done | `PhysicsBody::GroundAndWater`; see ../physics/water_physics.md |
| Villagers thrown into the sea drown | done | `src/ECS/VillagerDrowning.cpp`; see ../physics/water_physics.md |
| Sharks swim off the coast and take what falls in | done | `src/ECS/Sharks.cpp` (moved by the script's walk path, with their wake). Our wiki differs: the sharks have no AI and take nothing; the script moves them ([water.md](../../bw1-notes/water.md#sharks-class-whale)) |
| Seagulls fly over the coast | done | The birds (`src/ECS/AnimalBirds.cpp`); see ../animal/ |
| Shoals of fish scatter round things in the water | done | `src/ECS/FishShoals.cpp` (a splash scares them) |
| Fishermen's nets and fish farms stand in the water, cut by the sea plane | done | `src/ECS/FishFarms.cpp`, `DrawFishPlots` (`src/Graphics/RendererFishPlot.cpp`); see ../resources/ and ../building/ |
| A boat sits on the water where the story calls for one (unconfirmed which lands) | done | The missionaries' boat of Land 1 (`src/ECS/MissionaryBoat.cpp`, `src/Graphics/RendererBoat.cpp`); see ../story/ |
