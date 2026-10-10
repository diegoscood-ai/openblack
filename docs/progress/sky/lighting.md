# Lighting by time of day

The light the world is seen in: a palette of colours for the land through the day for each alignment, a dark colour, a
warm colour and the moon's colour, which light the land, the models and the sea, and close the haze in at dusk.

**Progress: 15/16 done, 1 partial — 97%**

How the original does it, in our wiki: [Day, night and weather in the original](../../bw1-notes/day-night-weather.md), [World rendering: original versus openblack](../../bw1-notes/rendering.md).

## The land's light

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The weather palette holds the good, neutral and evil colours of the land through the day | done | `LandLightPalette` and `LandLightTable` (`src/3D/LandLightTable.cpp`); tests `LandLightPalette.*` (`test/test_land_light.cpp`) |
| The land's colour of the moment comes from the palette by the sky type and the alignment the sky shows | done | `LandLightTable::Build` (sky type and the sky alignment); tests `LandLightTable.DuskClear`, `MatchesTheOldConventionEveryHour` |
| An overcast at the camera darkens the land's colour | done | `LandLightTable::Build` (no channel above 255 - 96 x overcast); test `LandLightTable.NightOverOneOvercastSmallFlash` |
| A flash of lightning takes every light and the haze towards white | done | `LandLightTable::Build` (the flash); tests `LandLightTable.NoonStormWithFlash`, `FlashZeroLeavesTheTable` |
| The dark, warm and moon colours change with the alignment | done | `LandLightTable::GetWarmColour`, `GetMoonColour` (`src/3D/LandLightTable.h`) |
| The land's light of the moment is kept by things made in it, such as a splash on the water | done | `src/3D/LandLightFrame.h` (the last table built); test `LandLightTable.RingColourFixedAtCreation` |

## Models and the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Models are lit by the land's light where they stand, and by the sun's direction | done | `src/Graphics/ModelLight.cpp`; see ../rendering/ |
| Trees take the land's brightness where they stand | done | `src/Graphics/TreeBrightness.cpp`; test `test/test_tree_brightness.cpp`; see ../nature/ |
| Mists and clouds take the land's light | done | `Clouds::Colour` (`src/3D/Clouds.cpp`) and the mists' light (`src/3D/Mists.cpp`) |
| The sea takes the land's light at full luminosity | done | see ../ocean/sea_surface.md |
| Distant things fade into a haze that is a third of the land's colour and closes in at dusk | done | `src/Graphics/Haze.cpp`, `assets/shaders/haze.sh`; test `test/test_haze_land_light.cpp` |
| Under a storm the haze thickens | done | `LandLightTable::Build` (the overcast draws the haze in) |
| At night, village lights, lanterns, the hand and fires light the cells around them | partial | `night_lights::Update` (`src/3D/NightLights.cpp`: the hand, the street lanterns and the gate's lamps), run by `VillageLightSystem::Update` (`Locator::villageLightSystem`) once a frame; the temple's lanterns are not lit; see ../town/ and ../rendering/ |
| The light at night is the moon's colour | done | `src/3D/LandLightTable.cpp` |
| Inside the temple the light comes from the temple's own lights, not the time of day | done | `src/3D/TempleLight.cpp`; see ../temple/ |
| Shadows of objects fall away from the sun | done | The static shadows are sheared along the fixed sun direction and the projected ones use the fixed sun point (`src/Graphics/ShadowMath.cpp`), as our wiki describes ([rendering.md](../../bw1-notes/rendering.md#shadows-three-systems-in-the-original)); see ../rendering/ |
