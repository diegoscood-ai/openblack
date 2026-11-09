# Cut scenes and films

The films (Bink videos) and the scripted cut scenes of the story: the camera moving through shots under cinema bars,
fades, and the advisors and characters acting them out.

**Progress: 8/20 done, 0 chosen, 7 partial, 0 inferred — 61%**

## Films

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The start-up logos, the pre-intro, the intro film, the loading screen's tip pictures and the creature's fall | partial 80% | the intro film (`SET_AVI_SEQUENCE`, `src/Video/VideoPlayer`, our own Bink decoder) and the creature's fall (`src/Video/FallingSpellVideo`) play; see ../video/bink_videos.md and [video.md](../../../video/playback.md#startup-and-loading-screen); re-measured 2026-10-21: the intro film, the fall, the logos, the pre-intro and the loading tips are shown now |

## Scripted cut scenes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene takes the camera and brings the cinema bars in | done | `StartCameraControl`, `SetWidescreen` (`src/CHLApi.cpp`, `ScreenFade`); checked in the Land 1 intro |
| The camera is set to a place and focus at once | done | `SetCameraPosition`, `SetCameraFocus` (`src/CHLApi.cpp`, `Camera/ScriptCamera`) |
| The camera glides to a place and focus over a time | done | `MoveCameraPosition`, `MoveCameraFocus`, `HasCameraArrived` (`Camera/ScriptCamera`, `test/test_script_camera_mode.cpp`); checked in the Land 1 intro, and the 2026-10-22 Land 1 runs (a new game with no skip, fixed time step) also played the second cinematic, the flight down the tunnel to the temple (it ends once the camera is within 75 of it) and the temple scene; see ../camera/script_camera.md |
| The camera follows or faces an object | done | `FocusFollow`, `PositionFollow`, `SetCameraToFaceObject`, `MoveCameraToFaceObject` (`src/CHLApi.cpp`); the intro's follow shots checked; see ../camera/follow_cameras.md |
| The camera runs a recorded camera path | partial 60% | `RunCameraPath` is real (`CameraPathSystem`); its scenes (the creatures in the glade) are not checked in game; see ../camera/camera_paths.md; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The picture fades to a colour and back between shots | done | `SetFade`, `SetFadeIn`, `FadeFinished` (`ScreenFade`); see ../camera/cinematics.md |
| Characters walk to marks, turn and play animations for the scene | partial 60% | `MoveGameThing`, `SetFocus`, `OverrideStateAnimation`, `WalkPath`, `SetScriptState` work for villagers and animals (the intro's family walks, checked) but not for creatures; re-measured 2026-10-21: they work for villagers and animals (checked in the intro), not for creatures |
| Special effects and sounds are started for the scene | partial 60% | `SpecialEffectPosition`, `SpecialEffectObject`, `PlaySoundEffect` are real (`src/CHLApi.cpp`); which effects match the original is not checked one by one; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| Game time can be set, stopped and moved on for a scene | done | `SetGameTime`, `GameTimeOnOff`, `MoveGameTime` (`src/CHLApi.cpp`) |
| Music for the scene | done | `StartMusic`, `StopMusic`, `AttachMusic` (`Audio/Services/GameMusic`); the intro's music plays; see ../audio/music.md |
| The game's speed can be slowed for a scene | done | `StartGameSpeed`, `SetGamespeed`, `EndGameSpeed` (`Help/ScriptControl`); used by the intro |
| Close camera clipping for close shots | pending | `SetGraphicsClipping` is real in `src/CHLApi.cpp` (the camera's own near plane is ported, `test/test_near_clipping.cpp`); see ../camera/cinematics.md |

## The story's set pieces

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The family and the drowning boy at the very start | partial 40% | the intro runs and is checked in game up to the hand-over (the family, the kiss, the son and the sharks, the film, the rescue); the 2026-10-22 Land 1 runs (a new game with no skip, fixed time step) reached the hand-over (text 5219) at about 153 s; the welcome dance (`DanceCreate`) and the high-detail family (`SetHighGraphicsDetail`) are real now, the family's high detail not checked against the original; see [land_1.md](land_1.md) and [tutorial.md](tutorial.md); re-measured 2026-10-22: every native it calls is real or works in part |
| The temple's completion | partial 60% | `CitadelGuide` calls only real natives; the 2026-10-22 Land 1 runs (a new game with no skip, fixed time step) played it through: texts 1998 and 2000, the player taken into the temple through its entrance and out with Escape, then 5212, 5199 and 5232, and Choose Your Creature starts; not compared with the original shot by shot; see [gold_scrolls/the_temple_is_finished.md](gold_scrolls/the_temple_is_finished.md) |
| The creatures in the glade | pending | the camera part of `CreaturesInGlade` is real; `CREATE` of a creature makes the glade's creatures, and `CreatureDoAction` forces the scripted actions through the creature's mind (`CreatureMindSystem::ScriptDoAction`); of the glade's action rows, 81 SitDown, 82 LookAtCamera, 168 PointAtCamera, 188 LookButDontApproach and 189 LookForever have an executor (`src/Creature/CreaturePlanActions.cpp`), 163 PointAtObject has none (nothing is shown); whether every `PLAYED` wait after them ends is not checked: no run has reached the glade yet |
| Khazar's arrival and death | pending | not yet reached (Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real), and the computer gods' hand natives are real; not checked in play; see land_2.md |
| Lethys stealing the creature | pending | not yet reached (Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real); the creature natives it needs are real or work in part (`CreatureDoAction` goes through the mind; a row with no executor shows nothing) |
| The vortex opening between lands | partial 60% | a script opens a vortex (`CREATE` of a vortex, `VortexParameters`, `src/ECS/Vortex`); Land 1's exit vortex opens at once in a game that skips the guide; the films are not done, and going through (`LOAD_MAP` changes land now) is not yet checked; see [portals.md](portals.md); re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| Nemesis's curse, the big fight and the ending | pending | not yet reached (the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real); see ending.md |
