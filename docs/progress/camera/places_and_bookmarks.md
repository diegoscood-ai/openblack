# Places and bookmarks

The keys that fly the camera to set places (the temple, the realm, the temple's rooms), the bookmarks the player drops
on the land to fly back to, and where the camera starts on a land. The creature key locks a follow camera onto the
creature: it is in [follow_cameras.md](follow_cameras.md).

**Progress: 2/21 done, 0 chosen, 11 partial, 0 inferred — 40%**

## Temple and realm keys

The original's keys are read in [the one-press keys](../../../interface/screen-and-readouts.md#the-one-press-keys-temple-realm-villager-names);
the keys are wired through `input::ShortcutKeys` (`src/Input/ShortcutKeys.cpp`, called each frame from `src/Game.cpp`).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A single tap of the temple key puts the view back to a pleasing one over what the camera looks at | partial 50% | `zoom_to::ZoomToPlaces::PressTemple` (`src/Camera/ZoomToPlaces.cpp`); `ZoomToPlaces.ASingleTapViewsWhatIsLookedAtFromAPleasingHeight`, `TapsFurtherApartThanTheDoubleTapTimeAreSingle`, `ThePitchesAreTheGamesFloats` (`test/camera/test_zoom_to_places.cpp`); the 500 ms, the distance 50 and the pitch are the original's. Missing: the original's point is along the view at the click parameters' mean distance, ours the land under the focus; the original flies there at once and builds the origin with its own point-from-angles; it times taps by the system clock |
| A double tap of the temple key flies the camera to the player's temple | partial 70% | `ZoomToPlaces.ADoubleTapFliesToTheTempleAndAnotherFliesBack`, `ShortcutKeys.TheTempleKeyZoomsToTheTemple` (`test/input/test_shortcut_keys.cpp`); 130 and the pitch under a quarter turn are the original's. Missing: the flight's 0.4 s (`SetFlight` has no time), the temple's map-coordinate rounding, the original's point-from-angles, the player's camera mode switch and the camera help's report |
| Without a temple the temple key flies over the realm | partial 70% | `ZoomToPlaces.WithoutATempleTheDoubleTapFliesOverTheRealm`. Missing: as the row above |
| The realm key flies the camera high over the middle of the island | partial 70% | `ZoomToPlaces.TheRealmKeyFliesOverTheRealmAndBack`, `ShortcutKeys.TheRealmKeyZoomsOverTheRealm`, `TheRealmIsHalfwayBetweenTheFirstAndLastBlocks`; 2200 and an eighth of a turn, and the realm point, are the original's. Missing: the 0.4 s flight and the rest as above |
| Pressing the key again while still looking there flies back to where the camera was | partial 70% | `ZoomToPlaces.LookingAwayTheRealmKeyFliesThereAgain`, `FlyingBackFromTheRealmEndsTheTap`; within 100 of the focus, as the original. Missing: the way back's 0.4 s |
| The keys do nothing inside the temple or while a script's cinema bars are in | done | `ShortcutKeys::Update` gates; `ShortcutKeys.NoKeyActsWhileAScriptHoldsTheCinemaBars`, `NoKeyActsInsideTheTemple`, `ASingleTapDoesNotMoveTheCameraWatchingAFight`; as the original's action pass and the temple's own key handler |
| The tutorial interface levels can hold back the temple and realm keys | done | (added) `help::interface_interaction::IsActionBlocked` asked by `ShortcutKeys`; `ShortcutKeys.TheInterfaceInteractionBlocksThePlaceKeysButNotTheNames`; the original's key switches ([one-press keys](../../../interface/screen-and-readouts.md#the-one-press-keys-temple-realm-villager-names)) |
| Holding the temple or realm key acts again on each repeat the system sends, and both keys in one frame both act | pending | (added) openblack acts once a press, and only on the temple key when both come in one frame ([pending](../../../interface/screen-and-readouts.md#pending)) |
| The keys that go straight into each room of the temple | partial 50% | `Game::ProcessTempleRoomKeys` (`src/Game.cpp`): `ZOOM_TO_INSIDE_TEMPLE` to `ZOOM_TO_LIBRARY` (`src/Input/KeyBindings.h`) enter the temple at that room or cut to it; see [temple_camera.md](temple_camera.md) and `../temple/`. Missing: no test; outside the temple the original's action pass does nothing while a script holds the cinema bars, where ours reads the room keys during script cameras and hand demos too; which room each action opens is not read |

## Bookmarks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
Read in the original on 2026-10-21 (bw1-decomp's bookmarks and the exe's key pass): eight bookmarks on the keys 1 to 8,
a ninth for the town and one for the view before a jump.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Ctrl with a number key drops a bookmark where the hand is | partial 50% | `src/Game.cpp` key handling, `CameraBookmarkSystem::SetBookmark`; the gates are the original's (not while a script holds the cinema bars or a tutorial level has a key switch off, only while the bookmarks are on). Missing: no test; the original drops it at the hand's point on the land whenever the key is pressed, and on the thing under the hand when there is one, where ours needs the hand on the land and keeps a fixed point; Ctrl with Alt does not drop one in the original |
| A number key flies the camera back to its bookmark, looking from where it was when dropped | partial 50% | `src/Game.cpp`: `SetFlight` to the bookmark's saved camera position (Creature Mode let go first); a later camera input drops the flight (`WorldCameraMouse.AKeyboardMoveDropsABookmarkFlight`, `AZoomDropsABookmarkFlight`). Missing: the original flies in 0.3 s to the bookmark's point plus the camera's offset kept at the drop, not inside the temple, and keeps the view it left as the before-jump bookmark; only the number key alone goes there, where ours goes with Shift or Alt held too ([the speed keys](../../../engine/game-clock.md#pending)) |
| Bookmarks show on the land as markers | partial 30% | `CameraBookmarkSystem::Update` pulses a placeholder marker. Missing: the original's marker pulses its width and height on a 2 s period, 2 above the point, hidden while a script holds the cinema bars and while the bookmarks are off; its picture is not matched; no test |
| Ctrl and Shift with a number key clear its bookmark, and a new land clears them all | pending | (reworded from "bookmarks can be cleared": the keys are the original's) `CameraBookmarkSystem::ClearBookmark` exists but nothing calls it. `SET_BOOKMARK_POSITION` does nothing in the original either (it only pops its four values), as our stub in `src/CHLApi.cpp` |
| Ctrl and Tab step to the next bookmark, Shift and Tab to the previous one | pending | (the keys read in the exe's key pass) not in our tree |
| Tab flies the camera to the player's next town, at its storage pit or else its centre | pending | (the key read in the exe's key pass) not in our tree; the view and offsets are in [flights to a place](../../../camera/player-camera.md#player-camera-flights-to-a-place) |
| Shift with a number key makes the camera follow the thing its bookmark is on, and again stops | pending | (the key read in the exe's key pass) not in our tree; our bookmarks are only points |
| The camera can go back to where it was before jumping to a bookmark | pending | not in our tree; the original keeps that view as a bookmark of its own at each jump. Which key goes back is not determined (a case of the exe's key table not read) |
| Bookmarks are kept in saved games | pending | no saved games; see `../engine/` |
| Scripts can turn the bookmark keys off and on, and a script restart turns them back on | partial 80% | (added) `PLAY_JC_SPECIAL` 14 and 15 (`src/CHLApi.cpp`) call `CameraBookmarkSystem::SetEnabled`, the bookmark keys in `src/Game.cpp` check it, and every land load (the map's clear, `InitStaticsValues`) and the scripts' restart set it again, as the exe ([intro](../../../scripts/land1-intro.md#jc-specials-and-the-confirmation-sounds), [what a land change keeps](../../../scripts/land-change-and-saving.md#what-a-land-change-keeps)). Missing: a test |

## Start of a land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script sets where the camera starts, looking towards the player's temple | partial 60% | `START_CAMERA_POS` in `src/LHScriptX/FeatureScriptCommands.cpp`; `test/camera/test_set_camera_pos.cpp` (land 1, two and three gods). Missing: the original's START_CAMERA_POS has not been read; ours builds the origin with glm from 120 at fixed angles ([pending](../../../camera/player-camera.md#pending)) |
| The start position with four gods | pending | `SetCameraPos.DISABLED_setCameraFourGods` is disabled (`TODO(#562)`): its expected view is not met, and the original's rule is not read |
