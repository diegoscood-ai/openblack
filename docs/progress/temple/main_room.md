# Main room

The temple's central hall: a pool with the island in relief floating over it as a map, five buttons choosing what the
map shows, a scroll of the world's statistics, and doors to every other room.

**Progress: 15/23 done, 4 partial — 74%**

## The room

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The room is drawn with its floor reflecting it | done | `Renderer::DrawTempleReflection` (`src/Graphics/RendererTemple.cpp`), `RenderingSystemTemple`; only the main room is mirrored, as our wiki's rendering page says |
| The pool shimmers, drawn twice over itself, turned | done | `Renderer::DrawTemplePool`, timed by `TempleInterior::GetPoolTime` |
| The temple is lit by the alignment of the realm the player came in from: red pulse for evil, slow rainbow for good | done | `src/3D/TempleLight.cpp`; test `test_temple_light` |
| The doors swing open as the camera walks through them, with door sounds | done | `src/3D/TempleDoors.cpp`; test `test_temple_doors` |
| Signs over the doors name the rooms and light up for the door under the hand | done | `src/3D/TempleSigns.cpp`; test `TempleSigns.TheMainRoomLightsTheSignOfTheDoorUnderTheCursor` |
| Tooltips say what the hand is over and what clicking does | partial | `src/3D/TempleToolTips.cpp`, shown by `TempleInterior::UpdateToolTips`; no test of its own; the pictures' and replay tooltips are todo |
| Moving about the room | done | see ../camera/temple_camera.md (`src/Camera/TempleCameraModel.cpp`) |

## The map in the pool

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The island floats over the pool in relief, from the land's heights and brightness, fading out at the coast | done | `src/3D/TempleMap.cpp`, `Renderer::DrawTempleMap`; test `test_temple_map` |
| The map's land texture is drawn afresh each visit | done | `TempleInterior::Activate` frames the map again and counts the visit (`GetVisits`, read by `Renderer::DrawTempleMapPass`) |
| Markers turn over the map for the temples and the creatures, in their player's colour | partial | `TempleInterior::UpdateMapMarkers`, `Renderer::DrawTempleMapMarkers`; test `TempleMap.ColoursMarkersByTheirPlayersLightened`; some lands give players others' colours (TODO in `src/3D/TempleMap.cpp`) |
| Markers for the challenges not done, miracles being cast and the players' influence | todo | TODO in `TempleInterior::UpdateMapMarkers` |
| A double click on the map leaves the temple for that place | done | `TempleInterior::LeaveForMapPoint`; see ../camera/temple_camera.md |
| The hand feels the click on the map | todo | TODO in `TempleInterior::LeaveForMapPoint` |

## The five buttons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Buttons choose what the map shows: temples, creatures, miracles being cast, influence and challenges | done | `src/3D/TempleToggles.cpp`; test `test_temple_toggles` |
| A button is pressed in or out, turning over with a click | done | `TempleToggles`; test `TempleToggles.TurnOverAsTheyAreLetGoOf` |
| Each button plays its own pitch of click | done | `TempleToggles::ClickOf`: each button's own down and up pitch, the creatures button's slip kept; test `TempleToggles.EachClicksAtItsOwnPitch` |
| Turning influence on recolours the map's land by owner | todo | TODO in `src/3D/TempleToggles.cpp` |
| The buttons' settings are kept with the saved game | todo | our tree has no saved game; see ../engine/saving_and_loading.md |

## The world's statistics scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scroll of the world: believers, men and women, births and deaths, sacrifices, buildings, wonders and disciples | partial | `src/3D/TempleScrolls.cpp`: the population is counted (`GatherScrollFacts` in `TempleInterior.cpp`), the rest is made up (`TempleScrolls::Facts::Mock`); `src/Game/GameStats.cpp` does not feed it yet ([../interface/statistics_counted.md](../interface/statistics_counted.md)) |
| The scroll turns up and down as the mouse drags it | done | `src/3D/TempleScroll.cpp`; test `test_temple_scroll` |
| Clicking a scroll brings the camera close and its text is drawn in front of it | done | `TempleScrolls::SetFocus` (from `TempleInterior`), with the camera's woosh (test `TempleSounds.TheCameraWooshesOneOfFourByTheClock`); see ../camera/temple_camera.md |
| Scrolls squeak as they turn | done | `src/3D/TempleScrolls.cpp`; tests `TempleScrolls.SqueakAsTheyStartToTurnAndAgainAfterStopping`, `TempleSounds.TheScrollsSqueakOneOfSixByTheClock` |
| Scroll text is written onto parchment in the game's font | partial | `src/3D/TempleScroll.cpp`, `src/3D/OrientedText.cpp`; tests `TempleScrollTexture.*`, `TempleScrollText.*`; the half-height glyphs and the pictures are todo (TODOs in `TempleScroll.cpp`) |
