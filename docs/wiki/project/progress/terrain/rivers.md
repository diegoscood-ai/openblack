# Rivers and lakes

Rivers are laid by the land's script as a chain of points. Along each stretch a channel cuts the land so the water
underneath shows, and a bed is blended into the land's colour. Lakes are low land below sea level that shows the same
water.

**Progress: 17/18 done, 0 chosen, 1 partial, 0 inferred — 97%**

## Laying the rivers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script makes each river and gives it its points in order | done | `FeatureScriptCommands::CreateStream`, `CreateStreamPoint`; `src/ECS/Components/Stream.h`; test `WaterQueries.drinking_water` (Land1.txt's 11 rivers) |
| A river runs from each point to the next, never from the last back to the first | done | `Stream::points` in script order, `ecs::CreateRiverFootprints` (`src/ECS/Rivers.cpp`) |
| Each stretch has a channel mesh, turned and stretched to reach the next point | done | `ecs::CreateRiverFootprints` (turned to the segment, stretched by its length / 30) |
| The channel clears the land's alpha so the water drawn underneath shows as the river | done | The channel lowers the block's alpha at its prints step, the lower of the land's and the channel's (`block_texture::PrintFootprint`'s alpha mode, from `ecs::land_prints::SourcesFor`; #222), which `assets/shaders/fs_terrain.sc` takes as the land's alpha; test `BlockPrints.AnAlphaPrintOnlyLowersTheAlpha` |
| Each stretch has a bed mesh blended into the land's colour, as a building's footprint is | done | The river bed is baked into the block's texture at its prints step with the land's other footprints (`block_texture::PrintFootprint`'s colour mode, from `ecs::land_prints::SourcesFor`; #222); tests `BlockPrints.FootprintsPrintAsTheOriginal`, `BlockBake.OnlyTheLandsFootprintsOverTheBlockPrint` |
| Rivers look the same in the sea's reflection | done | The original's mirrored land (`fn_007FF4F0`) draws each block with the land's own block draw (`fn_00877D20`, `fn_00874AA0`), so with the block texture whose alpha carries the channels and the same blend; the reflection flag only swaps the indices ([sea-coast-and-rivers.md](../../../world/sea-coast-and-rivers.md#rivers)). Ours: the land alpha (the alpha of each block's own texture, `s10_blockTexture`, into which the river channels are baked) goes to the mirrored land as to the land (`assets/shaders/fs_terrain.sc`), with the land's blend (`sea_pass::SeaPassState::LandState`; test `SeaPass.MirroredLandKeepsTheLandsBlend`) |
| Rivers are saved and loaded with the game, with their points | partial 50% | loaded: a saved game's rivers come back with their ids and points as saved, then their landscape footprints (`ecs::saved_world_load::MakeStreamEntity`, `ecs::CreateRiverFootprints`, as the original's `GStream::ResolveLoad` makes each river's; test `SavedWorldLoad.TheFrozenLandOneAutosavesRiversAndMistsAreTheirScripts`: the frozen Land 1 autosave's 11 rivers have Land1.txt's points in order); the writing waits for the saves' writers ([saved games](../../../engine/saved-games.md#openblack)) |
| The game can find the nearest point of a river to a position | done | `NearestStreamPos` and `FindNearestStreamPosTo` (`src/ECS/WaterQueries.cpp`), used by the search for drinking water; tests `WaterQueries.drinking_water`, `WaterQueriesSynthetic.drinking_waterSynthetic` |

## Water in rivers and lakes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The water in a river or lake is the sea, with its rows, ripple and colour, seen through the land | done | see ../ocean/sea_surface.md |
| Lakes are land below sea level, drawn flat with the water showing | done | `LandIslandInterface::GetDrawnHeightAt`, the coast alpha (`src/3D/CoastAlpha.cpp`) |
| Running water is heard near rivers | done | Cell ambient sound type 9 (`src/Audio/SoundMap.cpp`); see ../audio/; tests `AtmosAudio.MatchesOriginalSoundMap1`, `AtmosAudio.MatchesOriginalSoundMap2` |
| Still fresh water is heard near lakes | done | Cell ambient sound type 2 (`src/Audio/SoundMap.cpp`); tests `AtmosAudio.MatchesOriginalSoundMap1`, `AtmosAudio.MatchesOriginalSoundMap2` |
| Fires near or in water go out; a fireball over water steams | done | `src/ECS/Fire/FireEffect.cpp` (water and coast cells), `src/ECS/Fire/FireGraphic.cpp`; see ../physics/; tests `FireModel.SomethingNotBurningCoolsBySurfaceOverCapacityFasterInTheWetAndRain`, `FireGraphic.FireballSteamsInsteadOfFlames` |
| A forest's new trees may stand in water, as in the original | done | The forest's new trees take any free point (`src/ECS/Trees.cpp`), a water cell counting as free, as the original does ([trees.md](../../../world/trees.md#growth), ../miracles/water.md): new trees can stand in water in both |
| A creature wades in shallow water and avoids deep water | done | `CreatureLocomotionSystem.cpp`; see ../creature/; test `FlatLand.CreaturesWadeTheShallowsButNotTheOpenWater` |
| Villagers drown in deep water; creatures neither drown nor swim | done | `src/ECS/VillagerDrowning.cpp`; tests `test/test_villager_death.cpp`, scene `physics.sinking`. The exe has drowning only for villagers (`Villager::Drowning` 0x76A780, `EnterDrowning` 0x767410) and no swimming. See ../villager/ and ../physics/ |
| The land script's waterfall makes nothing; Land 3's waterfall is fixed scenery with its flow and sound | done | `FeatureScriptCommands::CreateWaterfall` makes an empty object that is the last created one, as the original (test `FootpathsTest.ALandScriptWaterfallIsAnEmptyObjectThatIsTheLastCreated`), and the Land 3 waterfall is `src/ECS/DesignedScenery.cpp` (tests `FrameAnim.waterfallFootRings` and `FrameAnim.scrolls`); the script's waterfall object is empty in the original, it neither draws nor sounds ([water.md](../../../world/sea-life-and-scenery.md#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur)) |
| Splashes and rings on rivers and lakes where things fall in | done | The same water hit as the sea's, by the cell's altitude (below 4 rings, below 3 splashes as water; `src/ECS/Systems/Implementations/DynamicsSystem.cpp`), so a lake or a river channel at sea level splashes; scene `physics.sinking`; see ../ocean/things_on_the_water.md; test `WaterPhysicsWorld.ALowRiverOrLakeRingsBelowFourAndIsWaterBelowThree` (`test/test_water_physics_world.cpp`, on synthetic and the game's data) |
