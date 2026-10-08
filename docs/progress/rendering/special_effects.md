# Special effects

The game's smaller visual effects that aren't particle effects: beams and glows of light, the hand's and the villages'
lights at night, footprints and rings on the water, highlights placed by challenges, chimney smoke and the effects laid
over the whole picture. The sea's reflections are in [../ocean/](../ocean/), rain splashes in
[../weather/](../weather/), gesture trails in [../gesture/](../gesture/).

**Progress: 12/18 done, 2 partial — 72%**

How the original does it, in our wiki: [Graphics engine parity: original versus openblack](../../bw1-notes/parity.md), [World rendering: original versus openblack](../../bw1-notes/rendering.md).

## Lights and glows

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spot light's beam: an open cone of eight sides drawn additively from the light, fading to black, its texture drifting | done | `AppendCone` in `src/Graphics/LightBeams.cpp`, drawn by the temple (`src/Graphics/RendererTemple.cpp`); test `test/graphics/test_light_beams.cpp` |
| Windows shed a volume of light drawn out from their edges, fading to nothing | done | `MakeVolumeLight` in `src/Graphics/LightBeams.cpp`, drawn by the temple; test `test/graphics/test_light_beams.cpp` |
| At night the hand carries a light that brightens the land around it, coming up as the land darkens | done | `src/Graphics/HandLight.cpp`; test `test/graphics/test_hand_light.cpp` |
| At night the hand's light glows warm on the water beneath it | done | `src/Graphics/HandWaterGlow.cpp`; test `test/graphics/test_hand_water_glow.cpp` |
| Village lanterns and campfires light the land around them at night, flickering | partial | street and country lanterns and the gate's lamps (`src/3D/NightLights.cpp`, run by `VillageLightSystem::Update`, `Locator::villageLightSystem`; `StreetLanternArchetype`) light the land cells at night, flickering; the temple door's lanterns are missing. Our wiki differs: the lit "campfires" are country lanterns drawn with the campfire mesh; a bonfire itself has no lantern light ([map-loading.md](../../bw1-notes/map-loading.md#map-script-objects-street-lanterns-bonfires-dead-trees-gates)) |

The columns of light over village centres and temples, the light round worship sites' altars, the glow of disciples,
the scroll and vortex beams and the falling-spell bursts are in [light_beams.md](light_beams.md).

## Marks on the land and water

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creatures leave prints as their footsteps fall, fading over about five seconds; they are not saved | done | `FootprintSystem` (`src/ECS/Systems/Implementations/FootprintSystem.cpp`), `src/Creature/CreatureFootprints.cpp`; tests `CreatureFootprints.GoneInAboutFiveSeconds`, `FootprintSystemTest.*` |
| Each species leaves its own print at its own size; the crocodile's has no size | done | `CreatureFootprints.h`; test `CreatureFootprints.SpeciesCells` |
| On the first of April every creature leaves smiley faces | done | `CreatureFootprints.h`; tests `CreatureFootprints.AprilFoolsSmileyKeepsTheSpeciesSize`, `FootprintSystemDate.*` |
| Prints stop being laid when there are as many as there can be | done | `CreatureFootprints`; test `CreatureFootprints.FullTrailDropsNewPrints` |
| Creatures leave wet prints after walking out of water, placed by species | todo |  |
| Rings spread on the water where something splashes, growing and fading over 0.7 seconds | done | `src/ECS/WaterRings.cpp`, `src/Particles/PSysWaterRings.cpp`: the hand's splash, thrown objects, sharks, the waterfall and the fish puzzle; test `test/test_psys_water.cpp` |

## Highlights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Challenges place highlights that pulse with a beam and can be tapped | partial | `src/ECS/ScriptHighlight.cpp`: highlights are made, pulse and can be tapped (`InterfaceTap`), but their glow column and sprite are worked out and not drawn; the beams are in [light_beams.md](light_beams.md); the scrolls in [../interface/scrolls_and_signs.md](../interface/scrolls_and_signs.md) |
| Highlighted things are drawn in a highlight material | todo | unconfirmed which things use it |

## Smoke

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Smoke rises from the chimneys of homes | done | `src/ECS/ChimneySmoke.cpp`, `src/Graphics/RendererSmoke.cpp` |

## Over the whole picture

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts fade the picture to a colour and back over whole seconds | done | `src/3D/ScreenFade.cpp` (SET_FADE and SET_FADE_IN per turn), drawn in the screen overlay pass |
| Cinema bars slide in for cut scenes, hiding the dialogs | done | `ScreenFade` bars over 2 s and snapped on for films (`src/3D/ScreenFade.cpp`, `src/Video/VideoPlayer.cpp`) |
| Motion blur | todo | the renderer keeps a motion blur amount (unconfirmed when the game uses it) |
| Scripts take screenshots of the world for the challenge room's pictures | todo | see [../temple/](../temple/) |
