# Special effects

The game's smaller visual effects that aren't particle effects: beams and glows of light, the hand's and the villages'
lights at night, footprints and rings on the water, highlights placed by challenges, chimney smoke and the effects laid
over the whole picture. The sea's reflections are in [../ocean/](../ocean/), rain splashes in
[../weather/](../weather/), gesture trails in [../gesture/](../gesture/).

**Progress: 12/18 done, 0 chosen, 3 partial, 0 inferred — 80%**

How the original does it, in our wiki: [Graphics engine parity: original versus openblack](../../../rendering/frame.md), [World rendering: original versus openblack](../../../rendering/frame.md).

## Lights and glows

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spot light's beam: an open cone of eight sides drawn additively from the light, fading to black, its texture drifting | done | `AppendCone` in `src/Graphics/LightBeams.cpp`, drawn by the temple (`src/Graphics/RendererTemple.cpp`); test `test/graphics/test_light_beams.cpp` |
| Windows shed a volume of light drawn out from their edges, fading to nothing | done | `MakeVolumeLight` in `src/Graphics/LightBeams.cpp`, drawn by the temple; test `test/graphics/test_light_beams.cpp` |
| At night the hand carries a light that brightens the land around it, coming up as the land darkens | done | `src/Graphics/HandLight.cpp`; test `test/graphics/test_hand_light.cpp` |
| At night the hand's light glows warm on the water beneath it | done | `src/Graphics/HandWaterGlow.cpp`; test `test/graphics/test_hand_water_glow.cpp` |
| Village lanterns and campfires light the land around them at night, flickering | partial 90% | street and country lanterns and the gate's lamps (`src/3D/NightLights.cpp`, run by `VillageLightSystem::Update`, `Locator::villageLightSystem`; `StreetLanternArchetype`) light the land cells at night, flickering, and so do the temple door's two lamps (`worship::citadel_lanterns`). Our wiki differs: the lit "campfires" are country lanterns drawn with the campfire mesh; a bonfire itself has no lantern light ([map-loading.md](../../../scripts/map-script-objects.md#map-script-objects-street-lanterns-bonfires-dead-trees-gates)); re-measured 2026-10-21: the lanterns and lamps light the land at night, flickering; the wiki's campfire naming is to be put right |

The columns of light over village centres and temples, the light round worship sites' altars, the glow of disciples,
the scroll and vortex beams and the falling-spell bursts are in [light_beams.md](light_beams.md).

## Marks on the land and water

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creatures leave prints as their footsteps fall, fading over about five seconds; they are not saved | done | `FootprintSystem` (`src/ECS/Systems/Implementations/FootprintSystem.cpp`), `src/Creature/CreatureFootprints.cpp`; tests `CreatureFootprints.GoneInAboutFiveSeconds`, `FootprintSystemTest.*` |
| Each species leaves its own print at its own size; the crocodile's has no size | done | `CreatureFootprints.h`; test `CreatureFootprints.SpeciesCells` |
| On the first of April every creature leaves smiley faces | done | `CreatureFootprints.h`; tests `CreatureFootprints.AprilFoolsSmileyKeepsTheSpeciesSize`, `FootprintSystemDate.*` |
| Prints stop being laid when there are as many as there can be | done | `CreatureFootprints`; test `CreatureFootprints.FullTrailDropsNewPrints` |
| Creatures leave wet prints after walking out of water, placed by species | pending |  |
| Rings spread on the water where something splashes, growing and fading over 0.7 seconds | done | `src/3D/WaterRings.cpp`, `src/Particles/ParticleWaterRings.cpp`: the hand's splash, thrown objects, sharks, the waterfall and the fish puzzle; test `test/test_particle_water.cpp` |

## Highlights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Challenges place highlights that pulse with a beam and can be tapped | partial 60% | `src/ECS/ScriptHighlight.cpp`: highlights are made, pulse and can be tapped (`InterfaceTap`), but their glow column and sprite are worked out and not drawn; the beams are in [light_beams.md](light_beams.md); the scrolls in [../interface/scrolls_and_signs.md](../interface/scrolls_and_signs.md) |
| Highlighted things are drawn in a highlight material | pending | unconfirmed which things use it |

## Smoke

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Smoke rises from the chimneys of homes | done | `src/3D/ChimneySmoke.cpp`, `src/Graphics/RendererSmoke.cpp`; tests `ChimneySmoke.PuffsRiseAndStartAgainAtTheChimney`, `ChimneySmoke.LitWhileSomeoneIsHome` |

## Over the whole picture

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts fade the picture to a colour and back over whole seconds | done | `src/Gui/ScreenFade.cpp` (SET_FADE and SET_FADE_IN per turn), drawn in the screen overlay pass; tests `ScriptFade.FadesToAColourOverTurns`, `ScriptFade.TheTimeIsTruncatedToWholeSeconds`, `ScriptFade.FadesBackToClear` |
| Cinema bars slide in for cut scenes, hiding the dialogs | done | `ScreenFade` bars over 2 s and snapped on for films (`src/Gui/ScreenFade.cpp`, `src/Video/VideoPlayer.cpp`); the dialogs are put away as the bars come in (`gui::HideAllDialogs`, see ../camera/cinematics.md); tests `CinemaBars.SlideInAndTurnBackFromWhereTheyAre`, `CinematicDirector.AFilmSnapsTheBarsIn`, `HideAllDialogs.TheMenuClosesAndTheTempleBoxesGoWithNeitherOkNorCancel` |
| Motion blur | pending | the renderer keeps a motion blur amount (unconfirmed when the game uses it) |
| Scripts take screenshots of the world for the challenge room's pictures | partial 85% | a challenge's record asks for its picture (`KeepChallengeRecord`, `src/CHLApi.cpp`), taken of the frame in memory, sampled, meaned and softened as the original (`ecs::challenge_pictures`, `src/ECS/ChallengePictures.cpp`, `RendererInterface::RequestScreenPicture`); of the frame already on the screen as the script runs, as the original's, the frame drawn aside and copied to the window while a running script can take one and read back only for a snapshot (`graphics::ShownFrameTargets`, frame time unchanged within the noise, 3.6 to 3.9 ms against 3.6 to 4.7 ms a frame on Land 1 at 1536 × 864, 2026-10-29); tests `ChallengePictures.*`; see [../temple/challenge_room.md](../temple/challenge_room.md) |
