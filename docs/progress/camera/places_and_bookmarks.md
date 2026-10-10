# Places and bookmarks

The keys that fly the camera to set places (the temple, the realm, the creature, the temple's rooms), the bookmarks the
player drops on the land to fly back to, and where the camera starts on a land.

**Progress: 3/20 done, 10 partial — 40%**

## Temple and realm keys

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A single tap of the temple key puts the view back to a pleasing one over what the camera looks at | partial | `ZoomToPlaces` (`src/Camera/ZoomToPlaces.cpp`; `ZoomToPlaces.ASingleTapViewsWhatIsLookedAtFromAPleasingHeight` in `test/camera/test_zoom_to_places.cpp`) is not wired: the temple key does nothing in the game yet |
| A double tap of the temple key flies the camera to the player's temple | partial | `ZoomToPlaces.ADoubleTapFliesToTheTempleAndAnotherFliesBack`; not wired into the game |
| Without a temple the temple key flies over the realm | partial | `ZoomToPlaces.WithoutATempleTheDoubleTapFliesOverTheRealm`; not wired |
| The realm key flies the camera high over the middle of the island | partial | `ZoomToPlaces.TheRealmKeyFliesOverTheRealmAndBack`; not wired |
| Pressing the key again while still looking there flies back to where the camera was | partial | `ZoomToPlaces.LookingAwayTheRealmKeyFliesThereAgain`; not wired |
| The keys do nothing inside the temple or while a script's cinema bars are in | todo | the temple and realm keys are not wired, so there is nothing to block; the temple's room keys are read even during script cameras (`Game::ProcessTempleRoomKeys`) |
| The keys that go straight into each room of the temple | done | `Game::ProcessTempleRoomKeys` (`src/Game.cpp`): `ZOOM_TO_INSIDE_TEMPLE` to `ZOOM_TO_LIBRARY` (`src/Input/KeyBindings.h`) enter the temple at that room or cut to it; see [temple_camera.md](temple_camera.md) and `../temple/` |
| The creature key flies the camera to the player's creature | partial | our tree locks the camera onto the creature instead (Creature Mode, see [follow_cameras.md](follow_cameras.md)); how the game's key behaves exactly is unconfirmed |

## Bookmarks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Ctrl with a number key drops a bookmark where the hand is | partial | `src/Game.cpp` key handling, `CameraBookmarkSystem::SetBookmark`; eight bookmarks, set only while the hand is on the land |
| A number key flies the camera back to its bookmark, looking from where it was when dropped | partial | `src/Game.cpp`: `SetFlight` to the bookmark's saved camera position (Creature Mode let go first); not compared with the game |
| Bookmarks show on the land as markers | partial | `CameraBookmarkSystem::Update` pulses a placeholder marker; the game's look not matched |
| Bookmarks can be cleared | todo | `CameraBookmarkSystem::ClearBookmark` exists but nothing calls it; `SET_BOOKMARK_POSITION` is a stub in `src/CHLApi.cpp` |
| Keys step to the next and previous bookmark | todo | not in our tree (unconfirmed key) |
| A key flies the camera to the player's own town | todo | not in our tree (unconfirmed key) |
| A bookmark on a moving thing follows it | todo | not in our tree (unconfirmed) |
| The camera can go back to where it was before jumping to a bookmark | todo | not in our tree |
| Bookmarks are kept in saved games | todo | no saved games; see `../engine/` |
| Scripts can turn the bookmark keys off and on, and a script restart turns them back on | done | (added) `PLAY_JC_SPECIAL` 14 and 15 (`src/CHLApi.cpp`) call `CameraBookmarkSystem::SetEnabled`, the bookmark keys in `src/Game.cpp` check it, and the script reboot sets it again ([intro](../../bw1-notes/intro.md#jc-specials-and-the-confirmation-sounds)) |

## Start of a land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script sets where the camera starts, looking towards the player's temple | done | `START_CAMERA_POS` in `src/LHScriptX/FeatureScriptCommands.cpp`; `test/camera/test_set_camera_pos.cpp` (land 1, two and three gods) |
| The start position with four gods | partial | `SetCameraPos.DISABLED_setCameraFourGods` is disabled |
